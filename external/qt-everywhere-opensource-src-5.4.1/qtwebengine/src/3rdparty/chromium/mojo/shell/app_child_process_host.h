// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_APP_CHILD_PROCESS_HOST_H_
#define MOJO_SHELL_APP_CHILD_PROCESS_HOST_H_

#include "base/macros.h"
#include "mojo/shell/app_child_process.mojom.h"
#include "mojo/shell/child_process_host.h"

namespace mojo {

namespace embedder {
struct ChannelInfo;
}

namespace shell {

// A subclass of |ChildProcessHost| to host a |TYPE_APP| child process, which
// runs a single app (loaded from the file system).
//
// Note: After |Start()|, this object must remain alive until the controller
// client's |AppCompleted()| is called.
class AppChildProcessHost : public ChildProcessHost,
                            public ChildProcessHost::Delegate {
 public:
  AppChildProcessHost(Context* context,
                      AppChildControllerClient* controller_client);
  virtual ~AppChildProcessHost();

  AppChildController* controller() {
    return controller_.get();
  }

 private:
  // |ChildProcessHost::Delegate| methods:
  virtual void WillStart() OVERRIDE;
  virtual void DidStart(bool success) OVERRIDE;

  // Callback for |embedder::CreateChannel()|.
  void DidCreateChannel(embedder::ChannelInfo* channel_info);

  AppChildControllerClient* const controller_client_;

  AppChildControllerPtr controller_;
  embedder::ChannelInfo* channel_info_;

  DISALLOW_COPY_AND_ASSIGN(AppChildProcessHost);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_APP_CHILD_PROCESS_HOST_H_
