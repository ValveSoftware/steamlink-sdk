// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_HEADER_VALIDATOR_H_
#define MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_HEADER_VALIDATOR_H_

#include "base/compiler_specific.h"
#include "mojo/public/cpp/bindings/bindings_export.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {

class MOJO_CPP_BINDINGS_EXPORT MessageHeaderValidator
    : NON_EXPORTED_BASE(public MessageReceiver) {
 public:
  MessageHeaderValidator();
  explicit MessageHeaderValidator(const std::string& description);

  // Sets the description associated with this validator. Used for reporting
  // more detailed validation errors.
  void SetDescription(const std::string& description);

  bool Accept(Message* message) override;

 private:
  std::string description_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_HEADER_VALIDATOR_H_
