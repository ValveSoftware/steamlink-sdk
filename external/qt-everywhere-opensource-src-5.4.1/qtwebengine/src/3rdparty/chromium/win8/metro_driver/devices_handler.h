// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_METRO_DRIVER_DEVICES_HANDLER_H_
#define CHROME_BROWSER_UI_METRO_DRIVER_DEVICES_HANDLER_H_

#include <windows.ui.core.h>

#include "base/basictypes.h"
#include "win8/metro_driver/print_handler.h"

namespace metro_driver {

// This class handles the devices charm.
class DevicesHandler {
 public:
  DevicesHandler();
  ~DevicesHandler();

  HRESULT Initialize(winui::Core::ICoreWindow* window);

 private:
  PrintHandler print_handler_;

  DISALLOW_COPY_AND_ASSIGN(DevicesHandler);
};

}  // namespace metro_driver

#endif  // CHROME_BROWSER_UI_METRO_DRIVER_DEVICES_HANDLER_H_
