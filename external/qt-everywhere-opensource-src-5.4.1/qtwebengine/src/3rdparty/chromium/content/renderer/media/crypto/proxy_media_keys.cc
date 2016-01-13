// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/crypto/proxy_media_keys.h"

#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "content/renderer/media/crypto/key_systems.h"
#include "content/renderer/media/crypto/renderer_cdm_manager.h"
#include "media/base/cdm_promise.h"

namespace content {

scoped_ptr<ProxyMediaKeys> ProxyMediaKeys::Create(
    const std::string& key_system,
    const GURL& security_origin,
    RendererCdmManager* manager,
    const media::SessionMessageCB& session_message_cb,
    const media::SessionReadyCB& session_ready_cb,
    const media::SessionClosedCB& session_closed_cb,
    const media::SessionErrorCB& session_error_cb) {
  DCHECK(manager);
  scoped_ptr<ProxyMediaKeys> proxy_media_keys(
      new ProxyMediaKeys(manager,
                         session_message_cb,
                         session_ready_cb,
                         session_closed_cb,
                         session_error_cb));
  proxy_media_keys->InitializeCdm(key_system, security_origin);
  return proxy_media_keys.Pass();
}

ProxyMediaKeys::~ProxyMediaKeys() {
  manager_->DestroyCdm(cdm_id_);
  manager_->UnregisterMediaKeys(cdm_id_);

  // Reject any outstanding promises.
  for (PromiseMap::iterator it = session_id_to_promise_map_.begin();
       it != session_id_to_promise_map_.end();
       ++it) {
    it->second->reject(
        media::MediaKeys::NOT_SUPPORTED_ERROR, 0, "The operation was aborted.");
  }
  session_id_to_promise_map_.clear();
}

void ProxyMediaKeys::CreateSession(
    const std::string& init_data_type,
    const uint8* init_data,
    int init_data_length,
    SessionType session_type,
    scoped_ptr<media::NewSessionCdmPromise> promise) {
  // TODO(xhwang): Move these checks up to blink and DCHECK here.
  // See http://crbug.com/342510
  CdmHostMsg_CreateSession_ContentType create_session_content_type;
  if (init_data_type == "audio/mp4" || init_data_type == "video/mp4") {
    create_session_content_type = CREATE_SESSION_TYPE_MP4;
  } else if (init_data_type == "audio/webm" || init_data_type == "video/webm") {
    create_session_content_type = CREATE_SESSION_TYPE_WEBM;
  } else {
    DLOG(ERROR) << "Unsupported EME CreateSession content type of "
                << init_data_type;
    promise->reject(
        NOT_SUPPORTED_ERROR,
        0,
        "Unsupported EME CreateSession init data type of " + init_data_type);
    return;
  }

  uint32 session_id = CreateSessionId();
  SavePromise(session_id, promise.PassAs<media::CdmPromise>());
  manager_->CreateSession(
      cdm_id_,
      session_id,
      create_session_content_type,
      std::vector<uint8>(init_data, init_data + init_data_length));
}

void ProxyMediaKeys::LoadSession(
    const std::string& web_session_id,
    scoped_ptr<media::NewSessionCdmPromise> promise) {
  // TODO(xhwang): Check key system and platform support for LoadSession in
  // blink and add NOTREACHED() here.
  DLOG(ERROR) << "ProxyMediaKeys doesn't support session loading.";
  promise->reject(NOT_SUPPORTED_ERROR, 0, "LoadSession() is not supported.");
}

void ProxyMediaKeys::UpdateSession(
    const std::string& web_session_id,
    const uint8* response,
    int response_length,
    scoped_ptr<media::SimpleCdmPromise> promise) {
  uint32 session_id = LookupSessionId(web_session_id);
  if (!session_id) {
    promise->reject(INVALID_ACCESS_ERROR, 0, "Session does not exist.");
    return;
  }

  SavePromise(session_id, promise.PassAs<media::CdmPromise>());
  manager_->UpdateSession(
      cdm_id_,
      session_id,
      std::vector<uint8>(response, response + response_length));
}

void ProxyMediaKeys::ReleaseSession(
    const std::string& web_session_id,
    scoped_ptr<media::SimpleCdmPromise> promise) {
  uint32 session_id = LookupSessionId(web_session_id);
  if (!session_id) {
    promise->reject(INVALID_ACCESS_ERROR, 0, "Session does not exist.");
    return;
  }

  SavePromise(session_id, promise.PassAs<media::CdmPromise>());
  manager_->ReleaseSession(cdm_id_, session_id);
}

void ProxyMediaKeys::OnSessionCreated(uint32 session_id,
                                      const std::string& web_session_id) {
  AssignWebSessionId(session_id, web_session_id);
  scoped_ptr<media::CdmPromise> promise = TakePromise(session_id);
  if (promise) {
    media::NewSessionCdmPromise* session_promise(
        static_cast<media::NewSessionCdmPromise*>(promise.get()));
    session_promise->resolve(web_session_id);
  }
}

void ProxyMediaKeys::OnSessionMessage(uint32 session_id,
                                      const std::vector<uint8>& message,
                                      const GURL& destination_url) {
  session_message_cb_.Run(
      LookupWebSessionId(session_id), message, destination_url);
}

void ProxyMediaKeys::OnSessionReady(uint32 session_id) {
  scoped_ptr<media::CdmPromise> promise = TakePromise(session_id);
  if (promise) {
    media::SimpleCdmPromise* simple_promise(
        static_cast<media::SimpleCdmPromise*>(promise.get()));
    simple_promise->resolve();
  } else {
    // Still needed for keyadded.
    const std::string web_session_id = LookupWebSessionId(session_id);
    session_ready_cb_.Run(web_session_id);
  }
}

void ProxyMediaKeys::OnSessionClosed(uint32 session_id) {
  const std::string web_session_id = LookupWebSessionId(session_id);
  DropWebSessionId(web_session_id);
  scoped_ptr<media::CdmPromise> promise = TakePromise(session_id);
  if (promise) {
    media::SimpleCdmPromise* simple_promise(
        static_cast<media::SimpleCdmPromise*>(promise.get()));
    simple_promise->resolve();
  } else {
    // It is possible for the CDM to close a session independent of a
    // Release() request.
    session_closed_cb_.Run(web_session_id);
  }
}

void ProxyMediaKeys::OnSessionError(uint32 session_id,
                                    media::MediaKeys::KeyError error_code,
                                    uint32 system_code) {
  const std::string web_session_id = LookupWebSessionId(session_id);
  media::MediaKeys::Exception exception_code;
  switch (error_code) {
    case media::MediaKeys::kClientError:
      exception_code = media::MediaKeys::CLIENT_ERROR;
      break;
    case media::MediaKeys::kOutputError:
      exception_code = media::MediaKeys::OUTPUT_ERROR;
      break;
    case media::MediaKeys::kUnknownError:
    default:
      exception_code = media::MediaKeys::UNKNOWN_ERROR;
      break;
  }

  scoped_ptr<media::CdmPromise> promise = TakePromise(session_id);
  if (promise) {
    promise->reject(exception_code, system_code, std::string());
    return;
  }

  // Errors generally happen in response to a request, but it is possible
  // for something bad to happen in the CDM and it needs to tell the client.
  session_error_cb_.Run(
      web_session_id, exception_code, system_code, std::string());
}

int ProxyMediaKeys::GetCdmId() const {
  return cdm_id_;
}

ProxyMediaKeys::ProxyMediaKeys(
    RendererCdmManager* manager,
    const media::SessionMessageCB& session_message_cb,
    const media::SessionReadyCB& session_ready_cb,
    const media::SessionClosedCB& session_closed_cb,
    const media::SessionErrorCB& session_error_cb)
    : manager_(manager),
      session_message_cb_(session_message_cb),
      session_ready_cb_(session_ready_cb),
      session_closed_cb_(session_closed_cb),
      session_error_cb_(session_error_cb),
      next_session_id_(1) {
  cdm_id_ = manager->RegisterMediaKeys(this);
}

void ProxyMediaKeys::InitializeCdm(const std::string& key_system,
                                   const GURL& security_origin) {
  manager_->InitializeCdm(cdm_id_, this, key_system, security_origin);
}

uint32_t ProxyMediaKeys::CreateSessionId() {
  return next_session_id_++;
}

void ProxyMediaKeys::AssignWebSessionId(uint32_t session_id,
                                        const std::string& web_session_id) {
  DCHECK(!ContainsKey(web_session_to_session_id_map_, web_session_id));
  DCHECK(session_id);
  web_session_to_session_id_map_.insert(
      std::make_pair(web_session_id, session_id));
}

uint32_t ProxyMediaKeys::LookupSessionId(
    const std::string& web_session_id) const {
  SessionIdMap::const_iterator it =
      web_session_to_session_id_map_.find(web_session_id);
  return (it != web_session_to_session_id_map_.end()) ? it->second : 0;
}

std::string ProxyMediaKeys::LookupWebSessionId(uint32_t session_id) const {
  for (SessionIdMap::const_iterator it = web_session_to_session_id_map_.begin();
       it != web_session_to_session_id_map_.end();
       ++it) {
    if (it->second == session_id)
      return it->first;
  }
  // Possible to get an error creating a session, so no |web_session_id|
  // available.
  return std::string();
}

void ProxyMediaKeys::DropWebSessionId(const std::string& web_session_id) {
  web_session_to_session_id_map_.erase(web_session_id);
}

void ProxyMediaKeys::SavePromise(uint32_t session_id,
                                 scoped_ptr<media::CdmPromise> promise) {
  // Should only be one promise outstanding for any |session_id|.
  DCHECK(!ContainsKey(session_id_to_promise_map_, session_id));
  session_id_to_promise_map_.add(session_id, promise.Pass());
}

scoped_ptr<media::CdmPromise> ProxyMediaKeys::TakePromise(uint32_t session_id) {
  PromiseMap::iterator it = session_id_to_promise_map_.find(session_id);
  // May not be a promise associated with this session for asynchronous events.
  if (it == session_id_to_promise_map_.end())
    return scoped_ptr<media::CdmPromise>();
  return session_id_to_promise_map_.take_and_erase(it);
}

}  // namespace content
