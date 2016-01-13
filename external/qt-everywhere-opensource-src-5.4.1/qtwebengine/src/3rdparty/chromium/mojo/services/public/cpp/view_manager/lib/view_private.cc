// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/lib/view_private.h"

namespace mojo {
namespace view_manager {

ViewPrivate::ViewPrivate(View* view)
    : view_(view) {
}

ViewPrivate::~ViewPrivate() {
}

// static
View* ViewPrivate::LocalCreate() {
  return new View;
}

}  // namespace view_manager
}  // namespace mojo
