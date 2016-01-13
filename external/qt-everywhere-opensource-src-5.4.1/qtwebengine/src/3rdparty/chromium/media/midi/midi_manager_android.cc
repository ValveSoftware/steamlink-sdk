// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "media/midi/midi_manager_usb.h"
#include "media/midi/usb_midi_device_factory_android.h"

namespace media {

MidiManager* MidiManager::Create() {
  return new MidiManagerUsb(
      scoped_ptr<UsbMidiDevice::Factory>(new UsbMidiDeviceFactoryAndroid));
}

}  // namespace media
