// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_MANAGER_H_
#define CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_MANAGER_H_

#include "base/id_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "content/public/renderer/render_view_observer.h"
#include "ipc/ipc_sender.h"

namespace blink {
class WebFrame;
struct WebPluginParams;
}

namespace content {

class BrowserPlugin;
class BrowserPluginManagerFactory;
class RenderViewImpl;

// BrowserPluginManager manages the routing of messages to the appropriate
// BrowserPlugin object based on its instance ID.
class CONTENT_EXPORT BrowserPluginManager
    : public RenderViewObserver,
      public base::RefCounted<BrowserPluginManager> {
 public:
  // Returns the one BrowserPluginManager for this process.
  static BrowserPluginManager* Create(RenderViewImpl* render_view);

  // Overrides factory for testing. Default (NULL) value indicates regular
  // (non-test) environment.
  static void set_factory_for_testing(BrowserPluginManagerFactory* factory) {
    BrowserPluginManager::factory_ = factory;
  }

  explicit BrowserPluginManager(RenderViewImpl* render_view);

  // Creates a new BrowserPlugin object.
  // BrowserPlugin is responsible for associating itself with the
  // BrowserPluginManager via AddBrowserPlugin. When it is destroyed, it is
  // responsible for removing its association via RemoveBrowserPlugin.
  virtual BrowserPlugin* CreateBrowserPlugin(
      RenderViewImpl* render_view,
      blink::WebFrame* frame,
      bool auto_navigate) = 0;

  void AddBrowserPlugin(int guest_instance_id, BrowserPlugin* browser_plugin);
  void RemoveBrowserPlugin(int guest_instance_id);
  BrowserPlugin* GetBrowserPlugin(int guest_instance_id) const;
  void UpdateDeviceScaleFactor(float device_scale_factor);
  void UpdateFocusState();
  RenderViewImpl* render_view() const { return render_view_.get(); }

  // RenderViewObserver implementation.

  // BrowserPluginManager must override the default Send behavior.
  virtual bool Send(IPC::Message* msg) OVERRIDE = 0;

  // Don't destroy the BrowserPluginManager when the RenderViewImpl goes away.
  // BrowserPluginManager's lifetime is managed by a reference count. Once
  // the host RenderViewImpl and all BrowserPlugins release their references,
  // then the BrowserPluginManager will be destroyed.
  virtual void OnDestruct() OVERRIDE {}

 protected:
  // Friend RefCounted so that the dtor can be non-public.
  friend class base::RefCounted<BrowserPluginManager>;

  // Static factory instance (always NULL for non-test).
  static BrowserPluginManagerFactory* factory_;

  virtual ~BrowserPluginManager();
  // This map is keyed by guest instance IDs.
  IDMap<BrowserPlugin> instances_;
  base::WeakPtr<RenderViewImpl> render_view_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginManager);
};

}  // namespace content

#endif //  CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_MANAGER_H_
