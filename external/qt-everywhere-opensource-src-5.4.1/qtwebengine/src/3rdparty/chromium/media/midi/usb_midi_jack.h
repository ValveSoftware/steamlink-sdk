// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_USB_MIDI_JACK_H_
#define MEDIA_MIDI_USB_MIDI_JACK_H_

#include "base/basictypes.h"
#include "media/base/media_export.h"

namespace media {

class UsbMidiDevice;

// UsbMidiJack represents an EMBEDDED MIDI jack.
struct MEDIA_EXPORT UsbMidiJack {
  // The direction of the endpoint associated with an EMBEDDED MIDI jack.
  // Note that an IN MIDI jack associated with an OUT endpoint has
  // ***DIRECTION_OUT*** direction.
  enum Direction {
    DIRECTION_IN,
    DIRECTION_OUT,
  };
  UsbMidiJack(UsbMidiDevice* device,
              uint8 jack_id,
              uint8 cable_number,
              uint8 endpoint_address)
      : device(device),
        jack_id(jack_id),
        cable_number(cable_number),
        endpoint_address(endpoint_address) {}
  // Not owned
  UsbMidiDevice* device;
  // The id of this jack unique in the interface.
  uint8 jack_id;
  // The cable number of this jack in the associated endpoint.
  uint8 cable_number;
  // The address of the endpoint that this jack is associated with.
  uint8 endpoint_address;

  Direction direction() const {
    return (endpoint_address & 0x80) ? DIRECTION_IN : DIRECTION_OUT;
  }
  uint8 endpoint_number() const {
    return (endpoint_address & 0xf);
  }
};

}  // namespace media

#endif  // MEDIA_MIDI_USB_MIDI_JACK_H_
