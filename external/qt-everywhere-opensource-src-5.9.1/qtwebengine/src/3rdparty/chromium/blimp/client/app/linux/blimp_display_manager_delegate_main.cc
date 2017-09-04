// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/app/linux/blimp_display_manager_delegate_main.h"

#include "base/run_loop.h"

namespace blimp {
namespace client {

void BlimpDisplayManagerDelegateMain::OnClosed() {
  base::MessageLoop::current()->QuitNow();
}

}  // namespace client
}  // namespace blimp
