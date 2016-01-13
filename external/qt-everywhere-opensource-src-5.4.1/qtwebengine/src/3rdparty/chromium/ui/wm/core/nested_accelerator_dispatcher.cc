// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/nested_accelerator_dispatcher.h"

#include "base/logging.h"
#include "ui/wm/core/nested_accelerator_delegate.h"

namespace wm {

NestedAcceleratorDispatcher::NestedAcceleratorDispatcher(
    NestedAcceleratorDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate);
}

NestedAcceleratorDispatcher::~NestedAcceleratorDispatcher() {
}

}  // namespace wm
