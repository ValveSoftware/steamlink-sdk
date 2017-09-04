// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_context_options.h"

#include <string>
#include <utility>

namespace headless {

namespace {

template <class T>
const T& ReturnOverriddenValue(const base::Optional<T>& value,
                               const T& default_value) {
  if (value) {
    return *value;
  } else {
    return default_value;
  }
}

}  // namespace

HeadlessBrowserContextOptions::HeadlessBrowserContextOptions(
    HeadlessBrowserContextOptions&& options) = default;

HeadlessBrowserContextOptions::~HeadlessBrowserContextOptions() = default;

HeadlessBrowserContextOptions& HeadlessBrowserContextOptions::operator=(
    HeadlessBrowserContextOptions&& options) = default;

HeadlessBrowserContextOptions::HeadlessBrowserContextOptions(
    HeadlessBrowser::Options* options)
    : browser_options_(options) {}

const std::string& HeadlessBrowserContextOptions::user_agent() const {
  return ReturnOverriddenValue(user_agent_, browser_options_->user_agent);
}

const net::HostPortPair& HeadlessBrowserContextOptions::proxy_server() const {
  return ReturnOverriddenValue(proxy_server_, browser_options_->proxy_server);
}

const std::string& HeadlessBrowserContextOptions::host_resolver_rules() const {
  return ReturnOverriddenValue(host_resolver_rules_,
                               browser_options_->host_resolver_rules);
}

const gfx::Size& HeadlessBrowserContextOptions::window_size() const {
  return ReturnOverriddenValue(window_size_, browser_options_->window_size);
}

const base::FilePath& HeadlessBrowserContextOptions::user_data_dir() const {
  return ReturnOverriddenValue(user_data_dir_, browser_options_->user_data_dir);
}

bool HeadlessBrowserContextOptions::incognito_mode() const {
  return ReturnOverriddenValue(incognito_mode_,
                               browser_options_->incognito_mode);
}

const ProtocolHandlerMap& HeadlessBrowserContextOptions::protocol_handlers()
    const {
  return protocol_handlers_;
}

ProtocolHandlerMap HeadlessBrowserContextOptions::TakeProtocolHandlers() {
  return std::move(protocol_handlers_);
}

const base::Callback<void(WebPreferences*)>&
HeadlessBrowserContextOptions::override_web_preferences_callback() const {
  return override_web_preferences_callback_;
}

}  // namespace headless
