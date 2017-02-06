// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/user_agent.h"

#include "build/build_config.h"

namespace mojo {
namespace common {

std::string GetUserAgent() {
  // TODO(jam): change depending on OS
#if defined(OS_ANDROID)
  return "Mozilla/5.0 (Linux; Android 5.1.1; Nexus 7 Build/LMY48G) "
         "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/42.0.2311.68 "
         "Safari/537.36";
#else
  return "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like "
         "Gecko) Chrome/42.0.2311.68 Safari/537.36";
#endif
}

}  // namespace common
}  // namespace mojo
