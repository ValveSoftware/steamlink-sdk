// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_BROWSER_MAIN_PARTS_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_BROWSER_MAIN_PARTS_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/task/cancelable_task_tracker.h"
#include "build/build_config.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/common/main_function_params.h"
#include "ui/aura/window_tree_host_observer.h"

class PrefService;

namespace content {
class BrowserContext;
struct MainFunctionParams;
}

namespace devtools_http_handler {
class DevToolsHttpHandler;
}

namespace views {
class Widget;
}

namespace extensions {

class AppWindowClient;
class DesktopController;
class ExtensionsBrowserClient;
class ExtensionsClient;
class ShellBrowserContext;
class ShellBrowserMainDelegate;
class ShellDeviceClient;
class ShellExtensionSystem;
class ShellOAuth2TokenService;
class ShellUpdateQueryParamsDelegate;

#if defined(OS_CHROMEOS)
class ShellAudioController;
class ShellNetworkController;
#endif

// Handles initialization of AppShell.
class ShellBrowserMainParts : public content::BrowserMainParts {
 public:
  ShellBrowserMainParts(const content::MainFunctionParams& parameters,
                        ShellBrowserMainDelegate* browser_main_delegate);
  ~ShellBrowserMainParts() override;

  ShellBrowserContext* browser_context() { return browser_context_.get(); }

  ShellExtensionSystem* extension_system() { return extension_system_; }

  // BrowserMainParts overrides.
  void PreEarlyInitialization() override;
  void PreMainMessageLoopStart() override;
  void PostMainMessageLoopStart() override;
  int PreCreateThreads() override;
  void PreMainMessageLoopRun() override;
  bool MainMessageLoopRun(int* result_code) override;
  void PostMainMessageLoopRun() override;
  void PostDestroyThreads() override;

 protected:
  // app_shell embedders may need custom extensions client interfaces.
  // This class takes ownership of the returned objects.
  virtual ExtensionsClient* CreateExtensionsClient();
  virtual ExtensionsBrowserClient* CreateExtensionsBrowserClient(
      content::BrowserContext* context,
      PrefService* service);

 private:
  // Creates and initializes the ExtensionSystem.
  void CreateExtensionSystem();

#if defined(OS_CHROMEOS)
  std::unique_ptr<ShellNetworkController> network_controller_;
  std::unique_ptr<ShellAudioController> audio_controller_;
#endif
  std::unique_ptr<DesktopController> desktop_controller_;
  std::unique_ptr<ShellBrowserContext> browser_context_;
  std::unique_ptr<PrefService> local_state_;
  std::unique_ptr<PrefService> user_pref_service_;
  std::unique_ptr<ShellDeviceClient> device_client_;
  std::unique_ptr<AppWindowClient> app_window_client_;
  std::unique_ptr<ExtensionsClient> extensions_client_;
  std::unique_ptr<ExtensionsBrowserClient> extensions_browser_client_;
  std::unique_ptr<devtools_http_handler::DevToolsHttpHandler>
      devtools_http_handler_;
  std::unique_ptr<ShellUpdateQueryParamsDelegate> update_query_params_delegate_;
  std::unique_ptr<ShellOAuth2TokenService> oauth2_token_service_;

  // Owned by the KeyedService system.
  ShellExtensionSystem* extension_system_;

  // For running app browsertests.
  const content::MainFunctionParams parameters_;

  // If true, indicates the main message loop should be run
  // in MainMessageLoopRun. If false, it has already been run.
  bool run_message_loop_;

  std::unique_ptr<ShellBrowserMainDelegate> browser_main_delegate_;

#if !defined(DISABLE_NACL)
  base::CancelableTaskTracker task_tracker_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ShellBrowserMainParts);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_BROWSER_MAIN_PARTS_H_
