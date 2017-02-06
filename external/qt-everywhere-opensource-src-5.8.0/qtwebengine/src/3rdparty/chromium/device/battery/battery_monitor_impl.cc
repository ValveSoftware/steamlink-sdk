// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/battery/battery_monitor_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"

namespace device {

// static
void BatteryMonitorImpl::Create(
    mojo::InterfaceRequest<BatteryMonitor> request) {
  new BatteryMonitorImpl(std::move(request));
}

BatteryMonitorImpl::BatteryMonitorImpl(
    mojo::InterfaceRequest<BatteryMonitor> request)
    : binding_(this, std::move(request)), status_to_report_(false) {
  // NOTE: DidChange may be called before AddCallback returns. This is done to
  // report current status.
  subscription_ = BatteryStatusService::GetInstance()->AddCallback(
      base::Bind(&BatteryMonitorImpl::DidChange, base::Unretained(this)));
}

BatteryMonitorImpl::~BatteryMonitorImpl() {
}

void BatteryMonitorImpl::QueryNextStatus(
    const QueryNextStatusCallback& callback) {
  if (!callback_.is_null()) {
    DVLOG(1) << "Overlapped call to QueryNextStatus!";
    delete this;
    return;
  }
  callback_ = callback;

  if (status_to_report_)
    ReportStatus();
}

void BatteryMonitorImpl::RegisterSubscription() {
}

void BatteryMonitorImpl::DidChange(const BatteryStatus& battery_status) {
  status_ = battery_status;
  status_to_report_ = true;

  if (!callback_.is_null())
    ReportStatus();
}

void BatteryMonitorImpl::ReportStatus() {
  callback_.Run(status_.Clone());
  callback_.Reset();

  status_to_report_ = false;
}

}  // namespace device
