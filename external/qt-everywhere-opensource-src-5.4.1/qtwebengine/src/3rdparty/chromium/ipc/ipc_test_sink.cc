// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_test_sink.h"

#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"

namespace IPC {

TestSink::TestSink() {
}

TestSink::~TestSink() {
}

bool TestSink::Send(Message* message) {
  OnMessageReceived(*message);
  delete message;
  return true;
}

bool TestSink::Connect() {
  NOTIMPLEMENTED();
  return false;
}

void TestSink::Close() {
  NOTIMPLEMENTED();
}

base::ProcessId TestSink::GetPeerPID() const {
  NOTIMPLEMENTED();
  return base::ProcessId();
}


bool TestSink::OnMessageReceived(const Message& msg) {
  ObserverListBase<Listener>::Iterator it(filter_list_);
  Listener* observer;
  while ((observer = it.GetNext()) != NULL) {
    if (observer->OnMessageReceived(msg))
      return true;
  }

  // No filter handled the message, so store it.
  messages_.push_back(Message(msg));
  return true;
}

void TestSink::ClearMessages() {
  messages_.clear();
}

const Message* TestSink::GetMessageAt(size_t index) const {
  if (index >= messages_.size())
    return NULL;
  return &messages_[index];
}

const Message* TestSink::GetFirstMessageMatching(uint32 id) const {
  for (size_t i = 0; i < messages_.size(); i++) {
    if (messages_[i].type() == id)
      return &messages_[i];
  }
  return NULL;
}

const Message* TestSink::GetUniqueMessageMatching(uint32 id) const {
  size_t found_index = 0;
  size_t found_count = 0;
  for (size_t i = 0; i < messages_.size(); i++) {
    if (messages_[i].type() == id) {
      found_count++;
      found_index = i;
    }
  }
  if (found_count != 1)
    return NULL;  // Didn't find a unique one.
  return &messages_[found_index];
}

void TestSink::AddFilter(Listener* filter) {
  filter_list_.AddObserver(filter);
}

void TestSink::RemoveFilter(Listener* filter) {
  filter_list_.RemoveObserver(filter);
}

#if defined(OS_POSIX) && !defined(OS_NACL)

int TestSink::GetClientFileDescriptor() const {
  NOTREACHED();
  return -1;
}

int TestSink::TakeClientFileDescriptor() {
  NOTREACHED();
  return -1;
}

#endif  // defined(OS_POSIX) && !defined(OS_NACL)

}  // namespace IPC
