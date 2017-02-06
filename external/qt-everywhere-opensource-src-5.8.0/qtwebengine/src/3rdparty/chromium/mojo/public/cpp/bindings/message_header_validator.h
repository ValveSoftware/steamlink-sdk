// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_HEADER_VALIDATOR_H_
#define MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_HEADER_VALIDATOR_H_

#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/message_filter.h"

namespace mojo {

class MessageHeaderValidator : public MessageFilter {
 public:
  explicit MessageHeaderValidator(MessageReceiver* sink = nullptr);
  MessageHeaderValidator(const std::string& description,
                         MessageReceiver* sink = nullptr);

  // Sets the description associated with this validator. Used for reporting
  // more detailed validation errors.
  void SetDescription(const std::string& description);

  bool Accept(Message* message) override;

 private:
  std::string description_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_HEADER_VALIDATOR_H_
