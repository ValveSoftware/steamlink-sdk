// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_HEADLESS_BROWSER_H_
#define HEADLESS_PUBLIC_HEADLESS_BROWSER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "headless/public/headless_browser_context.h"
#include "headless/public/headless_export.h"
#include "headless/public/headless_web_contents.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class MessagePump;
class SingleThreadTaskRunner;
}

namespace headless {

// This class represents the global headless browser instance. To get a pointer
// to one, call |HeadlessBrowserMain| to initiate the browser main loop. An
// instance of |HeadlessBrowser| will be passed to the callback given to that
// function.
class HEADLESS_EXPORT HeadlessBrowser {
 public:
  struct Options;

  // Create a new browser context which can be used to create tabs and isolate
  // them from one another.
  // Pointer to HeadlessBrowserContext becomes invalid after:
  // a) Calling HeadlessBrowserContext::Close or
  // b) Calling HeadlessBrowser::Shutdown
  virtual HeadlessBrowserContext::Builder CreateBrowserContextBuilder() = 0;

  virtual std::vector<HeadlessBrowserContext*> GetAllBrowserContexts() = 0;

  // Returns the HeadlessWebContents associated with the
  // |devtools_agent_host_id| if any.  Otherwise returns null.
  virtual HeadlessWebContents* GetWebContentsForDevToolsAgentHostId(
      const std::string& devtools_agent_host_id) = 0;

  // Returns HeadlessBrowserContext associated with the given id if any.
  // Otherwise returns null.
  virtual HeadlessBrowserContext* GetBrowserContextForId(
      const std::string& id) = 0;

  // Returns a task runner for submitting work to the browser file thread.
  virtual scoped_refptr<base::SingleThreadTaskRunner> BrowserFileThread()
      const = 0;

  // Returns a task runner for submitting work to the browser io thread.
  virtual scoped_refptr<base::SingleThreadTaskRunner> BrowserIOThread()
      const = 0;

  // Returns a task runner for submitting work to the browser main thread.
  virtual scoped_refptr<base::SingleThreadTaskRunner> BrowserMainThread()
      const = 0;

  // Requests browser to stop as soon as possible. |Run| will return as soon as
  // browser stops.
  // IMPORTANT: All pointers to HeadlessBrowserContexts and HeadlessWebContents
  // become invalid after calling this function.
  virtual void Shutdown() = 0;

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

  // Address at which DevTools should listen for connections. Disabled by
  // default.
  net::IPEndPoint devtools_endpoint;

  // Optional message pump that overrides the default. Must outlive the browser.
  base::MessagePump* message_pump;

  // Run the browser in single process mode instead of using separate renderer
  // processes as per default. Note that this also disables any sandboxing of
  // web content, which can be a security risk.
  bool single_process_mode;

  // Run the browser without renderer sandbox. This option can be
  // a security risk and should be used with caution.
  bool disable_sandbox;

  // Choose the GL implementation to use for rendering. A suitable
  // implementantion is selected by default. Setting this to an empty
  // string can be used to disable GL rendering (e.g., WebGL support).
  std::string gl_implementation;

  // Names of mojo services exposed by the browser to the renderer. These
  // services will be added to the browser's service manifest.
  std::unordered_set<std::string> mojo_service_names;

  // Default per-context options, can be specialized on per-context basis.

  std::string user_agent;

  // Address of the HTTP/HTTPS proxy server to use. The system proxy settings
  // are used by default.
  net::HostPortPair proxy_server;

  // Comma-separated list of rules that control how hostnames are mapped. See
  // chrome::switches::kHostRules for a description for the format.
  std::string host_resolver_rules;

  // Default window size. This is also used to create the window tree host and
  // as initial screen size. Defaults to 800x600.
  gfx::Size window_size;

  // Path to user data directory, where browser will look for its state.
  // If empty, default directory (where the binary is located) will be used.
  base::FilePath user_data_dir;

  // Run a browser context in an incognito mode. Enabled by default.
  bool incognito_mode;

  // Reminder: when adding a new field here, do not forget to add it to
  // HeadlessBrowserContextOptions (where appropriate).
 private:
  Options(int argc, const char** argv);

  DISALLOW_COPY_AND_ASSIGN(Options);
};

class HeadlessBrowser::Options::Builder {
 public:
  Builder(int argc, const char** argv);
  Builder();
  ~Builder();

  // Browser-wide settings.

  Builder& EnableDevToolsServer(const net::IPEndPoint& endpoint);
  Builder& SetMessagePump(base::MessagePump* message_pump);
  Builder& SetSingleProcessMode(bool single_process_mode);
  Builder& SetDisableSandbox(bool disable_sandbox);
  Builder& SetGLImplementation(const std::string& gl_implementation);
  Builder& AddMojoServiceName(const std::string& mojo_service_name);

  // Per-context settings.

  Builder& SetUserAgent(const std::string& user_agent);
  Builder& SetProxyServer(const net::HostPortPair& proxy_server);
  Builder& SetHostResolverRules(const std::string& host_resolver_rules);
  Builder& SetWindowSize(const gfx::Size& window_size);
  Builder& SetUserDataDir(const base::FilePath& user_data_dir);
  Builder& SetIncognitoMode(bool incognito_mode);

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
