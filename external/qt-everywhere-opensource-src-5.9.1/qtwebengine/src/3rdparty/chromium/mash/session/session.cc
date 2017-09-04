// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/session/session.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "mash/login/public/interfaces/login.mojom.h"
#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace {

void LogAndCallServiceRestartCallback(const std::string& url,
                                      const base::Closure& callback) {
  LOG(ERROR) << "Restarting service: " << url;
  callback.Run();
}

}  // namespace

namespace mash {
namespace session {

Session::Session() : screen_locked_(false) {}
Session::~Session() {}

void Session::OnStart() {
  StartWindowManager();
  StartQuickLaunch();

  // Launch a chrome window for dev convience; don't do this in the long term.
  context()->connector()->Connect("content_browser");
}

bool Session::OnConnect(const service_manager::ServiceInfo& remote_info,
                        service_manager::InterfaceRegistry* registry) {
  registry->AddInterface<mojom::Session>(this);
  return true;
}

void Session::Logout() {
  // TODO(beng): Notify connected listeners that login is happening, potentially
  // give them the option to stop it.
  mash::login::mojom::LoginPtr login;
  context()->connector()->ConnectToInterface("login", &login);
  login->ShowLoginUI();
  // This kills the user environment.
  base::MessageLoop::current()->QuitWhenIdle();
}

void Session::SwitchUser() {
  mash::login::mojom::LoginPtr login;
  context()->connector()->ConnectToInterface("login", &login);
  login->SwitchUser();
}

void Session::AddScreenlockStateListener(
    mojom::ScreenlockStateListenerPtr listener) {
  listener->ScreenlockStateChanged(screen_locked_);
  screenlock_listeners_.AddPtr(std::move(listener));
}

void Session::LockScreen() {
  if (screen_locked_)
    return;
  screen_locked_ = true;
  screenlock_listeners_.ForAllPtrs(
      [](mojom::ScreenlockStateListener* listener) {
        listener->ScreenlockStateChanged(true);
      });
  StartScreenlock();
}
void Session::UnlockScreen() {
  if (!screen_locked_)
    return;
  screen_locked_ = false;
  screenlock_listeners_.ForAllPtrs(
      [](mojom::ScreenlockStateListener* listener) {
        listener->ScreenlockStateChanged(false);
      });
  StopScreenlock();
}

void Session::Create(const service_manager::Identity& remote_identity,
                     mojom::SessionRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void Session::StartWindowManager() {
  StartRestartableService(
      "ash",
      base::Bind(&Session::StartWindowManager,
                 base::Unretained(this)));
}

void Session::StartQuickLaunch() {
  StartRestartableService(
      "quick_launch",
      base::Bind(&Session::StartQuickLaunch,
                 base::Unretained(this)));
}

void Session::StartScreenlock() {
  StartRestartableService(
      "screenlock",
      base::Bind(&Session::StartScreenlock,
                 base::Unretained(this)));
}

void Session::StopScreenlock() {
  auto connection = connections_.find("screenlock");
  DCHECK(connections_.end() != connection);
  connections_.erase(connection);
}

void Session::StartRestartableService(
    const std::string& url,
    const base::Closure& restart_callback) {
  // TODO(beng): This would be the place to insert logic that counted restarts
  //             to avoid infinite crash-restart loops.
  std::unique_ptr<service_manager::Connection> connection =
      context()->connector()->Connect(url);
  // Note: |connection| may be null if we've lost our connection to the service
  // manager.
  if (connection) {
    connection->SetConnectionLostClosure(
        base::Bind(&LogAndCallServiceRestartCallback, url, restart_callback));
    connections_[url] = std::move(connection);
  }
}

}  // namespace session
}  // namespace main
