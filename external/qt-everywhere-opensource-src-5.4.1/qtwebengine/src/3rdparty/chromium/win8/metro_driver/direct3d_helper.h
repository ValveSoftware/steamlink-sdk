// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIN8_METRO_DRIVER_DIRECT3D_HELPER_
#define WIN8_METRO_DRIVER_DIRECT3D_HELPER_

#include "base/basictypes.h"

#include <windows.ui.core.h>
#include <windows.foundation.h>
#include <d3d11_1.h>

namespace metro_driver {

// We need to initalize a Direct3D device and swapchain so that the browser
// can Present to our HWND. This class takes care of creating and keeping the
// swapchain up to date.
class Direct3DHelper {
 public:
  Direct3DHelper();
  ~Direct3DHelper();

  void Initialize(winui::Core::ICoreWindow* window);

 private:
  void CreateDeviceResources();
  void CreateWindowSizeDependentResources();

  winui::Core::ICoreWindow* window_;

  mswr::ComPtr<ID3D11Device1> d3d_device_;
  mswr::ComPtr<ID3D11DeviceContext1> d3d_context_;
  mswr::ComPtr<IDXGISwapChain1> swap_chain_;
  D3D_FEATURE_LEVEL feature_level_;

  ABI::Windows::Foundation::Rect window_bounds_;

  DISALLOW_COPY_AND_ASSIGN(Direct3DHelper);
};

}  // namespace metro_driver

#endif  // WIN8_METRO_DRIVER_DIRECT3D_HELPER_
