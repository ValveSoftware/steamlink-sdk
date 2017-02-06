// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_handle_win.h"

namespace IPC {

HandleForTransit CreateHandleForTransit(HANDLE handle) {
  HandleForTransit hft;
  hft.handle = handle;
  return hft;
}

}  // namespace IPC
