// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_HID_HID_DEVICE_MANAGER_H_
#define EXTENSIONS_BROWSER_API_HID_HID_DEVICE_MANAGER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_observer.h"
#include "base/threading/thread_checker.h"
#include "device/hid/hid_service.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/common/api/hid.h"

namespace device {
class HidDeviceFilter;
class HidDeviceInfo;
}

namespace extensions {

class Extension;

// This service maps devices enumerated by device::HidService to resource IDs
// returned by the chrome.hid API.
class HidDeviceManager : public BrowserContextKeyedAPI,
                         public device::HidService::Observer,
                         public EventRouter::Observer {
 public:
  typedef base::Callback<void(std::unique_ptr<base::ListValue>)>
      GetApiDevicesCallback;

  explicit HidDeviceManager(content::BrowserContext* context);
  ~HidDeviceManager() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<HidDeviceManager>* GetFactoryInstance();

  // Convenience method to get the HidDeviceManager for a profile.
  static HidDeviceManager* Get(content::BrowserContext* context) {
    return BrowserContextKeyedAPIFactory<HidDeviceManager>::Get(context);
  }

  // Enumerates available devices, taking into account the permissions held by
  // the given extension and the filters provided. The provided callback will
  // be posted to the calling thread's task runner with a list of device info
  // objects.
  void GetApiDevices(const Extension* extension,
                     const std::vector<device::HidDeviceFilter>& filters,
                     const GetApiDevicesCallback& callback);

  // Converts a list of HidDeviceInfo objects into a value that can be returned
  // through the API.
  std::unique_ptr<base::ListValue> GetApiDevicesFromList(
      const std::vector<scoped_refptr<device::HidDeviceInfo>>& devices);

  scoped_refptr<device::HidDeviceInfo> GetDeviceInfo(int resource_id);

  // Checks if |extension| has permission to open |device_info|. Set
  // |update_last_used| to update the timestamp in the DevicePermissionsManager.
  bool HasPermission(const Extension* extension,
                     scoped_refptr<device::HidDeviceInfo> device_info,
                     bool update_last_used);

 private:
  friend class BrowserContextKeyedAPIFactory<HidDeviceManager>;

  typedef std::map<int, device::HidDeviceId> ResourceIdToDeviceIdMap;
  typedef std::map<device::HidDeviceId, int> DeviceIdToResourceIdMap;

  struct GetApiDevicesParams;

  // KeyedService:
  void Shutdown() override;

  // BrowserContextKeyedAPI:
  static const char* service_name() { return "HidDeviceManager"; }
  static const bool kServiceHasOwnInstanceInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

  // EventRouter::Observer:
  void OnListenerAdded(const EventListenerInfo& details) override;

  // HidService::Observer:
  void OnDeviceAdded(scoped_refptr<device::HidDeviceInfo> device_info) override;
  void OnDeviceRemoved(
      scoped_refptr<device::HidDeviceInfo> device_info) override;

  // Wait to perform an initial enumeration and register a HidService::Observer
  // until the first API customer makes a request or registers an event
  // listener.
  void LazyInitialize();

  // Builds a list of device info objects representing the currently enumerated
  // devices, taking into account the permissions held by the given extension
  // and the filters provided.
  std::unique_ptr<base::ListValue> CreateApiDeviceList(
      const Extension* extension,
      const std::vector<device::HidDeviceFilter>& filters);
  void OnEnumerationComplete(
      const std::vector<scoped_refptr<device::HidDeviceInfo>>& devices);

  void DispatchEvent(events::HistogramValue histogram_value,
                     const std::string& event_name,
                     std::unique_ptr<base::ListValue> event_args,
                     scoped_refptr<device::HidDeviceInfo> device_info);

  base::ThreadChecker thread_checker_;
  content::BrowserContext* browser_context_ = nullptr;
  EventRouter* event_router_ = nullptr;
  bool initialized_ = false;
  ScopedObserver<device::HidService, device::HidService::Observer>
      hid_service_observer_;
  bool enumeration_ready_ = false;
  std::vector<std::unique_ptr<GetApiDevicesParams>> pending_enumerations_;
  int next_resource_id_ = 0;
  ResourceIdToDeviceIdMap device_ids_;
  DeviceIdToResourceIdMap resource_ids_;
  base::WeakPtrFactory<HidDeviceManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(HidDeviceManager);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_HID_HID_DEVICE_MANAGER_H_
