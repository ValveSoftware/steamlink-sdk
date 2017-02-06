// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_XBOX_DATA_FETCHER_MAC_H_
#define DEVICE_GAMEPAD_XBOX_DATA_FETCHER_MAC_H_

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>

#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_ioobject.h"
#include "base/mac/scoped_ioplugininterface.h"
#include "base/macros.h"

class XboxController {
 public:
  enum ControllerType {
    UNKNOWN_CONTROLLER,
    XBOX_360_CONTROLLER,
    XBOX_ONE_CONTROLLER
  };

  enum LEDPattern {
    LED_OFF = 0,

    // 2 quick flashes, then a series of slow flashes (about 1 per second).
    LED_FLASH = 1,

    // Flash three times then hold the LED on. This is the standard way to tell
    // the player which player number they are.
    LED_FLASH_TOP_LEFT = 2,
    LED_FLASH_TOP_RIGHT = 3,
    LED_FLASH_BOTTOM_LEFT = 4,
    LED_FLASH_BOTTOM_RIGHT = 5,

    // Simply turn on the specified LED and turn all other LEDs off.
    LED_HOLD_TOP_LEFT = 6,
    LED_HOLD_TOP_RIGHT = 7,
    LED_HOLD_BOTTOM_LEFT = 8,
    LED_HOLD_BOTTOM_RIGHT = 9,

    LED_ROTATE = 10,

    LED_FLASH_FAST = 11,
    LED_FLASH_SLOW = 12,  // Flash about once per 3 seconds

    // Flash alternating LEDs for a few seconds, then flash all LEDs about once
    // per second
    LED_ALTERNATE_PATTERN = 13,

    // 14 is just another boring flashing speed.

    // Flash all LEDs once then go black.
    LED_FLASH_ONCE = 15,

    LED_NUM_PATTERNS
  };

  struct Data {
    bool buttons[15];
    float triggers[2];
    float axes[4];
  };

  class Delegate {
   public:
    virtual void XboxControllerGotData(XboxController* controller,
                                       const Data& data) = 0;
    virtual void XboxControllerError(XboxController* controller) = 0;
  };

  explicit XboxController(Delegate* delegate_);
  virtual ~XboxController();

  bool OpenDevice(io_service_t service);

  void SetLEDPattern(LEDPattern pattern);

  UInt32 location_id() { return location_id_; }
  int GetVendorId() const;
  int GetProductId() const;
  ControllerType GetControllerType() const;

 private:
  static void WriteComplete(void* context, IOReturn result, void* arg0);
  static void GotData(void* context, IOReturn result, void* arg0);

  void ProcessXbox360Packet(size_t length);
  void ProcessXboxOnePacket(size_t length);
  void QueueRead();

  void IOError();

  void WriteXboxOneInit();

  // Handle for the USB device. IOUSBDeviceStruct320 is the latest version of
  // the device API that is supported on Mac OS 10.6.
  base::mac::ScopedIOPluginInterface<struct IOUSBDeviceStruct320> device_;

  // Handle for the interface on the device which sends button and analog data.
  // The other interfaces (for the ChatPad and headset) are ignored.
  base::mac::ScopedIOPluginInterface<struct IOUSBInterfaceStruct300> interface_;

  bool device_is_open_;
  bool interface_is_open_;

  base::ScopedCFTypeRef<CFRunLoopSourceRef> source_;

  // This will be set to the max packet size reported by the interface, which
  // is 32 bytes. I would have expected USB to do message framing itself, but
  // somehow we still sometimes (rarely!) get packets off the interface which
  // aren't correctly framed. The 360 controller frames its packets with a 2
  // byte header (type, total length) so we can reframe the packet data
  // ourselves.
  uint16_t read_buffer_size_;
  std::unique_ptr<uint8_t[]> read_buffer_;

  // The pattern that the LEDs on the device are currently displaying, or
  // LED_NUM_PATTERNS if unknown.
  LEDPattern led_pattern_;

  UInt32 location_id_;

  Delegate* delegate_;

  ControllerType controller_type_;
  int read_endpoint_;
  int control_endpoint_;

  DISALLOW_COPY_AND_ASSIGN(XboxController);
};

class XboxDataFetcher : public XboxController::Delegate {
 public:
  class Delegate {
   public:
    virtual void XboxDeviceAdd(XboxController* device) = 0;
    virtual void XboxDeviceRemove(XboxController* device) = 0;
    virtual void XboxValueChanged(XboxController* device,
                                  const XboxController::Data& data) = 0;
  };

  explicit XboxDataFetcher(Delegate* delegate);
  virtual ~XboxDataFetcher();

  bool RegisterForNotifications();
  bool RegisterForDeviceNotifications(
      int vendor_id,
      int product_id,
      base::mac::ScopedIOObject<io_iterator_t>* added_iter,
      base::mac::ScopedIOObject<io_iterator_t>* removed_iter);
  void UnregisterFromNotifications();

  XboxController* ControllerForLocation(UInt32 location_id);

 private:
  static void DeviceAdded(void* context, io_iterator_t iterator);
  static void DeviceRemoved(void* context, io_iterator_t iterator);
  void AddController(XboxController* controller);
  void RemoveController(XboxController* controller);
  void RemoveControllerByLocationID(uint32_t id);
  void XboxControllerGotData(XboxController* controller,
                             const XboxController::Data& data) override;
  void XboxControllerError(XboxController* controller) override;

  Delegate* delegate_;

  std::set<XboxController*> controllers_;

  bool listening_;

  // port_ owns source_, so this doesn't need to be a ScopedCFTypeRef, but we
  // do need to maintain a reference to it so we can invalidate it.
  CFRunLoopSourceRef source_;
  IONotificationPortRef port_;
  base::mac::ScopedIOObject<io_iterator_t> xbox_360_device_added_iter_;
  base::mac::ScopedIOObject<io_iterator_t> xbox_360_device_removed_iter_;
  base::mac::ScopedIOObject<io_iterator_t> xbox_one_device_added_iter_;
  base::mac::ScopedIOObject<io_iterator_t> xbox_one_device_removed_iter_;

  DISALLOW_COPY_AND_ASSIGN(XboxDataFetcher);
};

#endif  // DEVICE_GAMEPAD_XBOX_DATA_FETCHER_MAC_H_
