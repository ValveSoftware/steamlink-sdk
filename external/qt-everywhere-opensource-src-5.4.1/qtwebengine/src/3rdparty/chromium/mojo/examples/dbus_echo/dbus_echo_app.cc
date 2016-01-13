// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "mojo/public/cpp/application/application.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/cpp/system/macros.h"
#include "mojo/public/interfaces/service_provider/service_provider.mojom.h"
#include "mojo/services/dbus_echo/echo.mojom.h"

namespace mojo {
namespace examples {

class DBusEchoApp : public Application {
 public:
  DBusEchoApp() {}
  virtual ~DBusEchoApp() {}

  virtual void Initialize() MOJO_OVERRIDE {
    ConnectTo("dbus:org.chromium.EchoService/org/chromium/MojoImpl",
              &echo_service_);

    echo_service_->Echo(
        String::From("who"),
        base::Bind(&DBusEchoApp::OnEcho, base::Unretained(this)));
  }

 private:
  void OnEcho(String echoed) {
    LOG(INFO) << "echo'd " << echoed;
  }

  EchoServicePtr echo_service_;

  DISALLOW_COPY_AND_ASSIGN(DBusEchoApp);
};

}  // namespace examples

// static
Application* Application::Create() {
  return new examples::DBusEchoApp();
}

}  // namespace mojo
