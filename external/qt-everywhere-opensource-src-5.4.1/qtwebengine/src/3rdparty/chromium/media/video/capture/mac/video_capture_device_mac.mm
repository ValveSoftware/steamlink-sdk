// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/mac/video_capture_device_mac.h"

#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/usb/USBSpec.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/mac/scoped_ioobject.h"
#include "base/mac/scoped_ioplugininterface.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#import "media/video/capture/mac/avfoundation_glue.h"
#import "media/video/capture/mac/platform_video_capturing_mac.h"
#import "media/video/capture/mac/video_capture_device_avfoundation_mac.h"
#import "media/video/capture/mac/video_capture_device_qtkit_mac.h"

namespace media {

const int kMinFrameRate = 1;
const int kMaxFrameRate = 30;

// In device identifiers, the USB VID and PID are stored in 4 bytes each.
const size_t kVidPidSize = 4;

const struct Resolution {
  const int width;
  const int height;
} kQVGA = { 320, 240 },
  kVGA = { 640, 480 },
  kHD = { 1280, 720 };

const struct Resolution* const kWellSupportedResolutions[] = {
  &kQVGA,
  &kVGA,
  &kHD,
};

// Rescaling the image to fix the pixel aspect ratio runs the risk of making
// the aspect ratio worse, if QTKit selects a new source mode with a different
// shape. This constant ensures that we don't take this risk if the current
// aspect ratio is tolerable.
const float kMaxPixelAspectRatio = 1.15;

// The following constants are extracted from the specification "Universal
// Serial Bus Device Class Definition for Video Devices", Rev. 1.1 June 1, 2005.
// http://www.usb.org/developers/devclass_docs/USB_Video_Class_1_1.zip
// CS_INTERFACE: Sec. A.4 "Video Class-Specific Descriptor Types".
const int kVcCsInterface = 0x24;
// VC_PROCESSING_UNIT: Sec. A.5 "Video Class-Specific VC Interface Descriptor
// Subtypes".
const int kVcProcessingUnit = 0x5;
// SET_CUR: Sec. A.8 "Video Class-Specific Request Codes".
const int kVcRequestCodeSetCur = 0x1;
// PU_POWER_LINE_FREQUENCY_CONTROL: Sec. A.9.5 "Processing Unit Control
// Selectors".
const int kPuPowerLineFrequencyControl = 0x5;
// Sec. 4.2.2.3.5 Power Line Frequency Control.
const int k50Hz = 1;
const int k60Hz = 2;
const int kPuPowerLineFrequencyControlCommandSize = 1;

// Addition to the IOUSB family of structures, with subtype and unit ID.
typedef struct IOUSBInterfaceDescriptor {
  IOUSBDescriptorHeader header;
  UInt8 bDescriptorSubType;
  UInt8 bUnitID;
} IOUSBInterfaceDescriptor;

// TODO(ronghuawu): Replace this with CapabilityList::GetBestMatchedCapability.
void GetBestMatchSupportedResolution(int* width, int* height) {
  int min_diff = kint32max;
  int matched_width = *width;
  int matched_height = *height;
  int desired_res_area = *width * *height;
  for (size_t i = 0; i < arraysize(kWellSupportedResolutions); ++i) {
    int area = kWellSupportedResolutions[i]->width *
               kWellSupportedResolutions[i]->height;
    int diff = std::abs(desired_res_area - area);
    if (diff < min_diff) {
      min_diff = diff;
      matched_width = kWellSupportedResolutions[i]->width;
      matched_height = kWellSupportedResolutions[i]->height;
    }
  }
  *width = matched_width;
  *height = matched_height;
}

// Tries to create a user-side device interface for a given USB device. Returns
// true if interface was found and passes it back in |device_interface|. The
// caller should release |device_interface|.
static bool FindDeviceInterfaceInUsbDevice(
    const int vendor_id,
    const int product_id,
    const io_service_t usb_device,
    IOUSBDeviceInterface*** device_interface) {
  // Create a plug-in, i.e. a user-side controller to manipulate USB device.
  IOCFPlugInInterface** plugin;
  SInt32 score;  // Unused, but required for IOCreatePlugInInterfaceForService.
  kern_return_t kr =
      IOCreatePlugInInterfaceForService(usb_device,
                                        kIOUSBDeviceUserClientTypeID,
                                        kIOCFPlugInInterfaceID,
                                        &plugin,
                                        &score);
  if (kr != kIOReturnSuccess || !plugin) {
    DLOG(ERROR) << "IOCreatePlugInInterfaceForService";
    return false;
  }
  base::mac::ScopedIOPluginInterface<IOCFPlugInInterface> plugin_ref(plugin);

  // Fetch the Device Interface from the plug-in.
  HRESULT res =
      (*plugin)->QueryInterface(plugin,
                                CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID),
                                reinterpret_cast<LPVOID*>(device_interface));
  if (!SUCCEEDED(res) || !*device_interface) {
    DLOG(ERROR) << "QueryInterface, couldn't create interface to USB";
    return false;
  }
  return true;
}

// Tries to find a Video Control type interface inside a general USB device
// interface |device_interface|, and returns it in |video_control_interface| if
// found. The returned interface must be released in the caller.
static bool FindVideoControlInterfaceInDeviceInterface(
    IOUSBDeviceInterface** device_interface,
    IOCFPlugInInterface*** video_control_interface) {
  // Create an iterator to the list of Video-AVControl interfaces of the device,
  // then get the first interface in the list.
  io_iterator_t interface_iterator;
  IOUSBFindInterfaceRequest interface_request = {
    .bInterfaceClass = kUSBVideoInterfaceClass,
    .bInterfaceSubClass = kUSBVideoControlSubClass,
    .bInterfaceProtocol = kIOUSBFindInterfaceDontCare,
    .bAlternateSetting = kIOUSBFindInterfaceDontCare
  };
  kern_return_t kr =
      (*device_interface)->CreateInterfaceIterator(device_interface,
                                                   &interface_request,
                                                   &interface_iterator);
  if (kr != kIOReturnSuccess) {
    DLOG(ERROR) << "Could not create an iterator to the device's interfaces.";
    return false;
  }
  base::mac::ScopedIOObject<io_iterator_t> iterator_ref(interface_iterator);

  // There should be just one interface matching the class-subclass desired.
  io_service_t found_interface;
  found_interface = IOIteratorNext(interface_iterator);
  if (!found_interface) {
    DLOG(ERROR) << "Could not find a Video-AVControl interface in the device.";
    return false;
  }
  base::mac::ScopedIOObject<io_service_t> found_interface_ref(found_interface);

  // Create a user side controller (i.e. a "plug-in") for the found interface.
  SInt32 score;
  kr = IOCreatePlugInInterfaceForService(found_interface,
                                         kIOUSBInterfaceUserClientTypeID,
                                         kIOCFPlugInInterfaceID,
                                         video_control_interface,
                                         &score);
  if (kr != kIOReturnSuccess || !*video_control_interface) {
    DLOG(ERROR) << "IOCreatePlugInInterfaceForService";
    return false;
  }
  return true;
}

// Creates a control interface for |plugin_interface| and produces a command to
// set the appropriate Power Line frequency for flicker removal.
static void SetAntiFlickerInVideoControlInterface(
    IOCFPlugInInterface** plugin_interface,
    const int frequency) {
  // Create, the control interface for the found plug-in, and release
  // the intermediate plug-in.
  IOUSBInterfaceInterface** control_interface = NULL;
  HRESULT res = (*plugin_interface)->QueryInterface(
      plugin_interface,
      CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID),
      reinterpret_cast<LPVOID*>(&control_interface));
  if (!SUCCEEDED(res) || !control_interface ) {
    DLOG(ERROR) << "Couldn’t create control interface";
    return;
  }
  base::mac::ScopedIOPluginInterface<IOUSBInterfaceInterface>
      control_interface_ref(control_interface);

  // Find the device's unit ID presenting type 0x24 (kVcCsInterface) and
  // subtype 0x5 (kVcProcessingUnit). Inside this unit is where we find the
  // power line frequency removal setting, and this id is device dependent.
  int real_unit_id = -1;
  IOUSBDescriptorHeader* descriptor = NULL;
  IOUSBInterfaceDescriptor* cs_descriptor = NULL;
  IOUSBInterfaceInterface220** interface =
      reinterpret_cast<IOUSBInterfaceInterface220**>(control_interface);
  while ((descriptor = (*interface)->FindNextAssociatedDescriptor(
      interface, descriptor, kUSBAnyDesc))) {
    cs_descriptor =
        reinterpret_cast<IOUSBInterfaceDescriptor*>(descriptor);
    if ((descriptor->bDescriptorType == kVcCsInterface) &&
        (cs_descriptor->bDescriptorSubType == kVcProcessingUnit)) {
      real_unit_id = cs_descriptor->bUnitID;
      break;
    }
  }
  DVLOG_IF(1, real_unit_id == -1) << "This USB device doesn't seem to have a "
      << " VC_PROCESSING_UNIT, anti-flicker not available";
  if (real_unit_id == -1)
    return;

  if ((*control_interface)->USBInterfaceOpen(control_interface) !=
          kIOReturnSuccess) {
    DLOG(ERROR) << "Unable to open control interface";
    return;
  }

  // Create the control request and launch it to the device's control interface.
  // Note how the wIndex needs the interface number OR'ed in the lowest bits.
  IOUSBDevRequest command;
  command.bmRequestType = USBmakebmRequestType(kUSBOut,
                                               kUSBClass,
                                               kUSBInterface);
  command.bRequest = kVcRequestCodeSetCur;
  UInt8 interface_number;
  (*control_interface)->GetInterfaceNumber(control_interface,
                                           &interface_number);
  command.wIndex = (real_unit_id << 8) | interface_number;
  const int selector = kPuPowerLineFrequencyControl;
  command.wValue = (selector << 8);
  command.wLength = kPuPowerLineFrequencyControlCommandSize;
  command.wLenDone = 0;
  int power_line_flag_value = (frequency == 50) ? k50Hz : k60Hz;
  command.pData = &power_line_flag_value;

  IOReturn ret = (*control_interface)->ControlRequest(control_interface,
      0, &command);
  DLOG_IF(ERROR, ret != kIOReturnSuccess) << "Anti-flicker control request"
      << " failed (0x" << std::hex << ret << "), unit id: " << real_unit_id;
  DVLOG_IF(1, ret == kIOReturnSuccess) << "Anti-flicker set to " << frequency
      << "Hz";

  (*control_interface)->USBInterfaceClose(control_interface);
}

// Sets the flicker removal in a USB webcam identified by |vendor_id| and
// |product_id|, if available. The process includes first finding all USB
// devices matching the specified |vendor_id| and |product_id|; for each
// matching device, a device interface, and inside it a video control interface
// are created. The latter is used to a send a power frequency setting command.
static void SetAntiFlickerInUsbDevice(const int vendor_id,
                                      const int product_id,
                                      const int frequency) {
  if (frequency == 0)
    return;
  DVLOG(1) << "Setting Power Line Frequency to " << frequency << " Hz, device "
      << std::hex << vendor_id << "-" << product_id;

  // Compose a search dictionary with vendor and product ID.
  CFMutableDictionaryRef query_dictionary =
      IOServiceMatching(kIOUSBDeviceClassName);
  CFDictionarySetValue(query_dictionary, CFSTR(kUSBVendorName),
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &vendor_id));
  CFDictionarySetValue(query_dictionary, CFSTR(kUSBProductName),
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &product_id));

  io_iterator_t usb_iterator;
  kern_return_t kr = IOServiceGetMatchingServices(kIOMasterPortDefault,
                                                  query_dictionary,
                                                  &usb_iterator);
  if (kr != kIOReturnSuccess) {
    DLOG(ERROR) << "No devices found with specified Vendor and Product ID.";
    return;
  }
  base::mac::ScopedIOObject<io_iterator_t> usb_iterator_ref(usb_iterator);

  while (io_service_t usb_device = IOIteratorNext(usb_iterator)) {
    base::mac::ScopedIOObject<io_service_t> usb_device_ref(usb_device);

    IOUSBDeviceInterface** device_interface = NULL;
    if (!FindDeviceInterfaceInUsbDevice(vendor_id, product_id,
        usb_device, &device_interface)) {
      return;
    }
    base::mac::ScopedIOPluginInterface<IOUSBDeviceInterface>
        device_interface_ref(device_interface);

    IOCFPlugInInterface** video_control_interface = NULL;
    if (!FindVideoControlInterfaceInDeviceInterface(device_interface,
        &video_control_interface)) {
      return;
    }
    base::mac::ScopedIOPluginInterface<IOCFPlugInInterface>
        plugin_interface_ref(video_control_interface);

    SetAntiFlickerInVideoControlInterface(video_control_interface, frequency);
  }
}

const std::string VideoCaptureDevice::Name::GetModel() const {
  // Both PID and VID are 4 characters.
  if (unique_id_.size() < 2 * kVidPidSize) {
    return "";
  }

  // The last characters of device id is a concatenation of VID and then PID.
  const size_t vid_location = unique_id_.size() - 2 * kVidPidSize;
  std::string id_vendor = unique_id_.substr(vid_location, kVidPidSize);
  const size_t pid_location = unique_id_.size() - kVidPidSize;
  std::string id_product = unique_id_.substr(pid_location, kVidPidSize);

  return id_vendor + ":" + id_product;
}

VideoCaptureDeviceMac::VideoCaptureDeviceMac(const Name& device_name)
    : device_name_(device_name),
      tried_to_square_pixels_(false),
      task_runner_(base::MessageLoopProxy::current()),
      state_(kNotInitialized),
      capture_device_(nil),
      weak_factory_(this) {
  final_resolution_selected_ = AVFoundationGlue::IsAVFoundationSupported();
}

VideoCaptureDeviceMac::~VideoCaptureDeviceMac() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  [capture_device_ release];
}

void VideoCaptureDeviceMac::AllocateAndStart(
    const VideoCaptureParams& params,
    scoped_ptr<VideoCaptureDevice::Client> client) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (state_ != kIdle) {
    return;
  }
  int width = params.requested_format.frame_size.width();
  int height = params.requested_format.frame_size.height();
  int frame_rate = params.requested_format.frame_rate;

  // QTKit API can scale captured frame to any size requested, which would lead
  // to undesired aspect ratio changes. Try to open the camera with a known
  // supported format and let the client crop/pad the captured frames.
  if (!AVFoundationGlue::IsAVFoundationSupported())
    GetBestMatchSupportedResolution(&width, &height);

  client_ = client.Pass();
  if (device_name_.capture_api_type() == Name::AVFOUNDATION)
    LogMessage("Using AVFoundation for device: " + device_name_.name());
  else
    LogMessage("Using QTKit for device: " + device_name_.name());
  NSString* deviceId =
      [NSString stringWithUTF8String:device_name_.id().c_str()];

  [capture_device_ setFrameReceiver:this];

  if (![capture_device_ setCaptureDevice:deviceId]) {
    SetErrorState("Could not open capture device.");
    return;
  }
  if (frame_rate < kMinFrameRate)
    frame_rate = kMinFrameRate;
  else if (frame_rate > kMaxFrameRate)
    frame_rate = kMaxFrameRate;

  capture_format_.frame_size.SetSize(width, height);
  capture_format_.frame_rate = frame_rate;
  capture_format_.pixel_format = PIXEL_FORMAT_UYVY;

  // QTKit: Set the capture resolution only if this is VGA or smaller, otherwise
  // leave it unconfigured and start capturing: QTKit will produce frames at the
  // native resolution, allowing us to identify cameras whose native resolution
  // is too low for HD. This additional information comes at a cost in startup
  // latency, because the webcam will need to be reopened if its default
  // resolution is not HD or VGA.
  // AVfoundation is configured for all resolutions.
  if (AVFoundationGlue::IsAVFoundationSupported() || width <= kVGA.width ||
      height <= kVGA.height) {
    if (!UpdateCaptureResolution())
      return;
  }

  // Try setting the power line frequency removal (anti-flicker). The built-in
  // cameras are normally suspended so the configuration must happen right
  // before starting capture and during configuration.
  const std::string& device_model = device_name_.GetModel();
  if (device_model.length() > 2 * kVidPidSize) {
    std::string vendor_id = device_model.substr(0, kVidPidSize);
    std::string model_id = device_model.substr(kVidPidSize + 1);
    int vendor_id_as_int, model_id_as_int;
    if (base::HexStringToInt(base::StringPiece(vendor_id), &vendor_id_as_int) &&
        base::HexStringToInt(base::StringPiece(model_id), &model_id_as_int)) {
      SetAntiFlickerInUsbDevice(vendor_id_as_int, model_id_as_int,
          GetPowerLineFrequencyForLocation());
    }
  }

  if (![capture_device_ startCapture]) {
    SetErrorState("Could not start capture device.");
    return;
  }

  state_ = kCapturing;
}

void VideoCaptureDeviceMac::StopAndDeAllocate() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == kCapturing || state_ == kError) << state_;
  [capture_device_ stopCapture];

  [capture_device_ setCaptureDevice:nil];
  [capture_device_ setFrameReceiver:nil];
  client_.reset();
  state_ = kIdle;
  tried_to_square_pixels_ = false;
}

bool VideoCaptureDeviceMac::Init(
    VideoCaptureDevice::Name::CaptureApiType capture_api_type) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kNotInitialized);

  if (capture_api_type == Name::AVFOUNDATION) {
    capture_device_ =
        [[VideoCaptureDeviceAVFoundation alloc] initWithFrameReceiver:this];
  } else {
    capture_device_ =
        [[VideoCaptureDeviceQTKit alloc] initWithFrameReceiver:this];
  }

  if (!capture_device_)
    return false;

  state_ = kIdle;
  return true;
}

void VideoCaptureDeviceMac::ReceiveFrame(
    const uint8* video_frame,
    int video_frame_length,
    const VideoCaptureFormat& frame_format,
    int aspect_numerator,
    int aspect_denominator) {
  // This method is safe to call from a device capture thread, i.e. any thread
  // controlled by QTKit/AVFoundation.
  if (!final_resolution_selected_) {
    DCHECK(!AVFoundationGlue::IsAVFoundationSupported());
    if (capture_format_.frame_size.width() > kVGA.width ||
        capture_format_.frame_size.height() > kVGA.height) {
      // We are requesting HD.  Make sure that the picture is good, otherwise
      // drop down to VGA.
      bool change_to_vga = false;
      if (frame_format.frame_size.width() <
          capture_format_.frame_size.width() ||
          frame_format.frame_size.height() <
          capture_format_.frame_size.height()) {
        // These are the default capture settings, not yet configured to match
        // |capture_format_|.
        DCHECK(frame_format.frame_rate == 0);
        DVLOG(1) << "Switching to VGA because the default resolution is " <<
            frame_format.frame_size.ToString();
        change_to_vga = true;
      }

      if (capture_format_.frame_size == frame_format.frame_size &&
          aspect_numerator != aspect_denominator) {
        DVLOG(1) << "Switching to VGA because HD has nonsquare pixel " <<
            "aspect ratio " << aspect_numerator << ":" << aspect_denominator;
        change_to_vga = true;
      }

      if (change_to_vga)
        capture_format_.frame_size.SetSize(kVGA.width, kVGA.height);
    }

    if (capture_format_.frame_size == frame_format.frame_size &&
        !tried_to_square_pixels_ &&
        (aspect_numerator > kMaxPixelAspectRatio * aspect_denominator ||
         aspect_denominator > kMaxPixelAspectRatio * aspect_numerator)) {
      // The requested size results in non-square PAR. Shrink the frame to 1:1
      // PAR (assuming QTKit selects the same input mode, which is not
      // guaranteed).
      int new_width = capture_format_.frame_size.width();
      int new_height = capture_format_.frame_size.height();
      if (aspect_numerator < aspect_denominator)
        new_width = (new_width * aspect_numerator) / aspect_denominator;
      else
        new_height = (new_height * aspect_denominator) / aspect_numerator;
      capture_format_.frame_size.SetSize(new_width, new_height);
      tried_to_square_pixels_ = true;
    }

    if (capture_format_.frame_size == frame_format.frame_size) {
      final_resolution_selected_ = true;
    } else {
      UpdateCaptureResolution();
      // Let the resolution update sink through QTKit and wait for next frame.
      return;
    }
  }

  // QTKit capture source can change resolution if someone else reconfigures the
  // camera, and that is fine: http://crbug.com/353620. In AVFoundation, this
  // should not happen, it should resize internally.
  if (!AVFoundationGlue::IsAVFoundationSupported()) {
    capture_format_.frame_size = frame_format.frame_size;
  } else if (capture_format_.frame_size != frame_format.frame_size) {
    ReceiveError("Captured resolution " + frame_format.frame_size.ToString() +
        ", and expected " + capture_format_.frame_size.ToString());
    return;
  }

  client_->OnIncomingCapturedData(video_frame,
                                  video_frame_length,
                                  capture_format_,
                                  0,
                                  base::TimeTicks::Now());
}

void VideoCaptureDeviceMac::ReceiveError(const std::string& reason) {
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&VideoCaptureDeviceMac::SetErrorState,
                                    weak_factory_.GetWeakPtr(),
                                    reason));
}

void VideoCaptureDeviceMac::SetErrorState(const std::string& reason) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DLOG(ERROR) << reason;
  state_ = kError;
  client_->OnError(reason);
}

void VideoCaptureDeviceMac::LogMessage(const std::string& message) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (client_)
    client_->OnLog(message);
}

bool VideoCaptureDeviceMac::UpdateCaptureResolution() {
 if (![capture_device_ setCaptureHeight:capture_format_.frame_size.height()
                                  width:capture_format_.frame_size.width()
                              frameRate:capture_format_.frame_rate]) {
   ReceiveError("Could not configure capture device.");
   return false;
 }
 return true;
}

} // namespace media
