// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/view_manager_loader.h"

#include "mojo/public/cpp/application/application.h"
#include "mojo/services/view_manager/view_manager_init_service_impl.h"

namespace mojo {
namespace shell {

ViewManagerLoader::ViewManagerLoader() {
}

ViewManagerLoader::~ViewManagerLoader() {
}

void ViewManagerLoader::LoadService(
    ServiceManager* manager,
    const GURL& url,
    ScopedMessagePipeHandle service_provider_handle) {
  // TODO(sky): this needs some sort of authentication as well as making sure
  // we only ever have one active at a time.
  scoped_ptr<Application> app(new Application(service_provider_handle.Pass()));
  app->AddService<view_manager::service::ViewManagerInitServiceImpl>(
      app->service_provider());
  apps_.push_back(app.release());
}

void ViewManagerLoader::OnServiceError(ServiceManager* manager,
                                       const GURL& url) {
}

}  // namespace shell
}  // namespace mojo
