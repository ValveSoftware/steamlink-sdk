// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_UTIL_DETERMINISTIC_DISPATCHER_H_
#define HEADLESS_PUBLIC_UTIL_DETERMINISTIC_DISPATCHER_H_

#include <deque>
#include <map>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "headless/public/util/url_request_dispatcher.h"
#include "net/base/net_errors.h"

namespace headless {

class ManagedDispatchURLRequestJob;

// The purpose of this class is to queue up calls to OnHeadersComplete and
// OnStartError and dispatch them in order of URLRequestJob creation. This
// helps make renders deterministic at the cost of slower page loads.
class DeterministicDispatcher : public URLRequestDispatcher {
 public:
  explicit DeterministicDispatcher(
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner);

  ~DeterministicDispatcher() override;

  // UrlRequestDispatcher implementation:
  void JobCreated(ManagedDispatchURLRequestJob* job) override;
  void JobKilled(ManagedDispatchURLRequestJob* job) override;
  void JobFailed(ManagedDispatchURLRequestJob* job, net::Error error) override;
  void DataReady(ManagedDispatchURLRequestJob* job) override;
  void JobDeleted(ManagedDispatchURLRequestJob* job) override;

 private:
  void MaybeDispatchJobLocked();
  void MaybeDispatchJobOnIOThreadTask();

  scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner_;

  // Protects all members below.
  base::Lock lock_;

  std::deque<ManagedDispatchURLRequestJob*> pending_requests_;

  using StatusMap = std::map<ManagedDispatchURLRequestJob*, net::Error>;
  StatusMap ready_status_map_;

  // Whether or not a MaybeDispatchJobOnIoThreadTask has been posted on the
  // |io_thread_task_runner_|
  bool dispatch_pending_;

  base::WeakPtrFactory<DeterministicDispatcher> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeterministicDispatcher);
};

}  // namespace headless

#endif  // HEADLESS_PUBLIC_UTIL_DETERMINISTIC_DISPATCHER_H_
