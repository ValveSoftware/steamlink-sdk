// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/window_observer.h"

namespace mus {

////////////////////////////////////////////////////////////////////////////////
// WindowObserver, public:

WindowObserver::TreeChangeParams::TreeChangeParams()
    : target(nullptr),
      old_parent(nullptr),
      new_parent(nullptr),
      receiver(nullptr) {}

}  // namespace mus
