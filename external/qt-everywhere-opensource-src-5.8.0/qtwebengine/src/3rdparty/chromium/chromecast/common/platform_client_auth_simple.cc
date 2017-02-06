// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/platform_client_auth.h"

namespace chromecast {

// static
bool PlatformClientAuth::initialized_ = false;

// static
bool PlatformClientAuth::Initialize() {
  initialized_ = true;
  return true;
}

}  // namespace chromecast
