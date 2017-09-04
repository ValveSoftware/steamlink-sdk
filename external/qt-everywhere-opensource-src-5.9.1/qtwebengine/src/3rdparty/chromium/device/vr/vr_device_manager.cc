// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/vr_device_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "build/build_config.h"

#if defined(OS_ANDROID)
#include "device/vr/android/gvr/gvr_device_provider.h"
#endif

namespace device {

namespace {
VRDeviceManager* g_vr_device_manager = nullptr;
}

VRDeviceManager::VRDeviceManager()
    : vr_initialized_(false),
      keep_alive_(false),
      has_scheduled_poll_(false),
      has_activate_listeners_(false) {
// Register VRDeviceProviders for the current platform
#if defined(OS_ANDROID)
  RegisterProvider(base::MakeUnique<GvrDeviceProvider>());
#endif
}

VRDeviceManager::VRDeviceManager(std::unique_ptr<VRDeviceProvider> provider)
    : vr_initialized_(false), keep_alive_(true), has_scheduled_poll_(false) {
  thread_checker_.DetachFromThread();
  RegisterProvider(std::move(provider));
  SetInstance(this);
}

VRDeviceManager::~VRDeviceManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  StopSchedulingPollEvents();
  g_vr_device_manager = nullptr;
}

VRDeviceManager* VRDeviceManager::GetInstance() {
  if (!g_vr_device_manager)
    g_vr_device_manager = new VRDeviceManager();
  return g_vr_device_manager;
}

void VRDeviceManager::SetInstance(VRDeviceManager* instance) {
  // Unit tests can create multiple instances but only one should exist at any
  // given time so g_vr_device_manager should only go from nullptr to
  // non-nullptr and vice versa.
  CHECK_NE(!!instance, !!g_vr_device_manager);
  g_vr_device_manager = instance;
}

bool VRDeviceManager::HasInstance() {
  // For testing. Checks to see if a VRDeviceManager instance is active.
  return !!g_vr_device_manager;
}

void VRDeviceManager::AddService(VRServiceImpl* service) {
  // Loop through any currently active devices and send Connected messages to
  // the service. Future devices that come online will send a Connected message
  // when they are created.
  GetVRDevices(service);

  services_.push_back(service);
}

void VRDeviceManager::RemoveService(VRServiceImpl* service) {
  services_.erase(std::remove(services_.begin(), services_.end(), service),
                  services_.end());

  for (auto device : devices_) {
    device.second->RemoveService(service);
  }

  if (service->listening_for_activate()) {
    ListeningForActivateChanged(false);
  }

  if (services_.empty() && !keep_alive_) {
    // Delete the device manager when it has no active connections.
    delete g_vr_device_manager;
  }
}

bool VRDeviceManager::GetVRDevices(VRServiceImpl* service) {
  DCHECK(thread_checker_.CalledOnValidThread());

  InitializeProviders();

  std::vector<VRDevice*> devices;
  for (const auto& provider : providers_)
    provider->GetDevices(&devices);

  if (devices.empty())
    return false;

  for (auto* device : devices) {
    if (device->id() == VR_DEVICE_LAST_ID)
      continue;

    if (devices_.find(device->id()) == devices_.end())
      devices_[device->id()] = device;

    device->AddService(service);
  }

  return true;
}

unsigned int VRDeviceManager::GetNumberOfConnectedDevices() {
  DCHECK(thread_checker_.CalledOnValidThread());

  return static_cast<unsigned int>(devices_.size());
}

void VRDeviceManager::ListeningForActivateChanged(bool listening) {
  DCHECK(thread_checker_.CalledOnValidThread());

  bool activate_listeners = listening;
  if (!activate_listeners) {
    for (const auto& service : services_) {
      if (service->listening_for_activate()) {
        activate_listeners = true;
        break;
      }
    }
  }

  // Notify all the providers if this changes
  if (has_activate_listeners_ != activate_listeners) {
    has_activate_listeners_ = activate_listeners;
    for (const auto& provider : providers_)
      provider->SetListeningForActivate(has_activate_listeners_);
  }
}

VRDevice* VRDeviceManager::GetDevice(unsigned int index) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (index == 0) {
    return NULL;
  }

  DeviceMap::iterator iter = devices_.find(index);
  if (iter == devices_.end()) {
    return nullptr;
  }
  return iter->second;
}

void VRDeviceManager::InitializeProviders() {
  if (vr_initialized_) {
    return;
  }

  for (const auto& provider : providers_) {
    provider->Initialize();
  }

  vr_initialized_ = true;
}

void VRDeviceManager::RegisterProvider(
    std::unique_ptr<VRDeviceProvider> provider) {
  providers_.push_back(std::move(provider));
}

void VRDeviceManager::SchedulePollEvents() {
  if (has_scheduled_poll_)
    return;

  has_scheduled_poll_ = true;

  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(500), this,
               &VRDeviceManager::PollEvents);
}

void VRDeviceManager::PollEvents() {
  for (const auto& provider : providers_)
    provider->PollEvents();
}

void VRDeviceManager::StopSchedulingPollEvents() {
  if (has_scheduled_poll_)
    timer_.Stop();
}

}  // namespace device
