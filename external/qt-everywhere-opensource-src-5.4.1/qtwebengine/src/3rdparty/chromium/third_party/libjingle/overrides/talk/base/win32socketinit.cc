// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Redirect Libjingle's winsock initialization activity into Chromium's
// singleton object that managest precisely that for the browser.

#include "talk/base/win32socketinit.h"

#include "net/base/winsock_init.h"

#ifndef WIN32
#error "Only compile this on Windows"
#endif

namespace talk_base {

void EnsureWinsockInit() {
  net::EnsureWinsockInit();
}

}  // namespace talk_base
