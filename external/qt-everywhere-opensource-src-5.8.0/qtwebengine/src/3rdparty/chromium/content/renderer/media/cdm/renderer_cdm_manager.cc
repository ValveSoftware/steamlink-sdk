// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/cdm/renderer_cdm_manager.h"

#include <stddef.h>
#include <utility>

#include "base/stl_util.h"
#include "content/common/media/cdm_messages.h"
#include "content/renderer/media/cdm/proxy_media_keys.h"
#include "media/base/cdm_context.h"
#include "media/base/limits.h"

namespace content {

using media::MediaKeys;

// Maximum sizes for various EME API parameters. These are checks to prevent
// unnecessarily large messages from being passed around, and the sizes
// are somewhat arbitrary as the EME spec doesn't specify any limits.
const size_t kMaxSessionMessageLength = 10240;  // 10 KB

RendererCdmManager::RendererCdmManager(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      next_cdm_id_(media::CdmContext::kInvalidCdmId + 1) {}

RendererCdmManager::~RendererCdmManager() {
  DCHECK(proxy_media_keys_map_.empty())
      << "RendererCdmManager is owned by RenderFrameImpl and is destroyed only "
         "after all ProxyMediaKeys are destroyed and unregistered.";
}

bool RendererCdmManager::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RendererCdmManager, msg)
    IPC_MESSAGE_HANDLER(CdmMsg_SessionMessage, OnSessionMessage)
    IPC_MESSAGE_HANDLER(CdmMsg_SessionClosed, OnSessionClosed)
    IPC_MESSAGE_HANDLER(CdmMsg_LegacySessionError, OnLegacySessionError)
    IPC_MESSAGE_HANDLER(CdmMsg_SessionKeysChange, OnSessionKeysChange)
    IPC_MESSAGE_HANDLER(CdmMsg_SessionExpirationUpdate,
                        OnSessionExpirationUpdate)
    IPC_MESSAGE_HANDLER(CdmMsg_ResolvePromise, OnPromiseResolved)
    IPC_MESSAGE_HANDLER(CdmMsg_ResolvePromiseWithSession,
                        OnPromiseResolvedWithSession)
    IPC_MESSAGE_HANDLER(CdmMsg_RejectPromise, OnPromiseRejected)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RendererCdmManager::OnDestruct() {
  delete this;
}

void RendererCdmManager::InitializeCdm(int cdm_id,
                                       uint32_t promise_id,
                                       ProxyMediaKeys* media_keys,
                                       const std::string& key_system,
                                       const GURL& security_origin,
                                       bool use_hw_secure_codecs) {
  DCHECK(GetMediaKeys(cdm_id)) << "|cdm_id| not registered.";
  CdmHostMsg_InitializeCdm_Params params;
  params.key_system = key_system;
  params.security_origin = security_origin;
  params.use_hw_secure_codecs = use_hw_secure_codecs;
  Send(new CdmHostMsg_InitializeCdm(routing_id(), cdm_id, promise_id, params));
}

void RendererCdmManager::SetServerCertificate(
    int cdm_id,
    uint32_t promise_id,
    const std::vector<uint8_t>& certificate) {
  DCHECK(GetMediaKeys(cdm_id)) << "|cdm_id| not registered.";
  Send(new CdmHostMsg_SetServerCertificate(routing_id(), cdm_id, promise_id,
                                           certificate));
}

void RendererCdmManager::CreateSessionAndGenerateRequest(
    int cdm_id,
    uint32_t promise_id,
    media::MediaKeys::SessionType session_type,
    CdmHostMsg_CreateSession_InitDataType init_data_type,
    const std::vector<uint8_t>& init_data) {
  DCHECK(GetMediaKeys(cdm_id)) << "|cdm_id| not registered.";
  CdmHostMsg_CreateSessionAndGenerateRequest_Params params;
  params.render_frame_id = routing_id();
  params.cdm_id = cdm_id;
  params.promise_id = promise_id;
  params.session_type = session_type;
  params.init_data_type = init_data_type;
  params.init_data = init_data;
  Send(new CdmHostMsg_CreateSessionAndGenerateRequest(params));
}

void RendererCdmManager::LoadSession(int cdm_id,
                                     uint32_t promise_id,
                                     media::MediaKeys::SessionType session_type,
                                     const std::string& session_id) {
  DCHECK(GetMediaKeys(cdm_id)) << "|cdm_id| not registered.";
  Send(new CdmHostMsg_LoadSession(routing_id(), cdm_id, promise_id,
                                  session_type, session_id));
}

void RendererCdmManager::UpdateSession(int cdm_id,
                                       uint32_t promise_id,
                                       const std::string& session_id,
                                       const std::vector<uint8_t>& response) {
  DCHECK(GetMediaKeys(cdm_id)) << "|cdm_id| not registered.";
  Send(new CdmHostMsg_UpdateSession(routing_id(), cdm_id, promise_id,
                                    session_id, response));
}

void RendererCdmManager::CloseSession(int cdm_id,
                                      uint32_t promise_id,
                                      const std::string& session_id) {
  DCHECK(GetMediaKeys(cdm_id)) << "|cdm_id| not registered.";
  Send(new CdmHostMsg_CloseSession(routing_id(), cdm_id, promise_id,
                                   session_id));
}

void RendererCdmManager::RemoveSession(int cdm_id,
                                       uint32_t promise_id,
                                       const std::string& session_id) {
  DCHECK(GetMediaKeys(cdm_id)) << "|cdm_id| not registered.";
  Send(new CdmHostMsg_RemoveSession(routing_id(), cdm_id, promise_id,
                                    session_id));
}

void RendererCdmManager::DestroyCdm(int cdm_id) {
  DCHECK(GetMediaKeys(cdm_id)) << "|cdm_id| not registered.";
  Send(new CdmHostMsg_DestroyCdm(routing_id(), cdm_id));
}

void RendererCdmManager::OnSessionMessage(
    int cdm_id,
    const std::string& session_id,
    media::MediaKeys::MessageType message_type,
    const std::vector<uint8_t>& message,
    const GURL& legacy_destination_url) {
  if (message.size() > kMaxSessionMessageLength) {
    NOTREACHED();
    LOG(ERROR) << "Message is too long and dropped.";
    return;
  }

  ProxyMediaKeys* media_keys = GetMediaKeys(cdm_id);
  if (media_keys)
    media_keys->OnSessionMessage(session_id, message_type, message,
                                 legacy_destination_url);
}

void RendererCdmManager::OnSessionClosed(int cdm_id,
                                         const std::string& session_id) {
  ProxyMediaKeys* media_keys = GetMediaKeys(cdm_id);
  if (media_keys)
    media_keys->OnSessionClosed(session_id);
}

void RendererCdmManager::OnLegacySessionError(
    int cdm_id,
    const std::string& session_id,
    MediaKeys::Exception exception,
    uint32_t system_code,
    const std::string& error_message) {
  ProxyMediaKeys* media_keys = GetMediaKeys(cdm_id);
  if (media_keys)
    media_keys->OnLegacySessionError(session_id, exception, system_code,
                                     error_message);
}

void RendererCdmManager::OnSessionKeysChange(
    int cdm_id,
    const std::string& session_id,
    bool has_additional_usable_key,
    const std::vector<media::CdmKeyInformation>& key_info_vector) {
  ProxyMediaKeys* media_keys = GetMediaKeys(cdm_id);
  if (!media_keys)
    return;

  media::CdmKeysInfo keys_info;
  keys_info.reserve(key_info_vector.size());
  for (const auto& key_info : key_info_vector)
    keys_info.push_back(new media::CdmKeyInformation(key_info));

  media_keys->OnSessionKeysChange(session_id, has_additional_usable_key,
                                  std::move(keys_info));
}

void RendererCdmManager::OnSessionExpirationUpdate(
    int cdm_id,
    const std::string& session_id,
    const base::Time& new_expiry_time) {
  ProxyMediaKeys* media_keys = GetMediaKeys(cdm_id);
  if (media_keys)
    media_keys->OnSessionExpirationUpdate(session_id, new_expiry_time);
}

void RendererCdmManager::OnPromiseResolved(int cdm_id, uint32_t promise_id) {
  ProxyMediaKeys* media_keys = GetMediaKeys(cdm_id);
  if (media_keys)
    media_keys->OnPromiseResolved(promise_id);
}

void RendererCdmManager::OnPromiseResolvedWithSession(
    int cdm_id,
    uint32_t promise_id,
    const std::string& session_id) {
  if (session_id.length() > media::limits::kMaxSessionIdLength) {
    NOTREACHED();
    OnPromiseRejected(cdm_id, promise_id, MediaKeys::INVALID_ACCESS_ERROR, 0,
                      "Session ID is too long");
    return;
  }

  ProxyMediaKeys* media_keys = GetMediaKeys(cdm_id);
  if (media_keys)
    media_keys->OnPromiseResolvedWithSession(promise_id, session_id);
}

void RendererCdmManager::OnPromiseRejected(int cdm_id,
                                           uint32_t promise_id,
                                           MediaKeys::Exception exception,
                                           uint32_t system_code,
                                           const std::string& error_message) {
  ProxyMediaKeys* media_keys = GetMediaKeys(cdm_id);
  if (media_keys)
    media_keys->OnPromiseRejected(promise_id, exception, system_code,
                                  error_message);
}

int RendererCdmManager::RegisterMediaKeys(ProxyMediaKeys* media_keys) {
  int cdm_id = next_cdm_id_++;
  DCHECK_NE(cdm_id, media::CdmContext::kInvalidCdmId);
  DCHECK(!ContainsKey(proxy_media_keys_map_, cdm_id));
  proxy_media_keys_map_[cdm_id] = media_keys;
  return cdm_id;
}

void RendererCdmManager::UnregisterMediaKeys(int cdm_id) {
  DCHECK(ContainsKey(proxy_media_keys_map_, cdm_id));
  proxy_media_keys_map_.erase(cdm_id);
}

ProxyMediaKeys* RendererCdmManager::GetMediaKeys(int cdm_id) {
  std::map<int, ProxyMediaKeys*>::iterator iter =
      proxy_media_keys_map_.find(cdm_id);
  return (iter != proxy_media_keys_map_.end()) ? iter->second : NULL;
}

}  // namespace content
