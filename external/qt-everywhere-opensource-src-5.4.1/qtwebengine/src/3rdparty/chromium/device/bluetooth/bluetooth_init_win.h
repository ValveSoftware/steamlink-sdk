// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_INIT_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_INIT_WIN_H_

// windows.h needs to be included before BluetoothAPIs.h.
#include <windows.h>

#include <BluetoothAPIs.h>
#if defined(_WIN32_WINNT_WIN8) && _MSC_VER < 1700
// The Windows 8 SDK defines FACILITY_VISUALCPP in winerror.h.
#undef FACILITY_VISUALCPP
#endif
#include <delayimp.h>
#include <ws2def.h>
#include <ws2bth.h>

#pragma comment(lib, "Bthprops.lib")

namespace device {
namespace bluetooth_init_win {

// Returns true if the machine has a bluetooth stack available. The first call
// to this function will involve file IO, so it should be done on an appropriate
// thread. This function is not threadsafe.
bool HasBluetoothStack();

}  // namespace bluetooth_init_win
}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_INIT_WIN_H_
