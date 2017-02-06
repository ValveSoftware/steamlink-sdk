// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/dial/dial_api.h"

#include <stddef.h>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/dial/dial_api_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/dial.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"

using base::TimeDelta;
using content::BrowserThread;

namespace {

// How often to poll for devices.
const int kDialRefreshIntervalSecs = 120;

// We prune a device if it does not respond after this time.
const int kDialExpirationSecs = 240;

// The maximum number of devices retained at once in the registry.
const size_t kDialMaxDevices = 256;

}  // namespace

namespace extensions {

namespace dial = api::dial;

DialAPI::DialAPI(Profile* profile)
    : RefcountedKeyedService(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO)),
      profile_(profile) {
  EventRouter::Get(profile)
      ->RegisterObserver(this, dial::OnDeviceList::kEventName);
}

DialAPI::~DialAPI() {}

DialRegistry* DialAPI::dial_registry() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!dial_registry_.get()) {
    dial_registry_.reset(new DialRegistry(this,
        TimeDelta::FromSeconds(kDialRefreshIntervalSecs),
        TimeDelta::FromSeconds(kDialExpirationSecs),
        kDialMaxDevices));
  }
  return dial_registry_.get();
}

void DialAPI::OnListenerAdded(const EventListenerInfo& details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&DialAPI::NotifyListenerAddedOnIOThread, this));
}

void DialAPI::OnListenerRemoved(const EventListenerInfo& details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&DialAPI::NotifyListenerRemovedOnIOThread, this));
}

void DialAPI::NotifyListenerAddedOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  VLOG(2) << "DIAL device event listener added.";
  dial_registry()->OnListenerAdded();
}

void DialAPI::NotifyListenerRemovedOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  VLOG(2) << "DIAL device event listener removed";
  dial_registry()->OnListenerRemoved();
}

void DialAPI::OnDialDeviceEvent(const DialRegistry::DeviceList& devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&DialAPI::SendEventOnUIThread, this, devices));
}

void DialAPI::OnDialError(const DialRegistry::DialErrorCode code) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&DialAPI::SendErrorOnUIThread, this, code));
}

void DialAPI::SendEventOnUIThread(const DialRegistry::DeviceList& devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::vector<api::dial::DialDevice> args;
  for (const DialDeviceData& device : devices) {
    api::dial::DialDevice api_device;
    device.FillDialDevice(&api_device);
    args.push_back(std::move(api_device));
  }
  std::unique_ptr<base::ListValue> results =
      api::dial::OnDeviceList::Create(args);
  std::unique_ptr<Event> event(new Event(events::DIAL_ON_DEVICE_LIST,
                                         dial::OnDeviceList::kEventName,
                                         std::move(results)));
  EventRouter::Get(profile_)->BroadcastEvent(std::move(event));
}

void DialAPI::SendErrorOnUIThread(const DialRegistry::DialErrorCode code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  api::dial::DialError dial_error;
  switch (code) {
    case DialRegistry::DIAL_NO_LISTENERS:
      dial_error.code = api::dial::DIAL_ERROR_CODE_NO_LISTENERS;
      break;
    case DialRegistry::DIAL_NO_INTERFACES:
      dial_error.code = api::dial::DIAL_ERROR_CODE_NO_VALID_NETWORK_INTERFACES;
      break;
    case DialRegistry::DIAL_CELLULAR_NETWORK:
      dial_error.code = api::dial::DIAL_ERROR_CODE_CELLULAR_NETWORK;
      break;
    case DialRegistry::DIAL_NETWORK_DISCONNECTED:
      dial_error.code = api::dial::DIAL_ERROR_CODE_NETWORK_DISCONNECTED;
      break;
    case DialRegistry::DIAL_SOCKET_ERROR:
      dial_error.code = api::dial::DIAL_ERROR_CODE_SOCKET_ERROR;
      break;
    default:
      dial_error.code = api::dial::DIAL_ERROR_CODE_UNKNOWN;
      break;
  }

  std::unique_ptr<base::ListValue> results =
      api::dial::OnError::Create(dial_error);
  std::unique_ptr<Event> event(new Event(
      events::DIAL_ON_ERROR, dial::OnError::kEventName, std::move(results)));
  EventRouter::Get(profile_)->BroadcastEvent(std::move(event));
}

void DialAPI::ShutdownOnUIThread() {}

namespace api {

DialDiscoverNowFunction::DialDiscoverNowFunction()
    : dial_(NULL), result_(false) {
}

bool DialDiscoverNowFunction::Prepare() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(browser_context());
  dial_ = DialAPIFactory::GetForBrowserContext(browser_context()).get();
  return true;
}

void DialDiscoverNowFunction::Work() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  result_ = dial_->dial_registry()->DiscoverNow();
}

bool DialDiscoverNowFunction::Respond() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  SetResult(base::MakeUnique<base::FundamentalValue>(result_));
  return true;
}

}  // namespace api

}  // namespace extensions
