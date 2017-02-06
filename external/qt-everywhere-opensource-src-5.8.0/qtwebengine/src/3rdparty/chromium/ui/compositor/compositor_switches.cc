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

// Disable partial swap which is needed for some OpenGL drivers / emulators.
const char kUIDisablePartialSwap[] = "ui-disable-partial-swap";

const char kUIEnableRGBA4444Textures[] = "ui-enable-rgba-4444-textures";

const char kUIEnableZeroCopy[] = "ui-enable-zero-copy";

const char kUIShowPaintRects[] = "ui-show-paint-rects";

}  // namespace switches

namespace ui {

bool IsUIZeroCopyEnabled() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  return command_line.HasSwitch(switches::kUIEnableZeroCopy);
}

}  // namespace ui
