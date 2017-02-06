// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MAC_AUDIO_DEVICE_LISTENER_MAC_H_
#define MEDIA_AUDIO_MAC_AUDIO_DEVICE_LISTENER_MAC_H_

#include <CoreAudio/AudioHardware.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "media/base/media_export.h"

namespace media {

// AudioDeviceListenerMac facilitates execution of device listener callbacks
// issued via CoreAudio.
class MEDIA_EXPORT AudioDeviceListenerMac {
 public:
  // |listener_cb| will be called when a device change occurs; it's a permanent
  // callback and must outlive AudioDeviceListenerMac.  Note that |listener_cb|
  // might not be executed on the same thread as construction.
  explicit AudioDeviceListenerMac(const base::Closure& listener_cb);
  ~AudioDeviceListenerMac();

 private:
  friend class AudioDeviceListenerMacTest;
  static const AudioObjectPropertyAddress kDeviceChangePropertyAddress;

  static OSStatus OnDefaultDeviceChanged(
      AudioObjectID object, UInt32 num_addresses,
      const AudioObjectPropertyAddress addresses[], void* context);

  base::Closure listener_cb_;

  // AudioDeviceListenerMac must be constructed and destructed on the same
  // thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(AudioDeviceListenerMac);
};

}  // namespace media

#endif  // MEDIA_AUDIO_MAC_AUDIO_DEVICE_LISTENER_MAC_H_
