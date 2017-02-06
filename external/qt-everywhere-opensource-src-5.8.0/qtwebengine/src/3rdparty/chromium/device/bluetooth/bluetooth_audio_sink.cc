// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_audio_sink.h"

namespace {

// SBC codec(0x00) is the mandatory codec for A2DP audio sink.
// |capabilities| includes the mandatory support for A2DP audio sink.
// [Octet0]: 0x3f
//   Sampling frequency [0 0 1 1]: supports 44100 Hz and 48000 Hz.
//   Channel mode [1 1 1 1]: supports Mono, Dual Channel, Stereo, and Joint
//                           Stereo.
// [Octet1]: 0xff
//    Block length [1, 1, 1, 1]: supports block length of 4, 8, 12 and 16.
//    Subbands [1, 1]: supports 4 and 8 subbands.
//    Allocation Method [1, 1]: supports SNR and Loudness.
// [Octet2]: 0x12
//    Minimum bitpool value: 18 is the bitpool value for Mono.
// [Octet3]: 0x35
//    Maximum bitpool value: 53 is the bitpool value for Joint Stereo.
  const uint8_t kDefaultCodec = 0x00;
  const uint8_t kDefaultCapabilities[] = {0x3f, 0xff, 0x12, 0x35};

}  // namespace

namespace device {

// static
const uint16_t BluetoothAudioSink::kInvalidVolume = 128;

BluetoothAudioSink::Options::Options() : codec(kDefaultCodec) {
  capabilities.assign(kDefaultCapabilities,
                      kDefaultCapabilities + sizeof(kDefaultCapabilities));
}

BluetoothAudioSink::Options::~Options() {
}

BluetoothAudioSink::BluetoothAudioSink() {
}

BluetoothAudioSink::~BluetoothAudioSink() {
}

}  // namespace device
