// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/time_zone_monitor.h"

#include <windows.h>

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "ui/gfx/win/singleton_hwnd_observer.h"

namespace content {

class TimeZoneMonitorWin : public TimeZoneMonitor {
 public:
  TimeZoneMonitorWin()
      : TimeZoneMonitor(),
        singleton_hwnd_observer_(
            new gfx::SingletonHwndObserver(base::Bind(
                &TimeZoneMonitorWin::OnWndProc, base::Unretained(this)))) {}

  ~TimeZoneMonitorWin() override {}

 private:
  void OnWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message != WM_TIMECHANGE) {
      return;
    }

    NotifyRenderers();
  }

  std::unique_ptr<gfx::SingletonHwndObserver> singleton_hwnd_observer_;

  DISALLOW_COPY_AND_ASSIGN(TimeZoneMonitorWin);
};

// static
std::unique_ptr<TimeZoneMonitor> TimeZoneMonitor::Create() {
  return std::unique_ptr<TimeZoneMonitor>(new TimeZoneMonitorWin());
}

}  // namespace content
