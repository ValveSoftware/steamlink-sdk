// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_USB_MIDI_DEVICE_H_
#define MEDIA_MIDI_USB_MIDI_DEVICE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_vector.h"
#include "base/time/time.h"
#include "media/base/media_export.h"

namespace media {

class UsbMidiDevice;

// Delegate class for UsbMidiDevice.
// Each method is called when an corresponding event arrives at the device.
class MEDIA_EXPORT UsbMidiDeviceDelegate {
 public:
  virtual ~UsbMidiDeviceDelegate() {}

  // Called when USB-MIDI data arrives at |device|.
  virtual void ReceiveUsbMidiData(UsbMidiDevice* device,
                                  int endpoint_number,
                                  const uint8* data,
                                  size_t size,
                                  base::TimeTicks time) = 0;
};

// UsbMidiDevice represents a USB-MIDI device.
// This is an interface class and each platform-dependent implementation class
// will be a derived class.
class MEDIA_EXPORT UsbMidiDevice {
 public:
  typedef ScopedVector<UsbMidiDevice> Devices;

  // Factory class for USB-MIDI devices.
  // Each concrete implementation will find and create devices
  // in platform-dependent way.
  class Factory {
   public:
    typedef base::Callback<void(bool result, Devices* devices)> Callback;
    virtual ~Factory() {}
    // Enumerates devices.
    // Devices that have no USB-MIDI interfaces can be omitted.
    // When the operation succeeds, |callback| will be called with |true| and
    // devices.
    // Otherwise |callback| will be called with |false| and empty devices.
    // When this factory is destroyed during the operation, the operation
    // will be canceled silently (i.e. |callback| will not be called).
    // This function can be called at most once per instance.
    virtual void EnumerateDevices(UsbMidiDeviceDelegate* delegate,
                                  Callback callback) = 0;
  };

  virtual ~UsbMidiDevice() {}

  // Returns the descriptor of this device.
  virtual std::vector<uint8> GetDescriptor() = 0;

  // Sends |data| to the given USB endpoint of this device.
  virtual void Send(int endpoint_number, const std::vector<uint8>& data) = 0;
};

}  // namespace media

#endif  // MEDIA_MIDI_USB_MIDI_DEVICE_H_
