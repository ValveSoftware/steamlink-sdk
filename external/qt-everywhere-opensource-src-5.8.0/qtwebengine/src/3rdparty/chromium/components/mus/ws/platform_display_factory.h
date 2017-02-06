// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_PLATFORM_DISPLAY_FACTORY_H_
#define COMPONENTS_MUS_WS_PLATFORM_DISPLAY_FACTORY_H_

namespace mus {
namespace ws {

class PlatformDisplay;

// Abstract factory for PlatformDisplays. Used by tests to construct test
// PlatformDisplays.
class PlatformDisplayFactory {
 public:
  virtual PlatformDisplay* CreatePlatformDisplay() = 0;
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_PLATFORM_DISPLAY_FACTORY_H_
