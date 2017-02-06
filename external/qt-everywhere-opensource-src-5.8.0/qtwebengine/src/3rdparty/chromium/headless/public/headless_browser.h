// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_HEADLESS_BROWSER_H_
#define HEADLESS_PUBLIC_HEADLESS_BROWSER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "headless/public/headless_browser_context.h"
#include "headless/public/headless_export.h"
#include "headless/public/headless_web_contents.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"

namespace base {
class MessagePump;
class SingleThreadTaskRunner;
}

namespace gfx {
class Size;
}

namespace headless {

// This class represents the global headless browser instance. To get a pointer
// to one, call |HeadlessBrowserMain| to initiate the browser main loop. An
// instance of |HeadlessBrowser| will be passed to the callback given to that
// function.
class HEADLESS_EXPORT HeadlessBrowser {
 public:
  struct Options;

  // Open a new tab. Returns a builder object which can be used to set
  // properties for the new tab.
  virtual HeadlessWebContents::Builder CreateWebContentsBuilder() = 0;

  // Deprecated. Use CreateWebContentsBuilder() instead.
  virtual HeadlessWebContents* CreateWebContents(const GURL& initial_url,
                                                 const gfx::Size& size) = 0;

  virtual std::vector<HeadlessWebContents*> GetAllWebContents() = 0;

  // Returns a task runner for submitting work to the browser main thread.
  virtual scoped_refptr<base::SingleThreadTaskRunner> BrowserMainThread()
      const = 0;

  // Returns a task runner for submitting work to the browser file thread.
  virtual scoped_refptr<base::SingleThreadTaskRunner> BrowserFileThread()
      const = 0;

  // Requests browser to stop as soon as possible. |Run| will return as soon as
  // browser stops.
  virtual void Shutdown() = 0;

  // Create a new browser context, which can be used to isolate
  // HeadlessWebContents from one another.
  virtual HeadlessBrowserContext::Builder CreateBrowserContextBuilder() = 0;

 protected:
  HeadlessBrowser() {}
  virtual ~HeadlessBrowser() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(HeadlessBrowser);
};

// Embedding API overrides for the headless browser.
struct HeadlessBrowser::Options {
  class Builder;

  Options(Options&& options);
  ~Options();

  Options& operator=(Options&& options);

  // Command line options to be passed to browser.
  int argc;
  const char** argv;

  std::string user_agent;
  std::string navigator_platform;

  // Address at which DevTools should listen for connections. Disabled by
  // default.
  net::IPEndPoint devtools_endpoint;

  // Address of the HTTP/HTTPS proxy server to use. The system proxy settings
  // are used by default.
  net::HostPortPair proxy_server;

  // Optional message pump that overrides the default. Must outlive the browser.
  base::MessagePump* message_pump;

  // Comma-separated list of rules that control how hostnames are mapped. See
  // chrome::switches::kHostRules for a description for the format.
  std::string host_resolver_rules;

  // Run the browser in single process mode instead of using separate renderer
  // processes as per default. Note that this also disables any sandboxing of
  // web content, which can be a security risk.
  bool single_process_mode;

  // Custom network protocol handlers. These can be used to override URL
  // fetching for different network schemes.
  ProtocolHandlerMap protocol_handlers;

 private:
  Options(int argc, const char** argv);

  DISALLOW_COPY_AND_ASSIGN(Options);
};

class HeadlessBrowser::Options::Builder {
 public:
  Builder(int argc, const char** argv);
  Builder();
  ~Builder();

  Builder& SetUserAgent(const std::string& user_agent);
  Builder& EnableDevToolsServer(const net::IPEndPoint& endpoint);
  Builder& SetMessagePump(base::MessagePump* message_pump);
  Builder& SetProxyServer(const net::HostPortPair& proxy_server);
  Builder& SetHostResolverRules(const std::string& host_resolver_rules);
  Builder& SetSingleProcessMode(bool single_process_mode);
  Builder& SetProtocolHandlers(ProtocolHandlerMap protocol_handlers);

  Options Build();

 private:
  Options options_;

  DISALLOW_COPY_AND_ASSIGN(Builder);
};

// The headless browser may need to create child processes (e.g., a renderer
// which runs web content). This is done by re-executing the parent process as
// a zygote[1] and forking each child process from that zygote.
//
// For this to work, the embedder should call RunChildProcess as soon as
// possible (i.e., before creating any threads) to pass control to the headless
// library. In a browser process this function will return immediately, but in a
// child process it will never return. For example:
//
// int main(int argc, const char** argv) {
//   headless::RunChildProcessIfNeeded(argc, argv);
//   headless::HeadlessBrowser::Options::Builder builder(argc, argv);
//   return headless::HeadlessBrowserMain(
//       builder.Build(),
//       base::Callback<void(headless::HeadlessBrowser*)>());
// }
//
// [1]
// https://chromium.googlesource.com/chromium/src/+/master/docs/linux_zygote.md
void RunChildProcessIfNeeded(int argc, const char** argv);

// Main entry point for running the headless browser. This function constructs
// the headless browser instance, passing it to the given
// |on_browser_start_callback| callback. Note that since this function executes
// the main loop, it will only return after HeadlessBrowser::Shutdown() is
// called, returning the exit code for the process. It is not possible to
// initialize the browser again after it has been torn down.
int HeadlessBrowserMain(
    HeadlessBrowser::Options options,
    const base::Callback<void(HeadlessBrowser*)>& on_browser_start_callback);

}  // namespace headless

#endif  // HEADLESS_PUBLIC_HEADLESS_BROWSER_H_
