// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_MANAGER_H_
#define MEDIA_MIDI_MIDI_MANAGER_H_

#include <map>
#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/midi/midi_port_info.h"
#include "media/midi/midi_result.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace media {

// A MidiManagerClient registers with the MidiManager to receive MIDI data.
// See MidiManager::RequestAccess() and MidiManager::ReleaseAccess()
// for details.
class MEDIA_EXPORT MidiManagerClient {
 public:
  virtual ~MidiManagerClient() {}

  // CompleteStartSession() is called when platform dependent preparation is
  // finished.
  virtual void CompleteStartSession(int client_id, MidiResult result) = 0;

  // ReceiveMidiData() is called when MIDI data has been received from the
  // MIDI system.
  // |port_index| represents the specific input port from input_ports().
  // |data| represents a series of bytes encoding one or more MIDI messages.
  // |length| is the number of bytes in |data|.
  // |timestamp| is the time the data was received, in seconds.
  virtual void ReceiveMidiData(uint32 port_index,
                               const uint8* data,
                               size_t length,
                               double timestamp) = 0;

  // AccumulateMidiBytesSent() is called to acknowledge when bytes have
  // successfully been sent to the hardware.
  // This happens as a result of the client having previously called
  // MidiManager::DispatchSendMidiData().
  virtual void AccumulateMidiBytesSent(size_t n) = 0;
};

// Manages access to all MIDI hardware.
class MEDIA_EXPORT MidiManager {
 public:
  static const size_t kMaxPendingClientCount = 128;

  MidiManager();
  virtual ~MidiManager();

  // The constructor and the destructor will be called on the CrBrowserMain
  // thread.
  static MidiManager* Create();

  // A client calls StartSession() to receive and send MIDI data.
  // If the session is ready to start, the MIDI system is lazily initialized
  // and the client is registered to receive MIDI data.
  // CompleteStartSession() is called with MIDI_OK if the session is started.
  // Otherwise CompleteStartSession() is called with proper MidiResult code.
  // StartSession() and EndSession() can be called on the Chrome_IOThread.
  // CompleteStartSession() will be invoked on the same Chrome_IOThread.
  void StartSession(MidiManagerClient* client, int client_id);

  // A client calls EndSession() to stop receiving MIDI data.
  void EndSession(MidiManagerClient* client);

  // DispatchSendMidiData() is called when MIDI data should be sent to the MIDI
  // system.
  // This method is supposed to return immediately and should not block.
  // |port_index| represents the specific output port from output_ports().
  // |data| represents a series of bytes encoding one or more MIDI messages.
  // |length| is the number of bytes in |data|.
  // |timestamp| is the time to send the data, in seconds. A value of 0
  // means send "now" or as soon as possible.
  // The default implementation is for unsupported platforms.
  virtual void DispatchSendMidiData(MidiManagerClient* client,
                                    uint32 port_index,
                                    const std::vector<uint8>& data,
                                    double timestamp);

  // input_ports() is a list of MIDI ports for receiving MIDI data.
  // Each individual port in this list can be identified by its
  // integer index into this list.
  const MidiPortInfoList& input_ports() const { return input_ports_; }

  // output_ports() is a list of MIDI ports for sending MIDI data.
  // Each individual port in this list can be identified by its
  // integer index into this list.
  const MidiPortInfoList& output_ports() const { return output_ports_; }

 protected:
  friend class MidiManagerUsb;

  // Initializes the platform dependent MIDI system. MidiManager class has a
  // default implementation that synchronously calls CompleteInitialization()
  // with MIDI_NOT_SUPPORTED on the caller thread. A derived class for a
  // specific platform should override this method correctly.
  // This method is called on Chrome_IOThread thread inside StartSession().
  // Platform dependent initialization can be processed synchronously or
  // asynchronously. When the initialization is completed,
  // CompleteInitialization() should be called with |result|.
  // |result| should be MIDI_OK on success, otherwise a proper MidiResult.
  virtual void StartInitialization();

  // Called from a platform dependent implementation of StartInitialization().
  // It invokes CompleteInitializationInternal() on the thread that calls
  // StartSession() and distributes |result| to MIDIManagerClient objects in
  // |pending_clients_|.
  void CompleteInitialization(MidiResult result);

  void AddInputPort(const MidiPortInfo& info);
  void AddOutputPort(const MidiPortInfo& info);

  // Dispatches to all clients.
  // TODO(toyoshim): Fix the mac implementation to use
  // |ReceiveMidiData(..., base::TimeTicks)|.
  void ReceiveMidiData(uint32 port_index,
                       const uint8* data,
                       size_t length,
                       double timestamp);

  void ReceiveMidiData(uint32 port_index,
                       const uint8* data,
                       size_t length,
                       base::TimeTicks time) {
    ReceiveMidiData(port_index, data, length,
                    (time - base::TimeTicks()).InSecondsF());
  }

  size_t clients_size_for_testing() const { return clients_.size(); }
  size_t pending_clients_size_for_testing() const {
    return pending_clients_.size();
  }

 private:
  void CompleteInitializationInternal(MidiResult result);

  // Keeps track of all clients who wish to receive MIDI data.
  typedef std::set<MidiManagerClient*> ClientList;
  ClientList clients_;

  // Keeps track of all clients who are waiting for CompleteStartSession().
  typedef std::multimap<MidiManagerClient*, int> PendingClientMap;
  PendingClientMap pending_clients_;

  // Keeps a SingleThreadTaskRunner of the thread that calls StartSession in
  // order to invoke CompleteStartSession() on the thread.
  scoped_refptr<base::SingleThreadTaskRunner> session_thread_runner_;

  // Keeps true if platform dependent initialization is already completed.
  bool initialized_;

  // Keeps the platform dependent initialization result if initialization is
  // completed. Otherwise keeps MIDI_NOT_SUPPORTED.
  MidiResult result_;

  // Protects access to |clients_|, |pending_clients_|, |initialized_|, and
  // |result_|.
  base::Lock lock_;

  MidiPortInfoList input_ports_;
  MidiPortInfoList output_ports_;

  DISALLOW_COPY_AND_ASSIGN(MidiManager);
};

}  // namespace media

#endif  // MEDIA_MIDI_MIDI_MANAGER_H_
