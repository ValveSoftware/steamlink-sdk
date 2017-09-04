// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_NULL_BLIMP_MESSAGE_PROCESSOR_H_
#define BLIMP_NET_NULL_BLIMP_MESSAGE_PROCESSOR_H_

#include "blimp/net/blimp_message_processor.h"
#include "blimp/net/blimp_net_export.h"
#include "net/base/net_errors.h"

namespace blimp {

// Dumps all incoming messages into a black hole and immediately notifies the
// callback of success.
class BLIMP_NET_EXPORT NullBlimpMessageProcessor
    : public BlimpMessageProcessor {
 public:
  ~NullBlimpMessageProcessor() override;

  void ProcessMessage(std::unique_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override;
};

}  // namespace blimp

#endif  // BLIMP_NET_NULL_BLIMP_MESSAGE_PROCESSOR_H_
