// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/node_observer.h"

#include "base/basictypes.h"

namespace mojo {
namespace view_manager {

////////////////////////////////////////////////////////////////////////////////
// NodeObserver, public:

NodeObserver::TreeChangeParams::TreeChangeParams()
    : target(NULL),
      old_parent(NULL),
      new_parent(NULL),
      receiver(NULL),
      phase(NodeObserver::DISPOSITION_CHANGING) {}

}  // namespace view_manager
}  // namespace mojo
