// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_PLATFORM_DISPLAY_FACTORY_H_
#define SERVICES_UI_WS_PLATFORM_DISPLAY_FACTORY_H_

#include <memory>

namespace ui {
namespace ws {

class PlatformDisplay;

// Abstract factory for PlatformDisplays. Used by tests to construct test
// PlatformDisplays.
class PlatformDisplayFactory {
 public:
  virtual std::unique_ptr<PlatformDisplay> CreatePlatformDisplay() = 0;
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_PLATFORM_DISPLAY_FACTORY_H_
