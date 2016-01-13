// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/battery_status/battery_status_service.h"

#include "base/bind.h"
#include "content/browser/battery_status/battery_status_manager.h"
#include "content/public/browser/browser_thread.h"

namespace content {

BatteryStatusService::BatteryStatusService()
    : update_callback_(base::Bind(&BatteryStatusService::UpdateBatteryStatus,
                                  base::Unretained(this))),
      status_updated_(false),
      is_shutdown_(false) {
  callback_list_.set_removal_callback(
      base::Bind(&BatteryStatusService::ConsumersChanged,
                 base::Unretained(this)));
}

BatteryStatusService::~BatteryStatusService() {
}

BatteryStatusService* BatteryStatusService::GetInstance() {
  return Singleton<BatteryStatusService,
                   LeakySingletonTraits<BatteryStatusService> >::get();
}

scoped_ptr<BatteryStatusService::BatteryUpdateSubscription>
BatteryStatusService::AddCallback(const BatteryUpdateCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!is_shutdown_);

  if (!battery_fetcher_)
    battery_fetcher_.reset(new BatteryStatusManager(update_callback_));

  if (callback_list_.empty()) {
    bool success = battery_fetcher_->StartListeningBatteryChange();
    if (!success) {
        // Make sure the promise resolves with the default values in Blink.
        callback.Run(blink::WebBatteryStatus());
    }
  }

  if (status_updated_) {
    // Send recent status to the new callback if already available.
    callback.Run(status_);
  }

  return callback_list_.Add(callback);
}

void BatteryStatusService::ConsumersChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!is_shutdown_);

  if (callback_list_.empty()) {
      battery_fetcher_->StopListeningBatteryChange();
      status_updated_ = false;
  }
}

void BatteryStatusService::UpdateBatteryStatus(
    const blink::WebBatteryStatus& status) {
  DCHECK(!is_shutdown_);
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&BatteryStatusService::NotifyConsumers,
                                     base::Unretained(this), status));
}

void BatteryStatusService::NotifyConsumers(
    const blink::WebBatteryStatus& status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (callback_list_.empty())
    return;

  status_ = status;
  status_updated_ = true;
  callback_list_.Notify(status);
}

void BatteryStatusService::Shutdown() {
  if (!callback_list_.empty())
    battery_fetcher_->StopListeningBatteryChange();
  battery_fetcher_.reset();
  is_shutdown_ = true;
}

const BatteryStatusService::BatteryUpdateCallback&
BatteryStatusService::GetUpdateCallbackForTesting() const {
  return update_callback_;
}

void BatteryStatusService::SetBatteryManagerForTesting(
    BatteryStatusManager* test_battery_manager) {
  battery_fetcher_.reset(test_battery_manager);
  blink::WebBatteryStatus status;
  status_ = status;
  status_updated_ = false;
}

}  // namespace content
