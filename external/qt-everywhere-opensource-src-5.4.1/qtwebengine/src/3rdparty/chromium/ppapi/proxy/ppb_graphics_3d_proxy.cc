// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_graphics_3d_proxy.h"

#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/enter_proxy.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_command_buffer_proxy.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Graphics3D_API;
using ppapi::thunk::ResourceCreationAPI;

namespace ppapi {
namespace proxy {

namespace {

const int32 kCommandBufferSize = 1024 * 1024;
const int32 kTransferBufferSize = 1024 * 1024;

base::SharedMemoryHandle TransportSHMHandle(Dispatcher* dispatcher,
                                            base::SharedMemory* shm) {
  base::PlatformFile source =
      IPC::PlatformFileForTransitToPlatformFile(shm->handle());
  // Don't close the handle, it doesn't belong to us.
  return dispatcher->ShareHandleWithRemote(source, false);
}

gpu::CommandBuffer::State GetErrorState() {
  gpu::CommandBuffer::State error_state;
  error_state.error = gpu::error::kGenericError;
  return error_state;
}

}  // namespace

Graphics3D::Graphics3D(const HostResource& resource)
    : PPB_Graphics3D_Shared(resource) {
}

Graphics3D::~Graphics3D() {
  DestroyGLES2Impl();
}

bool Graphics3D::Init(gpu::gles2::GLES2Implementation* share_gles2) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForResource(this);
  if (!dispatcher)
    return false;

  command_buffer_.reset(
      new PpapiCommandBufferProxy(host_resource(), dispatcher));

  return CreateGLES2Impl(kCommandBufferSize, kTransferBufferSize,
                         share_gles2);
}

PP_Bool Graphics3D::SetGetBuffer(int32_t /* transfer_buffer_id */) {
  return PP_FALSE;
}

PP_Bool Graphics3D::Flush(int32_t put_offset) {
  return PP_FALSE;
}

scoped_refptr<gpu::Buffer> Graphics3D::CreateTransferBuffer(
    uint32_t size,
    int32_t* id) {
  *id = -1;
  return NULL;
}

PP_Bool Graphics3D::DestroyTransferBuffer(int32_t id) {
  return PP_FALSE;
}

gpu::CommandBuffer::State Graphics3D::WaitForTokenInRange(int32_t start,
                                                          int32_t end) {
  return GetErrorState();
}

gpu::CommandBuffer::State Graphics3D::WaitForGetOffsetInRange(int32_t start,
                                                              int32_t end) {
  return GetErrorState();
}

uint32_t Graphics3D::InsertSyncPoint() {
  NOTREACHED();
  return 0;
}

gpu::CommandBuffer* Graphics3D::GetCommandBuffer() {
  return command_buffer_.get();
}

gpu::GpuControl* Graphics3D::GetGpuControl() {
  return command_buffer_.get();
}

int32 Graphics3D::DoSwapBuffers() {
  gles2_impl()->SwapBuffers();
  IPC::Message* msg = new PpapiHostMsg_PPBGraphics3D_SwapBuffers(
      API_ID_PPB_GRAPHICS_3D, host_resource());
  msg->set_unblock(true);
  PluginDispatcher::GetForResource(this)->Send(msg);

  return PP_OK_COMPLETIONPENDING;
}

PPB_Graphics3D_Proxy::PPB_Graphics3D_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      callback_factory_(this) {
}

PPB_Graphics3D_Proxy::~PPB_Graphics3D_Proxy() {
}

// static
PP_Resource PPB_Graphics3D_Proxy::CreateProxyResource(
    PP_Instance instance,
    PP_Resource share_context,
    const int32_t* attrib_list) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_ERROR_BADARGUMENT;

  HostResource share_host;
  gpu::gles2::GLES2Implementation* share_gles2 = NULL;
  if (share_context != 0) {
    EnterResourceNoLock<PPB_Graphics3D_API> enter(share_context, true);
    if (enter.failed())
      return PP_ERROR_BADARGUMENT;

    PPB_Graphics3D_Shared* share_graphics =
        static_cast<PPB_Graphics3D_Shared*>(enter.object());
    share_host = share_graphics->host_resource();
    share_gles2 = share_graphics->gles2_impl();
  }

  std::vector<int32_t> attribs;
  if (attrib_list) {
    for (const int32_t* attr = attrib_list;
         attr[0] != PP_GRAPHICS3DATTRIB_NONE;
         attr += 2) {
      attribs.push_back(attr[0]);
      attribs.push_back(attr[1]);
    }
  }
  attribs.push_back(PP_GRAPHICS3DATTRIB_NONE);

  HostResource result;
  dispatcher->Send(new PpapiHostMsg_PPBGraphics3D_Create(
      API_ID_PPB_GRAPHICS_3D, instance, share_host, attribs, &result));
  if (result.is_null())
    return 0;

  scoped_refptr<Graphics3D> graphics_3d(new Graphics3D(result));
  if (!graphics_3d->Init(share_gles2))
    return 0;
  return graphics_3d->GetReference();
}

bool PPB_Graphics3D_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Graphics3D_Proxy, msg)
#if !defined(OS_NACL)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_Create,
                        OnMsgCreate)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_SetGetBuffer,
                        OnMsgSetGetBuffer)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_WaitForTokenInRange,
                        OnMsgWaitForTokenInRange)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_WaitForGetOffsetInRange,
                        OnMsgWaitForGetOffsetInRange)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_AsyncFlush, OnMsgAsyncFlush)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_CreateTransferBuffer,
                        OnMsgCreateTransferBuffer)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_DestroyTransferBuffer,
                        OnMsgDestroyTransferBuffer)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_SwapBuffers,
                        OnMsgSwapBuffers)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_InsertSyncPoint,
                        OnMsgInsertSyncPoint)
#endif  // !defined(OS_NACL)

    IPC_MESSAGE_HANDLER(PpapiMsg_PPBGraphics3D_SwapBuffersACK,
                        OnMsgSwapBuffersACK)
    IPC_MESSAGE_UNHANDLED(handled = false)

  IPC_END_MESSAGE_MAP()
  // FIXME(brettw) handle bad messages!
  return handled;
}

#if !defined(OS_NACL)
void PPB_Graphics3D_Proxy::OnMsgCreate(PP_Instance instance,
                                       HostResource share_context,
                                       const std::vector<int32_t>& attribs,
                                       HostResource* result) {
  if (attribs.empty() ||
      attribs.back() != PP_GRAPHICS3DATTRIB_NONE ||
      !(attribs.size() & 1))
    return;  // Bad message.

  thunk::EnterResourceCreation enter(instance);

  if (enter.succeeded()) {
    result->SetHostResource(
      instance,
      enter.functions()->CreateGraphics3DRaw(instance,
                                             share_context.host_resource(),
                                             &attribs.front()));
  }
}

void PPB_Graphics3D_Proxy::OnMsgSetGetBuffer(
    const HostResource& context,
    int32 transfer_buffer_id) {
  EnterHostFromHostResource<PPB_Graphics3D_API> enter(context);
  if (enter.succeeded())
    enter.object()->SetGetBuffer(transfer_buffer_id);
}

void PPB_Graphics3D_Proxy::OnMsgWaitForTokenInRange(
    const HostResource& context,
    int32 start,
    int32 end,
    gpu::CommandBuffer::State* state,
    bool* success) {
  EnterHostFromHostResource<PPB_Graphics3D_API> enter(context);
  if (enter.failed()) {
    *success = false;
    return;
  }
  *state = enter.object()->WaitForTokenInRange(start, end);
  *success = true;
}

void PPB_Graphics3D_Proxy::OnMsgWaitForGetOffsetInRange(
    const HostResource& context,
    int32 start,
    int32 end,
    gpu::CommandBuffer::State* state,
    bool* success) {
  EnterHostFromHostResource<PPB_Graphics3D_API> enter(context);
  if (enter.failed()) {
    *success = false;
    return;
  }
  *state = enter.object()->WaitForGetOffsetInRange(start, end);
  *success = true;
}

void PPB_Graphics3D_Proxy::OnMsgAsyncFlush(const HostResource& context,
                                           int32 put_offset) {
  EnterHostFromHostResource<PPB_Graphics3D_API> enter(context);
  if (enter.succeeded())
    enter.object()->Flush(put_offset);
}

void PPB_Graphics3D_Proxy::OnMsgCreateTransferBuffer(
    const HostResource& context,
    uint32 size,
    int32* id,
    ppapi::proxy::SerializedHandle* transfer_buffer) {
  transfer_buffer->set_null_shmem();
  EnterHostFromHostResource<PPB_Graphics3D_API> enter(context);
  if (enter.succeeded()) {
    scoped_refptr<gpu::Buffer> buffer =
        enter.object()->CreateTransferBuffer(size, id);
    if (!buffer)
      return;
    gpu::SharedMemoryBufferBacking* backing =
        static_cast<gpu::SharedMemoryBufferBacking*>(buffer->backing());
    DCHECK(backing && backing->shared_memory());
    transfer_buffer->set_shmem(
        TransportSHMHandle(dispatcher(), backing->shared_memory()),
        buffer->size());
  } else {
    *id = -1;
  }
}

void PPB_Graphics3D_Proxy::OnMsgDestroyTransferBuffer(
    const HostResource& context,
    int32 id) {
  EnterHostFromHostResource<PPB_Graphics3D_API> enter(context);
  if (enter.succeeded())
    enter.object()->DestroyTransferBuffer(id);
}

void PPB_Graphics3D_Proxy::OnMsgSwapBuffers(const HostResource& context) {
  EnterHostFromHostResourceForceCallback<PPB_Graphics3D_API> enter(
      context, callback_factory_,
      &PPB_Graphics3D_Proxy::SendSwapBuffersACKToPlugin, context);
  if (enter.succeeded())
    enter.SetResult(enter.object()->SwapBuffers(enter.callback()));
}

void PPB_Graphics3D_Proxy::OnMsgInsertSyncPoint(const HostResource& context,
                                                uint32* sync_point) {
  *sync_point = 0;
  EnterHostFromHostResource<PPB_Graphics3D_API> enter(context);
  if (enter.succeeded())
    *sync_point = enter.object()->InsertSyncPoint();
}
#endif  // !defined(OS_NACL)

void PPB_Graphics3D_Proxy::OnMsgSwapBuffersACK(const HostResource& resource,
                                              int32_t pp_error) {
  EnterPluginFromHostResource<PPB_Graphics3D_API> enter(resource);
  if (enter.succeeded())
    static_cast<Graphics3D*>(enter.object())->SwapBuffersACK(pp_error);
}

#if !defined(OS_NACL)
void PPB_Graphics3D_Proxy::SendSwapBuffersACKToPlugin(
    int32_t result,
    const HostResource& context) {
  dispatcher()->Send(new PpapiMsg_PPBGraphics3D_SwapBuffersACK(
      API_ID_PPB_GRAPHICS_3D, context, result));
}
#endif  // !defined(OS_NACL)

}  // namespace proxy
}  // namespace ppapi

