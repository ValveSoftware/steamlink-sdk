// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/application/application.h"

namespace mojo {

Application::Application() : service_registry_(this) {}

Application::Application(ScopedMessagePipeHandle service_provider_handle)
    : service_registry_(this, service_provider_handle.Pass()) {}

Application::Application(MojoHandle service_provider_handle)
    : service_registry_(
          this,
          mojo::MakeScopedHandle(
              MessagePipeHandle(service_provider_handle)).Pass()) {}

Application::~Application() {}

bool Application::AllowIncomingConnection(const mojo::String& service_name,
                                          const mojo::String& requestor_url) {
  return true;
}

void Application::BindServiceProvider(
  ScopedMessagePipeHandle service_provider_handle) {
  service_registry_.BindRemoteServiceProvider(service_provider_handle.Pass());
}

void Application::Initialize() {}

}  // namespace mojo
