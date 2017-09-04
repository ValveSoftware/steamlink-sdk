// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_USB_DEVICE_H_
#define DEVICE_USB_USB_DEVICE_H_

#include <stdint.h>

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "device/usb/usb_descriptors.h"
#include "url/gurl.h"

namespace device {

class UsbDeviceHandle;
struct WebUsbAllowedOrigins;

// A UsbDevice object represents a detected USB device, providing basic
// information about it. Methods other than simple property accessors must be
// called from the thread on which this object was created. For further
// manipulation of the device, a UsbDeviceHandle must be created from Open()
// method.
class UsbDevice : public base::RefCountedThreadSafe<UsbDevice> {
 public:
  using OpenCallback = base::Callback<void(scoped_refptr<UsbDeviceHandle>)>;
  using ResultCallback = base::Callback<void(bool success)>;

  // This observer interface should be used by objects that need only be
  // notified about the removal of a particular device as it is more efficient
  // than registering a large number of observers with UsbService::AddObserver.
  class Observer {
   public:
    virtual ~Observer();

    // This method is called when the UsbService that created this object
    // detects that the device has been disconnected from the host.
    virtual void OnDeviceRemoved(scoped_refptr<UsbDevice> device);
  };

  // A unique identifier which remains stable for the lifetime of this device
  // object (i.e., until the device is unplugged or the USB service dies.)
  const std::string& guid() const { return guid_; }

  // Accessors to basic information.
  uint16_t usb_version() const { return usb_version_; }
  uint8_t device_class() const { return device_class_; }
  uint8_t device_subclass() const { return device_subclass_; }
  uint8_t device_protocol() const { return device_protocol_; }
  uint16_t vendor_id() const { return vendor_id_; }
  uint16_t product_id() const { return product_id_; }
  uint16_t device_version() const { return device_version_; }
  const base::string16& manufacturer_string() const {
    return manufacturer_string_;
  }
  const base::string16& product_string() const { return product_string_; }
  const base::string16& serial_number() const { return serial_number_; }
  const WebUsbAllowedOrigins* webusb_allowed_origins() const {
    return webusb_allowed_origins_.get();
  }
  const GURL& webusb_landing_page() const { return webusb_landing_page_; }
  const std::vector<UsbConfigDescriptor>& configurations() const {
    return configurations_;
  }
  const UsbConfigDescriptor* active_configuration() const {
    return active_configuration_;
  }

  // On ChromeOS the permission_broker service must be used to open USB devices.
  // This function asks it to check whether a future Open call will be allowed.
  // On all other platforms this is a no-op and always returns true.
  virtual void CheckUsbAccess(const ResultCallback& callback);

  // On Android applications must request permission from the user to access a
  // USB device before it can be opened. After permission is granted the device
  // properties may contain information not previously available. On all other
  // platforms this is a no-op and always returns true.
  virtual void RequestPermission(const ResultCallback& callback);
  virtual bool permission_granted() const;

  // Creates a UsbDeviceHandle for further manipulation.
  virtual void Open(const OpenCallback& callback) = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  friend class UsbService;

  UsbDevice(uint16_t usb_version,
            uint8_t device_class,
            uint8_t device_subclass,
            uint8_t device_protocol,
            uint16_t vendor_id,
            uint16_t product_id,
            uint16_t device_version,
            const base::string16& manufacturer_string,
            const base::string16& product_string,
            const base::string16& serial_number);
  virtual ~UsbDevice();

  void ActiveConfigurationChanged(int configuration_value);
  void NotifyDeviceRemoved();

  std::list<UsbDeviceHandle*>& handles() { return handles_; }

  // These members must be mutable by subclasses as necessary during device
  // enumeration. To preserve the thread safety of this object they must remain
  // constant afterwards.
  uint16_t usb_version_;
  uint16_t device_version_;
  base::string16 manufacturer_string_;
  base::string16 product_string_;
  base::string16 serial_number_;
  std::unique_ptr<WebUsbAllowedOrigins> webusb_allowed_origins_;
  GURL webusb_landing_page_;

  // All of the device's configuration descriptors.
  std::vector<UsbConfigDescriptor> configurations_;

 private:
  friend class base::RefCountedThreadSafe<UsbDevice>;
  friend class UsbDeviceHandleImpl;
  friend class UsbDeviceHandleUsbfs;
  friend class UsbServiceAndroid;
  friend class UsbServiceImpl;
  friend class UsbServiceLinux;

  void OnDisconnect();
  void HandleClosed(UsbDeviceHandle* handle);

  const std::string guid_;
  const uint8_t device_class_;
  const uint8_t device_subclass_;
  const uint8_t device_protocol_;
  const uint16_t vendor_id_;
  const uint16_t product_id_;

  // The current device configuration descriptor. May be null if the device is
  // in an unconfigured state; if not null, it is a pointer to one of the
  // items at UsbDevice::configurations_.
  const UsbConfigDescriptor* active_configuration_ = nullptr;

  // Weak pointers to open handles. HandleClosed() will be called before each
  // is freed.
  std::list<UsbDeviceHandle*> handles_;

  base::ObserverList<Observer, true> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(UsbDevice);
};

}  // namespace device

#endif  // DEVICE_USB_USB_DEVICE_H_
