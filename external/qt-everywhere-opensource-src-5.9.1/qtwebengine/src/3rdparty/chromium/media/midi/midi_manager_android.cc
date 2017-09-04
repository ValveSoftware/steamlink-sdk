// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager_android.h"

#include "base/android/build_info.h"
#include "base/android/context_utils.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "jni/MidiManagerAndroid_jni.h"
#include "media/midi/midi_device_android.h"
#include "media/midi/midi_manager_usb.h"
#include "media/midi/midi_output_port_android.h"
#include "media/midi/midi_switches.h"
#include "media/midi/usb_midi_device_factory_android.h"

using base::android::JavaParamRef;
using midi::mojom::PortState;
using midi::mojom::Result;

namespace midi {

MidiManager* MidiManager::Create() {
  auto sdk_version = base::android::BuildInfo::GetInstance()->sdk_int();
  if (sdk_version <= base::android::SDK_VERSION_LOLLIPOP_MR1 ||
      !base::FeatureList::IsEnabled(features::kMidiManagerAndroid)) {
    return new MidiManagerUsb(std::unique_ptr<UsbMidiDevice::Factory>(
        new UsbMidiDeviceFactoryAndroid));
  }

  return new MidiManagerAndroid();
}

MidiManagerAndroid::MidiManagerAndroid() {}

MidiManagerAndroid::~MidiManagerAndroid() {
  base::AutoLock auto_lock(scheduler_lock_);
  CHECK(!scheduler_);
}

void MidiManagerAndroid::StartInitialization() {
  JNIEnv* env = base::android::AttachCurrentThread();

  uintptr_t pointer = reinterpret_cast<uintptr_t>(this);
  raw_manager_.Reset(Java_MidiManagerAndroid_create(
      env, base::android::GetApplicationContext(), pointer));

  {
    base::AutoLock auto_lock(scheduler_lock_);
    scheduler_.reset(new MidiScheduler(this));
  }

  Java_MidiManagerAndroid_initialize(env, raw_manager_);
}

void MidiManagerAndroid::Finalize() {
  // Destruct MidiScheduler on Chrome_IOThread.
  base::AutoLock auto_lock(scheduler_lock_);
  scheduler_.reset();
}

void MidiManagerAndroid::DispatchSendMidiData(MidiManagerClient* client,
                                              uint32_t port_index,
                                              const std::vector<uint8_t>& data,
                                              double timestamp) {
  if (port_index >= all_output_ports_.size()) {
    // |port_index| is provided by a renderer so we can't believe that it is
    // in the valid range.
    return;
  }
  DCHECK_EQ(output_ports().size(), all_output_ports_.size());
  if (output_ports()[port_index].state == PortState::CONNECTED) {
    // We treat send call as implicit open.
    // TODO(yhirano): Implement explicit open operation from the renderer.
    if (all_output_ports_[port_index]->Open()) {
      SetOutputPortState(port_index, PortState::OPENED);
    } else {
      // We cannot open the port. It's useless to send data to such a port.
      return;
    }
  }

  // output_streams_[port_index] is alive unless MidiManagerUsb is deleted.
  // The task posted to the MidiScheduler will be disposed safely on deleting
  // the scheduler.
  scheduler_->PostSendDataTask(
      client, data.size(), timestamp,
      base::Bind(&MidiOutputPortAndroid::Send,
                 base::Unretained(all_output_ports_[port_index]), data));
}

void MidiManagerAndroid::OnReceivedData(MidiInputPortAndroid* port,
                                        const uint8_t* data,
                                        size_t size,
                                        base::TimeTicks timestamp) {
  const auto i = input_port_to_index_.find(port);
  DCHECK(input_port_to_index_.end() != i);
  ReceiveMidiData(i->second, data, size, timestamp);
}

void MidiManagerAndroid::OnInitialized(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller,
    const JavaParamRef<jobjectArray>& devices) {
  jsize length = env->GetArrayLength(devices);

  for (jsize i = 0; i < length; ++i) {
    jobject raw_device = env->GetObjectArrayElement(devices, i);
    AddDevice(base::MakeUnique<MidiDeviceAndroid>(env, raw_device, this));
  }
  CompleteInitialization(Result::OK);
}

void MidiManagerAndroid::OnInitializationFailed(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller) {
  CompleteInitialization(Result::INITIALIZATION_ERROR);
}

void MidiManagerAndroid::OnAttached(JNIEnv* env,
                                    const JavaParamRef<jobject>& caller,
                                    const JavaParamRef<jobject>& raw_device) {
  AddDevice(base::MakeUnique<MidiDeviceAndroid>(env, raw_device, this));
}

void MidiManagerAndroid::OnDetached(JNIEnv* env,
                                    const JavaParamRef<jobject>& caller,
                                    const JavaParamRef<jobject>& raw_device) {
  for (auto* device : devices_) {
    if (device->HasRawDevice(env, raw_device)) {
      for (auto* port : device->input_ports()) {
        DCHECK(input_port_to_index_.end() != input_port_to_index_.find(port));
        size_t index = input_port_to_index_[port];
        SetInputPortState(index, PortState::DISCONNECTED);
      }
      for (auto* port : device->output_ports()) {
        DCHECK(output_port_to_index_.end() != output_port_to_index_.find(port));
        size_t index = output_port_to_index_[port];
        SetOutputPortState(index, PortState::DISCONNECTED);
      }
    }
  }
}

void MidiManagerAndroid::AddDevice(std::unique_ptr<MidiDeviceAndroid> device) {
  for (auto* port : device->input_ports()) {
    // We implicitly open input ports here, because there are no signal
    // from the renderer when to open.
    // TODO(yhirano): Implement open operation in Blink.
    PortState state = port->Open() ? PortState::OPENED : PortState::CONNECTED;

    const size_t index = all_input_ports_.size();
    all_input_ports_.push_back(port);
    // Port ID must be unique in a MIDI manager. This ID setting is
    // sufficiently unique although there is no user-friendly meaning.
    // TODO(yhirano): Use a hashed string as ID.
    const std::string id(
        base::StringPrintf("native:port-in-%ld", static_cast<long>(index)));

    input_port_to_index_.insert(std::make_pair(port, index));
    AddInputPort(MidiPortInfo(id, device->GetManufacturer(),
                              device->GetProductName(),
                              device->GetDeviceVersion(), state));
  }
  for (auto* port : device->output_ports()) {
    const size_t index = all_output_ports_.size();
    all_output_ports_.push_back(port);

    // Port ID must be unique in a MIDI manager. This ID setting is
    // sufficiently unique although there is no user-friendly meaning.
    // TODO(yhirano): Use a hashed string as ID.
    const std::string id(
        base::StringPrintf("native:port-out-%ld", static_cast<long>(index)));

    output_port_to_index_.insert(std::make_pair(port, index));
    AddOutputPort(
        MidiPortInfo(id, device->GetManufacturer(), device->GetProductName(),
                     device->GetDeviceVersion(), PortState::CONNECTED));
  }
  devices_.push_back(device.release());
}

bool MidiManagerAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace midi
