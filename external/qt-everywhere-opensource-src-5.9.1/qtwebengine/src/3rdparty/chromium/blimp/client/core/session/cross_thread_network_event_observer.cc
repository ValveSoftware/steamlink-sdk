// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/session/cross_thread_network_event_observer.h"

#include "base/bind.h"
#include "base/location.h"

namespace blimp {
namespace client {

CrossThreadNetworkEventObserver::CrossThreadNetworkEventObserver(
    const base::WeakPtr<NetworkEventObserver>& target,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : target_(target), task_runner_(task_runner) {}

CrossThreadNetworkEventObserver::~CrossThreadNetworkEventObserver() {}

void CrossThreadNetworkEventObserver::OnConnected() {
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&NetworkEventObserver::OnConnected, target_));
}

void CrossThreadNetworkEventObserver::OnDisconnected(int result) {
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&NetworkEventObserver::OnDisconnected, target_, result));
}

}  // namespace client
}  // namespace blimp
