// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/xbox_data_fetcher_mac.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/usb/USB.h>

#include "base/logging.h"
#include "base/mac/foundation_util.h"

namespace {
const int kVendorMicrosoft = 0x045e;
const int kProductXbox360Controller = 0x028e;
const int kProductXboxOneController = 0x02d1;

const int kXbox360ReadEndpoint = 1;
const int kXbox360ControlEndpoint = 2;

const int kXboxOneReadEndpoint = 2;
const int kXboxOneControlEndpoint = 1;

enum {
  STATUS_MESSAGE_BUTTONS = 0,
  STATUS_MESSAGE_LED = 1,

  // Apparently this message tells you if the rumble pack is disabled in the
  // controller. If the rumble pack is disabled, vibration control messages
  // have no effect.
  STATUS_MESSAGE_RUMBLE = 3,
};

enum {
  XBOX_ONE_STATUS_MESSAGE_BUTTONS = 0x20,
};

enum {
  CONTROL_MESSAGE_SET_RUMBLE = 0,
  CONTROL_MESSAGE_SET_LED = 1,
};

#pragma pack(push, 1)
struct Xbox360ButtonData {
  bool dpad_up : 1;
  bool dpad_down : 1;
  bool dpad_left : 1;
  bool dpad_right : 1;

  bool start : 1;
  bool back : 1;
  bool stick_left_click : 1;
  bool stick_right_click : 1;

  bool bumper_left : 1;
  bool bumper_right : 1;
  bool guide : 1;
  bool dummy1 : 1;  // Always 0.

  bool a : 1;
  bool b : 1;
  bool x : 1;
  bool y : 1;

  uint8_t trigger_left;
  uint8_t trigger_right;

  int16_t stick_left_x;
  int16_t stick_left_y;
  int16_t stick_right_x;
  int16_t stick_right_y;

  // Always 0.
  uint32_t dummy2;
  uint16_t dummy3;
};

struct XboxOneButtonData {
  bool sync : 1;
  bool dummy1 : 1;  // Always 0.
  bool start : 1;
  bool back : 1;

  bool a : 1;
  bool b : 1;
  bool x : 1;
  bool y : 1;

  bool dpad_up : 1;
  bool dpad_down : 1;
  bool dpad_left : 1;
  bool dpad_right : 1;

  bool bumper_left : 1;
  bool bumper_right : 1;
  bool stick_left_click : 1;
  bool stick_right_click : 1;

  uint16_t trigger_left;
  uint16_t trigger_right;

  int16_t stick_left_x;
  int16_t stick_left_y;
  int16_t stick_right_x;
  int16_t stick_right_y;
};
#pragma pack(pop)

static_assert(sizeof(Xbox360ButtonData) == 18, "xbox button data wrong size");
static_assert(sizeof(XboxOneButtonData) == 14, "xbox button data wrong size");

// From MSDN:
// http://msdn.microsoft.com/en-us/library/windows/desktop/ee417001(v=vs.85).aspx#dead_zone
const int16_t kLeftThumbDeadzone = 7849;
const int16_t kRightThumbDeadzone = 8689;
const uint8_t kXbox360TriggerDeadzone = 30;
const uint16_t kXboxOneTriggerMax = 1023;
const uint16_t kXboxOneTriggerDeadzone = 120;

void NormalizeAxis(int16_t x,
                   int16_t y,
                   int16_t deadzone,
                   float* x_out,
                   float* y_out) {
  float x_val = x;
  float y_val = y;

  // Determine how far the stick is pushed.
  float real_magnitude = std::sqrt(x_val * x_val + y_val * y_val);

  // Check if the controller is outside a circular dead zone.
  if (real_magnitude > deadzone) {
    // Clip the magnitude at its expected maximum value.
    float magnitude = std::min(32767.0f, real_magnitude);

    // Adjust magnitude relative to the end of the dead zone.
    magnitude -= deadzone;

    // Normalize the magnitude with respect to its expected range giving a
    // magnitude value of 0.0 to 1.0
    float ratio = (magnitude / (32767 - deadzone)) / real_magnitude;

    // Y is negated because xbox controllers have an opposite sign from
    // the 'standard controller' recommendations.
    *x_out = x_val * ratio;
    *y_out = -y_val * ratio;
  } else {
    // If the controller is in the deadzone zero out the magnitude.
    *x_out = *y_out = 0.0f;
  }
}

float NormalizeTrigger(uint8_t value) {
  return value < kXbox360TriggerDeadzone
             ? 0
             : static_cast<float>(value - kXbox360TriggerDeadzone) /
                   (std::numeric_limits<uint8_t>::max() -
                    kXbox360TriggerDeadzone);
}

float NormalizeXboxOneTrigger(uint16_t value) {
  return value < kXboxOneTriggerDeadzone
             ? 0
             : static_cast<float>(value - kXboxOneTriggerDeadzone) /
                   (kXboxOneTriggerMax - kXboxOneTriggerDeadzone);
}

void NormalizeXbox360ButtonData(const Xbox360ButtonData& data,
                                XboxController::Data* normalized_data) {
  normalized_data->buttons[0] = data.a;
  normalized_data->buttons[1] = data.b;
  normalized_data->buttons[2] = data.x;
  normalized_data->buttons[3] = data.y;
  normalized_data->buttons[4] = data.bumper_left;
  normalized_data->buttons[5] = data.bumper_right;
  normalized_data->buttons[6] = data.back;
  normalized_data->buttons[7] = data.start;
  normalized_data->buttons[8] = data.stick_left_click;
  normalized_data->buttons[9] = data.stick_right_click;
  normalized_data->buttons[10] = data.dpad_up;
  normalized_data->buttons[11] = data.dpad_down;
  normalized_data->buttons[12] = data.dpad_left;
  normalized_data->buttons[13] = data.dpad_right;
  normalized_data->buttons[14] = data.guide;
  normalized_data->triggers[0] = NormalizeTrigger(data.trigger_left);
  normalized_data->triggers[1] = NormalizeTrigger(data.trigger_right);
  NormalizeAxis(data.stick_left_x, data.stick_left_y, kLeftThumbDeadzone,
                &normalized_data->axes[0], &normalized_data->axes[1]);
  NormalizeAxis(data.stick_right_x, data.stick_right_y, kRightThumbDeadzone,
                &normalized_data->axes[2], &normalized_data->axes[3]);
}

void NormalizeXboxOneButtonData(const XboxOneButtonData& data,
                                XboxController::Data* normalized_data) {
  normalized_data->buttons[0] = data.a;
  normalized_data->buttons[1] = data.b;
  normalized_data->buttons[2] = data.x;
  normalized_data->buttons[3] = data.y;
  normalized_data->buttons[4] = data.bumper_left;
  normalized_data->buttons[5] = data.bumper_right;
  normalized_data->buttons[6] = data.back;
  normalized_data->buttons[7] = data.start;
  normalized_data->buttons[8] = data.stick_left_click;
  normalized_data->buttons[9] = data.stick_right_click;
  normalized_data->buttons[10] = data.dpad_up;
  normalized_data->buttons[11] = data.dpad_down;
  normalized_data->buttons[12] = data.dpad_left;
  normalized_data->buttons[13] = data.dpad_right;
  normalized_data->buttons[14] = data.sync;
  normalized_data->triggers[0] = NormalizeXboxOneTrigger(data.trigger_left);
  normalized_data->triggers[1] = NormalizeXboxOneTrigger(data.trigger_right);
  NormalizeAxis(data.stick_left_x, data.stick_left_y, kLeftThumbDeadzone,
                &normalized_data->axes[0], &normalized_data->axes[1]);
  NormalizeAxis(data.stick_right_x, data.stick_right_y, kRightThumbDeadzone,
                &normalized_data->axes[2], &normalized_data->axes[3]);
}

}  // namespace

XboxController::XboxController(Delegate* delegate)
    : device_(NULL),
      interface_(NULL),
      device_is_open_(false),
      interface_is_open_(false),
      read_buffer_size_(0),
      led_pattern_(LED_NUM_PATTERNS),
      location_id_(0),
      delegate_(delegate),
      controller_type_(UNKNOWN_CONTROLLER),
      read_endpoint_(0),
      control_endpoint_(0) {}

XboxController::~XboxController() {
  if (source_)
    CFRunLoopSourceInvalidate(source_);
  if (interface_ && interface_is_open_)
    (*interface_)->USBInterfaceClose(interface_);
  if (device_ && device_is_open_)
    (*device_)->USBDeviceClose(device_);
}

bool XboxController::OpenDevice(io_service_t service) {
  IOCFPlugInInterface** plugin;
  SInt32 score;  // Unused, but required for IOCreatePlugInInterfaceForService.
  kern_return_t kr = IOCreatePlugInInterfaceForService(
      service, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plugin,
      &score);
  if (kr != KERN_SUCCESS)
    return false;
  base::mac::ScopedIOPluginInterface<IOCFPlugInInterface> plugin_ref(plugin);

  HRESULT res = (*plugin)->QueryInterface(
      plugin, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID320),
      (LPVOID*)&device_);
  if (!SUCCEEDED(res) || !device_)
    return false;

  UInt16 vendor_id;
  kr = (*device_)->GetDeviceVendor(device_, &vendor_id);
  if (kr != KERN_SUCCESS || vendor_id != kVendorMicrosoft)
    return false;

  UInt16 product_id;
  kr = (*device_)->GetDeviceProduct(device_, &product_id);
  if (kr != KERN_SUCCESS)
    return false;

  IOUSBFindInterfaceRequest request;
  switch (product_id) {
    case kProductXbox360Controller:
      controller_type_ = XBOX_360_CONTROLLER;
      read_endpoint_ = kXbox360ReadEndpoint;
      control_endpoint_ = kXbox360ControlEndpoint;
      request.bInterfaceClass = 255;
      request.bInterfaceSubClass = 93;
      request.bInterfaceProtocol = 1;
      request.bAlternateSetting = kIOUSBFindInterfaceDontCare;
      break;
    case kProductXboxOneController:
      controller_type_ = XBOX_ONE_CONTROLLER;
      read_endpoint_ = kXboxOneReadEndpoint;
      control_endpoint_ = kXboxOneControlEndpoint;
      request.bInterfaceClass = 255;
      request.bInterfaceSubClass = 71;
      request.bInterfaceProtocol = 208;
      request.bAlternateSetting = kIOUSBFindInterfaceDontCare;
      break;
    default:
      return false;
  }

  // Open the device and configure it.
  kr = (*device_)->USBDeviceOpen(device_);
  if (kr != KERN_SUCCESS)
    return false;
  device_is_open_ = true;

  // Xbox controllers have one configuration option which has configuration
  // value 1. Try to set it and fail if it couldn't be configured.
  IOUSBConfigurationDescriptorPtr config_desc;
  kr = (*device_)->GetConfigurationDescriptorPtr(device_, 0, &config_desc);
  if (kr != KERN_SUCCESS)
    return false;
  kr = (*device_)->SetConfiguration(device_, config_desc->bConfigurationValue);
  if (kr != KERN_SUCCESS)
    return false;

  // The device has 4 interfaces. They are as follows:
  // Protocol 1:
  //  - Endpoint 1 (in) : Controller events, including button presses.
  //  - Endpoint 2 (out): Rumble pack and LED control
  // Protocol 2 has a single endpoint to read from a connected ChatPad device.
  // Protocol 3 is used by a connected headset device.
  // The device also has an interface on subclass 253, protocol 10 with no
  // endpoints.  It is unused.
  //
  // We don't currently support the ChatPad or headset, so protocol 1 is the
  // only protocol we care about.
  //
  // For more detail, see
  // https://github.com/Grumbel/xboxdrv/blob/master/PROTOCOL
  io_iterator_t iter;
  kr = (*device_)->CreateInterfaceIterator(device_, &request, &iter);
  if (kr != KERN_SUCCESS)
    return false;
  base::mac::ScopedIOObject<io_iterator_t> iter_ref(iter);

  // There should be exactly one USB interface which matches the requested
  // settings.
  io_service_t usb_interface = IOIteratorNext(iter);
  if (!usb_interface)
    return false;

  // We need to make an InterfaceInterface to communicate with the device
  // endpoint. This is the same process as earlier: first make a
  // PluginInterface from the io_service then make the InterfaceInterface from
  // that.
  IOCFPlugInInterface** plugin_interface;
  kr = IOCreatePlugInInterfaceForService(
      usb_interface, kIOUSBInterfaceUserClientTypeID, kIOCFPlugInInterfaceID,
      &plugin_interface, &score);
  if (kr != KERN_SUCCESS || !plugin_interface)
    return false;
  base::mac::ScopedIOPluginInterface<IOCFPlugInInterface> interface_ref(
      plugin_interface);

  // Release the USB interface, and any subsequent interfaces returned by the
  // iterator. (There shouldn't be any, but in case a future device does
  // contain more interfaces, this will serve to avoid memory leaks.)
  do {
    IOObjectRelease(usb_interface);
  } while ((usb_interface = IOIteratorNext(iter)));

  // Actually create the interface.
  res = (*plugin_interface)
            ->QueryInterface(plugin_interface,
                             CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID300),
                             (LPVOID*)&interface_);

  if (!SUCCEEDED(res) || !interface_)
    return false;

  // Actually open the interface.
  kr = (*interface_)->USBInterfaceOpen(interface_);
  if (kr != KERN_SUCCESS)
    return false;
  interface_is_open_ = true;

  CFRunLoopSourceRef source_ref;
  kr = (*interface_)->CreateInterfaceAsyncEventSource(interface_, &source_ref);
  if (kr != KERN_SUCCESS || !source_ref)
    return false;
  source_.reset(source_ref);
  CFRunLoopAddSource(CFRunLoopGetCurrent(), source_, kCFRunLoopDefaultMode);

  // The interface should have two pipes. Pipe 1 with direction kUSBIn and pipe
  // 2 with direction kUSBOut. Both pipes should have type kUSBInterrupt.
  uint8_t num_endpoints;
  kr = (*interface_)->GetNumEndpoints(interface_, &num_endpoints);
  if (kr != KERN_SUCCESS || num_endpoints < 2)
    return false;

  for (int i = 1; i <= 2; i++) {
    uint8_t direction;
    uint8_t number;
    uint8_t transfer_type;
    uint16_t max_packet_size;
    uint8_t interval;

    kr = (*interface_)
             ->GetPipeProperties(interface_, i, &direction, &number,
                                 &transfer_type, &max_packet_size, &interval);
    if (kr != KERN_SUCCESS || transfer_type != kUSBInterrupt) {
      return false;
    }
    if (i == read_endpoint_) {
      if (direction != kUSBIn)
        return false;
      read_buffer_.reset(new uint8_t[max_packet_size]);
      read_buffer_size_ = max_packet_size;
      QueueRead();
    } else if (i == control_endpoint_) {
      if (direction != kUSBOut)
        return false;
      if (controller_type_ == XBOX_ONE_CONTROLLER)
        WriteXboxOneInit();
    }
  }

  // The location ID is unique per controller, and can be used to track
  // controllers through reconnections (though if a controller is detached from
  // one USB hub and attached to another, the location ID will change).
  kr = (*device_)->GetLocationID(device_, &location_id_);
  if (kr != KERN_SUCCESS)
    return false;

  return true;
}

void XboxController::SetLEDPattern(LEDPattern pattern) {
  led_pattern_ = pattern;
  const UInt8 length = 3;

  // This buffer will be released in WriteComplete when WritePipeAsync
  // finishes.
  UInt8* buffer = new UInt8[length];
  buffer[0] = static_cast<UInt8>(CONTROL_MESSAGE_SET_LED);
  buffer[1] = length;
  buffer[2] = static_cast<UInt8>(pattern);
  kern_return_t kr =
      (*interface_)
          ->WritePipeAsync(interface_, control_endpoint_, buffer,
                           (UInt32)length, WriteComplete, buffer);
  if (kr != KERN_SUCCESS) {
    delete[] buffer;
    IOError();
    return;
  }
}

int XboxController::GetVendorId() const {
  return kVendorMicrosoft;
}

int XboxController::GetProductId() const {
  if (controller_type_ == XBOX_360_CONTROLLER)
    return kProductXbox360Controller;
  else
    return kProductXboxOneController;
}

XboxController::ControllerType XboxController::GetControllerType() const {
  return controller_type_;
}

void XboxController::WriteComplete(void* context, IOReturn result, void* arg0) {
  UInt8* buffer = static_cast<UInt8*>(context);
  delete[] buffer;

  // Ignoring any errors sending data, because they will usually only occur
  // when the device is disconnected, in which case it really doesn't matter if
  // the data got to the controller or not.
  if (result != kIOReturnSuccess)
    return;
}

void XboxController::GotData(void* context, IOReturn result, void* arg0) {
  size_t bytes_read = reinterpret_cast<size_t>(arg0);
  XboxController* controller = static_cast<XboxController*>(context);

  if (result != kIOReturnSuccess) {
    // This will happen if the device was disconnected. The gamepad has
    // probably been destroyed by a meteorite.
    controller->IOError();
    return;
  }

  if (controller->GetControllerType() == XBOX_360_CONTROLLER)
    controller->ProcessXbox360Packet(bytes_read);
  else
    controller->ProcessXboxOnePacket(bytes_read);

  // Queue up another read.
  controller->QueueRead();
}

void XboxController::ProcessXbox360Packet(size_t length) {
  if (length < 2)
    return;
  DCHECK(length <= read_buffer_size_);
  if (length > read_buffer_size_) {
    IOError();
    return;
  }
  uint8_t* buffer = read_buffer_.get();

  if (buffer[1] != length)
    // Length in packet doesn't match length reported by USB.
    return;

  uint8_t type = buffer[0];
  buffer += 2;
  length -= 2;
  switch (type) {
    case STATUS_MESSAGE_BUTTONS: {
      if (length != sizeof(Xbox360ButtonData))
        return;
      Xbox360ButtonData* data = reinterpret_cast<Xbox360ButtonData*>(buffer);
      Data normalized_data;
      NormalizeXbox360ButtonData(*data, &normalized_data);
      delegate_->XboxControllerGotData(this, normalized_data);
      break;
    }
    case STATUS_MESSAGE_LED:
      if (length != 3)
        return;
      // The controller sends one of these messages every time the LED pattern
      // is set, as well as once when it is plugged in.
      if (led_pattern_ == LED_NUM_PATTERNS && buffer[0] < LED_NUM_PATTERNS)
        led_pattern_ = static_cast<LEDPattern>(buffer[0]);
      break;
    default:
      // Unknown packet: ignore!
      break;
  }
}

void XboxController::ProcessXboxOnePacket(size_t length) {
  if (length < 2)
    return;
  DCHECK(length <= read_buffer_size_);
  if (length > read_buffer_size_) {
    IOError();
    return;
  }
  uint8_t* buffer = read_buffer_.get();

  uint8_t type = buffer[0];
  buffer += 4;
  length -= 4;
  switch (type) {
    case XBOX_ONE_STATUS_MESSAGE_BUTTONS: {
      if (length != sizeof(XboxOneButtonData))
        return;
      XboxOneButtonData* data = reinterpret_cast<XboxOneButtonData*>(buffer);
      Data normalized_data;
      NormalizeXboxOneButtonData(*data, &normalized_data);
      delegate_->XboxControllerGotData(this, normalized_data);
      break;
    }
    default:
      // Unknown packet: ignore!
      break;
  }
}

void XboxController::QueueRead() {
  kern_return_t kr =
      (*interface_)
          ->ReadPipeAsync(interface_, read_endpoint_, read_buffer_.get(),
                          read_buffer_size_, GotData, this);
  if (kr != KERN_SUCCESS)
    IOError();
}

void XboxController::IOError() {
  delegate_->XboxControllerError(this);
}

void XboxController::WriteXboxOneInit() {
  const UInt8 length = 2;

  // This buffer will be released in WriteComplete when WritePipeAsync
  // finishes.
  UInt8* buffer = new UInt8[length];
  buffer[0] = 0x05;
  buffer[1] = 0x20;
  kern_return_t kr =
      (*interface_)
          ->WritePipeAsync(interface_, control_endpoint_, buffer,
                           (UInt32)length, WriteComplete, buffer);
  if (kr != KERN_SUCCESS) {
    delete[] buffer;
    IOError();
    return;
  }
}

//-----------------------------------------------------------------------------

XboxDataFetcher::XboxDataFetcher(Delegate* delegate)
    : delegate_(delegate), listening_(false), source_(NULL), port_(NULL) {}

XboxDataFetcher::~XboxDataFetcher() {
  while (!controllers_.empty()) {
    RemoveController(*controllers_.begin());
  }
  UnregisterFromNotifications();
}

void XboxDataFetcher::DeviceAdded(void* context, io_iterator_t iterator) {
  DCHECK(context);
  XboxDataFetcher* fetcher = static_cast<XboxDataFetcher*>(context);
  io_service_t ref;
  while ((ref = IOIteratorNext(iterator))) {
    base::mac::ScopedIOObject<io_service_t> scoped_ref(ref);
    XboxController* controller = new XboxController(fetcher);
    if (controller->OpenDevice(ref)) {
      fetcher->AddController(controller);
    } else {
      delete controller;
    }
  }
}

void XboxDataFetcher::DeviceRemoved(void* context, io_iterator_t iterator) {
  DCHECK(context);
  XboxDataFetcher* fetcher = static_cast<XboxDataFetcher*>(context);
  io_service_t ref;
  while ((ref = IOIteratorNext(iterator))) {
    base::mac::ScopedIOObject<io_service_t> scoped_ref(ref);
    base::ScopedCFTypeRef<CFNumberRef> number(
        base::mac::CFCastStrict<CFNumberRef>(IORegistryEntryCreateCFProperty(
            ref, CFSTR(kUSBDevicePropertyLocationID), kCFAllocatorDefault,
            kNilOptions)));
    UInt32 location_id = 0;
    CFNumberGetValue(number, kCFNumberSInt32Type, &location_id);
    fetcher->RemoveControllerByLocationID(location_id);
  }
}

bool XboxDataFetcher::RegisterForNotifications() {
  if (listening_)
    return true;
  port_ = IONotificationPortCreate(kIOMasterPortDefault);
  if (!port_)
    return false;
  source_ = IONotificationPortGetRunLoopSource(port_);
  if (!source_)
    return false;
  CFRunLoopAddSource(CFRunLoopGetCurrent(), source_, kCFRunLoopDefaultMode);

  listening_ = true;

  if (!RegisterForDeviceNotifications(
          kVendorMicrosoft, kProductXboxOneController,
          &xbox_one_device_added_iter_, &xbox_one_device_removed_iter_))
    return false;

  if (!RegisterForDeviceNotifications(
          kVendorMicrosoft, kProductXbox360Controller,
          &xbox_360_device_added_iter_, &xbox_360_device_removed_iter_))
    return false;

  return true;
}

bool XboxDataFetcher::RegisterForDeviceNotifications(
    int vendor_id,
    int product_id,
    base::mac::ScopedIOObject<io_iterator_t>* added_iter,
    base::mac::ScopedIOObject<io_iterator_t>* removed_iter) {
  base::ScopedCFTypeRef<CFNumberRef> vendor_cf(
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &vendor_id));
  base::ScopedCFTypeRef<CFNumberRef> product_cf(
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &product_id));
  base::ScopedCFTypeRef<CFMutableDictionaryRef> matching_dict(
      IOServiceMatching(kIOUSBDeviceClassName));
  if (!matching_dict)
    return false;
  CFDictionarySetValue(matching_dict, CFSTR(kUSBVendorID), vendor_cf);
  CFDictionarySetValue(matching_dict, CFSTR(kUSBProductID), product_cf);

  // IOServiceAddMatchingNotification() releases the dictionary when it's done.
  // Retain it before each call to IOServiceAddMatchingNotification to keep
  // things balanced.
  CFRetain(matching_dict);
  io_iterator_t device_added_iter;
  IOReturn ret;
  ret = IOServiceAddMatchingNotification(port_, kIOFirstMatchNotification,
                                         matching_dict, DeviceAdded, this,
                                         &device_added_iter);
  added_iter->reset(device_added_iter);
  if (ret != kIOReturnSuccess) {
    LOG(ERROR) << "Error listening for Xbox controller add events: " << ret;
    return false;
  }
  DeviceAdded(this, added_iter->get());

  CFRetain(matching_dict);
  io_iterator_t device_removed_iter;
  ret = IOServiceAddMatchingNotification(port_, kIOTerminatedNotification,
                                         matching_dict, DeviceRemoved, this,
                                         &device_removed_iter);
  removed_iter->reset(device_removed_iter);
  if (ret != kIOReturnSuccess) {
    LOG(ERROR) << "Error listening for Xbox controller remove events: " << ret;
    return false;
  }
  DeviceRemoved(this, removed_iter->get());
  return true;
}

void XboxDataFetcher::UnregisterFromNotifications() {
  if (!listening_)
    return;
  listening_ = false;
  if (source_)
    CFRunLoopSourceInvalidate(source_);
  if (port_)
    IONotificationPortDestroy(port_);
  port_ = NULL;
}

XboxController* XboxDataFetcher::ControllerForLocation(UInt32 location_id) {
  for (std::set<XboxController*>::iterator i = controllers_.begin();
       i != controllers_.end(); ++i) {
    if ((*i)->location_id() == location_id)
      return *i;
  }
  return NULL;
}

void XboxDataFetcher::AddController(XboxController* controller) {
  DCHECK(!ControllerForLocation(controller->location_id()))
      << "Controller with location ID " << controller->location_id()
      << " already exists in the set of controllers.";
  controllers_.insert(controller);
  delegate_->XboxDeviceAdd(controller);
}

void XboxDataFetcher::RemoveController(XboxController* controller) {
  delegate_->XboxDeviceRemove(controller);
  controllers_.erase(controller);
  delete controller;
}

void XboxDataFetcher::RemoveControllerByLocationID(uint32_t location_id) {
  XboxController* controller = NULL;
  for (std::set<XboxController*>::iterator i = controllers_.begin();
       i != controllers_.end(); ++i) {
    if ((*i)->location_id() == location_id) {
      controller = *i;
      break;
    }
  }
  if (controller)
    RemoveController(controller);
}

void XboxDataFetcher::XboxControllerGotData(XboxController* controller,
                                            const XboxController::Data& data) {
  delegate_->XboxValueChanged(controller, data);
}

void XboxDataFetcher::XboxControllerError(XboxController* controller) {
  RemoveController(controller);
}
