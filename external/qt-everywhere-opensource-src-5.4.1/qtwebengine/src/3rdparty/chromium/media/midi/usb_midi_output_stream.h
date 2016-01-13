// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_USB_MIDI_OUTPUT_STREAM_H_
#define MEDIA_MIDI_USB_MIDI_OUTPUT_STREAM_H_

#include <vector>

#include "base/basictypes.h"
#include "media/base/media_export.h"
#include "media/midi/usb_midi_jack.h"

namespace media {

// UsbMidiOutputStream converts MIDI data to USB-MIDI data.
// See "USB Device Class Definition for MIDI Devices" Release 1.0,
// Section 4 "USB-MIDI Event Packets" for details.
class MEDIA_EXPORT UsbMidiOutputStream {
 public:
  explicit UsbMidiOutputStream(const UsbMidiJack& jack);

  // Converts |data| to USB-MIDI data and send it to the jack.
  void Send(const std::vector<uint8>& data);

  const UsbMidiJack& jack() const { return jack_; }

 private:
  size_t GetSize(const std::vector<uint8>& data) const;
  uint8_t Get(const std::vector<uint8>& data, size_t index) const;

  bool PushSysExMessage(const std::vector<uint8>& data,
                        size_t* current,
                        std::vector<uint8>* data_to_send);
  bool PushSysCommonMessage(const std::vector<uint8>& data,
                            size_t* current,
                            std::vector<uint8>* data_to_send);
  void PushSysRTMessage(const std::vector<uint8>& data,
                        size_t* current,
                        std::vector<uint8>* data_to_send);
  bool PushChannelMessage(const std::vector<uint8>& data,
                          size_t* current,
                          std::vector<uint8>* data_to_send);

  static const size_t kPacketContentSize = 3;

  UsbMidiJack jack_;
  size_t pending_size_;
  uint8 pending_data_[kPacketContentSize];
  bool is_sending_sysex_;

  DISALLOW_COPY_AND_ASSIGN(UsbMidiOutputStream);
};

}  // namespace media

#endif  // MEDIA_MIDI_USB_MIDI_OUTPUT_STREAM_H_
