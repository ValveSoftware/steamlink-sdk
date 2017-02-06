// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/scoped_wl.h"

#include <wayland-server-core.h>

namespace exo {
namespace wayland {

void WlDisplayDeleter::operator()(wl_display* display) const {
  wl_display_destroy(display);
}

}  // namespace wayland
}  // namespace exo
