// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_MANAGER_MAC_H_
#define MEDIA_MIDI_MIDI_MANAGER_MAC_H_

#include <CoreMIDI/MIDIServices.h>
#include <stdint.h>
#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/threading/thread.h"
#include "media/midi/midi_export.h"
#include "media/midi/midi_manager.h"
#include "media/midi/midi_port_info.h"

namespace media {
namespace midi {

class MIDI_EXPORT MidiManagerMac final : public MidiManager {
 public:
  MidiManagerMac();
  ~MidiManagerMac() override;

  // MidiManager implementation.
  void StartInitialization() override;
  void Finalize() override;
  void DispatchSendMidiData(MidiManagerClient* client,
                            uint32_t port_index,
                            const std::vector<uint8_t>& data,
                            double timestamp) override;

 private:
  // Runs a closure on |client_thread_|. It starts the thread if it isn't
  // running and the destructor isn't called.
  // Caller can bind base::Unretained(this) to |closure| since we join
  // |client_thread_| in the destructor.
  void RunOnClientThread(const base::Closure& closure);

  // Initializes CoreMIDI on |client_thread_| asynchronously. Called from
  // StartInitialization().
  void InitializeCoreMIDI();

  // CoreMIDI callback for MIDI notification.
  // Receives MIDI related event notifications from CoreMIDI.
  static void ReceiveMidiNotifyDispatch(const MIDINotification* message,
                                        void* refcon);
  void ReceiveMidiNotify(const MIDINotification* message);

  // CoreMIDI callback for MIDI data.
  // Each callback can contain multiple packets, each of which can contain
  // multiple MIDI messages.
  static void ReadMidiDispatch(const MIDIPacketList* packet_list,
                               void* read_proc_refcon,
                               void* src_conn_refcon);
  virtual void ReadMidi(MIDIEndpointRef source, const MIDIPacketList *pktlist);

  // An internal callback that runs on MidiSendThread.
  void SendMidiData(MidiManagerClient* client,
                    uint32_t port_index,
                    const std::vector<uint8_t>& data,
                    double timestamp);

  // CoreMIDI
  MIDIClientRef midi_client_;
  MIDIPortRef coremidi_input_;
  MIDIPortRef coremidi_output_;
  std::vector<uint8_t> midi_buffer_;

  // Keeps track of the index (0-based) for each of our sources.
  typedef std::map<MIDIEndpointRef, uint32_t> SourceMap;
  SourceMap source_map_;

  // Keeps track of all destinations.
  typedef std::vector<MIDIEndpointRef> DestinationVector;
  DestinationVector destinations_;

  // |client_thread_| is used to handle platform dependent operations.
  base::Thread client_thread_;

  // Sets true on destructing object to avoid starting |client_thread_| again.
  bool shutdown_;

  DISALLOW_COPY_AND_ASSIGN(MidiManagerMac);
};

}  // namespace midi
}  // namespace media

#endif  // MEDIA_MIDI_MIDI_MANAGER_MAC_H_
