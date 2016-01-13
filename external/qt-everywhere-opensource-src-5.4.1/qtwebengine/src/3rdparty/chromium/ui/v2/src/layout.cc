// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/v2/public/layout.h"

#include "ui/v2/public/view.h"
#include "ui/v2/src/view_private.h"

namespace v2 {

////////////////////////////////////////////////////////////////////////////////
// Layout, public:

Layout::~Layout() {}

void Layout::SetChildBounds(View* child,
                            const gfx::Rect& requested_bounds) {
  SetChildBoundsDirect(child, requested_bounds);
}

////////////////////////////////////////////////////////////////////////////////
// Layout, protected:

void Layout::SetChildBoundsDirect(View* child, const gfx::Rect& bounds) {
  ViewPrivate(child).set_bounds(bounds);
}

}  // namespace v2
