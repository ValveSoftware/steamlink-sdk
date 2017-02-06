// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_COMMON_PLATFORM_CLIENT_AUTH_H_
#define CHROMECAST_COMMON_PLATFORM_CLIENT_AUTH_H_

namespace chromecast {

class PlatformClientAuth {
 public:
  static bool Initialize();

private:
  PlatformClientAuth() {}
  ~PlatformClientAuth() {}

  static bool initialized_;
};

}  // namespace chromecast

#endif  // CHROMECAST_COMMON_PLATFORM_CLIENT_AUTH_H_
