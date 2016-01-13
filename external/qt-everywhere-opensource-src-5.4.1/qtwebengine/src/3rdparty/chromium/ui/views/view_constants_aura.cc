// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_constants_aura.h"

#include "ui/aura/window_property.h"
#include "ui/views/view.h"

DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(VIEWS_EXPORT, views::View*);

namespace views {

DEFINE_WINDOW_PROPERTY_KEY(views::View*, kHostViewKey, NULL);

DEFINE_WINDOW_PROPERTY_KEY(bool, kDesktopRootWindow, false);

}  // namespace views
