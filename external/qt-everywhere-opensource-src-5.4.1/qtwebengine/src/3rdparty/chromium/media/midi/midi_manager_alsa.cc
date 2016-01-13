// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager_alsa.h"

#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "media/midi/midi_port_info.h"

namespace media {

namespace {

// Per-output buffer. This can be smaller, but then large sysex messages
// will be (harmlessly) split across multiple seq events. This should
// not have any real practical effect, except perhaps to slightly reorder
// realtime messages with respect to sysex.
const size_t kSendBufferSize = 256;

// Constants for the capabilities we search for in inputs and outputs.
// See http://www.alsa-project.org/alsa-doc/alsa-lib/seq.html.
const unsigned int kRequiredInputPortCaps =
    SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ;
const unsigned int kRequiredOutputPortCaps =
    SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE;

int AddrToInt(const snd_seq_addr_t* addr) {
  return (addr->client << 8) | addr->port;
}

class CardInfo {
 public:
  CardInfo(const std::string name, const std::string manufacturer,
           const std::string driver)
      : name_(name), manufacturer_(manufacturer), driver_(driver) {
  }
  const std::string name_;
  const std::string manufacturer_;
  const std::string driver_;
};

}  // namespace

MidiManagerAlsa::MidiManagerAlsa()
    : in_client_(NULL),
      out_client_(NULL),
      out_client_id_(-1),
      in_port_(-1),
      decoder_(NULL),
      send_thread_("MidiSendThread"),
      event_thread_("MidiEventThread"),
      event_thread_shutdown_(false) {
  // Initialize decoder.
  snd_midi_event_new(0, &decoder_);
  snd_midi_event_no_status(decoder_, 1);
}

void MidiManagerAlsa::StartInitialization() {
  // TODO(agoode): Move off I/O thread. See http://crbug.com/374341.

  // Create client handles.
  int err = snd_seq_open(&in_client_, "hw", SND_SEQ_OPEN_INPUT, 0);
  if (err != 0) {
    VLOG(1) << "snd_seq_open fails: " << snd_strerror(err);
    return CompleteInitialization(MIDI_INITIALIZATION_ERROR);
  }
  int in_client_id = snd_seq_client_id(in_client_);
  err = snd_seq_open(&out_client_, "hw", SND_SEQ_OPEN_OUTPUT, 0);
  if (err != 0) {
    VLOG(1) << "snd_seq_open fails: " << snd_strerror(err);
    return CompleteInitialization(MIDI_INITIALIZATION_ERROR);
  }
  out_client_id_ = snd_seq_client_id(out_client_);

  // Name the clients.
  err = snd_seq_set_client_name(in_client_, "Chrome (input)");
  if (err != 0) {
    VLOG(1) << "snd_seq_set_client_name fails: " << snd_strerror(err);
    return CompleteInitialization(MIDI_INITIALIZATION_ERROR);
  }
  err = snd_seq_set_client_name(out_client_, "Chrome (output)");
  if (err != 0) {
    VLOG(1) << "snd_seq_set_client_name fails: " << snd_strerror(err);
    return CompleteInitialization(MIDI_INITIALIZATION_ERROR);
  }

  // Create input port.
  in_port_ = snd_seq_create_simple_port(in_client_, NULL,
                                        SND_SEQ_PORT_CAP_WRITE |
                                        SND_SEQ_PORT_CAP_NO_EXPORT,
                                        SND_SEQ_PORT_TYPE_MIDI_GENERIC |
                                        SND_SEQ_PORT_TYPE_APPLICATION);
  if (in_port_ < 0) {
    VLOG(1) << "snd_seq_create_simple_port fails: " << snd_strerror(in_port_);
    return CompleteInitialization(MIDI_INITIALIZATION_ERROR);
  }

  // Subscribe to the announce port.
  snd_seq_port_subscribe_t* subs;
  snd_seq_port_subscribe_alloca(&subs);
  snd_seq_addr_t announce_sender;
  snd_seq_addr_t announce_dest;
  announce_sender.client = SND_SEQ_CLIENT_SYSTEM;
  announce_sender.port = SND_SEQ_PORT_SYSTEM_ANNOUNCE;
  announce_dest.client = in_client_id;
  announce_dest.port = in_port_;
  snd_seq_port_subscribe_set_sender(subs, &announce_sender);
  snd_seq_port_subscribe_set_dest(subs, &announce_dest);
  err = snd_seq_subscribe_port(in_client_, subs);
  if (err != 0) {
    VLOG(1) << "snd_seq_subscribe_port on the announce port fails: "
            << snd_strerror(err);
    return CompleteInitialization(MIDI_INITIALIZATION_ERROR);
  }

  // Use a heuristic to extract the list of manufacturers for the hardware MIDI
  // devices. This won't work for all devices. It is also brittle until
  // hotplug is implemented. (See http://crbug.com/279097.)
  // TODO(agoode): Make manufacturer extraction simple and reliable.
  // http://crbug.com/377250.
  ScopedVector<CardInfo> cards;
  snd_ctl_card_info_t* card;
  snd_rawmidi_info_t* midi_out;
  snd_rawmidi_info_t* midi_in;
  snd_ctl_card_info_alloca(&card);
  snd_rawmidi_info_alloca(&midi_out);
  snd_rawmidi_info_alloca(&midi_in);
  for (int index = -1; !snd_card_next(&index) && index >= 0; ) {
    const std::string id = base::StringPrintf("hw:CARD=%i", index);
    snd_ctl_t* handle;
    int err = snd_ctl_open(&handle, id.c_str(), 0);
    if (err != 0) {
      VLOG(1) << "snd_ctl_open fails: " << snd_strerror(err);
      continue;
    }
    err = snd_ctl_card_info(handle, card);
    if (err != 0) {
      VLOG(1) << "snd_ctl_card_info fails: " << snd_strerror(err);
      snd_ctl_close(handle);
      continue;
    }
    // Enumerate any rawmidi devices (not subdevices) and extract CardInfo.
    for (int device = -1;
         !snd_ctl_rawmidi_next_device(handle, &device) && device >= 0; ) {
      bool output;
      bool input;
      snd_rawmidi_info_set_device(midi_out, device);
      snd_rawmidi_info_set_subdevice(midi_out, 0);
      snd_rawmidi_info_set_stream(midi_out, SND_RAWMIDI_STREAM_OUTPUT);
      output = snd_ctl_rawmidi_info(handle, midi_out) == 0;
      snd_rawmidi_info_set_device(midi_in, device);
      snd_rawmidi_info_set_subdevice(midi_in, 0);
      snd_rawmidi_info_set_stream(midi_in, SND_RAWMIDI_STREAM_INPUT);
      input = snd_ctl_rawmidi_info(handle, midi_in) == 0;
      if (!output && !input)
        continue;

      snd_rawmidi_info_t* midi = midi_out ? midi_out : midi_in;
      const std::string name = snd_rawmidi_info_get_name(midi);
      // We assume that card longname is in the format of
      // "<manufacturer> <name> at <bus>". Otherwise, we give up to detect
      // a manufacturer name here.
      std::string manufacturer;
      const std::string card_name = snd_ctl_card_info_get_longname(card);
      size_t at_index = card_name.rfind(" at ");
      if (std::string::npos != at_index) {
        size_t name_index = card_name.rfind(name, at_index - 1);
        if (std::string::npos != name_index)
          manufacturer = card_name.substr(0, name_index - 1);
      }
      const std::string driver = snd_ctl_card_info_get_driver(card);
      cards.push_back(new CardInfo(name, manufacturer, driver));
    }
  }

  // Enumerate all ports in all clients.
  snd_seq_client_info_t* client_info;
  snd_seq_client_info_alloca(&client_info);
  snd_seq_port_info_t* port_info;
  snd_seq_port_info_alloca(&port_info);

  snd_seq_client_info_set_client(client_info, -1);
  // Enumerate clients.
  uint32 current_input = 0;
  unsigned int current_card = 0;
  while (!snd_seq_query_next_client(in_client_, client_info)) {
    int client_id = snd_seq_client_info_get_client(client_info);
    if ((client_id == in_client_id) || (client_id == out_client_id_)) {
      // Skip our own clients.
      continue;
    }
    const std::string client_name = snd_seq_client_info_get_name(client_info);
    snd_seq_port_info_set_client(port_info, client_id);
    snd_seq_port_info_set_port(port_info, -1);

    std::string manufacturer;
    std::string driver;
    // In the current Alsa kernel implementation, hardware clients match the
    // cards in the same order.
    if ((snd_seq_client_info_get_type(client_info) == SND_SEQ_KERNEL_CLIENT) &&
        (current_card < cards.size())) {
      const CardInfo* info = cards[current_card];
      if (info->name_ == client_name) {
        manufacturer = info->manufacturer_;
        driver = info->driver_;
        current_card++;
      }
    }
    // Enumerate ports.
    while (!snd_seq_query_next_port(in_client_, port_info)) {
      unsigned int port_type = snd_seq_port_info_get_type(port_info);
      if (port_type & SND_SEQ_PORT_TYPE_MIDI_GENERIC) {
        const snd_seq_addr_t* addr = snd_seq_port_info_get_addr(port_info);
        const std::string name = snd_seq_port_info_get_name(port_info);
        const std::string id = base::StringPrintf("%d:%d %s",
                                                  addr->client,
                                                  addr->port,
                                                  name.c_str());
        std::string version;
        if (driver != "") {
          version = driver + " / ";
        }
        version += base::StringPrintf("ALSA library version %d.%d.%d",
                                      SND_LIB_MAJOR,
                                      SND_LIB_MINOR,
                                      SND_LIB_SUBMINOR);
        unsigned int caps = snd_seq_port_info_get_capability(port_info);
        if ((caps & kRequiredInputPortCaps) == kRequiredInputPortCaps) {
          // Subscribe to this port.
          const snd_seq_addr_t* sender = snd_seq_port_info_get_addr(port_info);
          snd_seq_addr_t dest;
          dest.client = snd_seq_client_id(in_client_);
          dest.port = in_port_;
          snd_seq_port_subscribe_set_sender(subs, sender);
          snd_seq_port_subscribe_set_dest(subs, &dest);
          err = snd_seq_subscribe_port(in_client_, subs);
          if (err != 0) {
            VLOG(1) << "snd_seq_subscribe_port fails: " << snd_strerror(err);
          } else {
            source_map_[AddrToInt(sender)] = current_input++;
            AddInputPort(MidiPortInfo(id, manufacturer, name, version));
          }
        }
        if ((caps & kRequiredOutputPortCaps) == kRequiredOutputPortCaps) {
          // Create a port for us to send on.
          int out_port =
              snd_seq_create_simple_port(out_client_, NULL,
                                         SND_SEQ_PORT_CAP_READ |
                                         SND_SEQ_PORT_CAP_NO_EXPORT,
                                         SND_SEQ_PORT_TYPE_MIDI_GENERIC |
                                         SND_SEQ_PORT_TYPE_APPLICATION);
          if (out_port < 0) {
            VLOG(1) << "snd_seq_create_simple_port fails: "
                    << snd_strerror(out_port);
            // Skip this output port for now.
            continue;
          }

          // Activate port subscription.
          snd_seq_addr_t sender;
          const snd_seq_addr_t* dest = snd_seq_port_info_get_addr(port_info);
          sender.client = snd_seq_client_id(out_client_);
          sender.port = out_port;
          snd_seq_port_subscribe_set_sender(subs, &sender);
          snd_seq_port_subscribe_set_dest(subs, dest);
          err = snd_seq_subscribe_port(out_client_, subs);
          if (err != 0) {
            VLOG(1) << "snd_seq_subscribe_port fails: " << snd_strerror(err);
            snd_seq_delete_simple_port(out_client_, out_port);
          } else {
            snd_midi_event_t* encoder;
            snd_midi_event_new(kSendBufferSize, &encoder);
            encoders_.push_back(encoder);
            out_ports_.push_back(out_port);
            AddOutputPort(MidiPortInfo(id, manufacturer, name, version));
          }
        }
      }
    }
  }

  event_thread_.Start();
  event_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&MidiManagerAlsa::EventReset, base::Unretained(this)));

  CompleteInitialization(MIDI_OK);
}

MidiManagerAlsa::~MidiManagerAlsa() {
  // Tell the event thread it will soon be time to shut down. This gives
  // us assurance the thread will stop in case the SND_SEQ_EVENT_CLIENT_EXIT
  // message is lost.
  {
    base::AutoLock lock(shutdown_lock_);
    event_thread_shutdown_ = true;
  }

  // Stop the send thread.
  send_thread_.Stop();

  // Close the out client. This will trigger the event thread to stop,
  // because of SND_SEQ_EVENT_CLIENT_EXIT.
  if (out_client_)
    snd_seq_close(out_client_);

  // Wait for the event thread to stop.
  event_thread_.Stop();

  // Close the in client.
  if (in_client_)
    snd_seq_close(in_client_);

  // Free the decoder.
  snd_midi_event_free(decoder_);

  // Free the encoders.
  for (EncoderList::iterator i = encoders_.begin(); i != encoders_.end(); ++i)
    snd_midi_event_free(*i);
}

void MidiManagerAlsa::SendMidiData(uint32 port_index,
                                   const std::vector<uint8>& data) {
  DCHECK(send_thread_.message_loop_proxy()->BelongsToCurrentThread());

  snd_midi_event_t* encoder = encoders_[port_index];
  for (unsigned int i = 0; i < data.size(); i++) {
    snd_seq_event_t event;
    int result = snd_midi_event_encode_byte(encoder, data[i], &event);
    if (result == 1) {
      // Full event, send it.
      snd_seq_ev_set_source(&event, out_ports_[port_index]);
      snd_seq_ev_set_subs(&event);
      snd_seq_ev_set_direct(&event);
      snd_seq_event_output_direct(out_client_, &event);
    }
  }
}

void MidiManagerAlsa::DispatchSendMidiData(MidiManagerClient* client,
                                           uint32 port_index,
                                           const std::vector<uint8>& data,
                                           double timestamp) {
  if (out_ports_.size() <= port_index)
    return;

  // Not correct right now. http://crbug.com/374341.
  if (!send_thread_.IsRunning())
    send_thread_.Start();

  base::TimeDelta delay;
  if (timestamp != 0.0) {
    base::TimeTicks time_to_send =
        base::TimeTicks() + base::TimeDelta::FromMicroseconds(
            timestamp * base::Time::kMicrosecondsPerSecond);
    delay = std::max(time_to_send - base::TimeTicks::Now(), base::TimeDelta());
  }

  send_thread_.message_loop()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&MidiManagerAlsa::SendMidiData, base::Unretained(this),
                 port_index, data), delay);

  // Acknowledge send.
  send_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&MidiManagerClient::AccumulateMidiBytesSent,
                 base::Unretained(client), data.size()));
}

void MidiManagerAlsa::EventReset() {
  event_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&MidiManagerAlsa::EventLoop, base::Unretained(this)));
}

void MidiManagerAlsa::EventLoop() {
  // Read available incoming MIDI data.
  snd_seq_event_t* event;
  int err = snd_seq_event_input(in_client_, &event);
  double timestamp =
      (base::TimeTicks::HighResNow() - base::TimeTicks()).InSecondsF();
  if (err == -ENOSPC) {
    VLOG(1) << "snd_seq_event_input detected buffer overrun";

      // We've lost events: check another way to see if we need to shut down.
      base::AutoLock lock(shutdown_lock_);
      if (event_thread_shutdown_) {
        return;
      }
  } else if (err < 0) {
      VLOG(1) << "snd_seq_event_input fails: " << snd_strerror(err);
      return;
  } else {
    // Check for disconnection of out client. This means "shut down".
    if (event->source.client == SND_SEQ_CLIENT_SYSTEM &&
        event->source.port == SND_SEQ_PORT_SYSTEM_ANNOUNCE &&
        event->type == SND_SEQ_EVENT_CLIENT_EXIT &&
        event->data.addr.client == out_client_id_) {
      return;
    }

    std::map<int, uint32>::iterator source_it =
        source_map_.find(AddrToInt(&event->source));
    if (source_it != source_map_.end()) {
      uint32 source = source_it->second;
      if (event->type == SND_SEQ_EVENT_SYSEX) {
        // Special! Variable-length sysex.
        ReceiveMidiData(source, static_cast<const uint8*>(event->data.ext.ptr),
                        event->data.ext.len,
                        timestamp);
      } else {
        // Otherwise, decode this and send that on.
        unsigned char buf[12];
        long count = snd_midi_event_decode(decoder_, buf, sizeof(buf), event);
        if (count <= 0) {
          if (count != -ENOENT) {
            // ENOENT means that it's not a MIDI message, which is not an
            // error, but other negative values are errors for us.
            VLOG(1) << "snd_midi_event_decoder fails " << snd_strerror(count);
          }
        } else {
          ReceiveMidiData(source, buf, count, timestamp);
        }
      }
    }
  }

  // Do again.
  event_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&MidiManagerAlsa::EventLoop, base::Unretained(this)));
}

MidiManager* MidiManager::Create() {
  return new MidiManagerAlsa();
}

}  // namespace media
