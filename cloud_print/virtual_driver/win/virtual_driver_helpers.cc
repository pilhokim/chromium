// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/virtual_driver/win/virtual_driver_helpers.h"
#include <windows.h>
#include <winspool.h>
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string16.h"
#include "base/win/windows_version.h"

namespace cloud_print {

const size_t kMaxMessageLen = 100;

void DisplayWindowsMessage(HWND hwnd,
                           HRESULT message_id,
                           const string16 &caption) {
  wchar_t message_text[kMaxMessageLen + 1] = L"";

  ::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL,
                  message_id,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  message_text,
                  kMaxMessageLen,
                  NULL);
  ::MessageBox(hwnd, message_text, caption.c_str(), MB_OK);
}

HRESULT GetLastHResult() {
  DWORD error_code = GetLastError();
  return HRESULT_FROM_WIN32(error_code);
}

string16 GetPortMonitorDllName() {
  if (IsSystem64Bit()) {
    return string16(L"gcp_portmon64.dll");
  } else {
    return string16(L"gcp_portmon.dll");
  }
}

HRESULT GetPrinterDriverDir(base::FilePath* path) {
  BYTE driver_dir_buffer[MAX_PATH * sizeof(wchar_t)];
  DWORD needed = 0;
  if (!GetPrinterDriverDirectory(NULL,
                                 NULL,
                                 1,
                                 driver_dir_buffer,
                                 MAX_PATH * sizeof(wchar_t),
                                 &needed)) {
    // We could try to allocate a larger buffer if needed > MAX_PATH
    // but that really shouldn't happen.
    return cloud_print::GetLastHResult();
  }
  *path = base::FilePath(reinterpret_cast<wchar_t*>(driver_dir_buffer));

  // The XPS driver is a "Level 3" driver
  *path = path->Append(L"3");
  return S_OK;
}

bool IsSystem64Bit() {
  base::win::OSInfo::WindowsArchitecture arch =
      base::win::OSInfo::GetInstance()->architecture();
  return (arch == base::win::OSInfo::X64_ARCHITECTURE) ||
         (arch == base::win::OSInfo::IA64_ARCHITECTURE);
}

string16 LoadLocalString(DWORD string_id) {
  static wchar_t dummy = L'\0';
// We never expect strings longer than MAX_PATH characters.
  static wchar_t buffer[MAX_PATH];
  HMODULE module = NULL;
  GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    &dummy,
                    &module);
  int count = LoadString(module,
                         string_id,
                         buffer,
                         MAX_PATH);
  CHECK_NE(0, count);
  return string16(buffer);
}
}

