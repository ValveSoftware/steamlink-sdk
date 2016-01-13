// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppp_content_decryptor_private_proxy.h"

#include "base/files/file.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/proxy/content_decryptor_private_serializer.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_buffer_proxy.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_buffer_api.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/thunk.h"

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Buffer_API;
using ppapi::thunk::PPB_Instance_API;

namespace ppapi {
namespace proxy {

namespace {

PP_Bool DescribeHostBufferResource(PP_Resource resource, uint32_t* size) {
  EnterResourceNoLock<PPB_Buffer_API> enter(resource, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->Describe(size);
}

// TODO(dmichael): Refactor so this handle sharing code is in one place.
PP_Bool ShareHostBufferResourceToPlugin(
    HostDispatcher* dispatcher,
    PP_Resource resource,
    base::SharedMemoryHandle* shared_mem_handle) {
  if (!dispatcher || resource == 0 || !shared_mem_handle)
    return PP_FALSE;
  EnterResourceNoLock<PPB_Buffer_API> enter(resource, true);
  if (enter.failed())
    return PP_FALSE;
  int handle;
  int32_t result = enter.object()->GetSharedMemory(&handle);
  if (result != PP_OK)
    return PP_FALSE;
  base::PlatformFile platform_file =
  #if defined(OS_WIN)
      reinterpret_cast<HANDLE>(static_cast<intptr_t>(handle));
  #elif defined(OS_POSIX)
      handle;
  #else
  #error Not implemented.
  #endif

  *shared_mem_handle = dispatcher->ShareHandleWithRemote(platform_file, false);
  return PP_TRUE;
}

// SerializedVarReceiveInput will decrement the reference count, but we want
// to give the recipient a reference. This utility function takes care of that
// work for the message handlers defined below.
PP_Var ExtractReceivedVarAndAddRef(Dispatcher* dispatcher,
                                   SerializedVarReceiveInput* serialized_var) {
  PP_Var var = serialized_var->Get(dispatcher);
  PpapiGlobals::Get()->GetVarTracker()->AddRefVar(var);
  return var;
}

bool InitializePppDecryptorBuffer(PP_Instance instance,
                                  HostDispatcher* dispatcher,
                                  PP_Resource resource,
                                  PPPDecryptor_Buffer* buffer) {
  if (!buffer) {
    NOTREACHED();
    return false;
  }

  if (resource == 0) {
    buffer->resource = HostResource();
    buffer->handle = base::SharedMemoryHandle();
    buffer->size = 0;
    return true;
  }

  HostResource host_resource;
  host_resource.SetHostResource(instance, resource);

  uint32_t size = 0;
  if (DescribeHostBufferResource(resource, &size) == PP_FALSE)
    return false;

  base::SharedMemoryHandle handle;
  if (ShareHostBufferResourceToPlugin(dispatcher,
                                      resource,
                                      &handle) == PP_FALSE)
    return false;

  buffer->resource = host_resource;
  buffer->handle = handle;
  buffer->size = size;
  return true;
}

void Initialize(PP_Instance instance,
                PP_Var key_system) {
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (!dispatcher) {
    NOTREACHED();
    return;
  }

  dispatcher->Send(
      new PpapiMsg_PPPContentDecryptor_Initialize(
          API_ID_PPP_CONTENT_DECRYPTOR_PRIVATE,
          instance,
          SerializedVarSendInput(dispatcher, key_system)));
}

void CreateSession(PP_Instance instance,
                   uint32_t promise_id,
                   PP_Var init_data_type,
                   PP_Var init_data,
                   PP_SessionType session_type) {
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (!dispatcher) {
    NOTREACHED();
    return;
  }

  dispatcher->Send(new PpapiMsg_PPPContentDecryptor_CreateSession(
      API_ID_PPP_CONTENT_DECRYPTOR_PRIVATE,
      instance,
      promise_id,
      SerializedVarSendInput(dispatcher, init_data_type),
      SerializedVarSendInput(dispatcher, init_data),
      session_type));
}

void LoadSession(PP_Instance instance,
                 uint32_t promise_id,
                 PP_Var web_session_id) {
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (!dispatcher) {
    NOTREACHED();
    return;
  }

  dispatcher->Send(new PpapiMsg_PPPContentDecryptor_LoadSession(
      API_ID_PPP_CONTENT_DECRYPTOR_PRIVATE,
      instance,
      promise_id,
      SerializedVarSendInput(dispatcher, web_session_id)));
}

void UpdateSession(PP_Instance instance,
                   uint32_t promise_id,
                   PP_Var web_session_id,
                   PP_Var response) {
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (!dispatcher) {
    NOTREACHED();
    return;
  }

  dispatcher->Send(new PpapiMsg_PPPContentDecryptor_UpdateSession(
      API_ID_PPP_CONTENT_DECRYPTOR_PRIVATE,
      instance,
      promise_id,
      SerializedVarSendInput(dispatcher, web_session_id),
      SerializedVarSendInput(dispatcher, response)));
}

void ReleaseSession(PP_Instance instance,
                    uint32_t promise_id,
                    PP_Var web_session_id) {
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (!dispatcher) {
    NOTREACHED();
    return;
  }

  dispatcher->Send(new PpapiMsg_PPPContentDecryptor_ReleaseSession(
      API_ID_PPP_CONTENT_DECRYPTOR_PRIVATE,
      instance,
      promise_id,
      SerializedVarSendInput(dispatcher, web_session_id)));
}

void Decrypt(PP_Instance instance,
             PP_Resource encrypted_block,
             const PP_EncryptedBlockInfo* encrypted_block_info) {
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (!dispatcher) {
    NOTREACHED();
    return;
  }

  PPPDecryptor_Buffer buffer;
  if (!InitializePppDecryptorBuffer(instance,
                                    dispatcher,
                                    encrypted_block,
                                    &buffer)) {
    NOTREACHED();
    return;
  }

  std::string serialized_block_info;
  if (!SerializeBlockInfo(*encrypted_block_info, &serialized_block_info)) {
    NOTREACHED();
    return;
  }

  // PluginResourceTracker in the plugin process assumes that resources that it
  // tracks have been addrefed on behalf of the plugin at the renderer side. So
  // we explicitly do it for |encryped_block| here.
  PpapiGlobals::Get()->GetResourceTracker()->AddRefResource(encrypted_block);

  dispatcher->Send(
      new PpapiMsg_PPPContentDecryptor_Decrypt(
          API_ID_PPP_CONTENT_DECRYPTOR_PRIVATE,
          instance,
          buffer,
          serialized_block_info));
}

void InitializeAudioDecoder(
    PP_Instance instance,
    const PP_AudioDecoderConfig* decoder_config,
    PP_Resource extra_data_buffer) {
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (!dispatcher) {
    NOTREACHED();
    return;
  }

  std::string serialized_decoder_config;
  if (!SerializeBlockInfo(*decoder_config, &serialized_decoder_config)) {
    NOTREACHED();
    return;
  }

  PPPDecryptor_Buffer buffer;
  if (!InitializePppDecryptorBuffer(instance,
                                    dispatcher,
                                    extra_data_buffer,
                                    &buffer)) {
    NOTREACHED();
    return;
  }

  // PluginResourceTracker in the plugin process assumes that resources that it
  // tracks have been addrefed on behalf of the plugin at the renderer side. So
  // we explicitly do it for |extra_data_buffer| here.
  PpapiGlobals::Get()->GetResourceTracker()->AddRefResource(extra_data_buffer);

  dispatcher->Send(
      new PpapiMsg_PPPContentDecryptor_InitializeAudioDecoder(
          API_ID_PPP_CONTENT_DECRYPTOR_PRIVATE,
          instance,
          serialized_decoder_config,
          buffer));
}

void InitializeVideoDecoder(
    PP_Instance instance,
    const PP_VideoDecoderConfig* decoder_config,
    PP_Resource extra_data_buffer) {
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (!dispatcher) {
    NOTREACHED();
    return;
  }

  std::string serialized_decoder_config;
  if (!SerializeBlockInfo(*decoder_config, &serialized_decoder_config)) {
    NOTREACHED();
    return;
  }

  PPPDecryptor_Buffer buffer;
  if (!InitializePppDecryptorBuffer(instance,
                                    dispatcher,
                                    extra_data_buffer,
                                    &buffer)) {
    NOTREACHED();
    return;
  }

  // PluginResourceTracker in the plugin process assumes that resources that it
  // tracks have been addrefed on behalf of the plugin at the renderer side. So
  // we explicitly do it for |extra_data_buffer| here.
  PpapiGlobals::Get()->GetResourceTracker()->AddRefResource(extra_data_buffer);

  dispatcher->Send(
      new PpapiMsg_PPPContentDecryptor_InitializeVideoDecoder(
          API_ID_PPP_CONTENT_DECRYPTOR_PRIVATE,
          instance,
          serialized_decoder_config,
          buffer));
}


void DeinitializeDecoder(PP_Instance instance,
                         PP_DecryptorStreamType decoder_type,
                         uint32_t request_id) {
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (!dispatcher) {
    NOTREACHED();
    return;
  }

  dispatcher->Send(
      new PpapiMsg_PPPContentDecryptor_DeinitializeDecoder(
          API_ID_PPP_CONTENT_DECRYPTOR_PRIVATE,
          instance,
          decoder_type,
          request_id));
}

void ResetDecoder(PP_Instance instance,
                  PP_DecryptorStreamType decoder_type,
                  uint32_t request_id) {
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (!dispatcher) {
    NOTREACHED();
    return;
  }

  dispatcher->Send(
      new PpapiMsg_PPPContentDecryptor_ResetDecoder(
          API_ID_PPP_CONTENT_DECRYPTOR_PRIVATE,
          instance,
          decoder_type,
          request_id));
}

void DecryptAndDecode(PP_Instance instance,
                      PP_DecryptorStreamType decoder_type,
                      PP_Resource encrypted_buffer,
                      const PP_EncryptedBlockInfo* encrypted_block_info) {
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (!dispatcher) {
    NOTREACHED();
    return;
  }

  PPPDecryptor_Buffer buffer;
  if (!InitializePppDecryptorBuffer(instance,
                                    dispatcher,
                                    encrypted_buffer,
                                    &buffer)) {
    NOTREACHED();
    return;
  }

  std::string serialized_block_info;
  if (!SerializeBlockInfo(*encrypted_block_info, &serialized_block_info)) {
    NOTREACHED();
    return;
  }

  // PluginResourceTracker in the plugin process assumes that resources that it
  // tracks have been addrefed on behalf of the plugin at the renderer side. So
  // we explicitly do it for |encrypted_buffer| here.
  PpapiGlobals::Get()->GetResourceTracker()->AddRefResource(encrypted_buffer);

  dispatcher->Send(
      new PpapiMsg_PPPContentDecryptor_DecryptAndDecode(
          API_ID_PPP_CONTENT_DECRYPTOR_PRIVATE,
          instance,
          decoder_type,
          buffer,
          serialized_block_info));
}

static const PPP_ContentDecryptor_Private content_decryptor_interface = {
  &Initialize,
  &CreateSession,
  &LoadSession,
  &UpdateSession,
  &ReleaseSession,
  &Decrypt,
  &InitializeAudioDecoder,
  &InitializeVideoDecoder,
  &DeinitializeDecoder,
  &ResetDecoder,
  &DecryptAndDecode
};

}  // namespace

PPP_ContentDecryptor_Private_Proxy::PPP_ContentDecryptor_Private_Proxy(
    Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      ppp_decryptor_impl_(NULL) {
  if (dispatcher->IsPlugin()) {
    ppp_decryptor_impl_ = static_cast<const PPP_ContentDecryptor_Private*>(
        dispatcher->local_get_interface()(
            PPP_CONTENTDECRYPTOR_PRIVATE_INTERFACE));
  }
}

PPP_ContentDecryptor_Private_Proxy::~PPP_ContentDecryptor_Private_Proxy() {
}

// static
const PPP_ContentDecryptor_Private*
    PPP_ContentDecryptor_Private_Proxy::GetProxyInterface() {
  return &content_decryptor_interface;
}

bool PPP_ContentDecryptor_Private_Proxy::OnMessageReceived(
    const IPC::Message& msg) {
  if (!dispatcher()->IsPlugin())
    return false;  // These are only valid from host->plugin.
                   // Don't allow the plugin to send these to the host.

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPP_ContentDecryptor_Private_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPContentDecryptor_Initialize,
                        OnMsgInitialize)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPContentDecryptor_CreateSession,
                        OnMsgCreateSession)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPContentDecryptor_LoadSession,
                        OnMsgLoadSession)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPContentDecryptor_UpdateSession,
                        OnMsgUpdateSession)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPContentDecryptor_ReleaseSession,
                        OnMsgReleaseSession)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPContentDecryptor_Decrypt,
                        OnMsgDecrypt)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPContentDecryptor_InitializeAudioDecoder,
                        OnMsgInitializeAudioDecoder)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPContentDecryptor_InitializeVideoDecoder,
                        OnMsgInitializeVideoDecoder)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPContentDecryptor_DeinitializeDecoder,
                        OnMsgDeinitializeDecoder)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPContentDecryptor_ResetDecoder,
                        OnMsgResetDecoder)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPContentDecryptor_DecryptAndDecode,
                        OnMsgDecryptAndDecode)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  return handled;
}

void PPP_ContentDecryptor_Private_Proxy::OnMsgInitialize(
    PP_Instance instance,
    SerializedVarReceiveInput key_system) {
  if (ppp_decryptor_impl_) {
    CallWhileUnlocked(
        ppp_decryptor_impl_->Initialize,
        instance,
        ExtractReceivedVarAndAddRef(dispatcher(), &key_system));
  }
}

void PPP_ContentDecryptor_Private_Proxy::OnMsgCreateSession(
    PP_Instance instance,
    uint32_t promise_id,
    SerializedVarReceiveInput init_data_type,
    SerializedVarReceiveInput init_data,
    PP_SessionType session_type) {
  if (ppp_decryptor_impl_) {
    CallWhileUnlocked(
        ppp_decryptor_impl_->CreateSession,
        instance,
        promise_id,
        ExtractReceivedVarAndAddRef(dispatcher(), &init_data_type),
        ExtractReceivedVarAndAddRef(dispatcher(), &init_data),
        session_type);
  }
}

void PPP_ContentDecryptor_Private_Proxy::OnMsgLoadSession(
    PP_Instance instance,
    uint32_t promise_id,
    SerializedVarReceiveInput web_session_id) {
  if (ppp_decryptor_impl_) {
    CallWhileUnlocked(
        ppp_decryptor_impl_->LoadSession,
        instance,
        promise_id,
        ExtractReceivedVarAndAddRef(dispatcher(), &web_session_id));
  }
}

void PPP_ContentDecryptor_Private_Proxy::OnMsgUpdateSession(
    PP_Instance instance,
    uint32_t promise_id,
    SerializedVarReceiveInput web_session_id,
    SerializedVarReceiveInput response) {
  if (ppp_decryptor_impl_) {
    CallWhileUnlocked(
        ppp_decryptor_impl_->UpdateSession,
        instance,
        promise_id,
        ExtractReceivedVarAndAddRef(dispatcher(), &web_session_id),
        ExtractReceivedVarAndAddRef(dispatcher(), &response));
  }
}

void PPP_ContentDecryptor_Private_Proxy::OnMsgReleaseSession(
    PP_Instance instance,
    uint32_t promise_id,
    SerializedVarReceiveInput web_session_id) {
  if (ppp_decryptor_impl_) {
    CallWhileUnlocked(
        ppp_decryptor_impl_->ReleaseSession,
        instance,
        promise_id,
        ExtractReceivedVarAndAddRef(dispatcher(), &web_session_id));
  }
}

void PPP_ContentDecryptor_Private_Proxy::OnMsgDecrypt(
    PP_Instance instance,
    const PPPDecryptor_Buffer& encrypted_buffer,
    const std::string& serialized_block_info) {
  ScopedPPResource plugin_resource(
      ScopedPPResource::PassRef(),
      PPB_Buffer_Proxy::AddProxyResource(encrypted_buffer.resource,
                                         encrypted_buffer.handle,
                                         encrypted_buffer.size));
  if (ppp_decryptor_impl_) {
    PP_EncryptedBlockInfo block_info;
    if (!DeserializeBlockInfo(serialized_block_info, &block_info))
      return;
    CallWhileUnlocked(ppp_decryptor_impl_->Decrypt,
                      instance,
                      plugin_resource.get(),
                      const_cast<const PP_EncryptedBlockInfo*>(&block_info));
  }
}

void PPP_ContentDecryptor_Private_Proxy::OnMsgInitializeAudioDecoder(
    PP_Instance instance,
    const std::string& serialized_decoder_config,
    const PPPDecryptor_Buffer& extra_data_buffer) {
  ScopedPPResource plugin_resource;
  if (extra_data_buffer.size > 0) {
    plugin_resource = ScopedPPResource(
        ScopedPPResource::PassRef(),
        PPB_Buffer_Proxy::AddProxyResource(extra_data_buffer.resource,
                                           extra_data_buffer.handle,
                                           extra_data_buffer.size));
  }

  PP_AudioDecoderConfig decoder_config;
  if (!DeserializeBlockInfo(serialized_decoder_config, &decoder_config))
      return;

  if (ppp_decryptor_impl_) {
    CallWhileUnlocked(
        ppp_decryptor_impl_->InitializeAudioDecoder,
        instance,
        const_cast<const PP_AudioDecoderConfig*>(&decoder_config),
        plugin_resource.get());
  }
}

void PPP_ContentDecryptor_Private_Proxy::OnMsgInitializeVideoDecoder(
    PP_Instance instance,
    const std::string& serialized_decoder_config,
    const PPPDecryptor_Buffer& extra_data_buffer) {
  ScopedPPResource plugin_resource;
  if (extra_data_buffer.resource.host_resource() != 0) {
    plugin_resource = ScopedPPResource(
        ScopedPPResource::PassRef(),
        PPB_Buffer_Proxy::AddProxyResource(extra_data_buffer.resource,
                                           extra_data_buffer.handle,
                                           extra_data_buffer.size));
  }

  PP_VideoDecoderConfig decoder_config;
  if (!DeserializeBlockInfo(serialized_decoder_config, &decoder_config))
      return;

  if (ppp_decryptor_impl_) {
    CallWhileUnlocked(
        ppp_decryptor_impl_->InitializeVideoDecoder,
        instance,
        const_cast<const PP_VideoDecoderConfig*>(&decoder_config),
        plugin_resource.get());
  }
}

void PPP_ContentDecryptor_Private_Proxy::OnMsgDeinitializeDecoder(
    PP_Instance instance,
    PP_DecryptorStreamType decoder_type,
    uint32_t request_id) {
  if (ppp_decryptor_impl_) {
    CallWhileUnlocked(
        ppp_decryptor_impl_->DeinitializeDecoder,
        instance,
        decoder_type,
        request_id);
  }
}

void PPP_ContentDecryptor_Private_Proxy::OnMsgResetDecoder(
    PP_Instance instance,
    PP_DecryptorStreamType decoder_type,
    uint32_t request_id) {
  if (ppp_decryptor_impl_) {
    CallWhileUnlocked(
        ppp_decryptor_impl_->ResetDecoder,
        instance,
        decoder_type,
        request_id);
  }
}

void PPP_ContentDecryptor_Private_Proxy::OnMsgDecryptAndDecode(
    PP_Instance instance,
    PP_DecryptorStreamType decoder_type,
    const PPPDecryptor_Buffer& encrypted_buffer,
    const std::string& serialized_block_info) {
  ScopedPPResource plugin_resource;
  if (encrypted_buffer.resource.host_resource() != 0) {
    plugin_resource = ScopedPPResource(
        ScopedPPResource::PassRef(),
        PPB_Buffer_Proxy::AddProxyResource(encrypted_buffer.resource,
                                           encrypted_buffer.handle,
                                           encrypted_buffer.size));
  }

  if (ppp_decryptor_impl_) {
    PP_EncryptedBlockInfo block_info;
    if (!DeserializeBlockInfo(serialized_block_info, &block_info))
      return;
    CallWhileUnlocked(
        ppp_decryptor_impl_->DecryptAndDecode,
        instance,
        decoder_type,
        plugin_resource.get(),
        const_cast<const PP_EncryptedBlockInfo*>(&block_info));
  }
}

}  // namespace proxy
}  // namespace ppapi
