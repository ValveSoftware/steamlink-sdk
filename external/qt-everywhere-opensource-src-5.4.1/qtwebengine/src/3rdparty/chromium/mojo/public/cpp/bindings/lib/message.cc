// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/message.h"

#include <assert.h>
#include <stdlib.h>

#include <algorithm>

namespace mojo {

Message::Message()
    : data_num_bytes_(0),
      data_(NULL) {
}

Message::~Message() {
  free(data_);

  for (std::vector<Handle>::iterator it = handles_.begin();
       it != handles_.end(); ++it) {
    if (it->is_valid())
      CloseRaw(*it);
  }
}

void Message::AllocUninitializedData(uint32_t num_bytes) {
  assert(!data_);
  data_num_bytes_ = num_bytes;
  data_ = static_cast<internal::MessageData*>(malloc(num_bytes));
}

void Message::AdoptData(uint32_t num_bytes, internal::MessageData* data) {
  assert(!data_);
  data_num_bytes_ = num_bytes;
  data_ = data;
}

void Message::Swap(Message* other) {
  std::swap(data_num_bytes_, other->data_num_bytes_);
  std::swap(data_, other->data_);
  std::swap(handles_, other->handles_);
}

MojoResult ReadAndDispatchMessage(MessagePipeHandle handle,
                                  MessageReceiver* receiver,
                                  bool* receiver_result) {
  MojoResult rv;

  uint32_t num_bytes = 0, num_handles = 0;
  rv = ReadMessageRaw(handle,
                      NULL,
                      &num_bytes,
                      NULL,
                      &num_handles,
                      MOJO_READ_MESSAGE_FLAG_NONE);
  if (rv != MOJO_RESULT_RESOURCE_EXHAUSTED)
    return rv;

  Message message;
  message.AllocUninitializedData(num_bytes);
  message.mutable_handles()->resize(num_handles);

  rv = ReadMessageRaw(handle,
                      message.mutable_data(),
                      &num_bytes,
                      message.mutable_handles()->empty()
                          ? NULL
                          : reinterpret_cast<MojoHandle*>(
                                &message.mutable_handles()->front()),
                      &num_handles,
                      MOJO_READ_MESSAGE_FLAG_NONE);
  if (receiver && rv == MOJO_RESULT_OK)
    *receiver_result = receiver->Accept(&message);

  return rv;
}

}  // namespace mojo
