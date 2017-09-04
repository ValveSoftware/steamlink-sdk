// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_HELIUM_TRANSPORT_H_
#define BLIMP_HELIUM_TRANSPORT_H_

#include <memory>

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "blimp/helium/result.h"

namespace blimp {
namespace helium {

// Pure virtual interface for a helium::Stream factory. Subclasses can use this
// interface to encapsulate transport-specific stream connection semantics.
class Transport {
 public:
  // Callback invoked with the stream and result code of a Connect or Accept
  // attempt.
  // The helium::Stream is assumed to be authenticated and ready to use.
  // If the connection attempt failed, the value of |stream| will be null and
  // |result| will be set to the relevant error code.
  using StreamCreatedCallback =
      base::CancelableCallback<void(std::unique_ptr<Stream> stream,
                                    Result result)>;

  // Callback to be invoked when the underlying transport transitions
  // between an offline/unusable state and an online/usable state.
  using AvailabilityChangedCallback = base::Callback<void(bool)>;

  virtual void SetAvailabilityChangedCallback(
      const AvailabilityChangedCallback& callback);

  // Asynchronously attempts to connect a new helium::Stream.
  // Multiple overlapping connection attempts are permitted.
  // The caller can cancel |cb| if it no longer needs the stream.
  virtual void Connect(const StreamCreatedCallback& cb) = 0;

  // Accepts an incoming connection from the peer.
  virtual void Accept(const StreamCreatedCallback& cb) = 0;

  // Returns true if the underlying transport has the necessary resources
  // and connectivity for Connect and Accept operations.
  virtual bool IsAvailable() = 0;
};

}  // namespace helium
}  // namespace blimp

#endif  // BLIMP_HELIUM_TRANSPORT_H_
