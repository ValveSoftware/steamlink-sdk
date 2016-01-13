// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// http://code.google.com/p/chromium/wiki/LinuxSandboxIPC

#ifndef CONTENT_BROWSER_RENDERER_HOST_SANDBOX_IPC_H_
#define CONTENT_BROWSER_RENDERER_HOST_SANDBOX_IPC_H_

#include <vector>

#include "base/files/scoped_file.h"
#include "base/memory/scoped_ptr.h"
#include "base/pickle.h"
#include "base/threading/simple_thread.h"
#include "content/child/blink_platform_impl.h"
#include "skia/ext/skia_utils_base.h"

namespace content {

class SandboxIPCHandler : public base::DelegateSimpleThread::Delegate {
 public:
  // lifeline_fd: the read end of a pipe which the main thread holds
  // the other end of.
  // browser_socket: the browser's end of the sandbox IPC socketpair.
  SandboxIPCHandler(int lifeline_fd, int browser_socket);
  virtual ~SandboxIPCHandler();

  virtual void Run() OVERRIDE;

 private:
  void EnsureWebKitInitialized();

  int FindOrAddPath(const SkString& path);

  void HandleRequestFromRenderer(int fd);

  void HandleFontMatchRequest(int fd,
                              const Pickle& pickle,
                              PickleIterator iter,
                              const std::vector<base::ScopedFD*>& fds);

  void HandleFontOpenRequest(int fd,
                             const Pickle& pickle,
                             PickleIterator iter,
                             const std::vector<base::ScopedFD*>& fds);

  void HandleGetFallbackFontForChar(int fd,
                                  const Pickle& pickle,
                                  PickleIterator iter,
                                  const std::vector<base::ScopedFD*>& fds);

  void HandleGetStyleForStrike(int fd,
                               const Pickle& pickle,
                               PickleIterator iter,
                               const std::vector<base::ScopedFD*>& fds);

  void HandleLocaltime(int fd,
                       const Pickle& pickle,
                       PickleIterator iter,
                       const std::vector<base::ScopedFD*>& fds);

  void HandleMakeSharedMemorySegment(int fd,
                                     const Pickle& pickle,
                                     PickleIterator iter,
                                     const std::vector<base::ScopedFD*>& fds);

  void HandleMatchWithFallback(int fd,
                               const Pickle& pickle,
                               PickleIterator iter,
                               const std::vector<base::ScopedFD*>& fds);

  void SendRendererReply(const std::vector<base::ScopedFD*>& fds,
                         const Pickle& reply,
                         int reply_fd);

  const int lifeline_fd_;
  const int browser_socket_;
  scoped_ptr<BlinkPlatformImpl> webkit_platform_support_;
  SkTDArray<SkString*> paths_;

  DISALLOW_COPY_AND_ASSIGN(SandboxIPCHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_SANDBOX_IPC_H_
