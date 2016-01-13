// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TIME_ZONE_MONITOR_H_
#define CONTENT_BROWSER_TIME_ZONE_MONITOR_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace content {

// TimeZoneMonitor watches the system time zone, and notifies renderers
// when it changes. Some renderer code caches the system time zone, so
// this notification is necessary to inform such code that cached
// timezone data may have become invalid. Due to sandboxing, it is not
// possible for renderer processes to monitor for system time zone
// changes themselves, so this must happen in the browser process.
//
// Sandboxing also may prevent renderer processes from reading the time
// zone when it does change, so platforms may have to deal with this in
// platform-specific ways:
//  - Mac uses a sandbox hole defined in content/renderer/renderer.sb.
//  - Linux-based platforms use ProxyLocaltimeCallToBrowser in
//    content/zygote/zygote_main_linux.cc and HandleLocaltime in
//    content/browser/renderer_host/sandbox_ipc_linux.cc to override
//    localtime in renderer processes with custom code that calls
//    localtime in the browser process via Chrome IPC.

class TimeZoneMonitor {
 public:
  // Returns a new TimeZoneMonitor object (likely a subclass) specific to the
  // platform.
  static scoped_ptr<TimeZoneMonitor> Create();

  virtual ~TimeZoneMonitor();

 protected:
  TimeZoneMonitor();

  // Loop over all renderers and notify them that the system time zone may
  // have changed.
  void NotifyRenderers();

 private:
  DISALLOW_COPY_AND_ASSIGN(TimeZoneMonitor);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TIME_ZONE_MONITOR_H_
