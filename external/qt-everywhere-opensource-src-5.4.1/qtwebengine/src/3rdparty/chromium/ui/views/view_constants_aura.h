// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_CONSTANTS_AURA_H_
#define UI_VIEWS_VIEW_CONSTANTS_AURA_H_

#include "ui/aura/window.h"
#include "ui/views/views_export.h"

namespace views {
class View;

// A property key to indicate the view the window is associated with. If
// specified, the z-order of the view, relative to other views, dictates the
// z-order of the window and its associated layer. The associated view must
// have the same parent widget as the window on which the property is set.
VIEWS_EXPORT extern const aura::WindowProperty<View*>* const kHostViewKey;

// A property key to indicate if a window is a desktop root window or not.
// This property is currently set during creation of desktop root window.
VIEWS_EXPORT extern const aura::WindowProperty<bool>* const kDesktopRootWindow;

}  // namespace views

#endif  // UI_VIEWS_VIEW_CONSTANTS_AURA_H_
