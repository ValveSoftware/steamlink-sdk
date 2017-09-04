// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/audio_device_listener_mac.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/mac_logging.h"
#include "base/message_loop/message_loop.h"
#include "base/pending_task.h"

namespace media {

// Property address to monitor for device changes.
const AudioObjectPropertyAddress
AudioDeviceListenerMac::kDeviceChangePropertyAddress = {
  kAudioHardwarePropertyDefaultOutputDevice,
  kAudioObjectPropertyScopeGlobal,
  kAudioObjectPropertyElementMaster
};

// Callback from the system when the default device changes; this must be called
// on the MessageLoop that created the AudioManager.
// static
OSStatus AudioDeviceListenerMac::OnDefaultDeviceChanged(
    AudioObjectID object, UInt32 num_addresses,
    const AudioObjectPropertyAddress addresses[], void* context) {
  if (object != kAudioObjectSystemObject)
    return noErr;

  for (UInt32 i = 0; i < num_addresses; ++i) {
    if (addresses[i].mSelector == kDeviceChangePropertyAddress.mSelector &&
        addresses[i].mScope == kDeviceChangePropertyAddress.mScope &&
        addresses[i].mElement == kDeviceChangePropertyAddress.mElement &&
        context) {
      static_cast<AudioDeviceListenerMac*>(context)->listener_cb_.Run();
      break;
    }
  }

  return noErr;
}

AudioDeviceListenerMac::AudioDeviceListenerMac(
    const base::Closure& listener_cb) {
  OSStatus result = AudioObjectAddPropertyListener(
      kAudioObjectSystemObject, &kDeviceChangePropertyAddress,
      &AudioDeviceListenerMac::OnDefaultDeviceChanged, this);

  if (result != noErr) {
    OSSTATUS_DLOG(ERROR, result)
        << "AudioObjectAddPropertyListener() failed!";
    return;
  }

  listener_cb_ = listener_cb;
}

AudioDeviceListenerMac::~AudioDeviceListenerMac() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (listener_cb_.is_null())
    return;

  // Since we're running on the same CFRunLoop, there can be no outstanding
  // callbacks in flight.
  OSStatus result = AudioObjectRemovePropertyListener(
      kAudioObjectSystemObject, &kDeviceChangePropertyAddress,
      &AudioDeviceListenerMac::OnDefaultDeviceChanged, this);
  OSSTATUS_DLOG_IF(ERROR, result != noErr, result)
      << "AudioObjectRemovePropertyListener() failed!";
}

}  // namespace media
