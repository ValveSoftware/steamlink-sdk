// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_CORE_PROTO_ZERO_MESSAGE_HANDLE_H_
#define COMPONENTS_TRACING_CORE_PROTO_ZERO_MESSAGE_HANDLE_H_

#include "base/macros.h"
#include "components/tracing/tracing_export.h"

namespace tracing {
namespace v2 {

class ProtoZeroMessage;

// ProtoZeroMessageHandle allows to decouple the lifetime of a proto message
// from the underlying storage. It gives the following guarantees:
// - The underlying message is finalized (if still alive) if the handle goes
//   out of scope.
// - In Debug / DCHECK_ALWAYS_ON builds, the handle becomes null once the
//   message is finalized. This is to enforce the append-only API. For instance
//   when adding two repeated messages, the addition of the 2nd one forces
//   the finalization of the first.
// Think about this as a WeakPtr<ProtoZeroMessage> which calls
// ProtoZeroMessage::Finalize() when going out of scope.

class TRACING_EXPORT ProtoZeroMessageHandleBase {
 public:
  ~ProtoZeroMessageHandleBase();

  // Move-only type.
  ProtoZeroMessageHandleBase(ProtoZeroMessageHandleBase&& other);
  ProtoZeroMessageHandleBase& operator=(ProtoZeroMessageHandleBase&& other);

 protected:
  explicit ProtoZeroMessageHandleBase(ProtoZeroMessage*);
  ProtoZeroMessage& operator*() const { return *message_; }
  ProtoZeroMessage* operator->() const { return message_; }

 private:
  friend class ProtoZeroMessage;

  void reset_message() { message_ = nullptr; }
  void Move(ProtoZeroMessageHandleBase* other);

  ProtoZeroMessage* message_;

  DISALLOW_COPY_AND_ASSIGN(ProtoZeroMessageHandleBase);
};

template <typename T>
class ProtoZeroMessageHandle : ProtoZeroMessageHandleBase {
 public:
  explicit ProtoZeroMessageHandle(T* message)
      : ProtoZeroMessageHandleBase(message) {}

  T& operator*() const {
    return static_cast<T&>(ProtoZeroMessageHandleBase::operator*());
  }

  T* operator->() const {
    return static_cast<T*>(ProtoZeroMessageHandleBase::operator->());
  }
};

}  // namespace v2
}  // namespace tracing

#endif  // COMPONENTS_TRACING_CORE_PROTO_ZERO_MESSAGE_HANDLE_H_
