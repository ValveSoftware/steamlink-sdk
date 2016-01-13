// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/midi_message_filter.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/media/midi_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "ipc/ipc_logging.h"

using media::MidiPortInfoList;
using base::AutoLock;

// The maximum number of bytes which we're allowed to send to the browser
// before getting acknowledgement back from the browser that they've been
// successfully sent.
static const size_t kMaxUnacknowledgedBytesSent = 10 * 1024 * 1024;  // 10 MB.

namespace content {

MidiMessageFilter::MidiMessageFilter(
    const scoped_refptr<base::MessageLoopProxy>& io_message_loop)
    : sender_(NULL),
      io_message_loop_(io_message_loop),
      main_message_loop_(base::MessageLoopProxy::current()),
      next_available_id_(0),
      unacknowledged_bytes_sent_(0) {
}

MidiMessageFilter::~MidiMessageFilter() {}

void MidiMessageFilter::Send(IPC::Message* message) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  if (!sender_) {
    delete message;
  } else {
    sender_->Send(message);
  }
}

bool MidiMessageFilter::OnMessageReceived(const IPC::Message& message) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MidiMessageFilter, message)
    IPC_MESSAGE_HANDLER(MidiMsg_SessionStarted, OnSessionStarted)
    IPC_MESSAGE_HANDLER(MidiMsg_DataReceived, OnDataReceived)
    IPC_MESSAGE_HANDLER(MidiMsg_AcknowledgeSentData, OnAcknowledgeSentData)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MidiMessageFilter::OnFilterAdded(IPC::Sender* sender) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  sender_ = sender;
}

void MidiMessageFilter::OnFilterRemoved() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());

  // Once removed, a filter will not be used again.  At this time all
  // delegates must be notified so they release their reference.
  OnChannelClosing();
}

void MidiMessageFilter::OnChannelClosing() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  sender_ = NULL;
}

void MidiMessageFilter::StartSession(blink::WebMIDIAccessorClient* client) {
  // Generate and keep track of a "client id" which is sent to the browser
  // to ask permission to talk to MIDI hardware.
  // This id is handed back when we receive the answer in OnAccessApproved().
  if (clients_.find(client) == clients_.end()) {
    int client_id = next_available_id_++;
    clients_[client] = client_id;

    io_message_loop_->PostTask(FROM_HERE,
        base::Bind(&MidiMessageFilter::StartSessionOnIOThread, this,
                   client_id));
  }
}

void MidiMessageFilter::StartSessionOnIOThread(int client_id) {
  Send(new MidiHostMsg_StartSession(client_id));
}

void MidiMessageFilter::RemoveClient(blink::WebMIDIAccessorClient* client) {
  ClientsMap::iterator i = clients_.find(client);
  if (i != clients_.end())
    clients_.erase(i);
}

// Received from browser.

void MidiMessageFilter::OnSessionStarted(
    int client_id,
    media::MidiResult result,
    MidiPortInfoList inputs,
    MidiPortInfoList outputs) {
  // Handle on the main JS thread.
  main_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&MidiMessageFilter::HandleSessionStarted, this,
                 client_id, result, inputs, outputs));
}

void MidiMessageFilter::HandleSessionStarted(
    int client_id,
    media::MidiResult result,
    MidiPortInfoList inputs,
    MidiPortInfoList outputs) {
  blink::WebMIDIAccessorClient* client = GetClientFromId(client_id);
  if (!client)
    return;

  if (result == media::MIDI_OK) {
    // Add the client's input and output ports.
    for (size_t i = 0; i < inputs.size(); ++i) {
      client->didAddInputPort(
          base::UTF8ToUTF16(inputs[i].id),
          base::UTF8ToUTF16(inputs[i].manufacturer),
          base::UTF8ToUTF16(inputs[i].name),
          base::UTF8ToUTF16(inputs[i].version));
    }

    for (size_t i = 0; i < outputs.size(); ++i) {
      client->didAddOutputPort(
          base::UTF8ToUTF16(outputs[i].id),
          base::UTF8ToUTF16(outputs[i].manufacturer),
          base::UTF8ToUTF16(outputs[i].name),
          base::UTF8ToUTF16(outputs[i].version));
    }
  }
  std::string error;
  std::string message;
  switch (result) {
    case media::MIDI_OK:
      break;
    case media::MIDI_NOT_SUPPORTED:
      error = "NotSupportedError";
      break;
    case media::MIDI_INITIALIZATION_ERROR:
      error = "InvalidStateError";
      message = "Platform dependent initialization failed.";
      break;
    default:
      NOTREACHED();
      error = "InvalidStateError";
      message = "Unknown internal error occurred.";
      break;
  }
  client->didStartSession(result == media::MIDI_OK, base::UTF8ToUTF16(error),
                          base::UTF8ToUTF16(message));
}

blink::WebMIDIAccessorClient*
MidiMessageFilter::GetClientFromId(int client_id) {
  // Iterating like this seems inefficient, but in practice there generally
  // will be very few clients (usually one).  Additionally, this lookup
  // usually happens one time during page load. So the performance hit is
  // negligible.
  for (ClientsMap::iterator i = clients_.begin(); i != clients_.end(); ++i) {
    if ((*i).second == client_id)
      return (*i).first;
  }
  return NULL;
}

void MidiMessageFilter::OnDataReceived(uint32 port,
                                       const std::vector<uint8>& data,
                                       double timestamp) {
  TRACE_EVENT0("midi", "MidiMessageFilter::OnDataReceived");

  main_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&MidiMessageFilter::HandleDataReceived, this,
                 port, data, timestamp));
}

void MidiMessageFilter::OnAcknowledgeSentData(size_t bytes_sent) {
  DCHECK_GE(unacknowledged_bytes_sent_, bytes_sent);
  if (unacknowledged_bytes_sent_ >= bytes_sent)
    unacknowledged_bytes_sent_ -= bytes_sent;
}

void MidiMessageFilter::HandleDataReceived(uint32 port,
                                           const std::vector<uint8>& data,
                                           double timestamp) {
  DCHECK(!data.empty());
  TRACE_EVENT0("midi", "MidiMessageFilter::HandleDataReceived");

  for (ClientsMap::iterator i = clients_.begin(); i != clients_.end(); ++i)
    (*i).first->didReceiveMIDIData(port, &data[0], data.size(), timestamp);
}

void MidiMessageFilter::SendMidiData(uint32 port,
                                     const uint8* data,
                                     size_t length,
                                     double timestamp) {
  if (length > kMaxUnacknowledgedBytesSent) {
    // TODO(toyoshim): buffer up the data to send at a later time.
    // For now we're just dropping these bytes on the floor.
    return;
  }

  std::vector<uint8> v(data, data + length);
  io_message_loop_->PostTask(FROM_HERE,
      base::Bind(&MidiMessageFilter::SendMidiDataOnIOThread, this,
                 port, v, timestamp));
}

void MidiMessageFilter::SendMidiDataOnIOThread(uint32 port,
                                               const std::vector<uint8>& data,
                                               double timestamp) {
  size_t n = data.size();
  if (n > kMaxUnacknowledgedBytesSent ||
      unacknowledged_bytes_sent_ > kMaxUnacknowledgedBytesSent ||
      n + unacknowledged_bytes_sent_ > kMaxUnacknowledgedBytesSent) {
    // TODO(toyoshim): buffer up the data to send at a later time.
    // For now we're just dropping these bytes on the floor.
    return;
  }

  unacknowledged_bytes_sent_ += n;

  // Send to the browser.
  Send(new MidiHostMsg_SendData(port, data, timestamp));
}

}  // namespace content
