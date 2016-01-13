// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_COMPOSITOR_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_COMPOSITOR_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/shared_impl/compositor_layer_data.h"

namespace base {
class SharedMemory;
}  // namespace

namespace cc {
class Layer;
}  // namespace cc

namespace content {

class PepperPluginInstanceImpl;
class RendererPpapiHost;

class PepperCompositorHost : public ppapi::host::ResourceHost {
 public:
  PepperCompositorHost(RendererPpapiHost* host,
                       PP_Instance instance,
                       PP_Resource resource);
  // Associates this device with the given plugin instance. You can pass NULL
  // to clear the existing device. Returns true on success. In this case, a
  // repaint of the page will also be scheduled. Failure means that the device
  // is already bound to a different instance, and nothing will happen.
  bool BindToInstance(PepperPluginInstanceImpl* new_instance);

  const scoped_refptr<cc::Layer> layer() { return layer_; };

  void ViewInitiatedPaint();
  void ViewFlushedPaint();

 private:
  virtual ~PepperCompositorHost();

  void ImageReleased(int32_t id,
                     const scoped_ptr<base::SharedMemory>& shared_memory,
                     uint32_t sync_point,
                     bool is_lost);
  void ResourceReleased(int32_t id,
                        uint32_t sync_point,
                        bool is_lost);
  void SendCommitLayersReplyIfNecessary();
  void UpdateLayer(const scoped_refptr<cc::Layer>& layer,
                   const ppapi::CompositorLayerData* old_layer,
                   const ppapi::CompositorLayerData* new_layer,
                   scoped_ptr<base::SharedMemory> image_shm);

  // ResourceMessageHandler overrides:
  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

  // ppapi::host::ResourceHost overrides:
  virtual bool IsCompositorHost() OVERRIDE;

  // Message handlers:
  int32_t OnHostMsgCommitLayers(
      ppapi::host::HostMessageContext* context,
      const std::vector<ppapi::CompositorLayerData>& layers,
      bool reset);

  // Non-owning pointer to the plugin instance this context is currently bound
  // to, if any. If the context is currently unbound, this will be NULL.
  PepperPluginInstanceImpl* bound_instance_;

  // The toplevel cc::Layer. It is the parent of other cc::Layers.
  scoped_refptr<cc::Layer> layer_;

  // A list of layers. It is used for updating layers' properties in
  // subsequent CommitLayers() calls.
  struct LayerData {
    LayerData(const scoped_refptr<cc::Layer>& cc,
              const ppapi::CompositorLayerData& pp);
    ~LayerData();

    scoped_refptr<cc::Layer> cc_layer;
    ppapi::CompositorLayerData pp_layer;
  };
  std::vector<LayerData> layers_;

  ppapi::host::ReplyMessageContext commit_layers_reply_context_;

  base::WeakPtrFactory<PepperCompositorHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperCompositorHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_COMPOSITOR_H_
