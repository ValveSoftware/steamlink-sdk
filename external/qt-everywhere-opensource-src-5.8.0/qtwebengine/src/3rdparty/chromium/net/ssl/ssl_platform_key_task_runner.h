// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_SSL_PLATFORM_KEY_TASK_RUNNER_H_
#define NET_SSL_SSL_PLATFORM_KEY_TASK_RUNNER_H_

#include "base/macros.h"
#include "base/threading/sequenced_worker_pool.h"

namespace net {

// Serialize all the private key operations on a single background
// thread to avoid problems with buggy smartcards.
class SSLPlatformKeyTaskRunner {
 public:
  SSLPlatformKeyTaskRunner();
  ~SSLPlatformKeyTaskRunner();

  scoped_refptr<base::SequencedTaskRunner> task_runner();

 private:
  scoped_refptr<base::SequencedWorkerPool> worker_pool_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(SSLPlatformKeyTaskRunner);
};

scoped_refptr<base::SequencedTaskRunner> GetSSLPlatformKeyTaskRunner();

}  // namespace net

#endif  // NET_SSL_SSL_PLATFORM_KEY_TASK_RUNNER_H_
