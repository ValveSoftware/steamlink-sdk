// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/application/application.h"

extern "C" APPLICATION_EXPORT MojoResult CDECL MojoMain(
    MojoHandle service_provider_handle) {
  base::CommandLine::Init(0, NULL);
  base::AtExitManager at_exit;
  base::MessageLoop loop;

  scoped_ptr<mojo::Application> app(mojo::Application::Create());
  app->BindServiceProvider(
      mojo::MakeScopedHandle(mojo::MessagePipeHandle(service_provider_handle)));
  app->Initialize();
  loop.Run();

  return MOJO_RESULT_OK;
}
