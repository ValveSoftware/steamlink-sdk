// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/ppb_graphics_3d_impl.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/gpu/client/command_buffer_proxy_impl.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/plugin_module.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "ppapi/c/ppp_graphics_3d.h"
#include "ppapi/thunk/enter.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "webkit/common/webpreferences.h"

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Graphics3D_API;
using blink::WebConsoleMessage;
using blink::WebLocalFrame;
using blink::WebPluginContainer;
using blink::WebString;

namespace content {

namespace {
const int32 kCommandBufferSize = 1024 * 1024;
const int32 kTransferBufferSize = 1024 * 1024;

}  // namespace.

PPB_Graphics3D_Impl::PPB_Graphics3D_Impl(PP_Instance instance)
    : PPB_Graphics3D_Shared(instance),
      bound_to_instance_(false),
      commit_pending_(false),
      sync_point_(0),
      has_alpha_(false),
      command_buffer_(NULL),
      weak_ptr_factory_(this) {}

PPB_Graphics3D_Impl::~PPB_Graphics3D_Impl() {
  DestroyGLES2Impl();
  if (command_buffer_) {
    DCHECK(channel_.get());
    channel_->DestroyCommandBuffer(command_buffer_);
    command_buffer_ = NULL;
  }

  channel_ = NULL;
}

// static
PP_Resource PPB_Graphics3D_Impl::Create(PP_Instance instance,
                                        PP_Resource share_context,
                                        const int32_t* attrib_list) {
  PPB_Graphics3D_API* share_api = NULL;
  if (share_context) {
    EnterResourceNoLock<PPB_Graphics3D_API> enter(share_context, true);
    if (enter.failed())
      return 0;
    share_api = enter.object();
  }
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d(
      new PPB_Graphics3D_Impl(instance));
  if (!graphics_3d->Init(share_api, attrib_list))
    return 0;
  return graphics_3d->GetReference();
}

// static
PP_Resource PPB_Graphics3D_Impl::CreateRaw(PP_Instance instance,
                                           PP_Resource share_context,
                                           const int32_t* attrib_list) {
  PPB_Graphics3D_API* share_api = NULL;
  if (share_context) {
    EnterResourceNoLock<PPB_Graphics3D_API> enter(share_context, true);
    if (enter.failed())
      return 0;
    share_api = enter.object();
  }
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d(
      new PPB_Graphics3D_Impl(instance));
  if (!graphics_3d->InitRaw(share_api, attrib_list))
    return 0;
  return graphics_3d->GetReference();
}

PP_Bool PPB_Graphics3D_Impl::SetGetBuffer(int32_t transfer_buffer_id) {
  GetCommandBuffer()->SetGetBuffer(transfer_buffer_id);
  return PP_TRUE;
}

scoped_refptr<gpu::Buffer> PPB_Graphics3D_Impl::CreateTransferBuffer(
    uint32_t size,
    int32_t* id) {
  return GetCommandBuffer()->CreateTransferBuffer(size, id);
}

PP_Bool PPB_Graphics3D_Impl::DestroyTransferBuffer(int32_t id) {
  GetCommandBuffer()->DestroyTransferBuffer(id);
  return PP_TRUE;
}

PP_Bool PPB_Graphics3D_Impl::Flush(int32_t put_offset) {
  GetCommandBuffer()->Flush(put_offset);
  return PP_TRUE;
}

gpu::CommandBuffer::State PPB_Graphics3D_Impl::WaitForTokenInRange(
    int32_t start,
    int32_t end) {
  GetCommandBuffer()->WaitForTokenInRange(start, end);
  return GetCommandBuffer()->GetLastState();
}

gpu::CommandBuffer::State PPB_Graphics3D_Impl::WaitForGetOffsetInRange(
    int32_t start,
    int32_t end) {
  GetCommandBuffer()->WaitForGetOffsetInRange(start, end);
  return GetCommandBuffer()->GetLastState();
}

uint32_t PPB_Graphics3D_Impl::InsertSyncPoint() {
  return command_buffer_->InsertSyncPoint();
}

bool PPB_Graphics3D_Impl::BindToInstance(bool bind) {
  bound_to_instance_ = bind;
  return true;
}

bool PPB_Graphics3D_Impl::IsOpaque() { return !has_alpha_; }

void PPB_Graphics3D_Impl::ViewInitiatedPaint() {
  commit_pending_ = false;

  if (HasPendingSwap())
    SwapBuffersACK(PP_OK);
}

void PPB_Graphics3D_Impl::ViewFlushedPaint() {}

int PPB_Graphics3D_Impl::GetCommandBufferRouteId() {
  DCHECK(command_buffer_);
  return command_buffer_->GetRouteID();
}

gpu::CommandBuffer* PPB_Graphics3D_Impl::GetCommandBuffer() {
  return command_buffer_;
}

gpu::GpuControl* PPB_Graphics3D_Impl::GetGpuControl() {
  return command_buffer_;
}

int32 PPB_Graphics3D_Impl::DoSwapBuffers() {
  DCHECK(command_buffer_);
  // We do not have a GLES2 implementation when using an OOP proxy.
  // The plugin-side proxy is responsible for adding the SwapBuffers command
  // to the command buffer in that case.
  if (gles2_impl())
    gles2_impl()->SwapBuffers();

  // Since the backing texture has been updated, a new sync point should be
  // inserted.
  sync_point_ = command_buffer_->InsertSyncPoint();

  if (bound_to_instance_) {
    // If we are bound to the instance, we need to ask the compositor
    // to commit our backing texture so that the graphics appears on the page.
    // When the backing texture will be committed we get notified via
    // ViewFlushedPaint().
    //
    // Don't need to check for NULL from GetPluginInstance since when we're
    // bound, we know our instance is valid.
    HostGlobals::Get()->GetInstance(pp_instance())->CommitBackingTexture();
    commit_pending_ = true;
  } else {
    // Wait for the command to complete on the GPU to allow for throttling.
    command_buffer_->Echo(base::Bind(&PPB_Graphics3D_Impl::OnSwapBuffers,
                                     weak_ptr_factory_.GetWeakPtr()));
  }

  return PP_OK_COMPLETIONPENDING;
}

bool PPB_Graphics3D_Impl::Init(PPB_Graphics3D_API* share_context,
                               const int32_t* attrib_list) {
  if (!InitRaw(share_context, attrib_list))
    return false;

  gpu::gles2::GLES2Implementation* share_gles2 = NULL;
  if (share_context) {
    share_gles2 =
        static_cast<PPB_Graphics3D_Shared*>(share_context)->gles2_impl();
  }

  return CreateGLES2Impl(kCommandBufferSize, kTransferBufferSize, share_gles2);
}

bool PPB_Graphics3D_Impl::InitRaw(PPB_Graphics3D_API* share_context,
                                  const int32_t* attrib_list) {
  PepperPluginInstanceImpl* plugin_instance =
      HostGlobals::Get()->GetInstance(pp_instance());
  if (!plugin_instance)
    return false;

  const WebPreferences& prefs =
      static_cast<RenderViewImpl*>(plugin_instance->GetRenderView())
          ->webkit_preferences();
  // 3D access might be disabled or blacklisted.
  if (!prefs.pepper_3d_enabled)
    return false;

  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  if (!render_thread)
    return false;

  channel_ = render_thread->EstablishGpuChannelSync(
      CAUSE_FOR_GPU_LAUNCH_PEPPERPLATFORMCONTEXT3DIMPL_INITIALIZE);
  if (!channel_.get())
    return false;

  gfx::Size surface_size;
  std::vector<int32> attribs;
  gfx::GpuPreference gpu_preference = gfx::PreferDiscreteGpu;
  // TODO(alokp): Change GpuChannelHost::CreateOffscreenCommandBuffer()
  // interface to accept width and height in the attrib_list so that
  // we do not need to filter for width and height here.
  if (attrib_list) {
    for (const int32_t* attr = attrib_list; attr[0] != PP_GRAPHICS3DATTRIB_NONE;
         attr += 2) {
      switch (attr[0]) {
        case PP_GRAPHICS3DATTRIB_WIDTH:
          surface_size.set_width(attr[1]);
          break;
        case PP_GRAPHICS3DATTRIB_HEIGHT:
          surface_size.set_height(attr[1]);
          break;
        case PP_GRAPHICS3DATTRIB_GPU_PREFERENCE:
          gpu_preference =
              (attr[1] == PP_GRAPHICS3DATTRIB_GPU_PREFERENCE_LOW_POWER)
                  ? gfx::PreferIntegratedGpu
                  : gfx::PreferDiscreteGpu;
          break;
        case PP_GRAPHICS3DATTRIB_ALPHA_SIZE:
          has_alpha_ = attr[1] > 0;
        // fall-through
        default:
          attribs.push_back(attr[0]);
          attribs.push_back(attr[1]);
          break;
      }
    }
    attribs.push_back(PP_GRAPHICS3DATTRIB_NONE);
  }

  CommandBufferProxyImpl* share_buffer = NULL;
  if (share_context) {
    PPB_Graphics3D_Impl* share_graphics =
        static_cast<PPB_Graphics3D_Impl*>(share_context);
    share_buffer = share_graphics->command_buffer_;
  }

  command_buffer_ = channel_->CreateOffscreenCommandBuffer(
      surface_size, share_buffer, attribs, GURL::EmptyGURL(), gpu_preference);
  if (!command_buffer_)
    return false;
  if (!command_buffer_->Initialize())
    return false;
  mailbox_ = gpu::Mailbox::Generate();
  if (!command_buffer_->ProduceFrontBuffer(mailbox_))
    return false;
  sync_point_ = command_buffer_->InsertSyncPoint();

  command_buffer_->SetChannelErrorCallback(base::Bind(
      &PPB_Graphics3D_Impl::OnContextLost, weak_ptr_factory_.GetWeakPtr()));

  command_buffer_->SetOnConsoleMessageCallback(base::Bind(
      &PPB_Graphics3D_Impl::OnConsoleMessage, weak_ptr_factory_.GetWeakPtr()));
  return true;
}

void PPB_Graphics3D_Impl::OnConsoleMessage(const std::string& message, int id) {
  if (!bound_to_instance_)
    return;
  WebPluginContainer* container =
      HostGlobals::Get()->GetInstance(pp_instance())->container();
  if (!container)
    return;
  WebLocalFrame* frame = container->element().document().frame();
  if (!frame)
    return;
  WebConsoleMessage console_message = WebConsoleMessage(
      WebConsoleMessage::LevelError, WebString(base::UTF8ToUTF16(message)));
  frame->addMessageToConsole(console_message);
}

void PPB_Graphics3D_Impl::OnSwapBuffers() {
  if (HasPendingSwap()) {
    // If we're off-screen, no need to trigger and wait for compositing.
    // Just send the swap-buffers ACK to the plugin immediately.
    commit_pending_ = false;
    SwapBuffersACK(PP_OK);
  }
}

void PPB_Graphics3D_Impl::OnContextLost() {
  // Don't need to check for NULL from GetPluginInstance since when we're
  // bound, we know our instance is valid.
  if (bound_to_instance_) {
    HostGlobals::Get()->GetInstance(pp_instance())->BindGraphics(pp_instance(),
                                                                 0);
  }

  // Send context lost to plugin. This may have been caused by a PPAPI call, so
  // avoid re-entering.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&PPB_Graphics3D_Impl::SendContextLost,
                 weak_ptr_factory_.GetWeakPtr()));
}

void PPB_Graphics3D_Impl::SendContextLost() {
  // By the time we run this, the instance may have been deleted, or in the
  // process of being deleted. Even in the latter case, we don't want to send a
  // callback after DidDestroy.
  PepperPluginInstanceImpl* instance =
      HostGlobals::Get()->GetInstance(pp_instance());
  if (!instance || !instance->container())
    return;

  // This PPB_Graphics3D_Impl could be deleted during the call to
  // GetPluginInterface (which sends a sync message in some cases). We still
  // send the Graphics3DContextLost to the plugin; the instance may care about
  // that event even though this context has been destroyed.
  PP_Instance this_pp_instance = pp_instance();
  const PPP_Graphics3D* ppp_graphics_3d = static_cast<const PPP_Graphics3D*>(
      instance->module()->GetPluginInterface(PPP_GRAPHICS_3D_INTERFACE));
  // We have to check *again* that the instance exists, because it could have
  // been deleted during GetPluginInterface(). Even the PluginModule could be
  // deleted, but in that case, the instance should also be gone, so the
  // GetInstance check covers both cases.
  if (ppp_graphics_3d && HostGlobals::Get()->GetInstance(this_pp_instance))
    ppp_graphics_3d->Graphics3DContextLost(this_pp_instance);
}

}  // namespace content
