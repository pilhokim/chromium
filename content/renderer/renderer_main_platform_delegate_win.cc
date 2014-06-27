// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_main_platform_delegate.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/win/scoped_comptr.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "content/common/sandbox_win.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/injection_test_win.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/render_thread_impl.h"
#include "sandbox/win/src/sandbox.h"
#include "skia/ext/vector_platform_device_emf_win.h"
#include "third_party/WebKit/public/web/win/WebFontRendering.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "third_party/skia/include/ports/SkTypeface_win.h"

#ifdef ENABLE_VTUNE_JIT_INTERFACE
#include "v8/src/third_party/vtune/v8-vtune.h"
#endif

#include <dwrite.h>

namespace content {
namespace {

// Windows-only skia sandbox support
// These are used for GDI-path rendering.
void SkiaPreCacheFont(const LOGFONT& logfont) {
  RenderThread* render_thread = RenderThread::Get();
  if (render_thread) {
    render_thread->PreCacheFont(logfont);
  }
}

void SkiaPreCacheFontCharacters(const LOGFONT& logfont,
                                const wchar_t* text,
                                unsigned int text_length) {
  RenderThreadImpl* render_thread_impl = RenderThreadImpl::current();
  if (render_thread_impl) {
    render_thread_impl->PreCacheFontCharacters(
        logfont,
        base::string16(text, text_length));
  }
}

// Windows-only DirectWrite support. These warm up the DirectWrite paths
// before sandbox lock down to allow Skia access to the Font Manager service.
bool CreateDirectWriteFactory(IDWriteFactory** factory) {
  typedef decltype(DWriteCreateFactory)* DWriteCreateFactoryProc;
  DWriteCreateFactoryProc dwrite_create_factory_proc =
      reinterpret_cast<DWriteCreateFactoryProc>(
          GetProcAddress(LoadLibraryW(L"dwrite.dll"), "DWriteCreateFactory"));
  if (!dwrite_create_factory_proc)
    return false;
  CHECK(SUCCEEDED(
      dwrite_create_factory_proc(DWRITE_FACTORY_TYPE_SHARED,
                                 __uuidof(IDWriteFactory),
                                 reinterpret_cast<IUnknown**>(factory))));
  return true;
}

void WarmupDirectWrite() {
  base::win::ScopedComPtr<IDWriteFactory> factory;
  if (!CreateDirectWriteFactory(factory.Receive()))
    return;

  base::win::ScopedComPtr<IDWriteFontCollection> font_collection;
  CHECK(SUCCEEDED(
      factory->GetSystemFontCollection(font_collection.Receive(), FALSE)));
  base::win::ScopedComPtr<IDWriteFontFamily> font_family;

  UINT32 index;
  BOOL exists;
  CHECK(SUCCEEDED(
      font_collection->FindFamilyName(L"Times New Roman", &index, &exists)));
  CHECK(exists);
  CHECK(
      SUCCEEDED(font_collection->GetFontFamily(index, font_family.Receive())));
  base::win::ScopedComPtr<IDWriteFont> font;
  base::win::ScopedComPtr<IDWriteFontFace> font_face;
  CHECK(SUCCEEDED(font_family->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL,
                                                    DWRITE_FONT_STRETCH_NORMAL,
                                                    DWRITE_FONT_STYLE_NORMAL,
                                                    font.Receive())));
  CHECK(SUCCEEDED(font->CreateFontFace(font_face.Receive())));
  DWRITE_GLYPH_METRICS gm;
  UINT16 glyph = L'S';
  CHECK(SUCCEEDED(font_face->GetDesignGlyphMetrics(&glyph, 1, &gm)));
}

}  // namespace

RendererMainPlatformDelegate::RendererMainPlatformDelegate(
    const MainFunctionParams& parameters)
        : parameters_(parameters),
          sandbox_test_module_(NULL) {
}

RendererMainPlatformDelegate::~RendererMainPlatformDelegate() {
}

void RendererMainPlatformDelegate::PlatformInitialize() {
  const CommandLine& command_line = parameters_.command_line;

#ifdef ENABLE_VTUNE_JIT_INTERFACE
  if (command_line.HasSwitch(switches::kEnableVtune))
    vTune::InitializeVtuneForV8();
#endif

  // Be mindful of what resources you acquire here. They can be used by
  // malicious code if the renderer gets compromised.
  bool no_sandbox = command_line.HasSwitch(switches::kNoSandbox);

  bool use_direct_write = ShouldUseDirectWrite();
  if (!no_sandbox) {
    // ICU DateFormat class (used in base/time_format.cc) needs to get the
    // Olson timezone ID by accessing the registry keys under
    // HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Time Zones.
    // After TimeZone::createDefault is called once here, the timezone ID is
    // cached and there's no more need to access the registry. If the sandbox
    // is disabled, we don't have to make this dummy call.
    scoped_ptr<icu::TimeZone> zone(icu::TimeZone::createDefault());

    if (use_direct_write) {
      WarmupDirectWrite();
    } else {
      SkTypeface_SetEnsureLOGFONTAccessibleProc(SkiaPreCacheFont);
      skia::SetSkiaEnsureTypefaceCharactersAccessible(
          SkiaPreCacheFontCharacters);
    }
  }
  blink::WebFontRendering::setUseDirectWrite(use_direct_write);
  blink::WebFontRendering::setUseSubpixelPositioning(use_direct_write);
}

void RendererMainPlatformDelegate::PlatformUninitialize() {
}

bool RendererMainPlatformDelegate::InitSandboxTests(bool no_sandbox) {
  const CommandLine& command_line = parameters_.command_line;

  DVLOG(1) << "Started renderer with " << command_line.GetCommandLineString();

  sandbox::TargetServices* target_services =
      parameters_.sandbox_info->target_services;

  if (target_services && !no_sandbox) {
      std::wstring test_dll_name =
          command_line.GetSwitchValueNative(switches::kTestSandbox);
    if (!test_dll_name.empty()) {
      sandbox_test_module_ = LoadLibrary(test_dll_name.c_str());
      DCHECK(sandbox_test_module_);
      if (!sandbox_test_module_) {
        return false;
      }
    }
  }
  return true;
}

bool RendererMainPlatformDelegate::EnableSandbox() {
  sandbox::TargetServices* target_services =
      parameters_.sandbox_info->target_services;

  if (target_services) {
    // Cause advapi32 to load before the sandbox is turned on.
    unsigned int dummy_rand;
    rand_s(&dummy_rand);
    // Warm up language subsystems before the sandbox is turned on.
    ::GetUserDefaultLangID();
    ::GetUserDefaultLCID();

    target_services->LowerToken();
    return true;
  }
  return false;
}

void RendererMainPlatformDelegate::RunSandboxTests(bool no_sandbox) {
  if (sandbox_test_module_) {
    RunRendererTests run_security_tests =
        reinterpret_cast<RunRendererTests>(GetProcAddress(sandbox_test_module_,
                                                          kRenderTestCall));
    DCHECK(run_security_tests);
    if (run_security_tests) {
      int test_count = 0;
      DVLOG(1) << "Running renderer security tests";
      BOOL result = run_security_tests(&test_count);
      CHECK(result) << "Test number " << test_count << " has failed.";
    }
  }
}

}  // namespace content
