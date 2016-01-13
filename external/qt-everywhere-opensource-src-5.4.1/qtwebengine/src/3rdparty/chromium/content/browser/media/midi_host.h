// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MIDI_HOST_H_
#define CONTENT_BROWSER_MEDIA_MIDI_HOST_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "media/midi/midi_manager.h"

namespace media {
class MidiManager;
class MidiMessageQueue;
}

namespace content {

class CONTENT_EXPORT MidiHost
    : public BrowserMessageFilter,
      public media::MidiManagerClient {
 public:
  // Called from UI thread from the owner of this object.
  MidiHost(int renderer_process_id, media::MidiManager* midi_manager);

  // BrowserMessageFilter implementation.
  virtual void OnDestruct() const OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // MidiManagerClient implementation.
  virtual void CompleteStartSession(int client_id,
                                    media::MidiResult result) OVERRIDE;
  virtual void ReceiveMidiData(uint32 port,
                               const uint8* data,
                               size_t length,
                               double timestamp) OVERRIDE;
  virtual void AccumulateMidiBytesSent(size_t n) OVERRIDE;

  // Start session to access MIDI hardware.
  void OnStartSession(int client_id);

  // Data to be sent to a MIDI output port.
  void OnSendData(uint32 port,
                  const std::vector<uint8>& data,
                  double timestamp);

 private:
  FRIEND_TEST_ALL_PREFIXES(MidiHostTest, IsValidWebMIDIData);
  friend class base::DeleteHelper<MidiHost>;
  friend class BrowserThread;

  virtual ~MidiHost();

  // Returns true if |data| fulfills the requirements of MidiOutput.send API
  // defined in the WebMIDI spec.
  // - |data| must be any number of complete MIDI messages (data abbreviation
  //    called "running status" is disallowed).
  // - 1-byte MIDI realtime messages can be placed at any position of |data|.
  static bool IsValidWebMIDIData(const std::vector<uint8>& data);

  int renderer_process_id_;

  // Represents if the renderer has a permission to send/receive MIDI SysEX
  // messages.
  bool has_sys_ex_permission_;

  // |midi_manager_| talks to the platform-specific MIDI APIs.
  // It can be NULL if the platform (or our current implementation)
  // does not support MIDI.  If not supported then a call to
  // OnRequestAccess() will always refuse access and a call to
  // OnSendData() will do nothing.
  media::MidiManager* const midi_manager_;

  // Buffers where data sent from each MIDI input port is stored.
  ScopedVector<media::MidiMessageQueue> received_messages_queues_;

  // The number of bytes sent to the platform-specific MIDI sending
  // system, but not yet completed.
  size_t sent_bytes_in_flight_;

  // The number of bytes successfully sent since the last time
  // we've acknowledged back to the renderer.
  size_t bytes_sent_since_last_acknowledgement_;

  // Protects access to |sent_bytes_in_flight_|.
  base::Lock in_flight_lock_;

  DISALLOW_COPY_AND_ASSIGN(MidiHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MIDI_HOST_H_
