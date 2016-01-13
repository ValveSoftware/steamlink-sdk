// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/dbus/dbus_external_service.h"
#include "mojo/embedder/channel_init.h"
#include "mojo/embedder/embedder.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/services/dbus_echo/echo.mojom.h"

namespace {
class EchoServiceImpl : public mojo::InterfaceImpl<mojo::EchoService> {
 public:
  EchoServiceImpl() {}
  virtual ~EchoServiceImpl() {}

 protected:
  virtual void Echo(
      const mojo::String& in_to_echo,
      const mojo::Callback<void(mojo::String)>& callback) OVERRIDE {
    DVLOG(1) << "Asked to echo " << in_to_echo;
    callback.Run(in_to_echo);
  }
};

const char kServiceName[] = "org.chromium.EchoService";
}  // anonymous namespace

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(settings);
  logging::SetLogItems(false,    // Process ID
                       false,    // Thread ID
                       false,    // Timestamp
                       false);   // Tick count

  mojo::embedder::Init();

  base::MessageLoopForIO message_loop;
  base::RunLoop run_loop;

  mojo::DBusExternalService<EchoServiceImpl> echo_service(kServiceName);
  echo_service.Start();

  run_loop.Run();
  return 0;
}
