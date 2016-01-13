// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_channel_manager.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_memory_manager.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/sync_point_manager.h"
#include "content/common/message_router.h"
#include "content/public/browser/content_browser_client.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_program_cache.h"
#include "gpu/command_buffer/service/shader_translator_cache.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_share_group.h"

namespace content {

GpuChannelManager::ImageOperation::ImageOperation(
    int32 sync_point, base::Closure callback)
    : sync_point(sync_point),
      callback(callback) {
}

GpuChannelManager::ImageOperation::~ImageOperation() {
}

GpuChannelManager::GpuChannelManager(MessageRouter* router,
                                     GpuWatchdog* watchdog,
                                     base::MessageLoopProxy* io_message_loop,
                                     base::WaitableEvent* shutdown_event)
    : weak_factory_(this),
      io_message_loop_(io_message_loop),
      shutdown_event_(shutdown_event),
      router_(router),
      gpu_memory_manager_(
          this,
          GpuMemoryManager::kDefaultMaxSurfacesWithFrontbufferSoftLimit),
      watchdog_(watchdog),
      sync_point_manager_(new SyncPointManager) {
  DCHECK(router_);
  DCHECK(io_message_loop);
  DCHECK(shutdown_event);
}

GpuChannelManager::~GpuChannelManager() {
  gpu_channels_.clear();
  if (default_offscreen_surface_.get()) {
    default_offscreen_surface_->Destroy();
    default_offscreen_surface_ = NULL;
  }
  DCHECK(image_operations_.empty());
  DCHECK(sync_point_gl_fences_.empty());
}

gpu::gles2::ProgramCache* GpuChannelManager::program_cache() {
  if (!program_cache_.get() &&
      (gfx::g_driver_gl.ext.b_GL_ARB_get_program_binary ||
       gfx::g_driver_gl.ext.b_GL_OES_get_program_binary) &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGpuProgramCache)) {
    program_cache_.reset(new gpu::gles2::MemoryProgramCache());
  }
  return program_cache_.get();
}

gpu::gles2::ShaderTranslatorCache*
GpuChannelManager::shader_translator_cache() {
  if (!shader_translator_cache_.get())
    shader_translator_cache_ = new gpu::gles2::ShaderTranslatorCache;
  return shader_translator_cache_.get();
}

void GpuChannelManager::RemoveChannel(int client_id) {
  Send(new GpuHostMsg_DestroyChannel(client_id));
  gpu_channels_.erase(client_id);
}

int GpuChannelManager::GenerateRouteID() {
  static int last_id = 0;
  return ++last_id;
}

void GpuChannelManager::AddRoute(int32 routing_id, IPC::Listener* listener) {
  router_->AddRoute(routing_id, listener);
}

void GpuChannelManager::RemoveRoute(int32 routing_id) {
  router_->RemoveRoute(routing_id);
}

GpuChannel* GpuChannelManager::LookupChannel(int32 client_id) {
  GpuChannelMap::const_iterator iter = gpu_channels_.find(client_id);
  if (iter == gpu_channels_.end())
    return NULL;
  else
    return iter->second;
}

bool GpuChannelManager::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuChannelManager, msg)
    IPC_MESSAGE_HANDLER(GpuMsg_EstablishChannel, OnEstablishChannel)
    IPC_MESSAGE_HANDLER(GpuMsg_CloseChannel, OnCloseChannel)
    IPC_MESSAGE_HANDLER(GpuMsg_CreateViewCommandBuffer,
                        OnCreateViewCommandBuffer)
    IPC_MESSAGE_HANDLER(GpuMsg_CreateImage, OnCreateImage)
    IPC_MESSAGE_HANDLER(GpuMsg_DeleteImage, OnDeleteImage)
    IPC_MESSAGE_HANDLER(GpuMsg_CreateGpuMemoryBuffer, OnCreateGpuMemoryBuffer)
    IPC_MESSAGE_HANDLER(GpuMsg_DestroyGpuMemoryBuffer, OnDestroyGpuMemoryBuffer)
    IPC_MESSAGE_HANDLER(GpuMsg_LoadedShader, OnLoadedShader)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool GpuChannelManager::Send(IPC::Message* msg) { return router_->Send(msg); }

void GpuChannelManager::OnEstablishChannel(int client_id, bool share_context) {
  IPC::ChannelHandle channel_handle;

  gfx::GLShareGroup* share_group = NULL;
  gpu::gles2::MailboxManager* mailbox_manager = NULL;
  if (share_context) {
    if (!share_group_.get()) {
      share_group_ = new gfx::GLShareGroup;
      DCHECK(!mailbox_manager_.get());
      mailbox_manager_ = new gpu::gles2::MailboxManager;
    }
    // Qt: Ask the browser client at the top to manage the context sharing.
    // This can only work with --in-process-gpu or --single-process.
    if (GetContentClient()->browser() && GetContentClient()->browser()->GetInProcessGpuShareGroup())
      share_group = GetContentClient()->browser()->GetInProcessGpuShareGroup();
    else
      share_group = share_group_.get();
    mailbox_manager = mailbox_manager_.get();
  }

  scoped_ptr<GpuChannel> channel(new GpuChannel(
      this, watchdog_, share_group, mailbox_manager, client_id, false));
  channel->Init(io_message_loop_.get(), shutdown_event_);
  channel_handle.name = channel->GetChannelName();

#if defined(OS_POSIX)
  // On POSIX, pass the renderer-side FD. Also mark it as auto-close so
  // that it gets closed after it has been sent.
  int renderer_fd = channel->TakeRendererFileDescriptor();
  DCHECK_NE(-1, renderer_fd);
  channel_handle.socket = base::FileDescriptor(renderer_fd, true);
#endif

  gpu_channels_.set(client_id, channel.Pass());

  Send(new GpuHostMsg_ChannelEstablished(channel_handle));
}

void GpuChannelManager::OnCloseChannel(
    const IPC::ChannelHandle& channel_handle) {
  for (GpuChannelMap::iterator iter = gpu_channels_.begin();
       iter != gpu_channels_.end(); ++iter) {
    if (iter->second->GetChannelName() == channel_handle.name) {
      gpu_channels_.erase(iter);
      return;
    }
  }
}

void GpuChannelManager::OnCreateViewCommandBuffer(
    const gfx::GLSurfaceHandle& window,
    int32 surface_id,
    int32 client_id,
    const GPUCreateCommandBufferConfig& init_params,
    int32 route_id) {
  DCHECK(surface_id);
  CreateCommandBufferResult result = CREATE_COMMAND_BUFFER_FAILED;

  GpuChannelMap::const_iterator iter = gpu_channels_.find(client_id);
  if (iter != gpu_channels_.end()) {
    result = iter->second->CreateViewCommandBuffer(
        window, surface_id, init_params, route_id);
  }

  Send(new GpuHostMsg_CommandBufferCreated(result));
}

void GpuChannelManager::CreateImage(
    gfx::PluginWindowHandle window, int32 client_id, int32 image_id) {
  gfx::Size size;

  GpuChannelMap::const_iterator iter = gpu_channels_.find(client_id);
  if (iter != gpu_channels_.end()) {
    iter->second->CreateImage(window, image_id, &size);
  }

  Send(new GpuHostMsg_ImageCreated(size));
}

void GpuChannelManager::OnCreateImage(
    gfx::PluginWindowHandle window, int32 client_id, int32 image_id) {
  DCHECK(image_id);

  if (image_operations_.empty()) {
    CreateImage(window, client_id, image_id);
  } else {
    image_operations_.push_back(
        new ImageOperation(0, base::Bind(&GpuChannelManager::CreateImage,
                                         base::Unretained(this),
                                         window,
                                         client_id,
                                         image_id)));
  }
}

void GpuChannelManager::DeleteImage(int32 client_id, int32 image_id) {
  GpuChannelMap::const_iterator iter = gpu_channels_.find(client_id);
  if (iter != gpu_channels_.end()) {
    iter->second->DeleteImage(image_id);
  }
}

void GpuChannelManager::OnDeleteImage(
    int32 client_id, int32 image_id, int32 sync_point) {
  DCHECK(image_id);

  if (!sync_point && image_operations_.empty()) {
    DeleteImage(client_id, image_id);
  } else {
    image_operations_.push_back(
        new ImageOperation(sync_point,
                           base::Bind(&GpuChannelManager::DeleteImage,
                                      base::Unretained(this),
                                      client_id,
                                      image_id)));
    if (sync_point) {
      sync_point_manager()->AddSyncPointCallback(
          sync_point,
          base::Bind(&GpuChannelManager::OnDeleteImageSyncPointRetired,
                     base::Unretained(this),
                     image_operations_.back()));
    }
  }
}

void GpuChannelManager::OnDeleteImageSyncPointRetired(
    ImageOperation* image_operation) {
  // Mark operation as no longer having a pending sync point.
  image_operation->sync_point = 0;

  // De-queue operations until we reach a pending sync point.
  while (!image_operations_.empty()) {
    // Check if operation has a pending sync point.
    if (image_operations_.front()->sync_point)
      return;

    image_operations_.front()->callback.Run();
    delete image_operations_.front();
    image_operations_.pop_front();
  }
}

void GpuChannelManager::OnCreateGpuMemoryBuffer(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    unsigned internalformat,
    unsigned usage) {
  Send(new GpuHostMsg_GpuMemoryBufferCreated(gfx::GpuMemoryBufferHandle()));
}

void GpuChannelManager::OnDestroyGpuMemoryBuffer(
    const gfx::GpuMemoryBufferHandle& handle,
    int32 sync_point) {
}

void GpuChannelManager::OnLoadedShader(std::string program_proto) {
  if (program_cache())
    program_cache()->LoadProgram(program_proto);
}

bool GpuChannelManager::HandleMessagesScheduled() {
  for (GpuChannelMap::iterator iter = gpu_channels_.begin();
       iter != gpu_channels_.end(); ++iter) {
    if (iter->second->handle_messages_scheduled())
      return true;
  }
  return false;
}

uint64 GpuChannelManager::MessagesProcessed() {
  uint64 messages_processed = 0;

  for (GpuChannelMap::iterator iter = gpu_channels_.begin();
       iter != gpu_channels_.end(); ++iter) {
    messages_processed += iter->second->messages_processed();
  }
  return messages_processed;
}

void GpuChannelManager::LoseAllContexts() {
  for (GpuChannelMap::iterator iter = gpu_channels_.begin();
       iter != gpu_channels_.end(); ++iter) {
    iter->second->MarkAllContextsLost();
  }
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&GpuChannelManager::OnLoseAllContexts,
                 weak_factory_.GetWeakPtr()));
}

void GpuChannelManager::OnLoseAllContexts() {
  gpu_channels_.clear();
}

gfx::GLSurface* GpuChannelManager::GetDefaultOffscreenSurface() {
  if (!default_offscreen_surface_.get()) {
    default_offscreen_surface_ =
        gfx::GLSurface::CreateOffscreenGLSurface(gfx::Size());
  }
  return default_offscreen_surface_.get();
}

}  // namespace content
