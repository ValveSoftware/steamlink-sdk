// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_REMOTING_SINK_OBSERVER_H_
#define MEDIA_REMOTING_REMOTING_SINK_OBSERVER_H_

#include "media/mojo/interfaces/remoting.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace media {

// This calss is a pure observer of remoting sink availability to indicate
// whether a remoting session can be started.
class RemotingSinkObserver final : public mojom::RemotingSource {
 public:
  RemotingSinkObserver(mojom::RemotingSourceRequest source_request,
                       mojom::RemoterPtr remoter);
  ~RemotingSinkObserver() override;

  // RemotingSource implementations.
  void OnSinkAvailable() override;
  void OnSinkGone() override;
  void OnStarted() override {}
  void OnStartFailed(mojom::RemotingStartFailReason reason) override {}
  void OnMessageFromSink(const std::vector<uint8_t>& message) override {}
  void OnStopped(mojom::RemotingStopReason reason) override {}

  bool is_sink_available() { return is_sink_available_; }

 private:
  const mojo::Binding<mojom::RemotingSource> binding_;
  const mojom::RemoterPtr remoter_;
  bool is_sink_available_ = false;

  DISALLOW_COPY_AND_ASSIGN(RemotingSinkObserver);
};

}  // namespace media

#endif  // MEDIA_REMOTING_REMOTING_SINK_OBSERVER_H_
