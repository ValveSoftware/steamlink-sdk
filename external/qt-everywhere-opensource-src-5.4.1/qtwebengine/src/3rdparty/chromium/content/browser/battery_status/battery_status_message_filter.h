// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BATTERY_STATUS_BATTERY_STATUS_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_BATTERY_STATUS_BATTERY_STATUS_MESSAGE_FILTER_H_

#include "content/browser/battery_status/battery_status_service.h"
#include "content/public/browser/browser_message_filter.h"

namespace content {

class BatteryStatusMessageFilter : public BrowserMessageFilter {
 public:
  BatteryStatusMessageFilter();

  // BrowserMessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  virtual ~BatteryStatusMessageFilter();

  void OnBatteryStatusStart();
  void OnBatteryStatusStop();
  void SendBatteryChange(const blink::WebBatteryStatus& status);

  BatteryStatusService::BatteryUpdateCallback callback_;
  scoped_ptr<BatteryStatusService::BatteryUpdateSubscription> subscription_;
  bool is_started_;

  DISALLOW_COPY_AND_ASSIGN(BatteryStatusMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BATTERY_STATUS_BATTERY_STATUS_MESSAGE_FILTER_H_
