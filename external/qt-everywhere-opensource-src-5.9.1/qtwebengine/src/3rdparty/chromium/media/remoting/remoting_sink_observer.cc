// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/remoting_sink_observer.h"

namespace media {

RemotingSinkObserver::RemotingSinkObserver(
    mojom::RemotingSourceRequest source_request,
    mojom::RemoterPtr remoter)
    : binding_(this, std::move(source_request)), remoter_(std::move(remoter)) {}

RemotingSinkObserver::~RemotingSinkObserver() {}

void RemotingSinkObserver::OnSinkAvailable() {
  is_sink_available_ = true;
}

void RemotingSinkObserver::OnSinkGone() {
  is_sink_available_ = false;
}

}  // namespace media
