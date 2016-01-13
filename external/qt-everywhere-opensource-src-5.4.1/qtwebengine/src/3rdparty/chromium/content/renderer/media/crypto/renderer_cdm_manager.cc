// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/crypto/renderer_cdm_manager.h"

#include "base/stl_util.h"
#include "content/common/media/cdm_messages.h"
#include "content/renderer/media/crypto/proxy_media_keys.h"

namespace content {

// Maximum sizes for various EME API parameters. These are checks to prevent
// unnecessarily large messages from being passed around, and the sizes
// are somewhat arbitrary as the EME spec doesn't specify any limits.
const size_t kMaxWebSessionIdLength = 512;
const size_t kMaxSessionMessageLength = 10240;  // 10 KB

RendererCdmManager::RendererCdmManager(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      next_cdm_id_(kInvalidCdmId + 1) {
}

RendererCdmManager::~RendererCdmManager() {
  DCHECK(proxy_media_keys_map_.empty())
      << "RendererCdmManager is owned by RenderFrameImpl and is destroyed only "
         "after all ProxyMediaKeys are destroyed and unregistered.";
}

bool RendererCdmManager::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RendererCdmManager, msg)
    IPC_MESSAGE_HANDLER(CdmMsg_SessionCreated, OnSessionCreated)
    IPC_MESSAGE_HANDLER(CdmMsg_SessionMessage, OnSessionMessage)
    IPC_MESSAGE_HANDLER(CdmMsg_SessionReady, OnSessionReady)
    IPC_MESSAGE_HANDLER(CdmMsg_SessionClosed, OnSessionClosed)
    IPC_MESSAGE_HANDLER(CdmMsg_SessionError, OnSessionError)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RendererCdmManager::InitializeCdm(int cdm_id,
                                       ProxyMediaKeys* media_keys,
                                       const std::string& key_system,
                                       const GURL& security_origin) {
  DCHECK(GetMediaKeys(cdm_id)) << "|cdm_id| not registered.";
  Send(new CdmHostMsg_InitializeCdm(
      routing_id(), cdm_id, key_system, security_origin));
}

void RendererCdmManager::CreateSession(
    int cdm_id,
    uint32 session_id,
    CdmHostMsg_CreateSession_ContentType content_type,
    const std::vector<uint8>& init_data) {
  DCHECK(GetMediaKeys(cdm_id)) << "|cdm_id| not registered.";
  Send(new CdmHostMsg_CreateSession(
      routing_id(), cdm_id, session_id, content_type, init_data));
}

void RendererCdmManager::UpdateSession(int cdm_id,
                                       uint32 session_id,
                                       const std::vector<uint8>& response) {
  DCHECK(GetMediaKeys(cdm_id)) << "|cdm_id| not registered.";
  Send(
      new CdmHostMsg_UpdateSession(routing_id(), cdm_id, session_id, response));
}

void RendererCdmManager::ReleaseSession(int cdm_id, uint32 session_id) {
  DCHECK(GetMediaKeys(cdm_id)) << "|cdm_id| not registered.";
  Send(new CdmHostMsg_ReleaseSession(routing_id(), cdm_id, session_id));
}

void RendererCdmManager::DestroyCdm(int cdm_id) {
  DCHECK(GetMediaKeys(cdm_id)) << "|cdm_id| not registered.";
  Send(new CdmHostMsg_DestroyCdm(routing_id(), cdm_id));
}

void RendererCdmManager::OnSessionCreated(int cdm_id,
                                          uint32 session_id,
                                          const std::string& web_session_id) {
  if (web_session_id.length() > kMaxWebSessionIdLength) {
    OnSessionError(cdm_id, session_id, media::MediaKeys::kUnknownError, 0);
    return;
  }

  ProxyMediaKeys* media_keys = GetMediaKeys(cdm_id);
  if (media_keys)
    media_keys->OnSessionCreated(session_id, web_session_id);
}

void RendererCdmManager::OnSessionMessage(int cdm_id,
                                          uint32 session_id,
                                          const std::vector<uint8>& message,
                                          const GURL& destination_url) {
  if (message.size() > kMaxSessionMessageLength) {
    OnSessionError(cdm_id, session_id, media::MediaKeys::kUnknownError, 0);
    return;
  }

  ProxyMediaKeys* media_keys = GetMediaKeys(cdm_id);
  if (media_keys)
    media_keys->OnSessionMessage(session_id, message, destination_url);
}

void RendererCdmManager::OnSessionReady(int cdm_id, uint32 session_id) {
  ProxyMediaKeys* media_keys = GetMediaKeys(cdm_id);
  if (media_keys)
    media_keys->OnSessionReady(session_id);
}

void RendererCdmManager::OnSessionClosed(int cdm_id, uint32 session_id) {
  ProxyMediaKeys* media_keys = GetMediaKeys(cdm_id);
  if (media_keys)
    media_keys->OnSessionClosed(session_id);
}

void RendererCdmManager::OnSessionError(int cdm_id,
                                        uint32 session_id,
                                        media::MediaKeys::KeyError error_code,
                                        uint32 system_code) {
  ProxyMediaKeys* media_keys = GetMediaKeys(cdm_id);
  if (media_keys)
    media_keys->OnSessionError(session_id, error_code, system_code);
}

int RendererCdmManager::RegisterMediaKeys(ProxyMediaKeys* media_keys) {
  int cdm_id = next_cdm_id_++;
  DCHECK_NE(cdm_id, kInvalidCdmId);
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
