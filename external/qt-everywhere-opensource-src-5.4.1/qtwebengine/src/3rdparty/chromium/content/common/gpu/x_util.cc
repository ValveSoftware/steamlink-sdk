// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/x_util.h"

#include <X11/Xutil.h>

namespace content {

void ScopedPtrXFree::operator()(void* x) const {
  ::XFree(x);
}

}  // namespace content
