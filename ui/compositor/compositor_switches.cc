// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositor_switches.h"

#include "base/command_line.h"

namespace switches {

// Forces tests to produce pixel output when they normally wouldn't.
const char kEnablePixelOutputInTests[] = "enable-pixel-output-in-tests";

const char kUIDisableThreadedCompositing[] = "ui-disable-threaded-compositing";

const char kUIEnableImplSidePainting[] = "ui-enable-impl-side-painting";

const char kUIEnableMapImage[] = "ui-enable-map-image";

const char kUIShowPaintRects[] = "ui-show-paint-rects";

}  // namespace switches

namespace ui {

bool IsUIImplSidePaintingEnabled() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  return command_line.HasSwitch(switches::kUIEnableImplSidePainting);
}

bool IsUIMapImageEnabled() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  return command_line.HasSwitch(switches::kUIEnableMapImage);
}

}  // namespace ui
