// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_SYNC_MESSAGE_H_
#define IPC_IPC_SYNC_MESSAGE_H_

#if defined(OS_WIN)
#include <windows.h>
#endif
#include <string>
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ipc/ipc_message.h"

namespace base {
class WaitableEvent;
}

namespace IPC {

class MessageReplyDeserializer;

class IPC_EXPORT SyncMessage : public Message {
 public:
  SyncMessage(int32 routing_id, uint32 type, PriorityValue priority,
              MessageReplyDeserializer* deserializer);
  virtual ~SyncMessage();

  // Call this to get a deserializer for the output parameters.
  // Note that this can only be called once, and the caller is responsible
  // for deleting the deserializer when they're done.
  MessageReplyDeserializer* GetReplyDeserializer();

  // If this message can cause the receiver to block while waiting for user
  // input (i.e. by calling MessageBox), then the caller needs to pump window
  // messages and dispatch asynchronous messages while waiting for the reply.
  // If this event is passed in, then window messages will start being pumped
  // when it's set.  Note that this behavior will continue even if the event is
  // later reset.  The event must be valid until after the Send call returns.
  void set_pump_messages_event(base::WaitableEvent* event) {
    pump_messages_event_ = event;
    if (event) {
      header()->flags |= PUMPING_MSGS_BIT;
    } else {
      header()->flags &= ~PUMPING_MSGS_BIT;
    }
  }

  // Call this if you always want to pump messages.  You can call this method
  // or set_pump_messages_event but not both.
  void EnableMessagePumping();

  base::WaitableEvent* pump_messages_event() const {
    return pump_messages_event_;
  }

  // Returns true if the message is a reply to the given request id.
  static bool IsMessageReplyTo(const Message& msg, int request_id);

  // Given a reply message, returns an iterator to the beginning of the data
  // (i.e. skips over the synchronous specific data).
  static PickleIterator GetDataIterator(const Message* msg);

  // Given a synchronous message (or its reply), returns its id.
  static int GetMessageId(const Message& msg);

  // Generates a reply message to the given message.
  static Message* GenerateReply(const Message* msg);

 private:
  struct SyncHeader {
    // unique ID (unique per sender)
    int message_id;
  };

  static bool ReadSyncHeader(const Message& msg, SyncHeader* header);
  static bool WriteSyncHeader(Message* msg, const SyncHeader& header);

  scoped_ptr<MessageReplyDeserializer> deserializer_;
  base::WaitableEvent* pump_messages_event_;
};

// Used to deserialize parameters from a reply to a synchronous message
class IPC_EXPORT MessageReplyDeserializer {
 public:
  virtual ~MessageReplyDeserializer() {}
  bool SerializeOutputParameters(const Message& msg);
 private:
  // Derived classes need to implement this, using the given iterator (which
  // is skipped past the header for synchronous messages).
  virtual bool SerializeOutputParameters(const Message& msg,
                                         PickleIterator iter) = 0;
};

// When sending a synchronous message, this structure contains an object
// that knows how to deserialize the response.
struct PendingSyncMsg {
  PendingSyncMsg(int id,
                 MessageReplyDeserializer* d,
                 base::WaitableEvent* e)
      : id(id), deserializer(d), done_event(e), send_result(false) { }
  int id;
  MessageReplyDeserializer* deserializer;
  base::WaitableEvent* done_event;
  bool send_result;
};

}  // namespace IPC

#endif  // IPC_IPC_SYNC_MESSAGE_H_
