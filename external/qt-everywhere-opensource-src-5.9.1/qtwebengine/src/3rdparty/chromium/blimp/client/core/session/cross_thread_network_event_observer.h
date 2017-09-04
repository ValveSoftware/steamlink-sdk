// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_SESSION_CROSS_THREAD_NETWORK_EVENT_OBSERVER_H_
#define BLIMP_CLIENT_CORE_SESSION_CROSS_THREAD_NETWORK_EVENT_OBSERVER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "blimp/client/core/session/network_event_observer.h"

namespace blimp {
namespace client {

// Posts NetworkEvents to an observer across the IO/UI thread boundary.
class CrossThreadNetworkEventObserver : public NetworkEventObserver {
 public:
  CrossThreadNetworkEventObserver(
      const base::WeakPtr<NetworkEventObserver>& target,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  ~CrossThreadNetworkEventObserver() override;

  // NetworkEventObserver implementation.
  void OnConnected() override;
  void OnDisconnected(int result) override;

 private:
  base::WeakPtr<NetworkEventObserver> target_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(CrossThreadNetworkEventObserver);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_SESSION_CROSS_THREAD_NETWORK_EVENT_OBSERVER_H_
