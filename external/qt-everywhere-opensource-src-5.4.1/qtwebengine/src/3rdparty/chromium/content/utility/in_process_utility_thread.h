// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_UTILITY_IN_PROCESS_UTILITY_THREAD_H_
#define CONTENT_UTILITY_IN_PROCESS_UTILITY_THREAD_H_

#include <string>

#include "base/threading/thread.h"
#include "content/common/content_export.h"

namespace content {

class ChildProcess;

class InProcessUtilityThread : public base::Thread {
 public:
  InProcessUtilityThread(const std::string& channel_id);
  virtual ~InProcessUtilityThread();

 private:
  // base::Thread implementation:
  virtual void Init() OVERRIDE;
  virtual void CleanUp() OVERRIDE;

  void InitInternal();

  std::string channel_id_;
  scoped_ptr<ChildProcess> child_process_;

  DISALLOW_COPY_AND_ASSIGN(InProcessUtilityThread);
};

CONTENT_EXPORT base::Thread* CreateInProcessUtilityThread(
    const std::string& channel_id);

}  // namespace content

#endif  // CONTENT_UTILITY_IN_PROCESS_UTILITY_THREAD_H_
