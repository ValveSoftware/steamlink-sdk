// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/application/application.h"
#include "mojo/services/network/network_context.h"
#include "mojo/services/network/network_service_impl.h"

extern "C" APPLICATION_EXPORT MojoResult CDECL MojoMain(
    MojoHandle service_provider_handle) {
  base::CommandLine::Init(0, NULL);
  base::AtExitManager at_exit;

  // The IO message loop allows us to use net::URLRequest on this thread.
  base::MessageLoopForIO loop;

  mojo::NetworkContext context;

  mojo::Application app;
  app.BindServiceProvider(
      mojo::MakeScopedHandle(mojo::MessagePipeHandle(service_provider_handle)));

  app.AddService<mojo::NetworkServiceImpl>(&context);

  loop.Run();
  return MOJO_RESULT_OK;
}
