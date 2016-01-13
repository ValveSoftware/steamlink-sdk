// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MAC_SCOPED_NS_DISABLE_SCREEN_UPDATES_H_
#define UI_GFX_MAC_SCOPED_NS_DISABLE_SCREEN_UPDATES_H_

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"

namespace gfx {

// A stack-based class to disable Cocoa screen updates. When instantiated, it
// disables screen updates and enables them when destroyed. Update disabling
// can be nested, and there is a time-maximum (about 1 second) after which
// Cocoa will automatically re-enable updating. This class doesn't attempt to
// overrule that.
class ScopedNSDisableScreenUpdates {
 public:
  ScopedNSDisableScreenUpdates() {
    NSDisableScreenUpdates();
  }
  ~ScopedNSDisableScreenUpdates() {
    NSEnableScreenUpdates();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedNSDisableScreenUpdates);
};

}  // namespace gfx

#endif  // UI_GFX_MAC_SCOPED_NS_DISABLE_SCREEN_UPDATES_H_
