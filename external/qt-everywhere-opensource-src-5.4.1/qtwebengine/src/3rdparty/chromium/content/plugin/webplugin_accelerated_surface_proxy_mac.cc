// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <OpenGL/OpenGL.h>

#include "content/plugin/webplugin_accelerated_surface_proxy_mac.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "content/plugin/webplugin_proxy.h"
#include "content/public/common/content_switches.h"
#include "ui/surface/accelerated_surface_mac.h"
#include "ui/surface/transport_dib.h"

namespace content {

WebPluginAcceleratedSurfaceProxy* WebPluginAcceleratedSurfaceProxy::Create(
    WebPluginProxy* plugin_proxy,
    gfx::GpuPreference gpu_preference) {
  // This code path shouldn't be taken if CA plugins are disabled
  // because the CA drawing model shouldn't be advertised.
  DCHECK(!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableCoreAnimationPlugins));

  AcceleratedSurface* surface = new AcceleratedSurface;
  // It's possible for OpenGL to fail to initialize (e.g., if an incompatible
  // mode is forced via flags), so handle that gracefully.
  if (!surface->Initialize(NULL, true, gpu_preference)) {
    delete surface;
    return NULL;
  }

  return new WebPluginAcceleratedSurfaceProxy(plugin_proxy, surface);
}

WebPluginAcceleratedSurfaceProxy::WebPluginAcceleratedSurfaceProxy(
    WebPluginProxy* plugin_proxy,
    AcceleratedSurface* surface)
        : plugin_proxy_(plugin_proxy),
          surface_(surface) {
}

WebPluginAcceleratedSurfaceProxy::~WebPluginAcceleratedSurfaceProxy() {
  if (surface_) {
    surface_->Destroy();
    delete surface_;
    surface_ = NULL;
  }
}

void WebPluginAcceleratedSurfaceProxy::SetSize(const gfx::Size& size) {
  if (!surface_)
    return;

  uint32 io_surface_id = surface_->SetSurfaceSize(size);
  // If allocation fails for some reason, still inform the plugin proxy.
  plugin_proxy_->AcceleratedPluginAllocatedIOSurface(
      size.width(), size.height(), io_surface_id);
}

CGLContextObj WebPluginAcceleratedSurfaceProxy::context() {
  return surface_ ? surface_->context() : NULL;
}

void WebPluginAcceleratedSurfaceProxy::StartDrawing() {
  if (!surface_)
    return;

  surface_->MakeCurrent();
  surface_->Clear(gfx::Rect(surface_->GetSize()));
}

void WebPluginAcceleratedSurfaceProxy::EndDrawing() {
  if (!surface_)
    return;

  surface_->SwapBuffers();
  plugin_proxy_->AcceleratedPluginSwappedIOSurface();
}

}  // namespace content
