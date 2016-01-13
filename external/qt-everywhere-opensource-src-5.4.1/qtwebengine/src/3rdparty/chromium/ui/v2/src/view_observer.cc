// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/v2/public/view_observer.h"

#include "base/basictypes.h"

namespace v2 {

////////////////////////////////////////////////////////////////////////////////
// ViewObserver, public:

ViewObserver::TreeChangeParams::TreeChangeParams()
    : target(NULL),
      old_parent(NULL),
      new_parent(NULL),
      receiver(NULL),
      phase(ViewObserver::DISPOSITION_CHANGING) {}

}  // namespace v2
