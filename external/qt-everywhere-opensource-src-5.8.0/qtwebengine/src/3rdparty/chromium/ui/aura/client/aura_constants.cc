// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/aura_constants.h"

#include "ui/aura/window_property.h"
#include "ui/gfx/geometry/rect.h"

DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(AURA_EXPORT, bool)
DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(AURA_EXPORT, ui::ModalType)
DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(AURA_EXPORT, gfx::Rect*)
DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(AURA_EXPORT, ui::InputMethod*)
DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(AURA_EXPORT, ui::WindowShowState)
DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(AURA_EXPORT, int)
DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(AURA_EXPORT, void*)

namespace aura {
namespace client {

// Alphabetical sort.

DEFINE_WINDOW_PROPERTY_KEY(bool, kAlwaysOnTopKey, false);
DEFINE_WINDOW_PROPERTY_KEY(bool, kAnimationsDisabledKey, false);
DEFINE_WINDOW_PROPERTY_KEY(bool, kCanMaximizeKey, false);
DEFINE_WINDOW_PROPERTY_KEY(bool, kCanMinimizeKey, false);
DEFINE_WINDOW_PROPERTY_KEY(bool, kCanResizeKey, true);
DEFINE_WINDOW_PROPERTY_KEY(bool, kConstrainedWindowKey, false);
DEFINE_WINDOW_PROPERTY_KEY(bool, kDrawAttentionKey, false);
DEFINE_WINDOW_PROPERTY_KEY(bool, kExcludeFromMruKey, false);
DEFINE_WINDOW_PROPERTY_KEY(Window*, kHostWindowKey, NULL);
DEFINE_WINDOW_PROPERTY_KEY(ui::ModalType, kModalKey, ui::MODAL_TYPE_NONE);
// gfx::Rect object for RestoreBoundsKey property is owned by the window
// and will be freed automatically.
DEFINE_OWNED_WINDOW_PROPERTY_KEY(gfx::Rect, kRestoreBoundsKey, NULL);
DEFINE_WINDOW_PROPERTY_KEY(
    ui::WindowShowState, kRestoreShowStateKey, ui::SHOW_STATE_DEFAULT);
DEFINE_WINDOW_PROPERTY_KEY(
    ui::WindowShowState, kShowStateKey, ui::SHOW_STATE_DEFAULT);
DEFINE_WINDOW_PROPERTY_KEY(int, kTopViewInset, 0);

}  // namespace client
}  // namespace aura
