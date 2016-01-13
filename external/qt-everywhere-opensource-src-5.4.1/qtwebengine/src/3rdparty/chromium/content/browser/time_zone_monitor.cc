// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/time_zone_monitor.h"

#include "base/logging.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

namespace content {

TimeZoneMonitor::TimeZoneMonitor() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

TimeZoneMonitor::~TimeZoneMonitor() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void TimeZoneMonitor::NotifyRenderers() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  for (RenderProcessHost::iterator iterator =
           RenderProcessHost::AllHostsIterator();
       !iterator.IsAtEnd();
       iterator.Advance()) {
    iterator.GetCurrentValue()->NotifyTimezoneChange();
  }
}

}  // namespace content
