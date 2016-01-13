// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"
#include "win8/metro_driver/devices_handler.h"

#include "base/logging.h"

namespace metro_driver {

DevicesHandler::DevicesHandler() {
}

DevicesHandler::~DevicesHandler() {
}

HRESULT DevicesHandler::Initialize(winui::Core::ICoreWindow* window) {
  HRESULT hr = print_handler_.Initialize(window);
  return hr;
}

}  // namespace metro_driver
