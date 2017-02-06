// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SOCKET_APP_FIREWALL_HOLE_MANAGER_H_
#define EXTENSIONS_BROWSER_API_SOCKET_APP_FIREWALL_HOLE_MANAGER_H_

#include <stdint.h>

#include <map>

#include "base/scoped_observer.h"
#include "chromeos/network/firewall_hole.h"
#include "extensions/browser/app_window/app_window_registry.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class AppFirewallHoleManager;

// Represents an open port in the system firewall that will be opened and closed
// automatically when the application has a visible window or not. The hole is
// closed on destruction.
class AppFirewallHole {
 public:
  typedef chromeos::FirewallHole::PortType PortType;

  ~AppFirewallHole();

  PortType type() const { return type_; }
  uint16_t port() const { return port_; }
  const std::string& extension_id() const { return extension_id_; }

 private:
  friend class AppFirewallHoleManager;

  AppFirewallHole(AppFirewallHoleManager* manager,
                  PortType type,
                  uint16_t port,
                  const std::string& extension_id);

  void SetVisible(bool app_visible);
  void OnFirewallHoleOpened(
      std::unique_ptr<chromeos::FirewallHole> firewall_hole);

  PortType type_;
  uint16_t port_;
  std::string extension_id_;
  bool app_visible_ = false;

  // This object is destroyed when the AppFirewallHoleManager that owns it is
  // destroyed and so a raw pointer is okay here.
  AppFirewallHoleManager* manager_;

  // This will hold the FirewallHole object if one is opened.
  std::unique_ptr<chromeos::FirewallHole> firewall_hole_;

  base::WeakPtrFactory<AppFirewallHole> weak_factory_;
};

// Tracks ports in the system firewall opened by an application so that they
// may be automatically opened and closed only when the application has a
// visible window.
class AppFirewallHoleManager : public KeyedService,
                               public AppWindowRegistry::Observer {
 public:
  explicit AppFirewallHoleManager(content::BrowserContext* context);
  ~AppFirewallHoleManager() override;

  // Returns the instance for a given browser context, or NULL if none.
  static AppFirewallHoleManager* Get(content::BrowserContext* context);

  // Takes ownership of the AppFirewallHole and will open a port on the system
  // firewall if the associated application is currently visible.
  std::unique_ptr<AppFirewallHole> Open(AppFirewallHole::PortType type,
                                        uint16_t port,
                                        const std::string& extension_id);

 private:
  friend class AppFirewallHole;

  void Close(AppFirewallHole* hole);

  // AppWindowRegistry::Observer
  void OnAppWindowRemoved(AppWindow* app_window) override;
  void OnAppWindowHidden(AppWindow* app_window) override;
  void OnAppWindowShown(AppWindow* app_window, bool was_hidden) override;

  content::BrowserContext* context_;
  ScopedObserver<AppWindowRegistry, AppWindowRegistry::Observer> observer_;
  std::multimap<std::string, AppFirewallHole*> tracked_holes_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SOCKET_APP_FIREWALL_HOLE_MANAGER_H_
