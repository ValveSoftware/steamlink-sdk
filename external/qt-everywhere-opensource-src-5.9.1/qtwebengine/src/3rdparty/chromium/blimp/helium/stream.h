// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_HELIUM_STREAM_H_
#define BLIMP_HELIUM_STREAM_H_

#include <memory>

#include "base/callback.h"
#include "blimp/helium/result.h"

namespace blimp {
namespace helium {

class HeliumMessage;

// Pure virtual interface for HeliumMessage-oriented transport streams.
// Details about how the helium::Stream is bound to the network layer are
// handled by subclasses of helium::Stream.
class Stream {
 public:
  using ReceiveMessageCallback =
      base::Callback<void(std::unique_ptr<HeliumMessage>, Result)>;

  virtual ~Stream() = default;

  // Sends |helium_message| over the Stream. |callback| is invoked when the
  // message is sent (or otherwise moved to the low-level write buffers),
  // which signals the caller that it is clear to send another message.
  //
  // The caller is responsible for ensuring that only one outstanding
  // SendMessage() call is made at a time.
  virtual void SendMessage(std::unique_ptr<HeliumMessage> helium_message,
                           const base::Callback<void(Result)>& callback) = 0;

  // Asynchronously reads a HeliumMessage from the stream.
  // The caller is responsible for ensuring that only one outstanding
  // ReceiveMessage() call is made at a time.
  //
  // In the event that an error occurred, a null pointer will be passed instead
  // of a HeliumMessage, with a HeliumResult describing the failure reason.
  // The HeliumStream object is considered inactive/unusable at this point and
  // should be discarded by its owner.
  virtual void ReceiveMessage(const ReceiveMessageCallback& on_receive_cb) = 0;
};

}  // namespace helium
}  // namespace blimp

#endif  // BLIMP_HELIUM_STREAM_H_
