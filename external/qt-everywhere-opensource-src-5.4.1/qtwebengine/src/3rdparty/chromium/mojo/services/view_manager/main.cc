// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/application/application.h"
#include "mojo/services/view_manager/view_manager_init_service_impl.h"

namespace mojo {
namespace view_manager {
namespace service {

class ViewManagerApp : public Application {
 public:
  ViewManagerApp() {}
  virtual ~ViewManagerApp() {}

  virtual void Initialize() MOJO_OVERRIDE {
    // TODO(sky): this needs some sort of authentication as well as making sure
    // we only ever have one active at a time.
    AddService<ViewManagerInitServiceImpl>(service_provider());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ViewManagerApp);
};

}  // namespace service
}  // namespace view_manager

// static
Application* Application::Create() {
  return new mojo::view_manager::service::ViewManagerApp();
}

}  // namespace mojo
