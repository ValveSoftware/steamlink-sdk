// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_BLIMP_MESSAGE_MULTIPLEXER_H_
#define BLIMP_NET_BLIMP_MESSAGE_MULTIPLEXER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/net/blimp_net_export.h"

namespace blimp {

class BlimpConnection;
class BlimpMessageProcessor;

// Creates MessageProcessors that receive outgoing messages and put them
// onto a multiplexed message stream.
// BlimpMessageMultiplexer is created on the UI thread, and then used and
// destroyed on the IO thread.
class BLIMP_NET_EXPORT BlimpMessageMultiplexer {
 public:
  // |output_processor|: A pointer to the MessageProcessor that will receive the
  // multiplexed message stream.
  explicit BlimpMessageMultiplexer(BlimpMessageProcessor* output_processor);

  ~BlimpMessageMultiplexer();

  // Creates a BlimpMessageProcessor object for sending messages of type
  // |feature_case|. Any number of senders can be created at a time for a given
  // type.
  std::unique_ptr<BlimpMessageProcessor> CreateSender(
      BlimpMessage::FeatureCase feature_case);

 private:
  base::WeakPtrFactory<BlimpMessageProcessor> output_weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlimpMessageMultiplexer);
};

}  // namespace blimp

#endif  // BLIMP_NET_BLIMP_MESSAGE_MULTIPLEXER_H_
