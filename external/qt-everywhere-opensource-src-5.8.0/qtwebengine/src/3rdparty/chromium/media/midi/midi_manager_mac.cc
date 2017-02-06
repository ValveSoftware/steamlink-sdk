// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager_mac.h"

#include <algorithm>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"

#include <CoreAudio/HostTime.h>
#include <stddef.h>

using base::IntToString;
using base::SysCFStringRefToUTF8;
using std::string;

// NB: System MIDI types are pointer types in 32-bit and integer types in
// 64-bit. Therefore, the initialization is the simplest one that satisfies both
// (if possible).

namespace media {
namespace midi {

namespace {

// Maximum buffer size that CoreMIDI can handle for MIDIPacketList.
const size_t kCoreMIDIMaxPacketListSize = 65536;
// Pessimistic estimation on available data size of MIDIPacketList.
const size_t kEstimatedMaxPacketDataSize = kCoreMIDIMaxPacketListSize / 2;

MidiPortInfo GetPortInfoFromEndpoint(MIDIEndpointRef endpoint) {
  string manufacturer;
  CFStringRef manufacturer_ref = NULL;
  OSStatus result = MIDIObjectGetStringProperty(
      endpoint, kMIDIPropertyManufacturer, &manufacturer_ref);
  if (result == noErr) {
    manufacturer = SysCFStringRefToUTF8(manufacturer_ref);
  } else {
    // kMIDIPropertyManufacturer is not supported in IAC driver providing
    // endpoints, and the result will be kMIDIUnknownProperty (-10835).
    DLOG(WARNING) << "Failed to get kMIDIPropertyManufacturer with status "
                  << result;
  }

  string name;
  CFStringRef name_ref = NULL;
  result = MIDIObjectGetStringProperty(endpoint, kMIDIPropertyDisplayName,
                                       &name_ref);
  if (result == noErr) {
    name = SysCFStringRefToUTF8(name_ref);
  } else {
    DLOG(WARNING) << "Failed to get kMIDIPropertyDisplayName with status "
                  << result;
  }

  string version;
  SInt32 version_number = 0;
  result = MIDIObjectGetIntegerProperty(
      endpoint, kMIDIPropertyDriverVersion, &version_number);
  if (result == noErr) {
    version = IntToString(version_number);
  } else {
    // kMIDIPropertyDriverVersion is not supported in IAC driver providing
    // endpoints, and the result will be kMIDIUnknownProperty (-10835).
    DLOG(WARNING) << "Failed to get kMIDIPropertyDriverVersion with status "
                  << result;
  }

  string id;
  SInt32 id_number = 0;
  result = MIDIObjectGetIntegerProperty(
      endpoint, kMIDIPropertyUniqueID, &id_number);
  if (result == noErr) {
    id = IntToString(id_number);
  } else {
    // On connecting some devices, e.g., nano KONTROL2, unknown endpoints
    // appear and disappear quickly and they fail on queries.
    // Let's ignore such ghost devices.
    // Same problems will happen if the device is disconnected before finishing
    // all queries.
    DLOG(WARNING) << "Failed to get kMIDIPropertyUniqueID with status "
                  << result;
  }

  const MidiPortState state = MIDI_PORT_OPENED;
  return MidiPortInfo(id, manufacturer, name, version, state);
}

double MIDITimeStampToSeconds(MIDITimeStamp timestamp) {
  UInt64 nanoseconds = AudioConvertHostTimeToNanos(timestamp);
  return static_cast<double>(nanoseconds) / 1.0e9;
}

MIDITimeStamp SecondsToMIDITimeStamp(double seconds) {
  UInt64 nanos = UInt64(seconds * 1.0e9);
  return AudioConvertNanosToHostTime(nanos);
}

}  // namespace

MidiManager* MidiManager::Create() {
  return new MidiManagerMac();
}

MidiManagerMac::MidiManagerMac()
    : midi_client_(0),
      coremidi_input_(0),
      coremidi_output_(0),
      client_thread_("MidiClientThread"),
      shutdown_(false) {
}

MidiManagerMac::~MidiManagerMac() = default;

void MidiManagerMac::StartInitialization() {
  // MIDIClient should be created on |client_thread_| to receive CoreMIDI event
  // notifications.
  RunOnClientThread(
      base::Bind(&MidiManagerMac::InitializeCoreMIDI, base::Unretained(this)));
}

void MidiManagerMac::Finalize() {
  // Wait for the termination of |client_thread_| before disposing MIDI ports.
  shutdown_ = true;
  client_thread_.Stop();

  if (coremidi_input_)
    MIDIPortDispose(coremidi_input_);
  if (coremidi_output_)
    MIDIPortDispose(coremidi_output_);
  if (midi_client_)
    MIDIClientDispose(midi_client_);
}

void MidiManagerMac::DispatchSendMidiData(MidiManagerClient* client,
                                          uint32_t port_index,
                                          const std::vector<uint8_t>& data,
                                          double timestamp) {
  RunOnClientThread(
      base::Bind(&MidiManagerMac::SendMidiData,
                 base::Unretained(this), client, port_index, data, timestamp));
}

void MidiManagerMac::RunOnClientThread(const base::Closure& closure) {
  if (shutdown_)
    return;

  if (!client_thread_.IsRunning())
    client_thread_.Start();

  client_thread_.task_runner()->PostTask(FROM_HERE, closure);
}

void MidiManagerMac::InitializeCoreMIDI() {
  DCHECK(client_thread_.task_runner()->BelongsToCurrentThread());

  // CoreMIDI registration.
  DCHECK_EQ(0u, midi_client_);
  OSStatus result =
      MIDIClientCreate(CFSTR("Chrome"), ReceiveMidiNotifyDispatch, this,
                       &midi_client_);
  if (result != noErr || midi_client_ == 0)
    return CompleteInitialization(Result::INITIALIZATION_ERROR);

  // Create input and output port.
  DCHECK_EQ(0u, coremidi_input_);
  result = MIDIInputPortCreate(
      midi_client_,
      CFSTR("MIDI Input"),
      ReadMidiDispatch,
      this,
      &coremidi_input_);
  if (result != noErr || coremidi_input_ == 0)
    return CompleteInitialization(Result::INITIALIZATION_ERROR);

  DCHECK_EQ(0u, coremidi_output_);
  result = MIDIOutputPortCreate(
      midi_client_,
      CFSTR("MIDI Output"),
      &coremidi_output_);
  if (result != noErr || coremidi_output_ == 0)
    return CompleteInitialization(Result::INITIALIZATION_ERROR);

  // Following loop may miss some newly attached devices, but such device will
  // be captured by ReceiveMidiNotifyDispatch callback.
  uint32_t destination_count = MIDIGetNumberOfDestinations();
  destinations_.resize(destination_count);
  for (uint32_t i = 0; i < destination_count; i++) {
    MIDIEndpointRef destination = MIDIGetDestination(i);
    if (destination == 0) {
      // One ore more devices may be detached.
      destinations_.resize(i);
      break;
    }

    // Keep track of all destinations (known as outputs by the Web MIDI API).
    // Cache to avoid any possible overhead in calling MIDIGetDestination().
    destinations_[i] = destination;

    MidiPortInfo info = GetPortInfoFromEndpoint(destination);
    AddOutputPort(info);
  }

  // Open connections from all sources. This loop also may miss new devices.
  uint32_t source_count = MIDIGetNumberOfSources();
  for (uint32_t i = 0; i < source_count; ++i) {
    // Receive from all sources.
    MIDIEndpointRef source = MIDIGetSource(i);
    if (source == 0)
      break;

    // Start listening.
    MIDIPortConnectSource(
        coremidi_input_, source, reinterpret_cast<void*>(source));

    // Keep track of all sources (known as inputs in Web MIDI API terminology).
    source_map_[source] = i;

    MidiPortInfo info = GetPortInfoFromEndpoint(source);
    AddInputPort(info);
  }

  // Allocate maximum size of buffer that CoreMIDI can handle.
  midi_buffer_.resize(kCoreMIDIMaxPacketListSize);

  CompleteInitialization(Result::OK);
}

// static
void MidiManagerMac::ReceiveMidiNotifyDispatch(const MIDINotification* message,
                                               void* refcon) {
  // This callback function is invoked on |client_thread_|.
  MidiManagerMac* manager = static_cast<MidiManagerMac*>(refcon);
  manager->ReceiveMidiNotify(message);
}

void MidiManagerMac::ReceiveMidiNotify(const MIDINotification* message) {
  DCHECK(client_thread_.task_runner()->BelongsToCurrentThread());

  if (kMIDIMsgObjectAdded == message->messageID) {
    // New device is going to be attached.
    const MIDIObjectAddRemoveNotification* notification =
        reinterpret_cast<const MIDIObjectAddRemoveNotification*>(message);
    MIDIEndpointRef endpoint =
        static_cast<MIDIEndpointRef>(notification->child);
    if (notification->childType == kMIDIObjectType_Source) {
      // Attaching device is an input device.
      auto it = source_map_.find(endpoint);
      if (it == source_map_.end()) {
        MidiPortInfo info = GetPortInfoFromEndpoint(endpoint);
        // If the device disappears before finishing queries, MidiPortInfo
        // becomes incomplete. Skip and do not cache such information here.
        // On kMIDIMsgObjectRemoved, the entry will be ignored because it
        // will not be found in the pool.
        if (!info.id.empty()) {
          uint32_t index = source_map_.size();
          source_map_[endpoint] = index;
          AddInputPort(info);
          MIDIPortConnectSource(
              coremidi_input_, endpoint, reinterpret_cast<void*>(endpoint));
        }
      } else {
        SetInputPortState(it->second, MIDI_PORT_OPENED);
      }
    } else if (notification->childType == kMIDIObjectType_Destination) {
      // Attaching device is an output device.
      auto it = std::find(destinations_.begin(), destinations_.end(), endpoint);
      if (it == destinations_.end()) {
        MidiPortInfo info = GetPortInfoFromEndpoint(endpoint);
        // Skip cases that queries are not finished correctly.
        if (!info.id.empty()) {
          destinations_.push_back(endpoint);
          AddOutputPort(info);
        }
      } else {
        SetOutputPortState(it - destinations_.begin(), MIDI_PORT_OPENED);
      }
    }
  } else if (kMIDIMsgObjectRemoved == message->messageID) {
    // Existing device is going to be detached.
    const MIDIObjectAddRemoveNotification* notification =
        reinterpret_cast<const MIDIObjectAddRemoveNotification*>(message);
    MIDIEndpointRef endpoint =
        static_cast<MIDIEndpointRef>(notification->child);
    if (notification->childType == kMIDIObjectType_Source) {
      // Detaching device is an input device.
      auto it = source_map_.find(endpoint);
      if (it != source_map_.end())
        SetInputPortState(it->second, MIDI_PORT_DISCONNECTED);
    } else if (notification->childType == kMIDIObjectType_Destination) {
      // Detaching device is an output device.
      auto it = std::find(destinations_.begin(), destinations_.end(), endpoint);
      if (it != destinations_.end())
        SetOutputPortState(it - destinations_.begin(), MIDI_PORT_DISCONNECTED);
    }
  }
}

// static
void MidiManagerMac::ReadMidiDispatch(const MIDIPacketList* packet_list,
                                      void* read_proc_refcon,
                                      void* src_conn_refcon) {
  // This method is called on a separate high-priority thread owned by CoreMIDI.

  MidiManagerMac* manager = static_cast<MidiManagerMac*>(read_proc_refcon);
#if __LP64__
  MIDIEndpointRef source = reinterpret_cast<uintptr_t>(src_conn_refcon);
#else
  MIDIEndpointRef source = static_cast<MIDIEndpointRef>(src_conn_refcon);
#endif

  // Dispatch to class method.
  manager->ReadMidi(source, packet_list);
}

void MidiManagerMac::ReadMidi(MIDIEndpointRef source,
                              const MIDIPacketList* packet_list) {
  // This method is called from ReadMidiDispatch() and runs on a separate
  // high-priority thread owned by CoreMIDI.

  // Lookup the port index based on the source.
  auto it = source_map_.find(source);
  if (it == source_map_.end())
    return;
  // This is safe since MidiManagerMac does not remove any existing
  // MIDIEndpointRef, and the order is saved.
  uint32_t port_index = it->second;

  // Go through each packet and process separately.
  const MIDIPacket* packet = &packet_list->packet[0];
  for (size_t i = 0; i < packet_list->numPackets; i++) {
    // Each packet contains MIDI data for one or more messages (like note-on).
    double timestamp_seconds = MIDITimeStampToSeconds(packet->timeStamp);

    ReceiveMidiData(
        port_index,
        packet->data,
        packet->length,
        timestamp_seconds);

    packet = MIDIPacketNext(packet);
  }
}

void MidiManagerMac::SendMidiData(MidiManagerClient* client,
                                  uint32_t port_index,
                                  const std::vector<uint8_t>& data,
                                  double timestamp) {
  DCHECK(client_thread_.task_runner()->BelongsToCurrentThread());

  // Lookup the destination based on the port index.
  if (static_cast<size_t>(port_index) >= destinations_.size())
    return;

  MIDITimeStamp coremidi_timestamp = SecondsToMIDITimeStamp(timestamp);
  MIDIEndpointRef destination = destinations_[port_index];

  size_t send_size;
  for (size_t sent_size = 0; sent_size < data.size(); sent_size += send_size) {
    MIDIPacketList* packet_list =
        reinterpret_cast<MIDIPacketList*>(midi_buffer_.data());
    MIDIPacket* midi_packet = MIDIPacketListInit(packet_list);
    // Limit the maximum payload size to kEstimatedMaxPacketDataSize that is
    // half of midi_buffer data size. MIDIPacketList and MIDIPacket consume
    // extra buffer areas for meta information, and available size is smaller
    // than buffer size. Here, we simply assume that at least half size is
    // available for data payload.
    send_size = std::min(data.size() - sent_size, kEstimatedMaxPacketDataSize);
    midi_packet = MIDIPacketListAdd(
        packet_list,
        kCoreMIDIMaxPacketListSize,
        midi_packet,
        coremidi_timestamp,
        send_size,
        &data[sent_size]);
    DCHECK(midi_packet);

    MIDISend(coremidi_output_, destination, packet_list);
  }

  AccumulateMidiBytesSent(client, data.size());
}

}  // namespace midi
}  // namespace media
