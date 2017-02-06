// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_CHILD_FRAME_COMPOSITING_HELPER_H_
#define CONTENT_RENDERER_CHILD_FRAME_COMPOSITING_HELPER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "cc/surfaces/surface_id.h"
#include "content/common/content_export.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class SharedMemory;
}

namespace cc {
struct SurfaceSequence;

class CompositorFrame;
class Layer;
class SolidColorLayer;
class SurfaceLayer;
}

namespace blink {
class WebFrame;
class WebPluginContainer;
class WebLayer;
}

namespace gfx {
class Rect;
class Size;
}

struct FrameHostMsg_CompositorFrameSwappedACK_Params;
struct FrameHostMsg_ReclaimCompositorResources_Params;

namespace content {

class BrowserPlugin;
class BrowserPluginManager;
class RenderFrameProxy;
class ThreadSafeSender;

class CONTENT_EXPORT ChildFrameCompositingHelper
    : public base::RefCounted<ChildFrameCompositingHelper> {
 public:
  static ChildFrameCompositingHelper* CreateForBrowserPlugin(
      const base::WeakPtr<BrowserPlugin>& browser_plugin);
  static ChildFrameCompositingHelper* CreateForRenderFrameProxy(
      RenderFrameProxy* render_frame_proxy);

  void OnContainerDestroy();
  void OnSetSurface(const cc::SurfaceId& surface_id,
                    const gfx::Size& frame_size,
                    float scale_factor,
                    const cc::SurfaceSequence& sequence);
  void UpdateVisibility(bool);
  void ChildFrameGone();

  cc::SurfaceId surface_id() const { return surface_id_; }

 protected:
  // Friend RefCounted so that the dtor can be non-public.
  friend class base::RefCounted<ChildFrameCompositingHelper>;

 private:
  ChildFrameCompositingHelper(
      const base::WeakPtr<BrowserPlugin>& browser_plugin,
      blink::WebFrame* frame,
      RenderFrameProxy* render_frame_proxy,
      int host_routing_id);

  virtual ~ChildFrameCompositingHelper();

  BrowserPluginManager* GetBrowserPluginManager();
  blink::WebPluginContainer* GetContainer();
  int GetInstanceID();

  void CheckSizeAndAdjustLayerProperties(const gfx::Size& new_size,
                                         float device_scale_factor,
                                         cc::Layer* layer);
  static void SatisfyCallback(scoped_refptr<ThreadSafeSender> sender,
                              int host_routing_id,
                              const cc::SurfaceSequence& sequence);
  static void SatisfyCallbackBrowserPlugin(
      scoped_refptr<ThreadSafeSender> sender,
      int host_routing_id,
      int browser_plugin_instance_id,
      const cc::SurfaceSequence& sequence);
  static void RequireCallback(scoped_refptr<ThreadSafeSender> sender,
                              int host_routing_id,
                              const cc::SurfaceId& id,
                              const cc::SurfaceSequence& sequence);
  static void RequireCallbackBrowserPlugin(
      scoped_refptr<ThreadSafeSender> sender,
      int host_routing_id,
      int browser_plugin_instance_id,
      const cc::SurfaceId& id,
      const cc::SurfaceSequence& sequence);
  void UpdateWebLayer(blink::WebLayer* layer);

  int host_routing_id_;

  gfx::Size buffer_size_;

  // The lifetime of this weak pointer should be greater than the lifetime of
  // other member objects, as they may access this pointer during their
  // destruction.
  base::WeakPtr<BrowserPlugin> browser_plugin_;
  RenderFrameProxy* render_frame_proxy_;

  std::unique_ptr<blink::WebLayer> web_layer_;
  cc::SurfaceId surface_id_;
  blink::WebFrame* frame_;

  DISALLOW_COPY_AND_ASSIGN(ChildFrameCompositingHelper);
};

}  // namespace content

#endif  // CONTENT_RENDERER_CHILD_FRAME_COMPOSITING_HELPER_H_
