// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "mojo/public/cpp/application/application.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"

namespace mojo {
namespace examples {

// ViewManagerInit is responsible for establishing the initial connection to
// the view manager. When established it loads |mojo_aura_demo|.
class ViewManagerInit : public Application {
 public:
  ViewManagerInit() {}
  virtual ~ViewManagerInit() {}

  virtual void Initialize() OVERRIDE {
    ConnectTo("mojo:mojo_view_manager", &view_manager_init_);
    view_manager_init_->EmbedRoot("mojo:mojo_aura_demo",
                                  base::Bind(&ViewManagerInit::DidConnect,
                                             base::Unretained(this)));
  }

 private:
  void DidConnect(bool result) {
    DCHECK(result);
    VLOG(1) << "ViewManagerInit::DidConnection result=" << result;
  }

  view_manager::ViewManagerInitServicePtr view_manager_init_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerInit);
};

}  // namespace examples

// static
Application* Application::Create() {
  return new examples::ViewManagerInit();
}

}  // namespace mojo
