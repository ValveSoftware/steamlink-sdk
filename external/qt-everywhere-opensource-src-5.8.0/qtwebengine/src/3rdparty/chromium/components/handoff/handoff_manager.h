// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HANDOFF_HANDOFF_MANAGER_H_
#define COMPONENTS_HANDOFF_HANDOFF_MANAGER_H_

#include <Foundation/Foundation.h>

#include "base/mac/objc_property_releaser.h"
#include "components/handoff/handoff_utility.h"
#include "url/gurl.h"

@class NSUserActivity;

// Maintains all of the state relevant to the Handoff feature. Allows Chrome to
// hand off the current active URL to other devices.
@interface HandoffManager : NSObject {
 @private
  base::mac::ObjCPropertyReleaser _propertyReleaser_HandoffManager;

  GURL _activeURL;
  NSUserActivity* _userActivity;
  handoff::Origin _origin;
}

// The active URL is defined as the URL of the most recently accessed tab. This
// method should be called any time the active URL might have changed. This
// method is idempotent.
- (void)updateActiveURL:(const GURL&)url;

@end

#endif  // COMPONENTS_HANDOFF_HANDOFF_MANAGER_H_
