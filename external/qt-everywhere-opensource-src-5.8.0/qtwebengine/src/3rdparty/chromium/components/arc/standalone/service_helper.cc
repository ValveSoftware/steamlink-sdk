// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/standalone/service_helper.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/posix/eintr_wrapper.h"

namespace arc {

// static
ServiceHelper* ServiceHelper::self_ = nullptr;

ServiceHelper::ServiceHelper() {
}

ServiceHelper::~ServiceHelper() {
  // Best efforts clean up, ignore the return values.
  sigaction(SIGINT, &old_sigint_, NULL);
  sigaction(SIGTERM, &old_sigterm_, NULL);
}

void ServiceHelper::Init(const base::Closure& closure) {
  CHECK(!ServiceHelper::self_);
  ServiceHelper::self_ = this;
  closure_ = closure;

  // Initialize pipe
  int pipe_fd[2];
  CHECK_EQ(0, pipe(pipe_fd));
  read_fd_.reset(pipe_fd[0]);
  write_fd_.reset(pipe_fd[1]);
  CHECK(base::SetNonBlocking(write_fd_.get()));
  CHECK(base::MessageLoopForIO::current()->WatchFileDescriptor(
        read_fd_.get(),
        true, /* persistent */
        base::MessageLoopForIO::WATCH_READ,
        &watcher_,
        this));

  struct sigaction action = {};
  CHECK_EQ(0, sigemptyset(&action.sa_mask));
  action.sa_handler = ServiceHelper::TerminationHandler;
  action.sa_flags = 0;

  CHECK_EQ(0, sigaction(SIGINT, &action, &old_sigint_));
  CHECK_EQ(0, sigaction(SIGTERM, &action, &old_sigterm_));
}

// static
void ServiceHelper::TerminationHandler(int /* signum */) {
  if (HANDLE_EINTR(write(self_->write_fd_.get(), "1", 1)) < 0) {
    _exit(2);  // We still need to exit the program, but in a brute force way.
  }
}

void ServiceHelper::OnFileCanReadWithoutBlocking(int fd) {
  CHECK_EQ(read_fd_.get(), fd);

  char c;
  // We don't really care about the return value, since it indicates closing.
  HANDLE_EINTR(read(read_fd_.get(), &c, 1));
  closure_.Run();
}

void ServiceHelper::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

}  // namespace arc
