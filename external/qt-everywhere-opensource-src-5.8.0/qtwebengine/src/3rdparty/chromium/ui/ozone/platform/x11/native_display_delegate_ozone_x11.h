// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_NATIVE_DISPLAY_DELEGATE_OZONE_X11_H_
#define UI_OZONE_PLATFORM_X11_NATIVE_DISPLAY_DELEGATE_OZONE_X11_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/ozone/common/native_display_delegate_ozone.h"

namespace ui {

class NativeDisplayDelegateOzoneX11 : public NativeDisplayDelegateOzone {
 public:
  NativeDisplayDelegateOzoneX11();
  ~NativeDisplayDelegateOzoneX11() override;

  void OnConfigurationChanged();

  // NativeDisplayDelegateOzone overrides:
  void Initialize() override;
  void AddObserver(NativeDisplayObserver* observer) override;
  void RemoveObserver(NativeDisplayObserver* observer) override;

 private:
  base::ObserverList<NativeDisplayObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(NativeDisplayDelegateOzoneX11);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_NATIVE_DISPLAY_DELEGATE_OZONE_X11_H_
