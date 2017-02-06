// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "ui/base/ui_base_switches.h"

namespace switches {

#if defined(OS_MACOSX) && !defined(OS_IOS)
// Fall back to using CAOpenGLLayers display content, instead of the IOSurface
// based overlay display path.
const char kDisableMacOverlays[] = "disable-mac-overlays";

// Disable use of cross-process CALayers to display content directly from the
// GPU process on Mac.
const char kDisableRemoteCoreAnimation[] = "disable-remote-core-animation";

// Show borders around CALayers corresponding to overlays and partial damage.
const char kShowMacOverlayBorders[] = "show-mac-overlay-borders";
#endif

#if defined(OS_WIN)
// Disables merging the key event (WM_KEY*) with the char event (WM_CHAR).
const char kDisableMergeKeyCharEvents[]     = "disable-merge-key-char-events";

// Enables merging the key event (WM_KEY*) with the char event (WM_CHAR).
const char kEnableMergeKeyCharEvents[]     = "enable-merge-key-char-events";
#endif

// Disables use of DWM composition for top level windows.
const char kDisableDwmComposition[] = "disable-dwm-composition";

// Disables large icons on the New Tab page.
const char kDisableIconNtp[] = "disable-icon-ntp";

// Disables touch adjustment.
const char kDisableTouchAdjustment[] = "disable-touch-adjustment";

// Disables touch event based drag and drop.
const char kDisableTouchDragDrop[] = "disable-touch-drag-drop";

// Disables additional visual feedback to touch input.
const char kDisableTouchFeedback[] = "disable-touch-feedback";

// Enables large icons on the New Tab page.
const char kEnableIconNtp[] = "enable-icon-ntp";

// Enables touch event based drag and drop.
const char kEnableTouchDragDrop[] = "enable-touch-drag-drop";

// The language file that we want to try to open. Of the form
// language[-country] where language is the 2 letter code from ISO-639.
const char kLang[] = "lang";

// Defines the speed of Material Design visual feedback animations.
const char kMaterialDesignInkDropAnimationSpeed[] =
    "material-design-ink-drop-animation-speed";

// Defines that Material Design visual feedback animations should be fast.
const char kMaterialDesignInkDropAnimationSpeedFast[] = "fast";

// Defines that Material Design visual feedback animations should be slow.
const char kMaterialDesignInkDropAnimationSpeedSlow[] = "slow";

// Enables top Chrome material design elements.
const char kTopChromeMD[] = "top-chrome-md";

// Material design mode for the |kTopChromeMD| switch.
const char kTopChromeMDMaterial[] = "material";

// Material design hybrid mode for the |kTopChromeMD| switch. Targeted for
// mouse/touch hybrid devices.
const char kTopChromeMDMaterialHybrid[] = "material-hybrid";

// Classic, non-material, mode for the |kTopChromeMD| switch.
const char kTopChromeMDNonMaterial[] = "non-material";

// Applies the material design mode passed via --top-chrome-md to elements
// throughout Chrome (not just top Chrome).
const char kExtendMdToSecondaryUi[] = "secondary-ui-md";

}  // namespace switches
