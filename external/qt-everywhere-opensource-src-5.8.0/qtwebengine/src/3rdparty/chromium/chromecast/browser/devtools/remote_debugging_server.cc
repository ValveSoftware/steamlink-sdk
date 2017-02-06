// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/devtools/remote_debugging_server.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chromecast/base/pref_names.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/devtools/cast_dev_tools_delegate.h"
#include "chromecast/common/cast_content_client.h"
#include "components/devtools_http_handler/devtools_http_handler.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
#include "net/base/net_errors.h"
#include "net/socket/tcp_server_socket.h"

#if defined(OS_ANDROID)
#include "content/public/browser/android/devtools_auth.h"
#include "net/socket/unix_domain_server_socket_posix.h"
#endif  // defined(OS_ANDROID)

using devtools_http_handler::DevToolsHttpHandler;

namespace chromecast {
namespace shell {

namespace {

const char kFrontEndURL[] =
    "https://chrome-devtools-frontend.appspot.com/serve_rev/%s/inspector.html";
const uint16_t kDefaultRemoteDebuggingPort = 9222;

const int kBackLog = 10;

#if defined(OS_ANDROID)
class UnixDomainServerSocketFactory
    : public DevToolsHttpHandler::ServerSocketFactory {
 public:
  explicit UnixDomainServerSocketFactory(const std::string& socket_name)
      : socket_name_(socket_name) {}

 private:
  // devtools_http_handler::DevToolsHttpHandler::ServerSocketFactory.
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    std::unique_ptr<net::UnixDomainServerSocket> socket(
        new net::UnixDomainServerSocket(
            base::Bind(&content::CanUserConnectToDevTools),
            true /* use_abstract_namespace */));
    if (socket->BindAndListen(socket_name_, kBackLog) != net::OK)
      return std::unique_ptr<net::ServerSocket>();

    return std::move(socket);
  }

  std::string socket_name_;

  DISALLOW_COPY_AND_ASSIGN(UnixDomainServerSocketFactory);
};
#else
class TCPServerSocketFactory
    : public DevToolsHttpHandler::ServerSocketFactory {
 public:
  TCPServerSocketFactory(const std::string& address, uint16_t port)
      : address_(address), port_(port) {}

 private:
  // devtools_http_handler::DevToolsHttpHandler::ServerSocketFactory.
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    std::unique_ptr<net::ServerSocket> socket(
        new net::TCPServerSocket(nullptr, net::NetLog::Source()));
    if (socket->ListenWithAddressAndPort(address_, port_, kBackLog) != net::OK)
      return std::unique_ptr<net::ServerSocket>();

    return socket;
  }

  std::string address_;
  uint16_t port_;

  DISALLOW_COPY_AND_ASSIGN(TCPServerSocketFactory);
};
#endif

std::unique_ptr<DevToolsHttpHandler::ServerSocketFactory> CreateSocketFactory(
    uint16_t port) {
#if defined(OS_ANDROID)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string socket_name = "cast_shell_devtools_remote";
  if (command_line->HasSwitch(switches::kRemoteDebuggingSocketName)) {
    socket_name = command_line->GetSwitchValueASCII(
        switches::kRemoteDebuggingSocketName);
  }
  return std::unique_ptr<DevToolsHttpHandler::ServerSocketFactory>(
      new UnixDomainServerSocketFactory(socket_name));
#else
  return std::unique_ptr<DevToolsHttpHandler::ServerSocketFactory>(
      new TCPServerSocketFactory("0.0.0.0", port));
#endif
}

std::string GetFrontendUrl() {
  return base::StringPrintf(kFrontEndURL, content::GetWebKitRevision().c_str());
}

}  // namespace

RemoteDebuggingServer::RemoteDebuggingServer(bool start_immediately)
    : port_(kDefaultRemoteDebuggingPort) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  pref_enabled_.Init(prefs::kEnableRemoteDebugging,
                     CastBrowserProcess::GetInstance()->pref_service(),
                     base::Bind(&RemoteDebuggingServer::OnEnabledChanged,
                                base::Unretained(this)));

  std::string port_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kRemoteDebuggingPort);
  if (!port_str.empty()) {
    int port = kDefaultRemoteDebuggingPort;
    if (base::StringToInt(port_str, &port)) {
      port_ = static_cast<uint16_t>(port);
    } else {
      port_ = kDefaultRemoteDebuggingPort;
    }
  }

  // Starts new dev tools, clearing port number saved in config.
  // Remote debugging in production must be triggered only by config server.
  pref_enabled_.SetValue(start_immediately && port_ != 0);
  OnEnabledChanged();
}

RemoteDebuggingServer::~RemoteDebuggingServer() {
  pref_enabled_.SetValue(false);
  OnEnabledChanged();
}

void RemoteDebuggingServer::OnEnabledChanged() {
  bool enabled = *pref_enabled_ && port_ != 0;
  if (enabled && !devtools_http_handler_) {
    devtools_http_handler_.reset(new DevToolsHttpHandler(
        CreateSocketFactory(port_),
        GetFrontendUrl(),
        new CastDevToolsDelegate(),
        base::FilePath(),
        base::FilePath(),
        std::string(),
        GetUserAgent()));
    LOG(INFO) << "Devtools started: port=" << port_;
  } else if (!enabled && devtools_http_handler_) {
    LOG(INFO) << "Stop devtools: port=" << port_;
    devtools_http_handler_.reset();
  }
}

}  // namespace shell
}  // namespace chromecast
