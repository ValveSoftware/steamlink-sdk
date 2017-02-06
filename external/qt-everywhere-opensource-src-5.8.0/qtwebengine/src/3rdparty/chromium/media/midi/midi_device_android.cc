// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_device_android.h"

#include <string>

#include "jni/MidiDeviceAndroid_jni.h"
#include "media/midi/midi_output_port_android.h"

namespace media {
namespace midi {

MidiDeviceAndroid::MidiDeviceAndroid(JNIEnv* env,
                                     jobject raw_device,
                                     MidiInputPortAndroid::Delegate* delegate)
    : raw_device_(env, raw_device) {
  ScopedJavaLocalRef<jobjectArray> raw_input_ports =
      Java_MidiDeviceAndroid_getInputPorts(env, raw_device);
  jsize num_input_ports = env->GetArrayLength(raw_input_ports.obj());

  for (jsize i = 0; i < num_input_ports; ++i) {
    jobject port = env->GetObjectArrayElement(raw_input_ports.obj(), i);
    input_ports_.push_back(new MidiInputPortAndroid(env, port, delegate));
  }

  ScopedJavaLocalRef<jobjectArray> raw_output_ports =
      Java_MidiDeviceAndroid_getOutputPorts(env, raw_device);
  jsize num_output_ports = env->GetArrayLength(raw_output_ports.obj());
  for (jsize i = 0; i < num_output_ports; ++i) {
    jobject port = env->GetObjectArrayElement(raw_output_ports.obj(), i);
    output_ports_.push_back(new MidiOutputPortAndroid(env, port));
  }
}

MidiDeviceAndroid::~MidiDeviceAndroid() {}

std::string MidiDeviceAndroid::GetManufacturer() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> ret =
      Java_MidiDeviceAndroid_getManufacturer(env, raw_device_.obj());
  return std::string(env->GetStringUTFChars(ret.obj(), nullptr),
                     env->GetStringUTFLength(ret.obj()));
}

std::string MidiDeviceAndroid::GetProductName() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> ret =
      Java_MidiDeviceAndroid_getProduct(env, raw_device_.obj());
  return std::string(env->GetStringUTFChars(ret.obj(), nullptr),
                     env->GetStringUTFLength(ret.obj()));
}

std::string MidiDeviceAndroid::GetDeviceVersion() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> ret =
      Java_MidiDeviceAndroid_getVersion(env, raw_device_.obj());
  return std::string(env->GetStringUTFChars(ret.obj(), nullptr),
                     env->GetStringUTFLength(ret.obj()));
}

bool MidiDeviceAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace midi
}  // namespace media
