// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/application/application.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/utility/run_loop.h"

extern "C" APPLICATION_EXPORT MojoResult CDECL MojoMain(
    MojoHandle service_provider_handle) {
  mojo::Environment env;
  mojo::RunLoop loop;

  mojo::Application* app = mojo::Application::Create();
  app->BindServiceProvider(
    mojo::MakeScopedHandle(mojo::MessagePipeHandle(service_provider_handle)));
  app->Initialize();
  loop.Run();
  delete app;

  return MOJO_RESULT_OK;
}
