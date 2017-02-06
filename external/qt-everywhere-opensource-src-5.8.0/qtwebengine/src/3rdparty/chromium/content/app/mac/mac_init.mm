// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/mac/mac_init.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/mac_util.h"

namespace content {

void InitializeMac() {
  [[NSUserDefaults standardUserDefaults] registerDefaults:@{
      // Exceptions routed to -[NSApplication reportException:] should crash
      // immediately, as opposed being swallowed or presenting UI that gives the
      // user a choice in the matter.
      @"NSApplicationCrashOnExceptions": @YES,

      // Prevent Cocoa from turning command-line arguments into -[NSApplication
      // application:openFile:], because they are handled directly. @"NO" looks
      // like a mistake, but the value really is supposed to be a string.
      @"NSTreatUnknownArgumentsAsOpen": @"NO",

      // CoreAnimation has poor performance and CoreAnimation and
      // non-CoreAnimation exhibit window flickering when layers are not hosted
      // in the window server, which is the default when not not using the
      // 10.9 SDK.
      // TODO: Remove this when we build with the 10.9 SDK.
      @"NSWindowHostsLayersInWindowServer": @(base::mac::IsOSMavericksOrLater())
  }];
}

}  // namespace content
