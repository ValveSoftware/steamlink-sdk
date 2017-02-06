// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_HANDLE_WIN_H_
#define IPC_HANDLE_WIN_H_

#include <windows.h>

namespace IPC {

// This class is a wrapper around HANDLE. It can be passed between Chrome
// processes via Chrome IPC.
struct HandleForTransit {
  HANDLE handle;
};

HandleForTransit CreateHandleForTransit(HANDLE handle);

}  // namespace IPC

#endif  // IPC_HANDLE_WIN_H_
