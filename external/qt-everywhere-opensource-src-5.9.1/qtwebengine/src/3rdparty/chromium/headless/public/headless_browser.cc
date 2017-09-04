// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/headless_browser.h"

#include <utility>

#include "content/public/common/user_agent.h"

using Options = headless::HeadlessBrowser::Options;
using Builder = headless::HeadlessBrowser::Options::Builder;

namespace headless {

// Product name for building the default user agent string.
namespace {
const char kProductName[] = "HeadlessChrome";
constexpr gfx::Size kDefaultWindowSize(800, 600);
}

Options::Options(int argc, const char** argv)
    : argc(argc),
      argv(argv),
      message_pump(nullptr),
      single_process_mode(false),
      disable_sandbox(false),
      gl_implementation("osmesa"),
      user_agent(content::BuildUserAgentFromProduct(kProductName)),
      window_size(kDefaultWindowSize),
      incognito_mode(true) {}

Options::Options(Options&& options) = default;

Options::~Options() {}

Options& Options::operator=(Options&& options) = default;

Builder::Builder(int argc, const char** argv) : options_(argc, argv) {}

Builder::Builder() : options_(0, nullptr) {}

Builder::~Builder() {}

Builder& Builder::SetUserAgent(const std::string& user_agent) {
  options_.user_agent = user_agent;
  return *this;
}

Builder& Builder::EnableDevToolsServer(const net::IPEndPoint& endpoint) {
  options_.devtools_endpoint = endpoint;
  return *this;
}

Builder& Builder::SetMessagePump(base::MessagePump* message_pump) {
  options_.message_pump = message_pump;
  return *this;
}

Builder& Builder::SetProxyServer(const net::HostPortPair& proxy_server) {
  options_.proxy_server = proxy_server;
  return *this;
}

Builder& Builder::SetHostResolverRules(const std::string& host_resolver_rules) {
  options_.host_resolver_rules = host_resolver_rules;
  return *this;
}

Builder& Builder::SetSingleProcessMode(bool single_process_mode) {
  options_.single_process_mode = single_process_mode;
  return *this;
}

Builder& Builder::SetDisableSandbox(bool disable_sandbox) {
  options_.disable_sandbox = disable_sandbox;
  return *this;
}

Builder& Builder::SetGLImplementation(const std::string& gl_implementation) {
  options_.gl_implementation = gl_implementation;
  return *this;
}

Builder& Builder::AddMojoServiceName(const std::string& mojo_service_name) {
  options_.mojo_service_names.insert(mojo_service_name);
  return *this;
}

Builder& Builder::SetUserDataDir(const base::FilePath& user_data_dir) {
  options_.user_data_dir = user_data_dir;
  return *this;
}

Builder& Builder::SetWindowSize(const gfx::Size& window_size) {
  options_.window_size = window_size;
  return *this;
}

Builder& Builder::SetIncognitoMode(bool incognito_mode) {
  options_.incognito_mode = incognito_mode;
  return *this;
}

Options Builder::Build() {
  return std::move(options_);
}

}  // namespace headless
