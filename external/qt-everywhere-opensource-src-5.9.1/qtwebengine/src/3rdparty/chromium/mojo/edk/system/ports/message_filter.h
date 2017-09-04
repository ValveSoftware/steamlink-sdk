// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_PORTS_MESSAGE_FILTER_H_
#define MOJO_EDK_SYSTEM_PORTS_MESSAGE_FILTER_H_

namespace mojo {
namespace edk {
namespace ports {

class Message;

// An interface which can be implemented to filter port messages according to
// arbitrary policy.
class MessageFilter {
 public:
  virtual ~MessageFilter() {}

  // Returns true of |message| should be accepted by whomever is applying this
  // filter. See MessageQueue::GetNextMessage(), for example.
  virtual bool Match(const Message& message) = 0;
};

}  // namespace ports
}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_PORTS_MESSAGE_FILTER_H_
