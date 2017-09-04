// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/core/proto_zero_message_handle.h"

#include "base/logging.h"
#include "components/tracing/core/proto_zero_message.h"

namespace tracing {
namespace v2 {

namespace {

inline void FinalizeMessageIfSet(ProtoZeroMessage* message) {
  if (message) {
    message->Finalize();
#if DCHECK_IS_ON()
    message->set_handle(nullptr);
#endif
  }
}

}  // namespace

ProtoZeroMessageHandleBase::ProtoZeroMessageHandleBase(
    ProtoZeroMessage* message)
    : message_(message) {
#if DCHECK_IS_ON()
  message_->set_handle(this);
#endif
}

ProtoZeroMessageHandleBase::~ProtoZeroMessageHandleBase() {
  FinalizeMessageIfSet(message_);
}

ProtoZeroMessageHandleBase::ProtoZeroMessageHandleBase(
    ProtoZeroMessageHandleBase&& other) {
  Move(&other);
}

ProtoZeroMessageHandleBase& ProtoZeroMessageHandleBase::operator=(
    ProtoZeroMessageHandleBase&& other) {
  // If the current handle was pointing to a message and is being reset to a new
  // one, finalize the old message.
  FinalizeMessageIfSet(message_);

  Move(&other);
  return *this;
}

void ProtoZeroMessageHandleBase::Move(ProtoZeroMessageHandleBase* other) {
  // In theory other->message_ could be nullptr, if |other| is a handle that has
  // been std::move-d (and hence empty). There isn't a legitimate use case for
  // doing so, though. Therefore this case is deliberately ignored (if hit, it
  // will manifest as a segfault when dereferencing |message_| below) to avoid a
  // useless null-check.
  message_ = other->message_;
  other->message_ = nullptr;
#if DCHECK_IS_ON()
  message_->set_handle(this);
#endif
}

}  // namespace v2
}  // namespace tracing
