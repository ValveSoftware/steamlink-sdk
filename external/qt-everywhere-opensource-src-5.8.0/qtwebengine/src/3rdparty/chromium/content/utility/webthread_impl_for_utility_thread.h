// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_UTILITY_WEBTHREAD_IMPL_FOR_UTILITY_THREAD_H_
#define CONTENT_UTILITY_WEBTHREAD_IMPL_FOR_UTILITY_THREAD_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/scheduler/child/webthread_base.h"

namespace content {

class WebThreadImplForUtilityThread : public scheduler::WebThreadBase {
 public:
  WebThreadImplForUtilityThread();
  ~WebThreadImplForUtilityThread() override;

  // blink::WebThread implementation.
  blink::WebScheduler* scheduler() const override;
  blink::PlatformThreadId threadId() const override;

  // WebThreadBase implementation.
  base::SingleThreadTaskRunner* GetTaskRunner() const override;
  scheduler::SingleThreadIdleTaskRunner* GetIdleTaskRunner() const override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  blink::PlatformThreadId thread_id_;

  DISALLOW_COPY_AND_ASSIGN(WebThreadImplForUtilityThread);
};

}  // namespace content

#endif  // CONTENT_UTILITY_WEBTHREAD_IMPL_FOR_UTILITY_THREAD_H_
