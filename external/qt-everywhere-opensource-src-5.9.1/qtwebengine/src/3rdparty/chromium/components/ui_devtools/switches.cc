// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/switches.h"

namespace ui {
namespace devtools {

// Enables DevTools server for UI (mus, ash, etc). Value should be the port the
// server is started on. Default port is 9332.
const char kEnableUiDevTools[] = "enable-ui-devtools";

}  // namespace devtools
}  // namespace ui
