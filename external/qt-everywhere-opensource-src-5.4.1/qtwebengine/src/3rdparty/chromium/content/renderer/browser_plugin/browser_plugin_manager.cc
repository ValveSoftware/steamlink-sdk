// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/browser_plugin_manager.h"

#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/browser_plugin/browser_plugin.h"
#include "content/renderer/browser_plugin/browser_plugin_manager_factory.h"
#include "content/renderer/browser_plugin/browser_plugin_manager_impl.h"

namespace content {

// static
BrowserPluginManagerFactory* BrowserPluginManager::factory_ = NULL;

BrowserPluginManager* BrowserPluginManager::Create(
    RenderViewImpl* render_view) {
  if (factory_)
    return factory_->CreateBrowserPluginManager(render_view);
  return new BrowserPluginManagerImpl(render_view);
}

BrowserPluginManager::BrowserPluginManager(RenderViewImpl* render_view)
    : RenderViewObserver(render_view),
      render_view_(render_view->AsWeakPtr()) {
}

BrowserPluginManager::~BrowserPluginManager() {
}

void BrowserPluginManager::AddBrowserPlugin(
    int guest_instance_id,
    BrowserPlugin* browser_plugin) {
  instances_.AddWithID(browser_plugin, guest_instance_id);
}

void BrowserPluginManager::RemoveBrowserPlugin(int guest_instance_id) {
  instances_.Remove(guest_instance_id);
}

BrowserPlugin* BrowserPluginManager::GetBrowserPlugin(
    int guest_instance_id) const {
  return instances_.Lookup(guest_instance_id);
}

void BrowserPluginManager::UpdateDeviceScaleFactor(float device_scale_factor) {
  IDMap<BrowserPlugin>::iterator iter(&instances_);
  while (!iter.IsAtEnd()) {
    iter.GetCurrentValue()->UpdateDeviceScaleFactor(device_scale_factor);
    iter.Advance();
  }
}

void BrowserPluginManager::UpdateFocusState() {
  IDMap<BrowserPlugin>::iterator iter(&instances_);
  while (!iter.IsAtEnd()) {
    iter.GetCurrentValue()->UpdateGuestFocusState();
    iter.Advance();
  }
}

}  // namespace content
