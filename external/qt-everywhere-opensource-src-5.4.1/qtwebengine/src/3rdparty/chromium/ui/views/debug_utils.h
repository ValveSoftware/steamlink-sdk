// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_DEBUG_UTILS_H_
#define UI_VIEWS_DEBUG_UTILS_H_

#include "ui/views/views_export.h"

namespace views {

class View;

// Log the view hierarchy.
VIEWS_EXPORT void PrintViewHierarchy(const View* view);

// Log the focus traversal hierarchy.
VIEWS_EXPORT void PrintFocusHierarchy(const View* view);

}  // namespace views

#endif  // UI_VIEWS_DEBUG_UTILS_H_
