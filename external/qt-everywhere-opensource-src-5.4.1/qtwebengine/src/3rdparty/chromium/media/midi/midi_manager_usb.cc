// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager_usb.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "media/midi/usb_midi_descriptor_parser.h"
#include "media/midi/usb_midi_device.h"
#include "media/midi/usb_midi_input_stream.h"
#include "media/midi/usb_midi_jack.h"
#include "media/midi/usb_midi_output_stream.h"

namespace media {

MidiManagerUsb::MidiManagerUsb(scoped_ptr<UsbMidiDevice::Factory> factory)
    : device_factory_(factory.Pass()) {
}

MidiManagerUsb::~MidiManagerUsb() {
}

void MidiManagerUsb::StartInitialization() {
  Initialize(
      base::Bind(&MidiManager::CompleteInitialization, base::Unretained(this)));
}

void MidiManagerUsb::Initialize(
    base::Callback<void(MidiResult result)> callback) {
  initialize_callback_ = callback;
  // This is safe because EnumerateDevices cancels the operation on destruction.
  device_factory_->EnumerateDevices(
      this,
      base::Bind(&MidiManagerUsb::OnEnumerateDevicesDone,
                 base::Unretained(this)));
}

void MidiManagerUsb::DispatchSendMidiData(MidiManagerClient* client,
                                          uint32_t port_index,
                                          const std::vector<uint8>& data,
                                          double timestamp) {
  DCHECK_LT(port_index, output_streams_.size());
  output_streams_[port_index]->Send(data);
  client->AccumulateMidiBytesSent(data.size());
}

void MidiManagerUsb::ReceiveUsbMidiData(UsbMidiDevice* device,
                                        int endpoint_number,
                                        const uint8* data,
                                        size_t size,
                                        base::TimeTicks time) {
  if (!input_stream_)
    return;
  input_stream_->OnReceivedData(device,
                                endpoint_number,
                                data,
                                size,
                                time);
}

void MidiManagerUsb::OnReceivedData(size_t jack_index,
                                    const uint8* data,
                                    size_t size,
                                    base::TimeTicks time) {
  ReceiveMidiData(jack_index, data, size, time);
}


void MidiManagerUsb::OnEnumerateDevicesDone(bool result,
                                            UsbMidiDevice::Devices* devices) {
  if (!result) {
    initialize_callback_.Run(MIDI_INITIALIZATION_ERROR);
    return;
  }
  devices->swap(devices_);
  for (size_t i = 0; i < devices_.size(); ++i) {
    UsbMidiDescriptorParser parser;
    std::vector<uint8> descriptor = devices_[i]->GetDescriptor();
    const uint8* data = descriptor.size() > 0 ? &descriptor[0] : NULL;
    std::vector<UsbMidiJack> jacks;
    bool parse_result = parser.Parse(devices_[i],
                                     data,
                                     descriptor.size(),
                                     &jacks);
    if (!parse_result) {
      initialize_callback_.Run(MIDI_INITIALIZATION_ERROR);
      return;
    }
    std::vector<UsbMidiJack> input_jacks;
    for (size_t j = 0; j < jacks.size(); ++j) {
      if (jacks[j].direction() == UsbMidiJack::DIRECTION_OUT) {
        output_streams_.push_back(new UsbMidiOutputStream(jacks[j]));
        // TODO(yhirano): Set appropriate properties.
        AddOutputPort(MidiPortInfo());
      } else {
        DCHECK_EQ(jacks[j].direction(), UsbMidiJack::DIRECTION_IN);
        input_jacks.push_back(jacks[j]);
        // TODO(yhirano): Set appropriate properties.
        AddInputPort(MidiPortInfo());
      }
    }
    input_stream_.reset(new UsbMidiInputStream(input_jacks, this));
  }
  initialize_callback_.Run(MIDI_OK);
}

}  // namespace media
