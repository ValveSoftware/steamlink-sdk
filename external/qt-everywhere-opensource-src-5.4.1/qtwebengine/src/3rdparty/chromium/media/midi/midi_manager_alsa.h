// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_MANAGER_ALSA_H_
#define MEDIA_MIDI_MIDI_MANAGER_ALSA_H_

#include <alsa/asoundlib.h>
#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "media/midi/midi_manager.h"

namespace media {

class MidiManagerAlsa : public MidiManager {
 public:
  MidiManagerAlsa();
  virtual ~MidiManagerAlsa();

  // MidiManager implementation.
  virtual void StartInitialization() OVERRIDE;
  virtual void DispatchSendMidiData(MidiManagerClient* client,
                                    uint32 port_index,
                                    const std::vector<uint8>& data,
                                    double timestamp) OVERRIDE;

 private:
  // An internal callback that runs on MidiSendThread.
  void SendMidiData(uint32 port_index,
                    const std::vector<uint8>& data);

  void EventReset();
  void EventLoop();

  // Alsa seq handles.
  snd_seq_t* in_client_;
  snd_seq_t* out_client_;
  int out_client_id_;

  // One input port, many output ports.
  int in_port_;
  std::vector<int> out_ports_;

  // Mapping from Alsa client:port to our index.
  typedef std::map<int, uint32> SourceMap;
  SourceMap source_map_;

  // Alsa event <-> MIDI coders.
  snd_midi_event_t* decoder_;
  typedef std::vector<snd_midi_event_t*> EncoderList;
  EncoderList encoders_;

  base::Thread send_thread_;
  base::Thread event_thread_;

  bool event_thread_shutdown_; // guarded by shutdown_lock_
  base::Lock shutdown_lock_; // guards event_thread_shutdown_

  DISALLOW_COPY_AND_ASSIGN(MidiManagerAlsa);
};

}  // namespace media

#endif  // MEDIA_MIDI_MIDI_MANAGER_ALSA_H_
