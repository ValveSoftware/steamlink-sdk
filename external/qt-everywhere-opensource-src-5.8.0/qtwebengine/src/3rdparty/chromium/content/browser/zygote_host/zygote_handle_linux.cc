// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/zygote_handle_linux.h"

#include "content/browser/zygote_host/zygote_communication_linux.h"

namespace content {

ZygoteHandle CreateZygote() {
  ZygoteHandle zygote = new ZygoteCommunication();
  zygote->Init();
  return zygote;
}

ZygoteHandle* GetGenericZygote() {
  static ZygoteHandle zygote;
  return &zygote;
}

}  // namespace content
