// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>

#include "base/logging.h"
#include "build/build_config.h"
#include "content/child/npapi/plugin_instance.h"

// When C++ exceptions are disabled, the C++ library defines |try| and
// |catch| so as to allow exception-expecting C++ code to build properly when
// language support for exceptions is not present.  These macros interfere
// with the use of |@try| and |@catch| in Objective-C files such as this one.
// Undefine these macros here, after everything has been #included, since
// there will be no C++ uses and only Objective-C uses from this point on.
#undef try
#undef catch

namespace content {

NPError PluginInstance::PopUpContextMenu(NPMenu* menu) {
  if (!currently_handled_event_)
    return NPERR_GENERIC_ERROR;

  CGRect main_display_bounds = CGDisplayBounds(CGMainDisplayID());
  NSPoint screen_point = NSMakePoint(
      plugin_origin_.x() + currently_handled_event_->data.mouse.pluginX,
      plugin_origin_.y() + currently_handled_event_->data.mouse.pluginY);
  // Plugin offsets are upper-left based, so flip vertically for Cocoa.
  screen_point.y = main_display_bounds.size.height - screen_point.y;

  NSMenu* nsmenu = reinterpret_cast<NSMenu*>(menu);
  NPError return_val = NPERR_NO_ERROR;
  @try {
    [nsmenu popUpMenuPositioningItem:nil atLocation:screen_point inView:nil];
  }
  @catch (NSException* e) {
    NSLog(@"Caught exception while handling PopUpContextMenu: %@", e);
    return_val = NPERR_GENERIC_ERROR;
  }

  return return_val;
}

ScopedCurrentPluginEvent::ScopedCurrentPluginEvent(PluginInstance* instance,
                                                   NPCocoaEvent* event)
    : instance_(instance) {
  instance_->set_currently_handled_event(event);
}

ScopedCurrentPluginEvent::~ScopedCurrentPluginEvent() {
  instance_->set_currently_handled_event(NULL);
}

}  // namespace content
