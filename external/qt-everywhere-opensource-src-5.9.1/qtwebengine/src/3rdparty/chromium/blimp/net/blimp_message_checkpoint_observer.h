// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_BLIMP_MESSAGE_CHECKPOINT_OBSERVER_H_
#define BLIMP_NET_BLIMP_MESSAGE_CHECKPOINT_OBSERVER_H_

#include <stdint.h>

namespace blimp {

// Allows objects to subscribe to message acknowledgment checkpoints.
class BlimpMessageCheckpointObserver {
 public:
  virtual ~BlimpMessageCheckpointObserver() {}

  // Invoked when the remote end has positively acknowledged the receipt of all
  // messages with ID <= |message_id|.
  virtual void OnMessageCheckpoint(int64_t message_id) = 0;
};

}  // namespace blimp

#endif  // BLIMP_NET_BLIMP_MESSAGE_CHECKPOINT_OBSERVER_H_
