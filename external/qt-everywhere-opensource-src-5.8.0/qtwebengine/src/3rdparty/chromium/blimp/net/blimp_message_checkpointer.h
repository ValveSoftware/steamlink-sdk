// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_BLIMP_MESSAGE_CHECKPOINTER_H_
#define BLIMP_NET_BLIMP_MESSAGE_CHECKPOINTER_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "blimp/net/blimp_message_processor.h"
#include "blimp/net/blimp_net_export.h"

namespace blimp {

class BlimpMessage;
class BlimpMessageCheckpointObserver;

// Utility class configured with incoming & outgoing MessageProcessors,
// responsible for dispatching checkpoint/ACK messages to the outgoing
// processor, as the incoming processor completes processing them.
// Checkpoint/ACK message dispatch may be deferred for a second or
// two to avoid saturating the link with ACK traffic; feature implementations
// need to account for this latency in their design.
// BlimpMessageCheckpointer is created on the UI thread, and then used and
// destroyed on the IO thread.
class BLIMP_NET_EXPORT BlimpMessageCheckpointer : public BlimpMessageProcessor {
 public:
  BlimpMessageCheckpointer(BlimpMessageProcessor* incoming_processor,
                           BlimpMessageProcessor* outgoing_processor,
                           BlimpMessageCheckpointObserver* checkpoint_observer);
  ~BlimpMessageCheckpointer() override;

  // BlimpMessageProcessor interface.
  void ProcessMessage(std::unique_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override;

 private:
  void InvokeCompletionCallback(const net::CompletionCallback& callback,
                                int result);
  void SendCheckpointAck();

  BlimpMessageProcessor* incoming_processor_;
  BlimpMessageProcessor* outgoing_processor_;
  BlimpMessageCheckpointObserver* checkpoint_observer_;

  // Holds the Id of the message that most recently completed processing.
  int64_t checkpoint_id_ = 0;

  // Used to batch multiple processed messages into a single ACK.
  base::OneShotTimer defer_timer_;

  // Used to abandon pending ProcessMessage completion callbacks on deletion.
  base::WeakPtrFactory<BlimpMessageCheckpointer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlimpMessageCheckpointer);
};

}  // namespace blimp

#endif  // BLIMP_NET_BLIMP_MESSAGE_CHECKPOINTER_H_
