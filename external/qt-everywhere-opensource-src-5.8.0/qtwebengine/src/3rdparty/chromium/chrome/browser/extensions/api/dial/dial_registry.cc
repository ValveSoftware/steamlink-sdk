// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/dial/dial_registry.h"

#include <memory>

#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/dial/dial_api.h"
#include "chrome/browser/extensions/api/dial/dial_device_data.h"
#include "chrome/browser/extensions/api/dial/dial_service.h"
#include "chrome/common/extensions/api/dial.h"
#include "components/net_log/chrome_net_log.h"

using base::Time;
using base::TimeDelta;
using net::NetworkChangeNotifier;

namespace extensions {

DialRegistry::DialRegistry(Observer* dial_api,
                           const base::TimeDelta& refresh_interval,
                           const base::TimeDelta& expiration,
                           const size_t max_devices)
  : num_listeners_(0),
    registry_generation_(0),
    last_event_registry_generation_(0),
    label_count_(0),
    refresh_interval_delta_(refresh_interval),
    expiration_delta_(expiration),
    max_devices_(max_devices),
    dial_api_(dial_api) {
  DCHECK(max_devices_ > 0);
  NetworkChangeNotifier::AddNetworkChangeObserver(this);
}

DialRegistry::~DialRegistry() {
  DCHECK(thread_checker_.CalledOnValidThread());
  NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

DialService* DialRegistry::CreateDialService() {
  DCHECK(g_browser_process->net_log());
  return new DialServiceImpl(g_browser_process->net_log());
}

void DialRegistry::ClearDialService() {
  dial_.reset();
}

base::Time DialRegistry::Now() const {
  return Time::Now();
}

void DialRegistry::OnListenerAdded() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (++num_listeners_ == 1) {
    VLOG(2) << "Listener added; starting periodic discovery.";
    StartPeriodicDiscovery();
  }
  // Event listeners with the current device list.
  // TODO(crbug.com/576817): Rework the DIAL API so we don't need to have extra
  // behaviors when adding listeners.
  SendEvent();
}

void DialRegistry::OnListenerRemoved() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(num_listeners_ > 0);
  if (--num_listeners_ == 0) {
    VLOG(2) << "Listeners removed; stopping periodic discovery.";
    StopPeriodicDiscovery();
  }
}

bool DialRegistry::ReadyToDiscover() {
  if (num_listeners_ == 0) {
    dial_api_->OnDialError(DIAL_NO_LISTENERS);
    return false;
  }
  if (NetworkChangeNotifier::IsOffline()) {
    dial_api_->OnDialError(DIAL_NETWORK_DISCONNECTED);
    return false;
  }
  if (NetworkChangeNotifier::IsConnectionCellular(
          NetworkChangeNotifier::GetConnectionType())) {
    dial_api_->OnDialError(DIAL_CELLULAR_NETWORK);
    return false;
  }
  return true;
}

bool DialRegistry::DiscoverNow() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!ReadyToDiscover()) {
    return false;
  }
  if (!dial_.get()) {
    dial_api_->OnDialError(DIAL_UNKNOWN);
    return false;
  }

  if (!dial_->HasObserver(this))
    NOTREACHED() << "DiscoverNow() called without observing dial_";

  // Force increment |registry_generation_| to ensure an event is sent even if
  // the device list did not change.
  bool started = dial_->Discover();
  if (started)
    ++registry_generation_;

  return started;
}

void DialRegistry::StartPeriodicDiscovery() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!ReadyToDiscover() || dial_.get())
    return;

  dial_.reset(CreateDialService());
  dial_->AddObserver(this);
  DoDiscovery();
  repeating_timer_.Start(FROM_HERE,
                         refresh_interval_delta_,
                         this,
                         &DialRegistry::DoDiscovery);
}

void DialRegistry::DoDiscovery() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(dial_.get());
  VLOG(2) << "About to discover!";
  dial_->Discover();
}

void DialRegistry::StopPeriodicDiscovery() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!dial_.get())
    return;

  repeating_timer_.Stop();
  dial_->RemoveObserver(this);
  ClearDialService();
}

bool DialRegistry::PruneExpiredDevices() {
  DCHECK(thread_checker_.CalledOnValidThread());
  bool pruned_device = false;
  DeviceByLabelMap::iterator i = device_by_label_map_.begin();
  while (i != device_by_label_map_.end()) {
    linked_ptr<DialDeviceData> device = i->second;
    if (IsDeviceExpired(*device)) {
      VLOG(2) << "Device " << device->label() << " expired, removing";
      const size_t num_erased_by_id =
          device_by_id_map_.erase(device->device_id());
      DCHECK_EQ(num_erased_by_id, 1u);
      device_by_label_map_.erase(i++);
      pruned_device = true;
    } else {
      ++i;
    }
  }
  return pruned_device;
}

bool DialRegistry::IsDeviceExpired(const DialDeviceData& device) const {
  Time now = Now();

  // Check against our default expiration timeout.
  Time default_expiration_time = device.response_time() + expiration_delta_;
  if (now > default_expiration_time)
    return true;

  // Check against the device's cache-control header, if set.
  if (device.has_max_age()) {
    Time max_age_expiration_time =
      device.response_time() + TimeDelta::FromSeconds(device.max_age());
    if (now > max_age_expiration_time)
      return true;
  }
  return false;
}

void DialRegistry::Clear() {
  DCHECK(thread_checker_.CalledOnValidThread());
  device_by_id_map_.clear();
  device_by_label_map_.clear();
  registry_generation_++;
}

void DialRegistry::MaybeSendEvent() {
  // Send an event if the device list has changed since the last event.
  bool needs_event = last_event_registry_generation_ < registry_generation_;
  VLOG(2) << "lerg = " << last_event_registry_generation_ << ", rg = "
          << registry_generation_ << ", needs_event = " << needs_event;
  if (needs_event)
    SendEvent();
}

void DialRegistry::SendEvent() {
  DeviceList device_list;
  for (DeviceByLabelMap::const_iterator i = device_by_label_map_.begin();
       i != device_by_label_map_.end(); i++) {
    device_list.push_back(*(i->second));
  }
  dial_api_->OnDialDeviceEvent(device_list);

  // Reset watermark.
  last_event_registry_generation_ = registry_generation_;
}

std::string DialRegistry::NextLabel() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return base::IntToString(++label_count_);
}

void DialRegistry::OnDiscoveryRequest(DialService* service) {
  DCHECK(thread_checker_.CalledOnValidThread());
  MaybeSendEvent();
}

void DialRegistry::OnDeviceDiscovered(DialService* service,
                                      const DialDeviceData& device) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Adds |device| to our list of devices or updates an existing device, unless
  // |device| is a duplicate. Returns true if the list was modified and
  // increments the list generation.
  linked_ptr<DialDeviceData> device_data(new DialDeviceData(device));
  DCHECK(!device_data->device_id().empty());
  DCHECK(device_data->label().empty());

  bool did_modify_list = false;
  DeviceByIdMap::iterator lookup_result =
      device_by_id_map_.find(device_data->device_id());

  if (lookup_result != device_by_id_map_.end()) {
    VLOG(2) << "Found device " << device_data->device_id() << ", merging";

    // Already have previous response.  Merge in data from this response and
    // track if there were any API visible changes.
    did_modify_list = lookup_result->second->UpdateFrom(*device_data);
  } else {
    did_modify_list = MaybeAddDevice(device_data);
  }

  if (did_modify_list)
    registry_generation_++;

  VLOG(2) << "did_modify_list = " << did_modify_list
          << ", generation = " << registry_generation_;
}

bool DialRegistry::MaybeAddDevice(
    const linked_ptr<DialDeviceData>& device_data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (device_by_id_map_.size() == max_devices_) {
    VLOG(1) << "Maximum registry size reached.  Cannot add device.";
    return false;
  }
  device_data->set_label(NextLabel());
  device_by_id_map_[device_data->device_id()] = device_data;
  device_by_label_map_[device_data->label()] = device_data;
  VLOG(2) << "Added device, id = " << device_data->device_id()
          << ", label = " << device_data->label();
  return true;
}

void DialRegistry::OnDiscoveryFinished(DialService* service) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (PruneExpiredDevices())
    registry_generation_++;
  MaybeSendEvent();
}

void DialRegistry::OnError(DialService* service,
                           const DialService::DialServiceErrorCode& code) {
  DCHECK(thread_checker_.CalledOnValidThread());
  switch (code) {
    case DialService::DIAL_SERVICE_SOCKET_ERROR:
      dial_api_->OnDialError(DIAL_SOCKET_ERROR);
      break;
    case DialService::DIAL_SERVICE_NO_INTERFACES:
      dial_api_->OnDialError(DIAL_NO_INTERFACES);
      break;
    default:
      NOTREACHED();
      dial_api_->OnDialError(DIAL_UNKNOWN);
      break;
  }
}

void DialRegistry::OnNetworkChanged(
    NetworkChangeNotifier::ConnectionType type) {
  switch (type) {
    case NetworkChangeNotifier::CONNECTION_NONE:
      if (dial_.get()) {
        VLOG(2) << "Lost connection, shutting down discovery and clearing"
                << " list.";
        dial_api_->OnDialError(DIAL_NETWORK_DISCONNECTED);

        StopPeriodicDiscovery();
        // TODO(justinlin): As an optimization, we can probably keep our device
        // list around and restore it if we reconnected to the exact same
        // network.
        Clear();
        MaybeSendEvent();
      }
      break;
    case NetworkChangeNotifier::CONNECTION_2G:
    case NetworkChangeNotifier::CONNECTION_3G:
    case NetworkChangeNotifier::CONNECTION_4G:
    case NetworkChangeNotifier::CONNECTION_ETHERNET:
    case NetworkChangeNotifier::CONNECTION_WIFI:
    case NetworkChangeNotifier::CONNECTION_UNKNOWN:
    case NetworkChangeNotifier::CONNECTION_BLUETOOTH:
      if (!dial_.get()) {
        VLOG(2) << "Connection detected, restarting discovery.";
        StartPeriodicDiscovery();
      }
      break;
  }
}

}  // namespace extensions
