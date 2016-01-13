// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"

#include <windows.h>
#include <wrl\implements.h>
#include <wrl\wrappers\corewrappers.h>
#include <windows.foundation.h>
#include <windows.graphics.display.h>
#include "base/win/windows_version.h"
#include "ui/gfx/win/dpi.h"
#include "winrt_utils.h"

float GetModernUIScale() {
  if (base::win::GetVersion() >= base::win::VERSION_WIN8) {
    Microsoft::WRL::ComPtr<
        ABI::Windows::Graphics::Display::IDisplayPropertiesStatics>
        display_properties;
    if (SUCCEEDED(winrt_utils::CreateActivationFactory(
        RuntimeClass_Windows_Graphics_Display_DisplayProperties,
        display_properties.GetAddressOf()))) {
      ABI::Windows::Graphics::Display::ResolutionScale resolution_scale;
      if (SUCCEEDED(display_properties->get_ResolutionScale(&resolution_scale)))
        return static_cast<float>(resolution_scale) / 100.0f;
    }
  }
  return gfx::GetDPIScale();
}
