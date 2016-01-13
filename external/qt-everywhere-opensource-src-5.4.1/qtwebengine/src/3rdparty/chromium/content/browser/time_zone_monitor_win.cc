// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/time_zone_monitor.h"

#include <windows.h>

#include "base/basictypes.h"
#include "ui/gfx/win/singleton_hwnd.h"

namespace content {

class TimeZoneMonitorWin : public TimeZoneMonitor,
                           public gfx::SingletonHwnd::Observer {
 public:
  TimeZoneMonitorWin() : TimeZoneMonitor() {
    gfx::SingletonHwnd::GetInstance()->AddObserver(this);
  }

  virtual ~TimeZoneMonitorWin() {
    gfx::SingletonHwnd::GetInstance()->RemoveObserver(this);
  }

  // gfx::SingletonHwnd::Observer implementation.
  virtual void OnWndProc(HWND hwnd,
                         UINT message,
                         WPARAM wparam,
                         LPARAM lparam) OVERRIDE {
    if (message != WM_TIMECHANGE) {
      return;
    }

    NotifyRenderers();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TimeZoneMonitorWin);
};

// static
scoped_ptr<TimeZoneMonitor> TimeZoneMonitor::Create() {
  return scoped_ptr<TimeZoneMonitor>(new TimeZoneMonitorWin());
}

}  // namespace content
