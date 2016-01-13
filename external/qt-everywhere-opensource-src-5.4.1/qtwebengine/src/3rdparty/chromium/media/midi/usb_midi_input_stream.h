// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_USB_MIDI_INPUT_STREAM_H_
#define MEDIA_MIDI_USB_MIDI_INPUT_STREAM_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/midi/usb_midi_jack.h"

namespace media {

class UsbMidiDevice;

// UsbMidiInputStream converts USB-MIDI data to MIDI data.
// See "USB Device Class Definition for MIDI Devices" Release 1.0,
// Section 4 "USB-MIDI Event Packets" for details.
class MEDIA_EXPORT UsbMidiInputStream {
 public:
  class MEDIA_EXPORT Delegate {
   public:
    virtual ~Delegate() {}
    // This function is called when some data arrives to a USB-MIDI jack.
    // An input USB-MIDI jack corresponds to an input MIDIPortInfo.
    virtual void OnReceivedData(size_t jack_index,
                                const uint8* data,
                                size_t size,
                                base::TimeTicks time) = 0;
  };

  // This is public for testing.
  struct JackUniqueKey {
    JackUniqueKey(UsbMidiDevice* device, int endpoint_number, int cable_number);
    bool operator==(const JackUniqueKey& that) const;
    bool operator<(const JackUniqueKey& that) const;

    UsbMidiDevice* device;
    int endpoint_number;
    int cable_number;
  };

  UsbMidiInputStream(const std::vector<UsbMidiJack>& jacks,
                     Delegate* delegate);
  ~UsbMidiInputStream();

  // This function should be called when some data arrives to a USB-MIDI
  // endpoint. This function converts the data to MIDI data and call
  // |delegate->OnReceivedData| with it.
  // |size| must be a multiple of |kPacketSize|.
  void OnReceivedData(UsbMidiDevice* device,
                      int endpoint_number,
                      const uint8* data,
                      size_t size,
                      base::TimeTicks time);

  std::vector<JackUniqueKey> RegisteredJackKeysForTesting() const;

 private:
  static const size_t kPacketSize = 4;
  // Processes a USB-MIDI Event Packet.
  // The first |kPacketSize| bytes of |packet| must be accessible.
  void ProcessOnePacket(UsbMidiDevice* device,
                        int endpoint_number,
                        const uint8* packet,
                        base::TimeTicks time);

  // A map from UsbMidiJack to its index in |jacks_|.
  std::map<JackUniqueKey, size_t> jack_dictionary_;

  // Not owned
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(UsbMidiInputStream);
};

}  // namespace media

#endif  // MEDIA_MIDI_USB_MIDI_INPUT_STREAM_H_
