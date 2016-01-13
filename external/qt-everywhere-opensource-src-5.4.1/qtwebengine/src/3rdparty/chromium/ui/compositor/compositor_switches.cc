// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositor_switches.h"

#include "base/command_line.h"

namespace switches {

// Enable compositing individual elements via hardware overlays when
// permitted by device.
const char kEnableHardwareOverlays[] = "enable-hardware-overlays";

// Forces tests to produce pixel output when they normally wouldn't.
const char kEnablePixelOutputInTests[] = "enable-pixel-output-in-tests";

const char kUIDisableThreadedCompositing[] = "ui-disable-threaded-compositing";

const char kUIEnableImplSidePainting[] = "ui-enable-impl-side-painting";

const char kUIEnableZeroCopy[] = "ui-enable-zero-copy";

const char kUIShowPaintRects[] = "ui-show-paint-rects";

}  // namespace switches

namespace ui {

bool IsUIImplSidePaintingEnabled() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  return command_line.HasSwitch(switches::kUIEnableImplSidePainting);
}

bool IsUIZeroCopyEnabled() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  return command_line.HasSwitch(switches::kUIEnableZeroCopy);
}

}  // namespace ui
