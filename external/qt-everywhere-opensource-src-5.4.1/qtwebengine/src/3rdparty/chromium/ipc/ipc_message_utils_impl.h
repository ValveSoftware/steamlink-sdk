// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains templates forward declared (but not defined) in
// ipc_message_utils.h so that they are only instantiated in certain files,
// notably a few IPC unit tests.

#ifndef IPC_IPC_MESSAGE_UTILS_IMPL_H_
#define IPC_IPC_MESSAGE_UTILS_IMPL_H_

namespace IPC {

template <class ParamType>
void MessageSchema<ParamType>::Write(Message* msg, const RefParam& p) {
  WriteParam(msg, p);
}

template <class ParamType>
bool MessageSchema<ParamType>::Read(const Message* msg, Param* p) {
  PickleIterator iter(*msg);
  if (ReadParam(msg, &iter, p))
    return true;
  NOTREACHED() << "Error deserializing message " << msg->type();
  return false;
}

template <class SendParamType, class ReplyParamType>
void SyncMessageSchema<SendParamType, ReplyParamType>::Write(
    Message* msg,
    const RefSendParam& send) {
  WriteParam(msg, send);
}

template <class SendParamType, class ReplyParamType>
bool SyncMessageSchema<SendParamType, ReplyParamType>::ReadSendParam(
    const Message* msg, SendParam* p) {
  PickleIterator iter = SyncMessage::GetDataIterator(msg);
  return ReadParam(msg, &iter, p);
}

template <class SendParamType, class ReplyParamType>
bool SyncMessageSchema<SendParamType, ReplyParamType>::ReadReplyParam(
    const Message* msg, typename TupleTypes<ReplyParam>::ValueTuple* p) {
  PickleIterator iter = SyncMessage::GetDataIterator(msg);
  return ReadParam(msg, &iter, p);
}

}  // namespace IPC

#endif  // IPC_IPC_MESSAGE_UTILS_IMPL_H_
