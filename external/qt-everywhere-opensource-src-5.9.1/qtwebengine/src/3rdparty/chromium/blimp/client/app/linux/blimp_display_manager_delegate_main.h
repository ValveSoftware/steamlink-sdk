// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_APP_LINUX_BLIMP_DISPLAY_MANAGER_DELEGATE_MAIN_H_
#define BLIMP_CLIENT_APP_LINUX_BLIMP_DISPLAY_MANAGER_DELEGATE_MAIN_H_

#include "blimp/client/app/linux/blimp_display_manager.h"

namespace blimp {
namespace client {

class BlimpDisplayManagerDelegateMain : public BlimpDisplayManagerDelegate {
 public:
  void OnClosed() override;
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_APP_LINUX_BLIMP_DISPLAY_MANAGER_DELEGATE_MAIN_H_
