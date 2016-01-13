// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/cdm/browser_cdm_manager.h"

#include "base/command_line.h"
#include "content/common/media/cdm_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "media/base/browser_cdm.h"
#include "media/base/browser_cdm_factory.h"
#include "media/base/media_switches.h"

namespace content {

using media::BrowserCdm;
using media::MediaKeys;

// Maximum lengths for various EME API parameters. These are checks to
// prevent unnecessarily large parameters from being passed around, and the
// lengths are somewhat arbitrary as the EME spec doesn't specify any limits.
const size_t kMaxInitDataLength = 64 * 1024;  // 64 KB
const size_t kMaxSessionResponseLength = 64 * 1024;  // 64 KB
const size_t kMaxKeySystemLength = 256;

// static
BrowserCdmManager* BrowserCdmManager::Create(RenderFrameHost* rfh) {
  return new BrowserCdmManager(rfh);
}

BrowserCdmManager::BrowserCdmManager(RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host),
      web_contents_(WebContents::FromRenderFrameHost(render_frame_host)),
      weak_ptr_factory_(this) {
}

BrowserCdmManager::~BrowserCdmManager() {
  // During the tear down process, OnDestroyCdm() may or may not be called
  // (e.g. WebContents may be destroyed before the render process is killed). So
  // we cannot DCHECK(cdm_map_.empty()) here. Instead, all CDMs in |cdm_map_|
  // will be destroyed here because they are owned by BrowserCdmManager.
}

BrowserCdm* BrowserCdmManager::GetCdm(int cdm_id) {
  return cdm_map_.get(cdm_id);
}

void BrowserCdmManager::OnSessionCreated(
    int cdm_id,
    uint32 session_id,
    const std::string& web_session_id) {
  Send(new CdmMsg_SessionCreated(
      RoutingID(), cdm_id, session_id, web_session_id));
}

void BrowserCdmManager::OnSessionMessage(
    int cdm_id,
    uint32 session_id,
    const std::vector<uint8>& message,
    const GURL& destination_url) {
  GURL verified_gurl = destination_url;
  if (!verified_gurl.is_valid() && !verified_gurl.is_empty()) {
    DLOG(WARNING) << "SessionMessage destination_url is invalid : "
                  << destination_url.possibly_invalid_spec();
    verified_gurl = GURL::EmptyGURL();  // Replace invalid destination_url.
  }

  Send(new CdmMsg_SessionMessage(
      RoutingID(), cdm_id, session_id, message, verified_gurl));
}

void BrowserCdmManager::OnSessionReady(int cdm_id, uint32 session_id) {
  Send(new CdmMsg_SessionReady(RoutingID(), cdm_id, session_id));
}

void BrowserCdmManager::OnSessionClosed(int cdm_id, uint32 session_id) {
  Send(new CdmMsg_SessionClosed(RoutingID(), cdm_id, session_id));
}

void BrowserCdmManager::OnSessionError(int cdm_id,
                                       uint32 session_id,
                                       MediaKeys::KeyError error_code,
                                       uint32 system_code) {
  Send(new CdmMsg_SessionError(
      RoutingID(), cdm_id, session_id, error_code, system_code));
}

void BrowserCdmManager::OnInitializeCdm(int cdm_id,
                                                const std::string& key_system,
                                                const GURL& security_origin) {
  if (key_system.size() > kMaxKeySystemLength) {
    // This failure will be discovered and reported by OnCreateSession()
    // as GetCdm() will return null.
    NOTREACHED() << "Invalid key system: " << key_system;
    return;
  }

  AddCdm(cdm_id, key_system, security_origin);
}

void BrowserCdmManager::OnCreateSession(
    int cdm_id,
    uint32 session_id,
    CdmHostMsg_CreateSession_ContentType content_type,
    const std::vector<uint8>& init_data) {
  if (init_data.size() > kMaxInitDataLength) {
    LOG(WARNING) << "InitData for ID: " << cdm_id
                 << " too long: " << init_data.size();
    OnSessionError(cdm_id, session_id, MediaKeys::kUnknownError, 0);
    return;
  }

  // Convert the session content type into a MIME type. "audio" and "video"
  // don't matter, so using "video" for the MIME type.
  // Ref:
  // https://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#dom-createsession
  std::string mime_type;
  switch (content_type) {
    case CREATE_SESSION_TYPE_WEBM:
      mime_type = "video/webm";
      break;
    case CREATE_SESSION_TYPE_MP4:
      mime_type = "video/mp4";
      break;
    default:
      NOTREACHED();
      return;
  }

#if defined(OS_ANDROID)
  if (CommandLine::ForCurrentProcess()
      ->HasSwitch(switches::kDisableInfobarForProtectedMediaIdentifier)) {
    CreateSessionIfPermitted(cdm_id, session_id, mime_type, init_data, true);
    return;
  }
#endif

  BrowserCdm* cdm = GetCdm(cdm_id);
  if (!cdm) {
    DLOG(WARNING) << "No CDM for ID " << cdm_id << " found";
    OnSessionError(cdm_id, session_id, MediaKeys::kUnknownError, 0);
    return;
  }

  std::map<int, GURL>::const_iterator iter =
      cdm_security_origin_map_.find(cdm_id);
  if (iter == cdm_security_origin_map_.end()) {
    NOTREACHED();
    OnSessionError(cdm_id, session_id, MediaKeys::kUnknownError, 0);
    return;
  }

  base::Closure cancel_callback;
  GetContentClient()->browser()->RequestProtectedMediaIdentifierPermission(
      web_contents_,
      iter->second,
      base::Bind(&BrowserCdmManager::CreateSessionIfPermitted,
                 weak_ptr_factory_.GetWeakPtr(),
                 cdm_id,
                 session_id,
                 mime_type,
                 init_data),
      &cancel_callback);
  if (!cancel_callback.is_null())
    cdm_cancel_permision_map_[cdm_id] = cancel_callback;
}

void BrowserCdmManager::OnUpdateSession(
    int cdm_id,
    uint32 session_id,
    const std::vector<uint8>& response) {
  BrowserCdm* cdm = GetCdm(cdm_id);
  if (!cdm) {
    DLOG(WARNING) << "No CDM for ID " << cdm_id << " found";
    OnSessionError(cdm_id, session_id, MediaKeys::kUnknownError, 0);
    return;
  }

  if (response.size() > kMaxSessionResponseLength) {
    LOG(WARNING) << "Response for ID " << cdm_id
                 << " is too long: " << response.size();
    OnSessionError(cdm_id, session_id, MediaKeys::kUnknownError, 0);
    return;
  }

  cdm->UpdateSession(session_id, &response[0], response.size());
}

void BrowserCdmManager::OnReleaseSession(int cdm_id, uint32 session_id) {
  BrowserCdm* cdm = GetCdm(cdm_id);
  if (!cdm) {
    DLOG(WARNING) << "No CDM for ID " << cdm_id << " found";
    OnSessionError(cdm_id, session_id, MediaKeys::kUnknownError, 0);
    return;
  }

  cdm->ReleaseSession(session_id);
}

void BrowserCdmManager::OnDestroyCdm(int cdm_id) {
  BrowserCdm* cdm = GetCdm(cdm_id);
  if (!cdm)
    return;

  CancelAllPendingSessionCreations(cdm_id);
  RemoveCdm(cdm_id);
}

void BrowserCdmManager::CancelAllPendingSessionCreations(int cdm_id) {
  if (cdm_cancel_permision_map_.count(cdm_id)) {
    cdm_cancel_permision_map_[cdm_id].Run();
    cdm_cancel_permision_map_.erase(cdm_id);
  }
}

void BrowserCdmManager::AddCdm(int cdm_id,
                               const std::string& key_system,
                               const GURL& security_origin) {
  DCHECK(!GetCdm(cdm_id));
  base::WeakPtr<BrowserCdmManager> weak_this = weak_ptr_factory_.GetWeakPtr();
  scoped_ptr<BrowserCdm> cdm(media::CreateBrowserCdm(
      key_system,
      base::Bind(&BrowserCdmManager::OnSessionCreated, weak_this, cdm_id),
      base::Bind(&BrowserCdmManager::OnSessionMessage, weak_this, cdm_id),
      base::Bind(&BrowserCdmManager::OnSessionReady, weak_this, cdm_id),
      base::Bind(&BrowserCdmManager::OnSessionClosed, weak_this, cdm_id),
      base::Bind(&BrowserCdmManager::OnSessionError, weak_this, cdm_id)));

  if (!cdm) {
    // This failure will be discovered and reported by OnCreateSession()
    // as GetCdm() will return null.
    DVLOG(1) << "failed to create CDM.";
    return;
  }

  cdm_map_.add(cdm_id, cdm.Pass());
  cdm_security_origin_map_[cdm_id] = security_origin;
}

void BrowserCdmManager::RemoveCdm(int cdm_id) {
  // TODO(xhwang): Detach CDM from the player it's set to. In prefixed
  // EME implementation the current code is fine because we always destroy the
  // player before we destroy the DrmBridge. This will not always be the case
  // in unprefixed EME implementation.
  cdm_map_.erase(cdm_id);
  cdm_security_origin_map_.erase(cdm_id);
  cdm_cancel_permision_map_.erase(cdm_id);
}

int BrowserCdmManager::RoutingID() {
  return render_frame_host_->GetRoutingID();
}

bool BrowserCdmManager::Send(IPC::Message* msg) {
  return render_frame_host_->Send(msg);
}

void BrowserCdmManager::CreateSessionIfPermitted(
    int cdm_id,
    uint32 session_id,
    const std::string& content_type,
    const std::vector<uint8>& init_data,
    bool permitted) {
  cdm_cancel_permision_map_.erase(cdm_id);
  if (!permitted) {
    OnSessionError(cdm_id, session_id, MediaKeys::kUnknownError, 0);
    return;
  }

  BrowserCdm* cdm = GetCdm(cdm_id);
  if (!cdm) {
    DLOG(WARNING) << "No CDM for ID: " << cdm_id << " found";
    OnSessionError(cdm_id, session_id, MediaKeys::kUnknownError, 0);
    return;
  }

  // This could fail, in which case a SessionError will be fired.
  cdm->CreateSession(session_id, content_type, &init_data[0], init_data.size());
}

}  // namespace content
