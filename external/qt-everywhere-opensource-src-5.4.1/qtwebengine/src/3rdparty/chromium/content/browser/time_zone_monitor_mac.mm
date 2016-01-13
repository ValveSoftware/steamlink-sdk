// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/time_zone_monitor.h"

#import <Foundation/Foundation.h>

namespace content {

class TimeZoneMonitorMac : public TimeZoneMonitor {
 public:
  TimeZoneMonitorMac() : TimeZoneMonitor() {
    NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
    notification_observer_ =
        [nc addObserverForName:NSSystemTimeZoneDidChangeNotification
                        object:nil
                         queue:nil
                    usingBlock:^(NSNotification* notification) {
                        NotifyRenderers();
                    }];
  }

  virtual ~TimeZoneMonitorMac() {
    NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
    [nc removeObserver:notification_observer_];
  }

 private:
  id notification_observer_;

  DISALLOW_COPY_AND_ASSIGN(TimeZoneMonitorMac);
};

// static
scoped_ptr<TimeZoneMonitor> TimeZoneMonitor::Create() {
  return scoped_ptr<TimeZoneMonitor>(new TimeZoneMonitorMac());
}

}  // namespace content
