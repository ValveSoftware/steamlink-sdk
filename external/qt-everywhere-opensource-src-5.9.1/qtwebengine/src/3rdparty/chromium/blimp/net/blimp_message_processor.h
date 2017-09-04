// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_BLIMP_MESSAGE_PROCESSOR_H_
#define BLIMP_NET_BLIMP_MESSAGE_PROCESSOR_H_

#include <memory>

#include "net/base/completion_callback.h"

namespace blimp {

class BlimpMessage;

// Interface implemented by components that process BlimpMessages.
// Message processing workflows can be created as a filter chain of
// BlimpMessageProcessors, and test processors can be added to validate portions
// of the filter chain in isolation.
class BlimpMessageProcessor {
 public:
  virtual ~BlimpMessageProcessor() {}

  // Processes the BlimpMessage asynchronously.
  // The result of the operation is returned to the caller via |callback|.
  virtual void ProcessMessage(std::unique_ptr<BlimpMessage> message,
                              const net::CompletionCallback& callback) = 0;
};

}  // namespace blimp

#endif  // BLIMP_NET_BLIMP_MESSAGE_PROCESSOR_H_
