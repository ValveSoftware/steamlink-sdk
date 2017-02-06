// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_service_impl.h"

#include <stdint.h>

#include <list>
#include <memory>
#include <set>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"
#include "device/usb/usb_device_handle.h"
#include "device/usb/usb_error.h"
#include "device/usb/webusb_descriptors.h"
#include "net/base/io_buffer.h"
#include "third_party/libusb/src/libusb/libusb.h"

#if defined(OS_WIN)
#include <setupapi.h>
#include <usbiodef.h>

#include "base/strings/string_util.h"
#include "device/core/device_info_query_win.h"
#endif  // OS_WIN

using net::IOBufferWithSize;

namespace device {

namespace {

// Standard USB requests and descriptor types:
const uint16_t kUsbVersion2_1 = 0x0210;

#if defined(OS_WIN)

bool IsWinUsbInterface(const std::string& device_path) {
  DeviceInfoQueryWin device_info_query;
  if (!device_info_query.device_info_list_valid()) {
    USB_PLOG(ERROR) << "Failed to create a device information set";
    return false;
  }

  // This will add the device so we can query driver info.
  if (!device_info_query.AddDevice(device_path.c_str())) {
    USB_PLOG(ERROR) << "Failed to get device interface data for "
                    << device_path;
    return false;
  }

  if (!device_info_query.GetDeviceInfo()) {
    USB_PLOG(ERROR) << "Failed to get device info for " << device_path;
    return false;
  }

  std::string buffer;
  if (!device_info_query.GetDeviceStringProperty(SPDRP_SERVICE, &buffer)) {
    USB_PLOG(ERROR) << "Failed to get device service property";
    return false;
  }

  USB_LOG(DEBUG) << "Driver for " << device_path << " is " << buffer << ".";
  if (base::StartsWith(buffer, "WinUSB", base::CompareCase::INSENSITIVE_ASCII))
    return true;
  return false;
}

#endif  // OS_WIN

void GetDeviceListOnBlockingThread(
    const std::string& new_device_path,
    scoped_refptr<UsbContext> usb_context,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::Callback<void(libusb_device**, size_t)> callback) {
#if defined(OS_WIN)
  if (!new_device_path.empty()) {
    if (!IsWinUsbInterface(new_device_path)) {
      // Wait to call libusb_get_device_list until libusb will be able to find
      // a WinUSB interface for the device.
      task_runner->PostTask(FROM_HERE, base::Bind(callback, nullptr, 0));
      return;
    }
  }
#endif  // defined(OS_WIN)

  libusb_device** platform_devices = NULL;
  const ssize_t device_count =
      libusb_get_device_list(usb_context->context(), &platform_devices);
  if (device_count < 0) {
    USB_LOG(ERROR) << "Failed to get device list: "
                   << ConvertPlatformUsbErrorToString(device_count);
    task_runner->PostTask(FROM_HERE, base::Bind(callback, nullptr, 0));
    return;
  }

  task_runner->PostTask(FROM_HERE,
                        base::Bind(callback, platform_devices, device_count));
}

void CloseHandleAndRunContinuation(scoped_refptr<UsbDeviceHandle> device_handle,
                                   const base::Closure& continuation) {
  device_handle->Close();
  continuation.Run();
}

void SaveStringsAndRunContinuation(
    scoped_refptr<UsbDeviceImpl> device,
    uint8_t manufacturer,
    uint8_t product,
    uint8_t serial_number,
    const base::Closure& continuation,
    std::unique_ptr<std::map<uint8_t, base::string16>> string_map) {
  if (manufacturer != 0)
    device->set_manufacturer_string((*string_map)[manufacturer]);
  if (product != 0)
    device->set_product_string((*string_map)[product]);
  if (serial_number != 0)
    device->set_serial_number((*string_map)[serial_number]);
  continuation.Run();
}

void OnReadBosDescriptor(scoped_refptr<UsbDeviceHandle> device_handle,
                         const base::Closure& barrier,
                         std::unique_ptr<WebUsbAllowedOrigins> allowed_origins,
                         const GURL& landing_page) {
  scoped_refptr<UsbDeviceImpl> device =
      static_cast<UsbDeviceImpl*>(device_handle->GetDevice().get());

  if (allowed_origins)
    device->set_webusb_allowed_origins(std::move(allowed_origins));
  if (landing_page.is_valid())
    device->set_webusb_landing_page(landing_page);

  barrier.Run();
}

void OnDeviceOpenedReadDescriptors(
    uint8_t manufacturer,
    uint8_t product,
    uint8_t serial_number,
    bool read_bos_descriptors,
    const base::Closure& success_closure,
    const base::Closure& failure_closure,
    scoped_refptr<UsbDeviceHandle> device_handle) {
  if (device_handle) {
    std::unique_ptr<std::map<uint8_t, base::string16>> string_map(
        new std::map<uint8_t, base::string16>());
    if (manufacturer != 0)
      (*string_map)[manufacturer] = base::string16();
    if (product != 0)
      (*string_map)[product] = base::string16();
    if (serial_number != 0)
      (*string_map)[serial_number] = base::string16();

    int count = 0;
    if (!string_map->empty())
      count++;
    if (read_bos_descriptors)
      count++;
    DCHECK_GT(count, 0);

    base::Closure barrier =
        base::BarrierClosure(count, base::Bind(&CloseHandleAndRunContinuation,
                                               device_handle, success_closure));

    if (!string_map->empty()) {
      scoped_refptr<UsbDeviceImpl> device =
          static_cast<UsbDeviceImpl*>(device_handle->GetDevice().get());

      ReadUsbStringDescriptors(
          device_handle, std::move(string_map),
          base::Bind(&SaveStringsAndRunContinuation, device, manufacturer,
                     product, serial_number, barrier));
    }

    if (read_bos_descriptors) {
      ReadWebUsbDescriptors(device_handle, base::Bind(&OnReadBosDescriptor,
                                                      device_handle, barrier));
    }
  } else {
    failure_closure.Run();
  }
}

}  // namespace

UsbServiceImpl::UsbServiceImpl(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : UsbService(base::ThreadTaskRunnerHandle::Get(), blocking_task_runner),
#if defined(OS_WIN)
      device_observer_(this),
#endif
      weak_factory_(this) {
  PlatformUsbContext platform_context = nullptr;
  int rv = libusb_init(&platform_context);
  if (rv != LIBUSB_SUCCESS || !platform_context) {
    USB_LOG(DEBUG) << "Failed to initialize libusb: "
                   << ConvertPlatformUsbErrorToString(rv);
    return;
  }
  context_ = new UsbContext(platform_context);

  rv = libusb_hotplug_register_callback(
      context_->context(),
      static_cast<libusb_hotplug_event>(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
                                        LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT),
      static_cast<libusb_hotplug_flag>(0), LIBUSB_HOTPLUG_MATCH_ANY,
      LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY,
      &UsbServiceImpl::HotplugCallback, this, &hotplug_handle_);
  if (rv == LIBUSB_SUCCESS) {
    hotplug_enabled_ = true;
  }

  RefreshDevices();
#if defined(OS_WIN)
  DeviceMonitorWin* device_monitor = DeviceMonitorWin::GetForAllInterfaces();
  if (device_monitor) {
    device_observer_.Add(device_monitor);
  }
#endif  // OS_WIN
}

UsbServiceImpl::~UsbServiceImpl() {
  if (hotplug_enabled_)
    libusb_hotplug_deregister_callback(context_->context(), hotplug_handle_);
  for (const auto& platform_device : ignored_devices_)
    libusb_unref_device(platform_device);
}

void UsbServiceImpl::GetDevices(const GetDevicesCallback& callback) {
  DCHECK(CalledOnValidThread());

  if (!context_) {
    task_runner()->PostTask(
        FROM_HERE,
        base::Bind(callback, std::vector<scoped_refptr<UsbDevice>>()));
    return;
  }

  if (hotplug_enabled_ && !enumeration_in_progress_) {
    // The device list is updated live when hotplug events are supported.
    UsbService::GetDevices(callback);
  } else {
    pending_enumeration_callbacks_.push_back(callback);
    RefreshDevices();
  }
}

#if defined(OS_WIN)

void UsbServiceImpl::OnDeviceAdded(const GUID& class_guid,
                                   const std::string& device_path) {
  // Only the root node of a composite USB device has the class GUID
  // GUID_DEVINTERFACE_USB_DEVICE but we want to wait until WinUSB is loaded.
  // This first pass filter will catch anything that's sitting on the USB bus
  // (including devices on 3rd party USB controllers) to avoid the more
  // expensive driver check that needs to be done on the FILE thread.
  if (device_path.find("usb") != std::string::npos) {
    pending_path_enumerations_.push(device_path);
    RefreshDevices();
  }
}

void UsbServiceImpl::OnDeviceRemoved(const GUID& class_guid,
                                     const std::string& device_path) {
  // The root USB device node is removed last.
  if (class_guid == GUID_DEVINTERFACE_USB_DEVICE) {
    RefreshDevices();
  }
}

#endif  // OS_WIN

void UsbServiceImpl::RefreshDevices() {
  DCHECK(CalledOnValidThread());
  DCHECK(context_);

  if (enumeration_in_progress_) {
    return;
  }

  enumeration_in_progress_ = true;
  DCHECK(devices_being_enumerated_.empty());

  std::string device_path;
  if (!pending_path_enumerations_.empty()) {
    device_path = pending_path_enumerations_.front();
    pending_path_enumerations_.pop();
  }

  blocking_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&GetDeviceListOnBlockingThread, device_path, context_,
                 task_runner(), base::Bind(&UsbServiceImpl::OnDeviceList,
                                           weak_factory_.GetWeakPtr())));
}

void UsbServiceImpl::OnDeviceList(libusb_device** platform_devices,
                                  size_t device_count) {
  DCHECK(CalledOnValidThread());
  if (!platform_devices) {
    RefreshDevicesComplete();
    return;
  }

  base::Closure refresh_complete =
      base::BarrierClosure(static_cast<int>(device_count),
                           base::Bind(&UsbServiceImpl::RefreshDevicesComplete,
                                      weak_factory_.GetWeakPtr()));
  std::list<PlatformUsbDevice> new_devices;
  std::set<PlatformUsbDevice> existing_ignored_devices;

  // Look for new and existing devices.
  for (size_t i = 0; i < device_count; ++i) {
    PlatformUsbDevice platform_device = platform_devices[i];
    // Ignore some devices.
    if (ContainsValue(ignored_devices_, platform_device)) {
      existing_ignored_devices.insert(platform_device);
      refresh_complete.Run();
      continue;
    }

    auto it = platform_devices_.find(platform_device);

    if (it == platform_devices_.end()) {
      new_devices.push_back(platform_device);
    } else {
      it->second->set_visited(true);
      refresh_complete.Run();
    }
  }

  // Remove devices not seen in this enumeration.
  for (PlatformDeviceMap::iterator it = platform_devices_.begin();
       it != platform_devices_.end();
       /* incremented internally */) {
    PlatformDeviceMap::iterator current = it++;
    const scoped_refptr<UsbDeviceImpl>& device = current->second;
    if (device->was_visited()) {
      device->set_visited(false);
    } else {
      RemoveDevice(device);
    }
  }

  // Remove devices not seen in this enumeration from |ignored_devices_|.
  for (auto it = ignored_devices_.begin(); it != ignored_devices_.end();
       /* incremented internally */) {
    auto current = it++;
    if (!ContainsValue(existing_ignored_devices, *current)) {
      libusb_unref_device(*current);
      ignored_devices_.erase(current);
    }
  }

  for (PlatformUsbDevice platform_device : new_devices) {
    EnumerateDevice(platform_device, refresh_complete);
  }

  libusb_free_device_list(platform_devices, true);
}

void UsbServiceImpl::RefreshDevicesComplete() {
  DCHECK(CalledOnValidThread());
  DCHECK(enumeration_in_progress_);

  enumeration_ready_ = true;
  enumeration_in_progress_ = false;
  devices_being_enumerated_.clear();

  if (!pending_enumeration_callbacks_.empty()) {
    std::vector<scoped_refptr<UsbDevice>> result;
    result.reserve(devices().size());
    for (const auto& map_entry : devices())
      result.push_back(map_entry.second);

    std::vector<GetDevicesCallback> callbacks;
    callbacks.swap(pending_enumeration_callbacks_);
    for (const GetDevicesCallback& callback : callbacks)
      callback.Run(result);
  }

  if (!pending_path_enumerations_.empty()) {
    RefreshDevices();
  }
}

void UsbServiceImpl::EnumerateDevice(PlatformUsbDevice platform_device,
                                     const base::Closure& refresh_complete) {
  DCHECK(context_);
  devices_being_enumerated_.insert(platform_device);

  libusb_device_descriptor descriptor;
  int rv = libusb_get_device_descriptor(platform_device, &descriptor);
  if (rv == LIBUSB_SUCCESS) {
    if (descriptor.bDeviceClass == LIBUSB_CLASS_HUB) {
      // Don't try to enumerate hubs. We never want to connect to a hub.
      libusb_ref_device(platform_device);
      ignored_devices_.insert(platform_device);
      refresh_complete.Run();
      return;
    }

    scoped_refptr<UsbDeviceImpl> device(new UsbDeviceImpl(
        context_, platform_device, descriptor, blocking_task_runner()));
    base::Closure add_device =
        base::Bind(&UsbServiceImpl::AddDevice, weak_factory_.GetWeakPtr(),
                   refresh_complete, device);
    base::Closure enumeration_failed = base::Bind(
        &UsbServiceImpl::EnumerationFailed, weak_factory_.GetWeakPtr(),
        platform_device, refresh_complete);
    bool read_bos_descriptors = descriptor.bcdUSB >= kUsbVersion2_1;

    if (descriptor.iManufacturer == 0 && descriptor.iProduct == 0 &&
        descriptor.iSerialNumber == 0 && !read_bos_descriptors) {
      // Don't bother disturbing the device if it has no descriptors to offer.
      add_device.Run();
    } else {
      device->Open(base::Bind(&OnDeviceOpenedReadDescriptors,
                              descriptor.iManufacturer, descriptor.iProduct,
                              descriptor.iSerialNumber, read_bos_descriptors,
                              add_device, enumeration_failed));
    }
  } else {
    USB_LOG(EVENT) << "Failed to get device descriptor: "
                   << ConvertPlatformUsbErrorToString(rv);
    refresh_complete.Run();
  }
}

void UsbServiceImpl::AddDevice(const base::Closure& refresh_complete,
                               scoped_refptr<UsbDeviceImpl> device) {
  auto it = devices_being_enumerated_.find(device->platform_device());
  if (it == devices_being_enumerated_.end()) {
    // Device was removed while being enumerated.
    refresh_complete.Run();
    return;
  }

  platform_devices_[device->platform_device()] = device;
  DCHECK(!ContainsKey(devices(), device->guid()));
  devices()[device->guid()] = device;

  USB_LOG(USER) << "USB device added: vendor=" << device->vendor_id() << " \""
                << device->manufacturer_string()
                << "\", product=" << device->product_id() << " \""
                << device->product_string() << "\", serial=\""
                << device->serial_number() << "\", guid=" << device->guid();

  if (enumeration_ready_) {
    NotifyDeviceAdded(device);
  }

  refresh_complete.Run();
}

void UsbServiceImpl::RemoveDevice(scoped_refptr<UsbDeviceImpl> device) {
  platform_devices_.erase(device->platform_device());
  devices().erase(device->guid());

  USB_LOG(USER) << "USB device removed: guid=" << device->guid();

  NotifyDeviceRemoved(device);
  device->OnDisconnect();
}

// static
int LIBUSB_CALL UsbServiceImpl::HotplugCallback(libusb_context* context,
                                                PlatformUsbDevice device,
                                                libusb_hotplug_event event,
                                                void* user_data) {
  // It is safe to access the UsbServiceImpl* here because libusb takes a lock
  // around registering, deregistering and calling hotplug callback functions
  // and so guarantees that this function will not be called by the event
  // processing thread after it has been deregistered.
  UsbServiceImpl* self = reinterpret_cast<UsbServiceImpl*>(user_data);
  switch (event) {
    case LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED:
      libusb_ref_device(device);  // Released in OnPlatformDeviceAdded.
      if (self->task_runner()->BelongsToCurrentThread()) {
        self->OnPlatformDeviceAdded(device);
      } else {
        self->task_runner()->PostTask(
            FROM_HERE, base::Bind(&UsbServiceImpl::OnPlatformDeviceAdded,
                                  base::Unretained(self), device));
      }
      break;
    case LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT:
      libusb_ref_device(device);  // Released in OnPlatformDeviceRemoved.
      if (self->task_runner()->BelongsToCurrentThread()) {
        self->OnPlatformDeviceRemoved(device);
      } else {
        self->task_runner()->PostTask(
            FROM_HERE, base::Bind(&UsbServiceImpl::OnPlatformDeviceRemoved,
                                  base::Unretained(self), device));
      }
      break;
    default:
      NOTREACHED();
  }

  return 0;
}

void UsbServiceImpl::OnPlatformDeviceAdded(PlatformUsbDevice platform_device) {
  DCHECK(CalledOnValidThread());
  DCHECK(!ContainsKey(platform_devices_, platform_device));
  EnumerateDevice(platform_device, base::Bind(&base::DoNothing));
  libusb_unref_device(platform_device);
}

void UsbServiceImpl::OnPlatformDeviceRemoved(
    PlatformUsbDevice platform_device) {
  DCHECK(CalledOnValidThread());
  PlatformDeviceMap::iterator it = platform_devices_.find(platform_device);
  if (it != platform_devices_.end()) {
    RemoveDevice(it->second);
  } else {
    devices_being_enumerated_.erase(platform_device);
  }
  libusb_unref_device(platform_device);
}

void UsbServiceImpl::EnumerationFailed(PlatformUsbDevice platform_device,
                                       const base::Closure& refresh_complete) {
  libusb_ref_device(platform_device);
  ignored_devices_.insert(platform_device);
  refresh_complete.Run();
}

}  // namespace device
