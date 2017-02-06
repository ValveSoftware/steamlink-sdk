// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_SANDBOX_HOST_LINUX_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_SANDBOX_HOST_LINUX_H_

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "base/threading/simple_thread.h"
#include "content/browser/renderer_host/sandbox_ipc_linux.h"
#include "content/common/content_export.h"

namespace base {
template <typename T> struct DefaultSingletonTraits;
}

namespace content {

// This is a singleton object which handles sandbox requests from the
// renderers.
class CONTENT_EXPORT RenderSandboxHostLinux {
 public:
  // Returns the singleton instance.
  static RenderSandboxHostLinux* GetInstance();

  // Get the file descriptor which renderers should be given in order to signal
  // crashes to the browser.
  int GetRendererSocket() const {
    DCHECK(initialized_);
    return renderer_socket_;
  }
  void Init();

 private:
  friend struct base::DefaultSingletonTraits<RenderSandboxHostLinux>;
  // This object must be constructed on the main thread.
  RenderSandboxHostLinux();
  ~RenderSandboxHostLinux();

  bool ShutdownIPCChannel();

  // Whether Init() has been called yet.
  bool initialized_;

  int renderer_socket_;
  int childs_lifeline_fd_;

  std::unique_ptr<SandboxIPCHandler> ipc_handler_;
  std::unique_ptr<base::DelegateSimpleThread> ipc_thread_;

  DISALLOW_COPY_AND_ASSIGN(RenderSandboxHostLinux);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_SANDBOX_HOST_LINUX_H_
