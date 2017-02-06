// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_STANDALONE_SERVICE_HELPER_
#define COMPONENTS_ARC_STANDALONE_SERVICE_HELPER_

#include <signal.h>

#include "base/files/scoped_file.h"
#include "base/message_loop/message_loop.h"

namespace arc {

// Helper class to set up service-like processes.
class ServiceHelper : public base::MessageLoopForIO::Watcher {
 public:
  ServiceHelper();
  ~ServiceHelper() override;

  // Must be called after message loop instantiation.
  void Init(const base::Closure& closure);

  // MessageLoopForIO::Watcher
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

 private:
  static void TerminationHandler(int /* signum */);

  // Static variable to guarantee instantiated only once per process.
  static ServiceHelper* self_;

  base::Closure closure_;
  base::ScopedFD read_fd_;
  base::ScopedFD write_fd_;
  base::MessageLoopForIO::FileDescriptorWatcher watcher_;
  struct sigaction old_sigint_;
  struct sigaction old_sigterm_;

  DISALLOW_COPY_AND_ASSIGN(ServiceHelper);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_STANDALONE_SERVICE_HELPER_
