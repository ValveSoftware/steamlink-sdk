// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/application/application.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"

namespace mojo {
namespace examples {

class DemoLauncher : public Application {
 public:
  DemoLauncher() {}
  virtual ~DemoLauncher() {}

 private:
  // Overridden from Application:
  virtual void Initialize() MOJO_OVERRIDE {
    ConnectTo<view_manager::ViewManagerInitService>("mojo:mojo_view_manager",
                                                    &view_manager_init_);
    view_manager_init_->EmbedRoot("mojo:mojo_window_manager",
                                  base::Bind(&DemoLauncher::OnConnect,
                                             base::Unretained(this)));
  }

  void OnConnect(bool success) {}

  view_manager::ViewManagerInitServicePtr view_manager_init_;

  DISALLOW_COPY_AND_ASSIGN(DemoLauncher);
};

}  // namespace examples

// static
Application* Application::Create() {
  return new examples::DemoLauncher;
}

}  // namespace mojo
