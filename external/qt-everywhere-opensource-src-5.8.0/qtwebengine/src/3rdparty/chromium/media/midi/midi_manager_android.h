// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_MANAGER_ANDROID_H_
#define MEDIA_MIDI_MIDI_MANAGER_ANDROID_H_

#include <jni.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/containers/hash_tables.h"
#include "base/memory/scoped_vector.h"
#include "base/time/time.h"
#include "media/midi/midi_input_port_android.h"
#include "media/midi/midi_manager.h"
#include "media/midi/midi_scheduler.h"

namespace media {
namespace midi {

class MidiDeviceAndroid;
class MidiOutputPortAndroid;

// MidiManagerAndroid is a MidiManager subclass for Android M or newer. For
// older android OSes, we use MidiManagerUsb.
class MidiManagerAndroid final : public MidiManager,
                                 public MidiInputPortAndroid::Delegate {
 public:
  MidiManagerAndroid();
  ~MidiManagerAndroid() override;

  // MidiManager implementation.
  void StartInitialization() override;
  void DispatchSendMidiData(MidiManagerClient* client,
                            uint32_t port_index,
                            const std::vector<uint8_t>& data,
                            double timestamp) override;

  // MidiInputPortAndroid::Delegate implementation.
  void OnReceivedData(MidiInputPortAndroid*,
                      const uint8_t* data,
                      size_t size,
                      base::TimeTicks timestamp) override;

  // Called from the Java world.
  void OnInitialized(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& caller,
                     const base::android::JavaParamRef<jobjectArray>& devices);
  void OnAttached(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& caller,
                  const base::android::JavaParamRef<jobject>& device);
  void OnDetached(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& caller,
                  const base::android::JavaParamRef<jobject>& device);

  static bool Register(JNIEnv* env);

 private:
  void AddDevice(std::unique_ptr<MidiDeviceAndroid> device);
  void AddInputPortAndroid(MidiInputPortAndroid* port,
                           MidiDeviceAndroid* device);
  void AddOutputPortAndroid(MidiOutputPortAndroid* port,
                            MidiDeviceAndroid* device);

  ScopedVector<MidiDeviceAndroid> devices_;
  // All ports held in |devices_|. Each device has ownership of ports, but we
  // can store pointers here because a device will keep its ports while it is
  // alive.
  std::vector<MidiInputPortAndroid*> all_input_ports_;
  // A dictionary from a port to its index.
  // input_port_to_index_[all_input_ports_[i]] == i for each valid |i|.
  base::hash_map<MidiInputPortAndroid*, size_t> input_port_to_index_;

  // Ditto for output ports.
  std::vector<MidiOutputPortAndroid*> all_output_ports_;
  base::hash_map<MidiOutputPortAndroid*, size_t> output_port_to_index_;

  base::android::ScopedJavaGlobalRef<jobject> raw_manager_;
  std::unique_ptr<MidiScheduler> scheduler_;
};

}  // namespace midi
}  // namespace media

#endif  // MEDIA_MIDI_MIDI_MANAGER_ANDROID_H_
