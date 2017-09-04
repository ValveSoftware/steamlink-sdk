// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_DEVICE_ANDROID_H_
#define MEDIA_MIDI_MIDI_DEVICE_ANDROID_H_

#include <jni.h>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_vector.h"
#include "media/midi/midi_input_port_android.h"

namespace midi {

class MidiOutputPortAndroid;

class MidiDeviceAndroid final {
 public:
  MidiDeviceAndroid(JNIEnv* env,
                    jobject raw_device,
                    MidiInputPortAndroid::Delegate* delegate);
  ~MidiDeviceAndroid();

  std::string GetManufacturer();
  std::string GetProductName();
  std::string GetDeviceVersion();

  const ScopedVector<MidiInputPortAndroid>& input_ports() const {
    return input_ports_;
  }
  const ScopedVector<MidiOutputPortAndroid>& output_ports() const {
    return output_ports_;
  }
  bool HasRawDevice(JNIEnv* env, jobject raw_device) const {
    return env->IsSameObject(raw_device_.obj(), raw_device);
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> raw_device_;
  ScopedVector<MidiInputPortAndroid> input_ports_;
  ScopedVector<MidiOutputPortAndroid> output_ports_;
};

}  // namespace midi

#endif  // MEDIA_MIDI_MIDI_DEVICE_ANDROID_H_
