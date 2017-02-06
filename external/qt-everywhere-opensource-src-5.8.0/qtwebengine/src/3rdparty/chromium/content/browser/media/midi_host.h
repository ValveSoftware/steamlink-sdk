// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MIDI_HOST_H_
#define CONTENT_BROWSER_MEDIA_MIDI_HOST_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "media/midi/midi_manager.h"
#include "media/midi/midi_port_info.h"

namespace media {
namespace midi {
class MidiManager;
class MidiMessageQueue;
}
}

namespace content {

class CONTENT_EXPORT MidiHost : public BrowserMessageFilter,
                                public media::midi::MidiManagerClient {
 public:
  // Called from UI thread from the owner of this object.
  MidiHost(int renderer_process_id, media::midi::MidiManager* midi_manager);

  // BrowserMessageFilter implementation.
  void OnChannelClosing() override;
  void OnDestruct() const override;
  bool OnMessageReceived(const IPC::Message& message) override;

  // MidiManagerClient implementation.
  void CompleteStartSession(media::midi::Result result) override;
  void AddInputPort(const media::midi::MidiPortInfo& info) override;
  void AddOutputPort(const media::midi::MidiPortInfo& info) override;
  void SetInputPortState(uint32_t port,
                         media::midi::MidiPortState state) override;
  void SetOutputPortState(uint32_t port,
                          media::midi::MidiPortState state) override;
  void ReceiveMidiData(uint32_t port,
                       const uint8_t* data,
                       size_t length,
                       double timestamp) override;
  void AccumulateMidiBytesSent(size_t n) override;
  void Detach() override;

  // Start session to access MIDI hardware.
  void OnStartSession();

  // Data to be sent to a MIDI output port.
  void OnSendData(uint32_t port,
                  const std::vector<uint8_t>& data,
                  double timestamp);

  void OnEndSession();

 protected:
  ~MidiHost() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(MidiHostTest, IsValidWebMIDIData);
  friend class base::DeleteHelper<MidiHost>;
  friend class BrowserThread;

  // Returns true if |data| fulfills the requirements of MidiOutput.send API
  // defined in the Web MIDI spec.
  // - |data| must be any number of complete MIDI messages (data abbreviation
  //    called "running status" is disallowed).
  // - 1-byte MIDI realtime messages can be placed at any position of |data|.
  static bool IsValidWebMIDIData(const std::vector<uint8_t>& data);

  int renderer_process_id_;

  // Represents if the renderer has a permission to send/receive MIDI SysEX
  // messages.
  bool has_sys_ex_permission_;

  // Represents if a session is requested to start.
  bool is_session_requested_;

  // |midi_manager_| talks to the platform-specific MIDI APIs.
  // It can be NULL if the platform (or our current implementation)
  // does not support MIDI.  If not supported then a call to
  // OnRequestAccess() will always refuse access and a call to
  // OnSendData() will do nothing.
  media::midi::MidiManager* midi_manager_;

  // Buffers where data sent from each MIDI input port is stored.
  ScopedVector<media::midi::MidiMessageQueue> received_messages_queues_;

  // Protects access to |received_messages_queues_|;
  base::Lock messages_queues_lock_;

  // The number of bytes sent to the platform-specific MIDI sending
  // system, but not yet completed.
  size_t sent_bytes_in_flight_;

  // The number of bytes successfully sent since the last time
  // we've acknowledged back to the renderer.
  size_t bytes_sent_since_last_acknowledgement_;

  // Protects access to |sent_bytes_in_flight_|.
  base::Lock in_flight_lock_;

  // How many output port exists.
  uint32_t output_port_count_;

  // Protects access to |output_port_count_|.
  base::Lock output_port_count_lock_;

  DISALLOW_COPY_AND_ASSIGN(MidiHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MIDI_HOST_H_
