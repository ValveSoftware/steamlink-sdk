// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif
#include <stack>

#include "base/atomic_sequence_num.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "ipc/ipc_sync_message.h"

namespace {

struct WaitableEventLazyInstanceTraits
    : public base::DefaultLazyInstanceTraits<base::WaitableEvent> {
  static base::WaitableEvent* New(void* instance) {
    // Use placement new to initialize our instance in our preallocated space.
    return new (instance) base::WaitableEvent(true, true);
  }
};

base::LazyInstance<base::WaitableEvent, WaitableEventLazyInstanceTraits>
    dummy_event = LAZY_INSTANCE_INITIALIZER;

base::StaticAtomicSequenceNumber g_next_id;

}  // namespace

namespace IPC {

#define kSyncMessageHeaderSize 4

SyncMessage::SyncMessage(
    int32 routing_id,
    uint32 type,
    PriorityValue priority,
    MessageReplyDeserializer* deserializer)
    : Message(routing_id, type, priority),
      deserializer_(deserializer),
      pump_messages_event_(NULL)
      {
  set_sync();
  set_unblock(true);

  // Add synchronous message data before the message payload.
  SyncHeader header;
  header.message_id = g_next_id.GetNext();
  WriteSyncHeader(this, header);
}

SyncMessage::~SyncMessage() {
}

MessageReplyDeserializer* SyncMessage::GetReplyDeserializer() {
  DCHECK(deserializer_.get());
  return deserializer_.release();
}

void SyncMessage::EnableMessagePumping() {
  DCHECK(!pump_messages_event_);
  set_pump_messages_event(dummy_event.Pointer());
}

bool SyncMessage::IsMessageReplyTo(const Message& msg, int request_id) {
  if (!msg.is_reply())
    return false;

  return GetMessageId(msg) == request_id;
}

PickleIterator SyncMessage::GetDataIterator(const Message* msg) {
  PickleIterator iter(*msg);
  if (!iter.SkipBytes(kSyncMessageHeaderSize))
    return PickleIterator();
  else
    return iter;
}

int SyncMessage::GetMessageId(const Message& msg) {
  if (!msg.is_sync() && !msg.is_reply())
    return 0;

  SyncHeader header;
  if (!ReadSyncHeader(msg, &header))
    return 0;

  return header.message_id;
}

Message* SyncMessage::GenerateReply(const Message* msg) {
  DCHECK(msg->is_sync());

  Message* reply = new Message(msg->routing_id(), IPC_REPLY_ID,
                               msg->priority());
  reply->set_reply();

  SyncHeader header;

  // use the same message id, but this time reply bit is set
  header.message_id = GetMessageId(*msg);
  WriteSyncHeader(reply, header);

  return reply;
}

bool SyncMessage::ReadSyncHeader(const Message& msg, SyncHeader* header) {
  DCHECK(msg.is_sync() || msg.is_reply());

  PickleIterator iter(msg);
  bool result = msg.ReadInt(&iter, &header->message_id);
  if (!result) {
    NOTREACHED();
    return false;
  }

  return true;
}

bool SyncMessage::WriteSyncHeader(Message* msg, const SyncHeader& header) {
  DCHECK(msg->is_sync() || msg->is_reply());
  DCHECK(msg->payload_size() == 0);
  bool result = msg->WriteInt(header.message_id);
  if (!result) {
    NOTREACHED();
    return false;
  }

  // Note: if you add anything here, you need to update kSyncMessageHeaderSize.
  DCHECK(kSyncMessageHeaderSize == msg->payload_size());

  return true;
}


bool MessageReplyDeserializer::SerializeOutputParameters(const Message& msg) {
  return SerializeOutputParameters(msg, SyncMessage::GetDataIterator(&msg));
}

}  // namespace IPC
