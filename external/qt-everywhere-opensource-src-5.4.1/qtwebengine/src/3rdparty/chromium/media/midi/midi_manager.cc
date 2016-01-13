// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"

namespace media {

MidiManager::MidiManager()
    : initialized_(false),
      result_(MIDI_NOT_SUPPORTED) {
}

MidiManager::~MidiManager() {
}

#if !defined(OS_MACOSX) && !defined(OS_WIN) && !defined(USE_ALSA) && \
    !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
MidiManager* MidiManager::Create() {
  return new MidiManager;
}
#endif

void MidiManager::StartSession(MidiManagerClient* client, int client_id) {
  bool session_is_ready;
  bool session_needs_initialization = false;
  bool too_many_pending_clients_exist = false;

  {
    base::AutoLock auto_lock(lock_);
    session_is_ready = initialized_;
    if (!session_is_ready) {
      // Do not accept a new request if the pending client list contains too
      // many clients.
      too_many_pending_clients_exist =
          pending_clients_.size() >= kMaxPendingClientCount;

      if (!too_many_pending_clients_exist) {
        // Call StartInitialization() only for the first request.
        session_needs_initialization = pending_clients_.empty();
        pending_clients_.insert(std::make_pair(client, client_id));
      }
    }
  }

  // Lazily initialize the MIDI back-end.
  if (!session_is_ready) {
    if (session_needs_initialization) {
      TRACE_EVENT0("midi", "MidiManager::StartInitialization");
      session_thread_runner_ =
          base::MessageLoop::current()->message_loop_proxy();
      StartInitialization();
    }
    if (too_many_pending_clients_exist) {
      // Return an error immediately if there are too many requests.
      client->CompleteStartSession(client_id, MIDI_INITIALIZATION_ERROR);
      return;
    }
    // CompleteInitialization() will be called asynchronously when platform
    // dependent initialization is finished.
    return;
  }

  // Platform dependent initialization was already finished for previously
  // initialized clients.
  MidiResult result;
  {
    base::AutoLock auto_lock(lock_);
    if (result_ == MIDI_OK)
      clients_.insert(client);
    result = result_;
  }
  client->CompleteStartSession(client_id, result);
}

void MidiManager::EndSession(MidiManagerClient* client) {
  base::AutoLock auto_lock(lock_);
  clients_.erase(client);
  pending_clients_.erase(client);
}

void MidiManager::DispatchSendMidiData(MidiManagerClient* client,
                                       uint32 port_index,
                                       const std::vector<uint8>& data,
                                       double timestamp) {
  NOTREACHED();
}

void MidiManager::StartInitialization() {
  CompleteInitialization(MIDI_NOT_SUPPORTED);
}

void MidiManager::CompleteInitialization(MidiResult result) {
  DCHECK(session_thread_runner_.get());
  // It is safe to post a task to the IO thread from here because the IO thread
  // should have stopped if the MidiManager is going to be destructed.
  session_thread_runner_->PostTask(
      FROM_HERE,
      base::Bind(&MidiManager::CompleteInitializationInternal,
                 base::Unretained(this),
                 result));
}

void MidiManager::AddInputPort(const MidiPortInfo& info) {
  input_ports_.push_back(info);
}

void MidiManager::AddOutputPort(const MidiPortInfo& info) {
  output_ports_.push_back(info);
}

void MidiManager::ReceiveMidiData(
    uint32 port_index,
    const uint8* data,
    size_t length,
    double timestamp) {
  base::AutoLock auto_lock(lock_);

  for (ClientList::iterator i = clients_.begin(); i != clients_.end(); ++i)
    (*i)->ReceiveMidiData(port_index, data, length, timestamp);
}

void MidiManager::CompleteInitializationInternal(MidiResult result) {
  TRACE_EVENT0("midi", "MidiManager::CompleteInitialization");

  base::AutoLock auto_lock(lock_);
  DCHECK(clients_.empty());
  DCHECK(!initialized_);
  initialized_ = true;
  result_ = result;

  for (PendingClientMap::iterator it = pending_clients_.begin();
       it != pending_clients_.end();
       ++it) {
    if (result_ == MIDI_OK)
      clients_.insert(it->first);
    it->first->CompleteStartSession(it->second, result_);
  }
  pending_clients_.clear();
}

}  // namespace media
