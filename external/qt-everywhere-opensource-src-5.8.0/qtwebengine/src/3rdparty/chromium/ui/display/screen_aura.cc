// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/screen.h"

#include "base/logging.h"

namespace display {

Screen* CreateNativeScreen() {
  NOTREACHED() << "Implementation should be installed at higher level.";
  return NULL;
}

}  // namespace display
