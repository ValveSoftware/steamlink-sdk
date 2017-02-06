// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/device_permissions_prompt.h"

#include <utility>

#include "base/bind.h"
#include "base/i18n/message_formatter.h"
#include "base/scoped_observer.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "device/core/device_client.h"
#include "device/hid/hid_device_filter.h"
#include "device/hid/hid_device_info.h"
#include "device/hid/hid_service.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_device_filter.h"
#include "device/usb/usb_ids.h"
#include "device/usb/usb_service.h"
#include "extensions/browser/api/device_permissions_manager.h"
#include "extensions/common/extension.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/permission_broker_client.h"
#include "device/hid/hid_device_info_linux.h"
#endif  // defined(OS_CHROMEOS)

using device::HidDeviceFilter;
using device::HidService;
using device::UsbDevice;
using device::UsbDeviceFilter;
using device::UsbService;

namespace extensions {

namespace {

void NoopHidCallback(const std::vector<scoped_refptr<device::HidDeviceInfo>>&) {
}

void NoopUsbCallback(const std::vector<scoped_refptr<device::UsbDevice>>&) {}

class UsbDeviceInfo : public DevicePermissionsPrompt::Prompt::DeviceInfo {
 public:
  UsbDeviceInfo(scoped_refptr<UsbDevice> device) : device_(device) {
    name_ = DevicePermissionsManager::GetPermissionMessage(
        device->vendor_id(), device->product_id(),
        device->manufacturer_string(), device->product_string(),
        base::string16(),  // Serial number is displayed separately.
        true);
    serial_number_ = device->serial_number();
  }

  ~UsbDeviceInfo() override {}

  const scoped_refptr<UsbDevice>& device() const { return device_; }

 private:
  // TODO(reillyg): Convert this to a weak reference when UsbDevice has a
  // connected flag.
  scoped_refptr<UsbDevice> device_;
};

class UsbDevicePermissionsPrompt : public DevicePermissionsPrompt::Prompt,
                                   public device::UsbService::Observer {
 public:
  UsbDevicePermissionsPrompt(
      const Extension* extension,
      content::BrowserContext* context,
      bool multiple,
      const std::vector<UsbDeviceFilter>& filters,
      const DevicePermissionsPrompt::UsbDevicesCallback& callback)
      : Prompt(extension, context, multiple),
        filters_(filters),
        callback_(callback),
        service_observer_(this) {}

 private:
  ~UsbDevicePermissionsPrompt() override {}

  // DevicePermissionsPrompt::Prompt implementation:
  void SetObserver(
      DevicePermissionsPrompt::Prompt::Observer* observer) override {
    DevicePermissionsPrompt::Prompt::SetObserver(observer);

    if (observer) {
      UsbService* service = device::DeviceClient::Get()->GetUsbService();
      if (service && !service_observer_.IsObserving(service)) {
        service->GetDevices(
            base::Bind(&UsbDevicePermissionsPrompt::OnDevicesEnumerated, this));
        service_observer_.Add(service);
      }
    }
  }

  base::string16 GetHeading() const override {
    return l10n_util::GetSingleOrMultipleStringUTF16(
        IDS_USB_DEVICE_PERMISSIONS_PROMPT_TITLE, multiple());
  }

  void Dismissed() override {
    DevicePermissionsManager* permissions_manager =
        DevicePermissionsManager::Get(browser_context());
    std::vector<scoped_refptr<UsbDevice>> devices;
    for (const auto& device : devices_) {
      if (device->granted()) {
        const UsbDeviceInfo* usb_device =
            static_cast<const UsbDeviceInfo*>(device.get());
        devices.push_back(usb_device->device());
        if (permissions_manager) {
          permissions_manager->AllowUsbDevice(extension()->id(),
                                              usb_device->device());
        }
      }
    }
    DCHECK(multiple() || devices.size() <= 1);
    callback_.Run(devices);
    callback_.Reset();
  }

  // device::UsbService::Observer implementation:
  void OnDeviceAdded(scoped_refptr<UsbDevice> device) override {
    if (!(filters_.empty() || UsbDeviceFilter::MatchesAny(device, filters_))) {
      return;
    }

    std::unique_ptr<DeviceInfo> device_info(new UsbDeviceInfo(device));
    device->CheckUsbAccess(
        base::Bind(&UsbDevicePermissionsPrompt::AddCheckedDevice, this,
                   base::Passed(&device_info)));
  }

  void OnDeviceRemoved(scoped_refptr<UsbDevice> device) override {
    for (auto it = devices_.begin(); it != devices_.end(); ++it) {
      const UsbDeviceInfo* entry =
          static_cast<const UsbDeviceInfo*>((*it).get());
      if (entry->device() == device) {
        devices_.erase(it);
        if (observer()) {
          observer()->OnDevicesChanged();
        }
        return;
      }
    }
  }

  void OnDevicesEnumerated(
      const std::vector<scoped_refptr<UsbDevice>>& devices) {
    for (const auto& device : devices) {
      OnDeviceAdded(device);
    }
  }

  std::vector<UsbDeviceFilter> filters_;
  DevicePermissionsPrompt::UsbDevicesCallback callback_;
  ScopedObserver<UsbService, UsbService::Observer> service_observer_;
};

class HidDeviceInfo : public DevicePermissionsPrompt::Prompt::DeviceInfo {
 public:
  HidDeviceInfo(scoped_refptr<device::HidDeviceInfo> device) : device_(device) {
    name_ = DevicePermissionsManager::GetPermissionMessage(
        device->vendor_id(), device->product_id(),
        base::string16(),  // HID devices include manufacturer in product name.
        base::UTF8ToUTF16(device->product_name()),
        base::string16(),  // Serial number is displayed separately.
        false);
    serial_number_ = base::UTF8ToUTF16(device->serial_number());
  }

  ~HidDeviceInfo() override {}

  const scoped_refptr<device::HidDeviceInfo>& device() const { return device_; }

 private:
  scoped_refptr<device::HidDeviceInfo> device_;
};

class HidDevicePermissionsPrompt : public DevicePermissionsPrompt::Prompt,
                                   public device::HidService::Observer {
 public:
  HidDevicePermissionsPrompt(
      const Extension* extension,
      content::BrowserContext* context,
      bool multiple,
      const std::vector<HidDeviceFilter>& filters,
      const DevicePermissionsPrompt::HidDevicesCallback& callback)
      : Prompt(extension, context, multiple),
        filters_(filters),
        callback_(callback),
        service_observer_(this) {}

 private:
  ~HidDevicePermissionsPrompt() override {}

  // DevicePermissionsPrompt::Prompt implementation:
  void SetObserver(
      DevicePermissionsPrompt::Prompt::Observer* observer) override {
    DevicePermissionsPrompt::Prompt::SetObserver(observer);

    if (observer) {
      HidService* service = device::DeviceClient::Get()->GetHidService();
      if (service && !service_observer_.IsObserving(service)) {
        service->GetDevices(
            base::Bind(&HidDevicePermissionsPrompt::OnDevicesEnumerated, this));
        service_observer_.Add(service);
      }
    }
  }

  base::string16 GetHeading() const override {
    return l10n_util::GetSingleOrMultipleStringUTF16(
        IDS_HID_DEVICE_PERMISSIONS_PROMPT_TITLE, multiple());
  }

  void Dismissed() override {
    DevicePermissionsManager* permissions_manager =
        DevicePermissionsManager::Get(browser_context());
    std::vector<scoped_refptr<device::HidDeviceInfo>> devices;
    for (const auto& device : devices_) {
      if (device->granted()) {
        const HidDeviceInfo* hid_device =
            static_cast<const HidDeviceInfo*>(device.get());
        devices.push_back(hid_device->device());
        if (permissions_manager) {
          permissions_manager->AllowHidDevice(extension()->id(),
                                              hid_device->device());
        }
      }
    }
    DCHECK(multiple() || devices.size() <= 1);
    callback_.Run(devices);
    callback_.Reset();
  }

  // device::HidService::Observer implementation:
  void OnDeviceAdded(scoped_refptr<device::HidDeviceInfo> device) override {
    if (HasUnprotectedCollections(device) &&
        (filters_.empty() || HidDeviceFilter::MatchesAny(device, filters_))) {
      std::unique_ptr<DeviceInfo> device_info(new HidDeviceInfo(device));
#if defined(OS_CHROMEOS)
      chromeos::PermissionBrokerClient* client =
          chromeos::DBusThreadManager::Get()->GetPermissionBrokerClient();
      DCHECK(client) << "Could not get permission broker client.";
      device::HidDeviceInfoLinux* linux_device_info =
          static_cast<device::HidDeviceInfoLinux*>(device.get());
      client->CheckPathAccess(
          linux_device_info->device_node(),
          base::Bind(&HidDevicePermissionsPrompt::AddCheckedDevice, this,
                     base::Passed(&device_info)));
#else
      AddCheckedDevice(std::move(device_info), true);
#endif  // defined(OS_CHROMEOS)
    }
  }

  void OnDeviceRemoved(scoped_refptr<device::HidDeviceInfo> device) override {
    for (auto it = devices_.begin(); it != devices_.end(); ++it) {
      const HidDeviceInfo* entry =
          static_cast<const HidDeviceInfo*>((*it).get());
      if (entry->device() == device) {
        devices_.erase(it);
        if (observer()) {
          observer()->OnDevicesChanged();
        }
        return;
      }
    }
  }

  void OnDevicesEnumerated(
      const std::vector<scoped_refptr<device::HidDeviceInfo>>& devices) {
    for (const auto& device : devices) {
      OnDeviceAdded(device);
    }
  }

  bool HasUnprotectedCollections(scoped_refptr<device::HidDeviceInfo> device) {
    for (const auto& collection : device->collections()) {
      if (!collection.usage.IsProtected()) {
        return true;
      }
    }
    return false;
  }

  std::vector<HidDeviceFilter> filters_;
  DevicePermissionsPrompt::HidDevicesCallback callback_;
  ScopedObserver<HidService, HidService::Observer> service_observer_;
};

}  // namespace

DevicePermissionsPrompt::Prompt::DeviceInfo::DeviceInfo() {
}

DevicePermissionsPrompt::Prompt::DeviceInfo::~DeviceInfo() {
}

DevicePermissionsPrompt::Prompt::Observer::~Observer() {
}

DevicePermissionsPrompt::Prompt::Prompt(const Extension* extension,
                                        content::BrowserContext* context,
                                        bool multiple)
    : extension_(extension), browser_context_(context), multiple_(multiple) {
}

void DevicePermissionsPrompt::Prompt::SetObserver(Observer* observer) {
  observer_ = observer;
}

base::string16 DevicePermissionsPrompt::Prompt::GetPromptMessage() const {
  return base::i18n::MessageFormatter::FormatWithNumberedArgs(
      l10n_util::GetStringUTF16(IDS_DEVICE_PERMISSIONS_PROMPT),
      multiple_ ? "multiple" : "single", extension_->name());
}

base::string16 DevicePermissionsPrompt::Prompt::GetDeviceName(
    size_t index) const {
  DCHECK_LT(index, devices_.size());
  return devices_[index]->name();
}

base::string16 DevicePermissionsPrompt::Prompt::GetDeviceSerialNumber(
    size_t index) const {
  DCHECK_LT(index, devices_.size());
  return devices_[index]->serial_number();
}

void DevicePermissionsPrompt::Prompt::GrantDevicePermission(size_t index) {
  DCHECK_LT(index, devices_.size());
  devices_[index]->set_granted();
}

DevicePermissionsPrompt::Prompt::~Prompt() {
}

void DevicePermissionsPrompt::Prompt::AddCheckedDevice(
    std::unique_ptr<DeviceInfo> device,
    bool allowed) {
  if (allowed) {
    devices_.push_back(std::move(device));
    if (observer_) {
      observer_->OnDevicesChanged();
    }
  }
}

DevicePermissionsPrompt::DevicePermissionsPrompt(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
}

DevicePermissionsPrompt::~DevicePermissionsPrompt() {
}

void DevicePermissionsPrompt::AskForUsbDevices(
    const Extension* extension,
    content::BrowserContext* context,
    bool multiple,
    const std::vector<UsbDeviceFilter>& filters,
    const UsbDevicesCallback& callback) {
  prompt_ = new UsbDevicePermissionsPrompt(extension, context, multiple,
                                           filters, callback);
  ShowDialog();
}

void DevicePermissionsPrompt::AskForHidDevices(
    const Extension* extension,
    content::BrowserContext* context,
    bool multiple,
    const std::vector<HidDeviceFilter>& filters,
    const HidDevicesCallback& callback) {
  prompt_ = new HidDevicePermissionsPrompt(extension, context, multiple,
                                           filters, callback);
  ShowDialog();
}

// static
scoped_refptr<DevicePermissionsPrompt::Prompt>
DevicePermissionsPrompt::CreateHidPromptForTest(const Extension* extension,
                                                bool multiple) {
  return make_scoped_refptr(new HidDevicePermissionsPrompt(
      extension, nullptr, multiple, std::vector<HidDeviceFilter>(),
      base::Bind(&NoopHidCallback)));
}

// static
scoped_refptr<DevicePermissionsPrompt::Prompt>
DevicePermissionsPrompt::CreateUsbPromptForTest(const Extension* extension,
                                                bool multiple) {
  return make_scoped_refptr(new UsbDevicePermissionsPrompt(
      extension, nullptr, multiple, std::vector<UsbDeviceFilter>(),
      base::Bind(&NoopUsbCallback)));
}

}  // namespace extensions
