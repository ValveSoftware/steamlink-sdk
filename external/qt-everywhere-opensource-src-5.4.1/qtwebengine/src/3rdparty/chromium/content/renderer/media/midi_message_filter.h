// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MIDI_MESSAGE_FILTER_H_
#define CONTENT_RENDERER_MEDIA_MIDI_MESSAGE_FILTER_H_

#include <map>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "ipc/message_filter.h"
#include "media/midi/midi_port_info.h"
#include "media/midi/midi_result.h"
#include "third_party/WebKit/public/platform/WebMIDIAccessorClient.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

// MessageFilter that handles MIDI messages.
class CONTENT_EXPORT MidiMessageFilter : public IPC::MessageFilter {
 public:
  explicit MidiMessageFilter(
      const scoped_refptr<base::MessageLoopProxy>& io_message_loop);

  // Each client registers for MIDI access here.
  // If permission is granted, then the client's
  // addInputPort() and addOutputPort() methods will be called,
  // giving the client access to receive and send data.
  void StartSession(blink::WebMIDIAccessorClient* client);
  void RemoveClient(blink::WebMIDIAccessorClient* client);

  // A client will only be able to call this method if it has a suitable
  // output port (from addOutputPort()).
  void SendMidiData(uint32 port,
                    const uint8* data,
                    size_t length,
                    double timestamp);

  // IO message loop associated with this message filter.
  scoped_refptr<base::MessageLoopProxy> io_message_loop() const {
    return io_message_loop_;
  }

 protected:
  virtual ~MidiMessageFilter();

 private:
  // Sends an IPC message using |sender_|.
  void Send(IPC::Message* message);

  // IPC::MessageFilter override. Called on |io_message_loop|.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnFilterAdded(IPC::Sender* sender) OVERRIDE;
  virtual void OnFilterRemoved() OVERRIDE;
  virtual void OnChannelClosing() OVERRIDE;

  // Called when the browser process has approved (or denied) access to
  // MIDI hardware.
  void OnSessionStarted(int client_id,
                        media::MidiResult result,
                        media::MidiPortInfoList inputs,
                        media::MidiPortInfoList outputs);

  // Called when the browser process has sent MIDI data containing one or
  // more messages.
  void OnDataReceived(uint32 port,
                      const std::vector<uint8>& data,
                      double timestamp);

  // From time-to-time, the browser incrementally informs us of how many bytes
  // it has successfully sent. This is part of our throttling process to avoid
  // sending too much data before knowing how much has already been sent.
  void OnAcknowledgeSentData(size_t bytes_sent);

  void HandleSessionStarted(int client_id,
                            media::MidiResult result,
                            media::MidiPortInfoList inputs,
                            media::MidiPortInfoList outputs);

  void HandleDataReceived(uint32 port,
                          const std::vector<uint8>& data,
                          double timestamp);

  void StartSessionOnIOThread(int client_id);

  void SendMidiDataOnIOThread(uint32 port,
                              const std::vector<uint8>& data,
                              double timestamp);

  blink::WebMIDIAccessorClient* GetClientFromId(int client_id);

  // IPC sender for Send(); must only be accessed on |io_message_loop_|.
  IPC::Sender* sender_;

  // Message loop on which IPC calls are driven.
  const scoped_refptr<base::MessageLoopProxy> io_message_loop_;

  // Main thread's message loop.
  scoped_refptr<base::MessageLoopProxy> main_message_loop_;

  // Keeps track of all MIDI clients.
  // We map client to "client id" used to track permission.
  // When access has been approved, we add the input and output ports to
  // the client, allowing it to actually receive and send MIDI data.
  typedef std::map<blink::WebMIDIAccessorClient*, int> ClientsMap;
  ClientsMap clients_;

  // Dishes out client ids.
  int next_available_id_;

  size_t unacknowledged_bytes_sent_;

  DISALLOW_COPY_AND_ASSIGN(MidiMessageFilter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MIDI_MESSAGE_FILTER_H_
