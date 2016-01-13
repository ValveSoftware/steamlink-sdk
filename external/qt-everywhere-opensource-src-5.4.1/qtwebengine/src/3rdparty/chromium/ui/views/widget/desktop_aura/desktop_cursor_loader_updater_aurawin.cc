// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_cursor_loader_updater.h"

namespace views {

// static
scoped_ptr<DesktopCursorLoaderUpdater> DesktopCursorLoaderUpdater::Create() {
  return scoped_ptr<DesktopCursorLoaderUpdater>().Pass();
}

}  // namespace views
