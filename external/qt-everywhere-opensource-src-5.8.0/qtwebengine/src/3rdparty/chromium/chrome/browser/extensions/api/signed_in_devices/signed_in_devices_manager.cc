// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/signed_in_devices/signed_in_devices_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/lazy_instance.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/signed_in_devices/signed_in_devices_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/extensions/api/signed_in_devices.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/sync_driver/device_info.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"

using sync_driver::DeviceInfo;
namespace extensions {

namespace {
void FillDeviceInfo(const DeviceInfo& device_info,
                    api::signed_in_devices::DeviceInfo* api_device_info) {
  api_device_info->id = device_info.public_id();
  api_device_info->name = device_info.client_name();
  api_device_info->os = api::signed_in_devices::ParseOS(
      device_info.GetOSString());
  api_device_info->type = api::signed_in_devices::ParseDeviceType(
      device_info.GetDeviceTypeString());
  api_device_info->chrome_version = device_info.chrome_version();
}
}  // namespace

SignedInDevicesChangeObserver::SignedInDevicesChangeObserver(
    const std::string& extension_id,
    Profile* profile) : extension_id_(extension_id),
                        profile_(profile) {
  ProfileSyncService* pss = ProfileSyncServiceFactory::GetForProfile(profile_);
  if (pss) {
    DCHECK(pss->GetDeviceInfoTracker());
    pss->GetDeviceInfoTracker()->AddObserver(this);
  }
}

SignedInDevicesChangeObserver::~SignedInDevicesChangeObserver() {
  ProfileSyncService* pss = ProfileSyncServiceFactory::GetForProfile(profile_);
  if (pss) {
    DCHECK(pss->GetDeviceInfoTracker());
    pss->GetDeviceInfoTracker()->RemoveObserver(this);
  }
}

void SignedInDevicesChangeObserver::OnDeviceInfoChange() {
  // There is a change in the list of devices. Get all devices and send them to
  // the listener.
  ScopedVector<DeviceInfo> devices = GetAllSignedInDevices(extension_id_,
                                                           profile_);

  std::vector<api::signed_in_devices::DeviceInfo> args;
  for (const DeviceInfo* info : devices) {
    api::signed_in_devices::DeviceInfo api_device;
    FillDeviceInfo(*info, &api_device);
    args.push_back(std::move(api_device));
  }

  std::unique_ptr<base::ListValue> result =
      api::signed_in_devices::OnDeviceInfoChange::Create(args);
  std::unique_ptr<Event> event(
      new Event(events::SIGNED_IN_DEVICES_ON_DEVICE_INFO_CHANGE,
                api::signed_in_devices::OnDeviceInfoChange::kEventName,
                std::move(result)));

  event->restrict_to_browser_context = profile_;

  EventRouter::Get(profile_)
      ->DispatchEventToExtension(extension_id_, std::move(event));
}

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<SignedInDevicesManager> > g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<SignedInDevicesManager>*
SignedInDevicesManager::GetFactoryInstance() {
  return g_factory.Pointer();
}

SignedInDevicesManager::SignedInDevicesManager()
    : profile_(NULL), extension_registry_observer_(this) {
}

SignedInDevicesManager::SignedInDevicesManager(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)),
      extension_registry_observer_(this) {
  EventRouter* router = EventRouter::Get(profile_);
  if (router) {
    router->RegisterObserver(
        this, api::signed_in_devices::OnDeviceInfoChange::kEventName);
  }

  // Register for unload event so we could clear all our listeners when
  // extensions have unloaded.
  extension_registry_observer_.Add(ExtensionRegistry::Get(profile_));
}

SignedInDevicesManager::~SignedInDevicesManager() {
  if (profile_) {
    EventRouter* router = EventRouter::Get(profile_);
    if (router)
      router->UnregisterObserver(this);
  }
}

void SignedInDevicesManager::OnListenerAdded(
    const EventListenerInfo& details) {
  for (ScopedVector<SignedInDevicesChangeObserver>::const_iterator it =
           change_observers_.begin();
           it != change_observers_.end();
           ++it) {
    if ((*it)->extension_id() == details.extension_id) {
      DCHECK(false) <<"OnListenerAded fired twice for same extension";
      return;
    }
  }

  change_observers_.push_back(new SignedInDevicesChangeObserver(
      details.extension_id,
      profile_));
}

void SignedInDevicesManager::OnListenerRemoved(
    const EventListenerInfo& details) {
  RemoveChangeObserverForExtension(details.extension_id);
}

void SignedInDevicesManager::RemoveChangeObserverForExtension(
    const std::string& extension_id) {
  for (ScopedVector<SignedInDevicesChangeObserver>::iterator it =
           change_observers_.begin();
           it != change_observers_.end();
           ++it) {
    if ((*it)->extension_id() == extension_id) {
      change_observers_.erase(it);
      return;
    }
  }
}

void SignedInDevicesManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  RemoveChangeObserverForExtension(extension->id());
}

}  // namespace extensions
