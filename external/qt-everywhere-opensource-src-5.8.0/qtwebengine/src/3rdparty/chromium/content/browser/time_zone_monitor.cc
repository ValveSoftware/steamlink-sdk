// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/time_zone_monitor.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace content {

TimeZoneMonitor::TimeZoneMonitor() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

TimeZoneMonitor::~TimeZoneMonitor() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void TimeZoneMonitor::NotifyRenderers() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if defined(OS_CHROMEOS) || U_ICU_VERSION_MAJOR_NUM < 54
  // On CrOS and older ICU versions, ICU's default tz is already set to a new zone.
  // No need to redetect it with detectHostTimeZone().
  std::unique_ptr<icu::TimeZone> new_zone(icu::TimeZone::createDefault());
#else
  icu::TimeZone* new_zone = icu::TimeZone::detectHostTimeZone();
#if defined(OS_LINUX)
  // We get here multiple times on Linux per a single tz change, but
  // want to update the ICU default zone and notify renderer only once.
  std::unique_ptr<icu::TimeZone> current_zone(icu::TimeZone::createDefault());
  if (*current_zone == *new_zone) {
    VLOG(1) << "timezone already updated";
    delete new_zone;
    return;
  }
#endif
  icu::TimeZone::adoptDefault(new_zone);
#endif
  icu::UnicodeString zone_id;
  std::string zone_id_str;
  new_zone->getID(zone_id).toUTF8String(zone_id_str);
  VLOG(1) << "timezone reset to " << zone_id_str;
  for (RenderProcessHost::iterator iterator =
           RenderProcessHost::AllHostsIterator();
       !iterator.IsAtEnd();
       iterator.Advance()) {
    iterator.GetCurrentValue()->NotifyTimezoneChange(zone_id_str);
  }
}

}  // namespace content
