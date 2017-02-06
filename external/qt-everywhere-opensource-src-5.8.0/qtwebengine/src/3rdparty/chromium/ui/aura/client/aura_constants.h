// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_AURA_CONSTANTS_H_
#define UI_AURA_CLIENT_AURA_CONSTANTS_H_

#include "ui/aura/aura_export.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"

namespace ui {
class InputMethod;
}

namespace aura {
namespace client {

// Alphabetical sort.

// A property key to store always-on-top flag.
AURA_EXPORT extern const WindowProperty<bool>* const kAlwaysOnTopKey;

// A property key to store whether animations are disabled for the window. Type
// of value is an int.
AURA_EXPORT extern const WindowProperty<bool>* const kAnimationsDisabledKey;

// A property key to store the can-maximize flag.
AURA_EXPORT extern const WindowProperty<bool>* const kCanMaximizeKey;

// A property key to store the can-minimize flag.
AURA_EXPORT extern const WindowProperty<bool>* const kCanMinimizeKey;

// A property key to store the can-resize flag.
AURA_EXPORT extern const WindowProperty<bool>* const kCanResizeKey;

// A property key to store if a window is a constrained window or not.
AURA_EXPORT extern const WindowProperty<bool>* const kConstrainedWindowKey;

// A property key to indicate that a window should show that it deserves
// attention.
AURA_EXPORT extern const aura::WindowProperty<bool>* const kDrawAttentionKey;

// A property key to indicate that a window should be excluded from the most
// recently used windows list.
AURA_EXPORT extern const aura::WindowProperty<bool>* const kExcludeFromMruKey;

// A property key to store the host window of a window. This lets
// WebContentsViews find the windows that should constrain NPAPI plugins.
AURA_EXPORT extern const WindowProperty<Window*>* const kHostWindowKey;

// A property key to store the window modality.
AURA_EXPORT extern const WindowProperty<ui::ModalType>* const kModalKey;

// A property key to store the restore bounds for a window.
AURA_EXPORT extern const WindowProperty<gfx::Rect*>* const kRestoreBoundsKey;

// A property key to store ui::WindowShowState for restoring a window.
// Used in Ash to remember the show state before the window was minimized.
AURA_EXPORT extern const WindowProperty<ui::WindowShowState>* const
    kRestoreShowStateKey;

// A property key to store ui::WindowShowState for a window.
// See ui/base/ui_base_types.h for its definition.
AURA_EXPORT extern const WindowProperty<ui::WindowShowState>* const
    kShowStateKey;

// The inset of the topmost view in the client view from the top of the
// non-client view. The topmost view depends on the window type. The topmost
// view is the tab strip for tabbed browser windows, the toolbar for popups,
// the web contents for app windows and varies for fullscreen windows.
AURA_EXPORT extern const aura::WindowProperty<int>* const kTopViewInset;

// Alphabetical sort.

}  // namespace client
}  // namespace aura

#endif  // UI_AURA_CLIENT_AURA_CONSTANTS_H_
