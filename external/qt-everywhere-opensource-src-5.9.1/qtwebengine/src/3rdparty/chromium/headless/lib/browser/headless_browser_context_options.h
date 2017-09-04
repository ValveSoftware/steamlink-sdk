// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_CONTEXT_OPTIONS_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_CONTEXT_OPTIONS_H_

#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/optional.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_browser_context.h"

namespace headless {

// Represents options which can be customized for a given BrowserContext.
// Provides a fallback to default browser-side options when an option
// is not set for a particular BrowserContext.
class HeadlessBrowserContextOptions {
 public:
  HeadlessBrowserContextOptions(HeadlessBrowserContextOptions&& options);
  ~HeadlessBrowserContextOptions();

  HeadlessBrowserContextOptions& operator=(
      HeadlessBrowserContextOptions&& options);

  const std::string& user_agent() const;

  // See HeadlessBrowser::Options::proxy_server.
  const net::HostPortPair& proxy_server() const;

  // See HeadlessBrowser::Options::host_resolver_rules.
  const std::string& host_resolver_rules() const;

  const gfx::Size& window_size() const;

  // See HeadlessBrowser::Options::user_data_dir.
  const base::FilePath& user_data_dir() const;

  // Set HeadlessBrowser::Options::incognito_mode.
  bool incognito_mode() const;

  // Custom network protocol handlers. These can be used to override URL
  // fetching for different network schemes.
  const ProtocolHandlerMap& protocol_handlers() const;
  // Since ProtocolHandlerMap is move-only, this method takes ownership of them.
  ProtocolHandlerMap TakeProtocolHandlers();

  // Callback that is invoked to override WebPreferences for RenderViews
  // created within this HeadlessBrowserContext.
  const base::Callback<void(WebPreferences*)>&
  override_web_preferences_callback() const;

 private:
  friend class HeadlessBrowserContext::Builder;

  explicit HeadlessBrowserContextOptions(HeadlessBrowser::Options*);

  HeadlessBrowser::Options* browser_options_;

  base::Optional<std::string> user_agent_;
  base::Optional<net::HostPortPair> proxy_server_;
  base::Optional<std::string> host_resolver_rules_;
  base::Optional<gfx::Size> window_size_;
  base::Optional<base::FilePath> user_data_dir_;
  base::Optional<bool> incognito_mode_;

  ProtocolHandlerMap protocol_handlers_;
  base::Callback<void(WebPreferences*)> override_web_preferences_callback_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessBrowserContextOptions);
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_CONTEXT_OPTIONS_H_
