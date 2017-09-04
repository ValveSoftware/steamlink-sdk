// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TimeZoneMonitorClient_h
#define TimeZoneMonitorClient_h

#include "device/time_zone_monitor/public/interfaces/time_zone_monitor.mojom-blink.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace blink {

class TimeZoneMonitorClient final
    : public device::mojom::blink::TimeZoneMonitorClient {
 public:
  static void Init();

  ~TimeZoneMonitorClient() override;

 private:
  TimeZoneMonitorClient();

  // device::mojom::blink::TimeZoneClient:
  void OnTimeZoneChange(const String& timeZoneInfo) override;

  mojo::Binding<device::mojom::blink::TimeZoneMonitorClient> m_binding;
};

}  // namespace blink

#endif  // TimeZoneMonitorClient_h
