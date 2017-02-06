// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/midi_message_filter.h"

#include <algorithm>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "content/common/media/midi_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "ipc/ipc_logging.h"

using base::AutoLock;

// The maximum number of bytes which we're allowed to send to the browser
// before getting acknowledgement back from the browser that they've been
// successfully sent.
static const size_t kMaxUnacknowledgedBytesSent = 10 * 1024 * 1024;  // 10 MB.

namespace content {

MidiMessageFilter::MidiMessageFilter(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : sender_(nullptr),
      io_task_runner_(io_task_runner),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      session_result_(media::midi::Result::NOT_INITIALIZED),
      unacknowledged_bytes_sent_(0u) {
}

MidiMessageFilter::~MidiMessageFilter() {}

void MidiMessageFilter::AddClient(blink::WebMIDIAccessorClient* client) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("midi", "MidiMessageFilter::AddClient");
  clients_waiting_session_queue_.push_back(client);
  if (session_result_ != media::midi::Result::NOT_INITIALIZED) {
    HandleClientAdded(session_result_);
  } else if (clients_waiting_session_queue_.size() == 1u) {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&MidiMessageFilter::StartSessionOnIOThread, this));
  }
}

void MidiMessageFilter::RemoveClient(blink::WebMIDIAccessorClient* client) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  clients_.erase(client);
  ClientsQueue::iterator it = std::find(clients_waiting_session_queue_.begin(),
                                        clients_waiting_session_queue_.end(),
                                        client);
  if (it != clients_waiting_session_queue_.end())
    clients_waiting_session_queue_.erase(it);
  if (clients_.empty() && clients_waiting_session_queue_.empty()) {
    session_result_ = media::midi::Result::NOT_INITIALIZED;
    inputs_.clear();
    outputs_.clear();
    io_task_runner_->PostTask(
        FROM_HERE, base::Bind(&MidiMessageFilter::EndSessionOnIOThread, this));
  }
}

void MidiMessageFilter::SendMidiData(uint32_t port,
                                     const uint8_t* data,
                                     size_t length,
                                     double timestamp) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if ((kMaxUnacknowledgedBytesSent - unacknowledged_bytes_sent_) < length) {
    // TODO(toyoshim): buffer up the data to send at a later time.
    // For now we're just dropping these bytes on the floor.
    return;
  }

  unacknowledged_bytes_sent_ += length;
  std::vector<uint8_t> v(data, data + length);
  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&MidiMessageFilter::SendMidiDataOnIOThread, this,
                            port, v, timestamp));
}

void MidiMessageFilter::StartSessionOnIOThread() {
  TRACE_EVENT0("midi", "MidiMessageFilter::StartSessionOnIOThread");
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  Send(new MidiHostMsg_StartSession());
}

void MidiMessageFilter::SendMidiDataOnIOThread(uint32_t port,
                                               const std::vector<uint8_t>& data,
                                               double timestamp) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  Send(new MidiHostMsg_SendData(port, data, timestamp));
}

void MidiMessageFilter::EndSessionOnIOThread() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  Send(new MidiHostMsg_EndSession());
}

void MidiMessageFilter::Send(IPC::Message* message) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (!sender_) {
    delete message;
  } else {
    sender_->Send(message);
  }
}

bool MidiMessageFilter::OnMessageReceived(const IPC::Message& message) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MidiMessageFilter, message)
    IPC_MESSAGE_HANDLER(MidiMsg_SessionStarted, OnSessionStarted)
    IPC_MESSAGE_HANDLER(MidiMsg_AddInputPort, OnAddInputPort)
    IPC_MESSAGE_HANDLER(MidiMsg_AddOutputPort, OnAddOutputPort)
    IPC_MESSAGE_HANDLER(MidiMsg_SetInputPortState, OnSetInputPortState)
    IPC_MESSAGE_HANDLER(MidiMsg_SetOutputPortState, OnSetOutputPortState)
    IPC_MESSAGE_HANDLER(MidiMsg_DataReceived, OnDataReceived)
    IPC_MESSAGE_HANDLER(MidiMsg_AcknowledgeSentData, OnAcknowledgeSentData)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MidiMessageFilter::OnFilterAdded(IPC::Sender* sender) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  sender_ = sender;
}

void MidiMessageFilter::OnFilterRemoved() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  // Once removed, a filter will not be used again.  At this time all
  // delegates must be notified so they release their reference.
  OnChannelClosing();
}

void MidiMessageFilter::OnChannelClosing() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  sender_ = nullptr;
}

void MidiMessageFilter::OnSessionStarted(media::midi::Result result) {
  TRACE_EVENT0("midi", "MidiMessageFilter::OnSessionStarted");
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  // Handle on the main JS thread.
  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&MidiMessageFilter::HandleClientAdded, this, result));
}

void MidiMessageFilter::OnAddInputPort(media::midi::MidiPortInfo info) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&MidiMessageFilter::HandleAddInputPort, this, info));
}

void MidiMessageFilter::OnAddOutputPort(media::midi::MidiPortInfo info) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&MidiMessageFilter::HandleAddOutputPort, this, info));
}

void MidiMessageFilter::OnSetInputPortState(uint32_t port,
                                            media::midi::MidiPortState state) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&MidiMessageFilter::HandleSetInputPortState, this,
                            port, state));
}

void MidiMessageFilter::OnSetOutputPortState(uint32_t port,
                                             media::midi::MidiPortState state) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&MidiMessageFilter::HandleSetOutputPortState, this,
                            port, state));
}

void MidiMessageFilter::OnDataReceived(uint32_t port,
                                       const std::vector<uint8_t>& data,
                                       double timestamp) {
  TRACE_EVENT0("midi", "MidiMessageFilter::OnDataReceived");
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  // Handle on the main JS thread.
  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&MidiMessageFilter::HandleDataReceived, this, port,
                            data, timestamp));
}

void MidiMessageFilter::OnAcknowledgeSentData(size_t bytes_sent) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&MidiMessageFilter::HandleAckknowledgeSentData,
                            this, bytes_sent));
}

void MidiMessageFilter::HandleClientAdded(media::midi::Result result) {
  TRACE_EVENT0("midi", "MidiMessageFilter::HandleClientAdded");
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  session_result_ = result;
  std::string error;
  std::string message;
  switch (result) {
    case media::midi::Result::OK:
      break;
    case media::midi::Result::NOT_SUPPORTED:
      error = "NotSupportedError";
      break;
    case media::midi::Result::INITIALIZATION_ERROR:
      error = "InvalidStateError";
      message = "Platform dependent initialization failed.";
      break;
    default:
      NOTREACHED();
      error = "InvalidStateError";
      message = "Unknown internal error occurred.";
      break;
  }
  base::string16 error16 = base::UTF8ToUTF16(error);
  base::string16 message16 = base::UTF8ToUTF16(message);

  // A for-loop using iterators does not work because |client| may touch
  // |clients_waiting_session_queue_| in callbacks.
  while (!clients_waiting_session_queue_.empty()) {
    auto client = clients_waiting_session_queue_.back();
    clients_waiting_session_queue_.pop_back();
    if (result == media::midi::Result::OK) {
      // Add the client's input and output ports.
      for (const auto& info : inputs_) {
        client->didAddInputPort(
            base::UTF8ToUTF16(info.id),
            base::UTF8ToUTF16(info.manufacturer),
            base::UTF8ToUTF16(info.name),
            base::UTF8ToUTF16(info.version),
            ToBlinkState(info.state));
      }

      for (const auto& info : outputs_) {
        client->didAddOutputPort(
            base::UTF8ToUTF16(info.id),
            base::UTF8ToUTF16(info.manufacturer),
            base::UTF8ToUTF16(info.name),
            base::UTF8ToUTF16(info.version),
            ToBlinkState(info.state));
      }
    }
    client->didStartSession(result == media::midi::Result::OK, error16,
                            message16);
    clients_.insert(client);
  }
}

void MidiMessageFilter::HandleAddInputPort(media::midi::MidiPortInfo info) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  inputs_.push_back(info);
  const base::string16 id = base::UTF8ToUTF16(info.id);
  const base::string16 manufacturer = base::UTF8ToUTF16(info.manufacturer);
  const base::string16 name = base::UTF8ToUTF16(info.name);
  const base::string16 version = base::UTF8ToUTF16(info.version);
  const blink::WebMIDIAccessorClient::MIDIPortState state =
      ToBlinkState(info.state);
  for (auto client : clients_)
    client->didAddInputPort(id, manufacturer, name, version, state);
}

void MidiMessageFilter::HandleAddOutputPort(media::midi::MidiPortInfo info) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  outputs_.push_back(info);
  const base::string16 id = base::UTF8ToUTF16(info.id);
  const base::string16 manufacturer = base::UTF8ToUTF16(info.manufacturer);
  const base::string16 name = base::UTF8ToUTF16(info.name);
  const base::string16 version = base::UTF8ToUTF16(info.version);
  const blink::WebMIDIAccessorClient::MIDIPortState state =
      ToBlinkState(info.state);
  for (auto client : clients_)
    client->didAddOutputPort(id, manufacturer, name, version, state);
}

void MidiMessageFilter::HandleDataReceived(uint32_t port,
                                           const std::vector<uint8_t>& data,
                                           double timestamp) {
  TRACE_EVENT0("midi", "MidiMessageFilter::HandleDataReceived");
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(!data.empty());

  for (auto client : clients_)
    client->didReceiveMIDIData(port, &data[0], data.size(), timestamp);
}

void MidiMessageFilter::HandleAckknowledgeSentData(size_t bytes_sent) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK_GE(unacknowledged_bytes_sent_, bytes_sent);
  if (unacknowledged_bytes_sent_ >= bytes_sent)
    unacknowledged_bytes_sent_ -= bytes_sent;
}

void MidiMessageFilter::HandleSetInputPortState(
    uint32_t port,
    media::midi::MidiPortState state) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  inputs_[port].state = state;
  for (auto client : clients_)
    client->didSetInputPortState(port, ToBlinkState(state));
}

void MidiMessageFilter::HandleSetOutputPortState(
    uint32_t port,
    media::midi::MidiPortState state) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  outputs_[port].state = state;
  for (auto client : clients_)
    client->didSetOutputPortState(port, ToBlinkState(state));
}

}  // namespace content
