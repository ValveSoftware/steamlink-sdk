// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIN8_METRO_DRIVER_STDAFX_H_
#define WIN8_METRO_DRIVER_STDAFX_H_

#include <wrl\implements.h>
#include <wrl\module.h>
#include <wrl\event.h>
#include <wrl\wrappers\corewrappers.h>

#include <activation.h>
#include <d2d1_1.h>
#include <d3d11_1.h>
#include <roapi.h>
#include <stdio.h>
#include <wincodec.h>
#include <windows.h>

#include <windows.applicationmodel.core.h>
#include <windows.applicationModel.datatransfer.h>
#include <windows.graphics.printing.h>
#include <windows.storage.pickers.h>
#include <windows.ui.notifications.h>

namespace mswr =       Microsoft::WRL;
namespace mswrw =      Microsoft::WRL::Wrappers;

namespace winapp =     ABI::Windows::ApplicationModel;
namespace windata =    ABI::Windows::Data;
namespace winxml =     ABI::Windows::Data::Xml;
namespace windevs =    ABI::Windows::Devices;
namespace winfoundtn = ABI::Windows::Foundation;
namespace wingfx =     ABI::Windows::Graphics;
namespace winui =      ABI::Windows::UI;
namespace winsys =     ABI::Windows::System;
namespace winstorage = ABI::Windows::Storage;

#endif  // WIN8_METRO_DRIVER_STDAFX_H_
