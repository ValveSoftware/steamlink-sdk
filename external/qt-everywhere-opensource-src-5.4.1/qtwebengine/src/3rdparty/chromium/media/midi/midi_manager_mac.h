// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_MANAGER_MAC_H_
#define MEDIA_MIDI_MIDI_MANAGER_MAC_H_

#include <CoreMIDI/MIDIServices.h>
#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/threading/thread.h"
#include "media/midi/midi_manager.h"
#include "media/midi/midi_port_info.h"

namespace media {

class MEDIA_EXPORT MidiManagerMac : public MidiManager {
 public:
  MidiManagerMac();
  virtual ~MidiManagerMac();

  // MidiManager implementation.
  virtual void StartInitialization() OVERRIDE;
  virtual void DispatchSendMidiData(MidiManagerClient* client,
                                    uint32 port_index,
                                    const std::vector<uint8>& data,
                                    double timestamp) OVERRIDE;

 private:
  // CoreMIDI callback for MIDI data.
  // Each callback can contain multiple packets, each of which can contain
  // multiple MIDI messages.
  static void ReadMidiDispatch(
      const MIDIPacketList *pktlist,
      void *read_proc_refcon,
      void *src_conn_refcon);
  virtual void ReadMidi(MIDIEndpointRef source, const MIDIPacketList *pktlist);

  // An internal callback that runs on MidiSendThread.
  void SendMidiData(MidiManagerClient* client,
                    uint32 port_index,
                    const std::vector<uint8>& data,
                    double timestamp);

  // Helper
  static media::MidiPortInfo GetPortInfoFromEndpoint(MIDIEndpointRef endpoint);
  static double MIDITimeStampToSeconds(MIDITimeStamp timestamp);
  static MIDITimeStamp SecondsToMIDITimeStamp(double seconds);

  // CoreMIDI
  MIDIClientRef midi_client_;
  MIDIPortRef coremidi_input_;
  MIDIPortRef coremidi_output_;

  enum{ kMaxPacketListSize = 512 };
  char midi_buffer_[kMaxPacketListSize];
  MIDIPacketList* packet_list_;
  MIDIPacket* midi_packet_;

  typedef std::map<MIDIEndpointRef, uint32> SourceMap;

  // Keeps track of the index (0-based) for each of our sources.
  SourceMap source_map_;

  // Keeps track of all destinations.
  std::vector<MIDIEndpointRef> destinations_;

  // |send_thread_| is used to send MIDI data.
  base::Thread send_thread_;

  DISALLOW_COPY_AND_ASSIGN(MidiManagerMac);
};

}  // namespace media

#endif  // MEDIA_MIDI_MIDI_MANAGER_MAC_H_
