// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_USB_MIDI_DEVICE_ANDROID_H_
#define MEDIA_MIDI_USB_MIDI_DEVICE_ANDROID_H_

#include <jni.h>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/callback.h"
#include "media/base/media_export.h"
#include "media/midi/usb_midi_device.h"

namespace media {

class MEDIA_EXPORT UsbMidiDeviceAndroid : public UsbMidiDevice {
 public:
  typedef base::android::ScopedJavaLocalRef<jobject> ObjectRef;

  static scoped_ptr<Factory> CreateFactory();

  UsbMidiDeviceAndroid(ObjectRef raw_device, UsbMidiDeviceDelegate* delegate);
  virtual ~UsbMidiDeviceAndroid();

  // UsbMidiDevice implementation.
  virtual std::vector<uint8> GetDescriptor() OVERRIDE;
  virtual void Send(int endpoint_number,
                    const std::vector<uint8>& data) OVERRIDE;

  // Called by the Java world.
  void OnData(JNIEnv* env,
              jobject caller,
              jint endpoint_number,
              jbyteArray data);

  static bool RegisterUsbMidiDevice(JNIEnv* env);

 private:
  // The actual device object.
  base::android::ScopedJavaGlobalRef<jobject> raw_device_;
  UsbMidiDeviceDelegate* delegate_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(UsbMidiDeviceAndroid);
};

}  // namespace media

#endif  // MEDIA_MIDI_USB_MIDI_DEVICE_ANDROID_H_
