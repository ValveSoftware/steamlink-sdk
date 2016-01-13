// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_compositor_host.h"

#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "cc/layers/layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/texture_layer.h"
#include "cc/resources/texture_mailbox.h"
#include "cc/trees/layer_tree_host.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/pepper/gfx_conversion.h"
#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/ppb_image_data_impl.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_image_data_api.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/transform.h"

using ppapi::host::HostMessageContext;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_ImageData_API;

namespace content {

namespace {

int32_t VerifyCommittedLayer(
    const ppapi::CompositorLayerData* old_layer,
    const ppapi::CompositorLayerData* new_layer,
    scoped_ptr<base::SharedMemory>* image_shm) {
  if (!new_layer->is_valid())
    return PP_ERROR_BADARGUMENT;

  if (new_layer->color) {
    // Make sure the old layer is a color layer too.
    if (old_layer && !old_layer->color)
      return PP_ERROR_BADARGUMENT;
    return PP_OK;
  }

  if (new_layer->texture) {
    if (old_layer) {
      // Make sure the old layer is a texture layer too.
      if (!new_layer->texture)
        return PP_ERROR_BADARGUMENT;
      // The mailbox should be same, if the resource_id is not changed.
      if (new_layer->common.resource_id == old_layer->common.resource_id) {
        if (new_layer->texture->mailbox != old_layer->texture->mailbox)
          return PP_ERROR_BADARGUMENT;
        return PP_OK;
      }
    }
    if (!new_layer->texture->mailbox.Verify())
      return PP_ERROR_BADARGUMENT;
    return PP_OK;
  }

  if (new_layer->image) {
    if (old_layer) {
      // Make sure the old layer is an image layer too.
      if (!new_layer->image)
        return PP_ERROR_BADARGUMENT;
      // The image data resource should be same, if the resource_id is not
      // changed.
      if (new_layer->common.resource_id == old_layer->common.resource_id) {
        if (new_layer->image->resource != old_layer->image->resource)
          return PP_ERROR_BADARGUMENT;
        return PP_OK;
      }
    }
    EnterResourceNoLock<PPB_ImageData_API> enter(new_layer->image->resource,
                                                 true);
    if (enter.failed())
      return PP_ERROR_BADRESOURCE;

    // TODO(penghuang): support all kinds of image.
    PP_ImageDataDesc desc;
    if (enter.object()->Describe(&desc) != PP_TRUE ||
        desc.stride != desc.size.width * 4 ||
        desc.format != PP_IMAGEDATAFORMAT_RGBA_PREMUL) {
      return PP_ERROR_BADARGUMENT;
    }

    int handle;
    uint32_t byte_count;
    if (enter.object()->GetSharedMemory(&handle, &byte_count) != PP_OK)
      return PP_ERROR_FAILED;

#if defined(OS_WIN)
    base::SharedMemoryHandle shm_handle;
    if (!::DuplicateHandle(::GetCurrentProcess(),
                           reinterpret_cast<base::SharedMemoryHandle>(handle),
                           ::GetCurrentProcess(),
                           &shm_handle,
                           0,
                           FALSE,
                           DUPLICATE_SAME_ACCESS)) {
      return PP_ERROR_FAILED;
    }
#else
    base::SharedMemoryHandle shm_handle(dup(handle), false);
#endif
    image_shm->reset(new base::SharedMemory(shm_handle, true));
    if (!(*image_shm)->Map(desc.stride * desc.size.height)) {
      image_shm->reset();
      return PP_ERROR_NOMEMORY;
    }
    return PP_OK;
  }

  return PP_ERROR_BADARGUMENT;
}

}  // namespace

PepperCompositorHost::LayerData::LayerData(
    const scoped_refptr<cc::Layer>& cc,
    const ppapi::CompositorLayerData& pp) : cc_layer(cc), pp_layer(pp) {}

PepperCompositorHost::LayerData::~LayerData() {}

PepperCompositorHost::PepperCompositorHost(
    RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      bound_instance_(NULL),
      weak_factory_(this) {
  layer_ = cc::Layer::Create();
  // TODO(penghuang): SetMasksToBounds() can be expensive if the layer is
  // transformed. Possibly better could be to explicitly clip the child layers
  // (by modifying their bounds).
  layer_->SetMasksToBounds(true);
  layer_->SetIsDrawable(true);
}

PepperCompositorHost::~PepperCompositorHost() {
  // Unbind from the instance when destroyed if we're still bound.
  if (bound_instance_)
    bound_instance_->BindGraphics(bound_instance_->pp_instance(), 0);
}

bool PepperCompositorHost::BindToInstance(
    PepperPluginInstanceImpl* new_instance) {
  if (new_instance && new_instance->pp_instance() != pp_instance())
    return false;  // Can't bind other instance's contexts.
  if (bound_instance_ == new_instance)
    return true;  // Rebinding the same device, nothing to do.
  if (bound_instance_ && new_instance)
    return false;  // Can't change a bound device.
  bound_instance_ = new_instance;
  if (!bound_instance_)
    SendCommitLayersReplyIfNecessary();

  return true;
}

void PepperCompositorHost::ViewInitiatedPaint() {
  SendCommitLayersReplyIfNecessary();
}

void PepperCompositorHost::ViewFlushedPaint() {}

void PepperCompositorHost::ImageReleased(
    int32_t id,
    const scoped_ptr<base::SharedMemory>& shared_memory,
    uint32_t sync_point,
    bool is_lost) {
  ResourceReleased(id, sync_point, is_lost);
}

void PepperCompositorHost::ResourceReleased(int32_t id,
                                            uint32_t sync_point,
                                            bool is_lost) {
  host()->SendUnsolicitedReply(
      pp_resource(),
      PpapiPluginMsg_Compositor_ReleaseResource(id, sync_point, is_lost));
}

void PepperCompositorHost::SendCommitLayersReplyIfNecessary() {
  if (!commit_layers_reply_context_.is_valid())
    return;
  host()->SendReply(commit_layers_reply_context_,
                    PpapiPluginMsg_Compositor_CommitLayersReply());
  commit_layers_reply_context_ = ppapi::host::ReplyMessageContext();
}

void PepperCompositorHost::UpdateLayer(
    const scoped_refptr<cc::Layer>& layer,
    const ppapi::CompositorLayerData* old_layer,
    const ppapi::CompositorLayerData* new_layer,
    scoped_ptr<base::SharedMemory> image_shm) {
  // Always update properties on cc::Layer, because cc::Layer
  // will ignore any setting with unchanged value.
  layer->SetIsDrawable(true);
  layer->SetBlendMode(SkXfermode::kSrcOver_Mode);
  layer->SetOpacity(new_layer->common.opacity);
  layer->SetBounds(PP_ToGfxSize(new_layer->common.size));
  layer->SetTransformOrigin(gfx::Point3F(new_layer->common.size.width / 2,
                                         new_layer->common.size.height / 2,
                                         0.0f));

  gfx::Transform transform(gfx::Transform::kSkipInitialization);
  transform.matrix().setColMajorf(new_layer->common.transform.matrix);
  layer->SetTransform(transform);

  // Consider a (0,0,0,0) rect as no clip rect.
  if (new_layer->common.clip_rect.point.x != 0 ||
      new_layer->common.clip_rect.point.y != 0 ||
      new_layer->common.clip_rect.size.width != 0 ||
      new_layer->common.clip_rect.size.height != 0) {
    scoped_refptr<cc::Layer> clip_parent = layer->parent();
    if (clip_parent == layer_) {
      // Create a clip parent layer, if it does not exist.
      clip_parent = cc::Layer::Create();
      clip_parent->SetMasksToBounds(true);
      clip_parent->SetIsDrawable(true);
      layer_->ReplaceChild(layer, clip_parent);
      clip_parent->AddChild(layer);
    }
    gfx::Point position = PP_ToGfxPoint(new_layer->common.clip_rect.point);
    clip_parent->SetPosition(position);
    clip_parent->SetBounds(PP_ToGfxSize(new_layer->common.clip_rect.size));
    layer->SetPosition(gfx::Point(-position.x(), -position.y()));
  } else if (layer->parent() != layer_) {
    // Remove the clip parent layer.
    layer_->ReplaceChild(layer->parent(), layer);
    layer->SetPosition(gfx::Point());
  }

  if (new_layer->color) {
    layer->SetBackgroundColor(SkColorSetARGBMacro(
        new_layer->color->alpha * 255,
        new_layer->color->red * 255,
        new_layer->color->green * 255,
        new_layer->color->blue * 255));
    return;
  }

  if (new_layer->texture) {
    scoped_refptr<cc::TextureLayer> texture_layer(
        static_cast<cc::TextureLayer*>(layer.get()));
    if (!old_layer ||
        new_layer->common.resource_id != old_layer->common.resource_id) {
      cc::TextureMailbox mailbox(new_layer->texture->mailbox,
                                 GL_TEXTURE_2D,
                                 new_layer->texture->sync_point);
      texture_layer->SetTextureMailbox(mailbox,
          cc::SingleReleaseCallback::Create(
              base::Bind(&PepperCompositorHost::ResourceReleased,
                         weak_factory_.GetWeakPtr(),
                         new_layer->common.resource_id)));;
    }
    texture_layer->SetPremultipliedAlpha(new_layer->texture->premult_alpha);
    gfx::RectF rect = PP_ToGfxRectF(new_layer->texture->source_rect);
    texture_layer->SetUV(rect.origin(), rect.bottom_right());
    return;
  }

  if (new_layer->image) {
    if (!old_layer ||
        new_layer->common.resource_id != old_layer->common.resource_id) {
      scoped_refptr<cc::TextureLayer> image_layer(
          static_cast<cc::TextureLayer*>(layer.get()));
      EnterResourceNoLock<PPB_ImageData_API> enter(new_layer->image->resource,
                                                   true);
      DCHECK(enter.succeeded());

      // TODO(penghuang): support all kinds of image.
      PP_ImageDataDesc desc;
      PP_Bool rv = enter.object()->Describe(&desc);
      DCHECK_EQ(rv, PP_TRUE);
      DCHECK_EQ(desc.stride, desc.size.width * 4);
      DCHECK_EQ(desc.format, PP_IMAGEDATAFORMAT_RGBA_PREMUL);

      cc::TextureMailbox mailbox(image_shm.get(),
                                 PP_ToGfxSize(desc.size));
      image_layer->SetTextureMailbox(mailbox,
          cc::SingleReleaseCallback::Create(
              base::Bind(&PepperCompositorHost::ImageReleased,
                         weak_factory_.GetWeakPtr(),
                         new_layer->common.resource_id,
                         base::Passed(&image_shm))));

      // ImageData is always premultiplied alpha.
      image_layer->SetPremultipliedAlpha(true);
    }
    return;
  }
  // Should not be reached.
  NOTREACHED();
}

int32_t PepperCompositorHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperCompositorHost, msg)
  PPAPI_DISPATCH_HOST_RESOURCE_CALL(
      PpapiHostMsg_Compositor_CommitLayers, OnHostMsgCommitLayers)
  PPAPI_END_MESSAGE_MAP()
  return ppapi::host::ResourceHost::OnResourceMessageReceived(msg, context);
}

bool PepperCompositorHost::IsCompositorHost() {
  return true;
}

int32_t PepperCompositorHost::OnHostMsgCommitLayers(
    HostMessageContext* context,
    const std::vector<ppapi::CompositorLayerData>& layers,
    bool reset) {
  if (commit_layers_reply_context_.is_valid())
    return PP_ERROR_INPROGRESS;

  scoped_ptr<scoped_ptr<base::SharedMemory>[]> image_shms;
  if (layers.size() > 0) {
    image_shms.reset(new scoped_ptr<base::SharedMemory>[layers.size()]);
    if (!image_shms)
      return PP_ERROR_NOMEMORY;
    // Verfiy the layers first, if an error happens, we will return the error to
    // plugin and keep current layers set by the previous CommitLayers()
    // unchanged.
    for (size_t i = 0; i < layers.size(); ++i) {
      const ppapi::CompositorLayerData* old_layer = NULL;
      if (!reset && i < layers_.size())
        old_layer = &layers_[i].pp_layer;
      int32_t rv = VerifyCommittedLayer(old_layer, &layers[i], &image_shms[i]);
      if (rv != PP_OK)
        return rv;
    }
  }

  // ResetLayers() has been called, we need rebuild layer stack.
  if (reset) {
    layer_->RemoveAllChildren();
    layers_.clear();
  }

  for (size_t i = 0; i < layers.size(); ++i) {
    const ppapi::CompositorLayerData* pp_layer = &layers[i];
    LayerData* data = i >= layers_.size() ? NULL : &layers_[i];
    DCHECK(!data || data->cc_layer);
    scoped_refptr<cc::Layer> cc_layer = data ? data->cc_layer : NULL;
    ppapi::CompositorLayerData* old_layer = data ? &data->pp_layer : NULL;

    if (!cc_layer) {
      if (pp_layer->color)
        cc_layer = cc::SolidColorLayer::Create();
      else if (pp_layer->texture || pp_layer->image)
        cc_layer = cc::TextureLayer::CreateForMailbox(NULL);
      layer_->AddChild(cc_layer);
    }

    UpdateLayer(cc_layer, old_layer, pp_layer, image_shms[i].Pass());

    if (old_layer)
      *old_layer = *pp_layer;
    else
      layers_.push_back(LayerData(cc_layer, *pp_layer));
  }

  // We need to force a commit for each CommitLayers() call, even if no layers
  // changed since the last call to CommitLayers(). This is so
  // WiewInitiatedPaint() will always be called.
  if (layer_->layer_tree_host())
    layer_->layer_tree_host()->SetNeedsCommit();

  // If the host is not bound to the instance, return PP_OK immediately.
  if (!bound_instance_)
    return PP_OK;

  commit_layers_reply_context_ = context->MakeReplyMessageContext();
  return PP_OK_COMPLETIONPENDING;
}

}  // namespace content
