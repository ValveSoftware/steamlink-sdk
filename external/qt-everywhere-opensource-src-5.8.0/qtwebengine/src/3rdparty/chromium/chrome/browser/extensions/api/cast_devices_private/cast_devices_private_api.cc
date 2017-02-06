// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/cast_devices_private/cast_devices_private_api.h"

#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/extensions/api/cast_devices_private.h"

namespace extensions {

namespace {

ash::CastConfigDelegate::Receiver ConvertReceiverType(
    const api::cast_devices_private::Receiver& receiver) {
  ash::CastConfigDelegate::Receiver result;
  result.id = receiver.id;
  result.name = base::UTF8ToUTF16(receiver.name);
  return result;
}

ash::CastConfigDelegate::Activity ConvertActivityType(
    const api::cast_devices_private::Activity& activity) {
  ash::CastConfigDelegate::Activity result;
  result.id = activity.id;
  result.title = base::UTF8ToUTF16(activity.title);
  if (activity.tab_id)
    result.tab_id = *activity.tab_id;
  else
    result.tab_id = ash::CastConfigDelegate::Activity::TabId::UNKNOWN;
  return result;
}

ash::CastConfigDelegate::ReceiverAndActivity ConvertReceiverAndActivityType(
    const api::cast_devices_private::Receiver& receiver,
    const api::cast_devices_private::Activity* activity) {
  ash::CastConfigDelegate::ReceiverAndActivity result;
  result.receiver = ConvertReceiverType(receiver);
  if (activity)
    result.activity = ConvertActivityType(*activity);
  return result;
}

}  // namespace

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<CastDeviceUpdateListeners>> g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<CastDeviceUpdateListeners>*
CastDeviceUpdateListeners::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
CastDeviceUpdateListeners* CastDeviceUpdateListeners::Get(
    content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<CastDeviceUpdateListeners>::Get(context);
}

CastDeviceUpdateListeners::CastDeviceUpdateListeners(
    content::BrowserContext* context) {}

CastDeviceUpdateListeners::~CastDeviceUpdateListeners() {}

void CastDeviceUpdateListeners::AddObserver(
    ash::CastConfigDelegate::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void CastDeviceUpdateListeners::RemoveObserver(
    ash::CastConfigDelegate::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void CastDeviceUpdateListeners::NotifyCallbacks(
    const ReceiverAndActivityList& devices) {
  FOR_EACH_OBSERVER(ash::CastConfigDelegate::Observer, observer_list_,
                    OnDevicesUpdated(devices));
}

CastDevicesPrivateUpdateDevicesFunction::
    CastDevicesPrivateUpdateDevicesFunction() {}

CastDevicesPrivateUpdateDevicesFunction::
    ~CastDevicesPrivateUpdateDevicesFunction() {}

ExtensionFunction::ResponseAction
CastDevicesPrivateUpdateDevicesFunction::Run() {
  auto params =
      api::cast_devices_private::UpdateDevices::Params::Create(*args_);

  CastDeviceUpdateListeners::ReceiverAndActivityList devices;
  for (const api::cast_devices_private::ReceiverActivity& device :
       params->devices) {
    devices.push_back(
        ConvertReceiverAndActivityType(device.receiver, device.activity.get()));
  }

  auto listeners = CastDeviceUpdateListeners::Get(browser_context());
  listeners->NotifyCallbacks(devices);

  return RespondNow(NoArguments());
}

}  // namespace extensions
