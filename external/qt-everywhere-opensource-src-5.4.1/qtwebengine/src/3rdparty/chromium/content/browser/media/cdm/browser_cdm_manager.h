// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CDM_BROWSER_CDM_MANAGER_H_
#define CONTENT_BROWSER_MEDIA_CDM_BROWSER_CDM_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/common/media/cdm_messages_enums.h"
#include "ipc/ipc_message.h"
// TODO(xhwang): Drop this when KeyError is moved to a common header.
#include "media/base/media_keys.h"
#include "url/gurl.h"

namespace media {
class BrowserCdm;
}

namespace content {

class RenderFrameHost;
class WebContents;

// This class manages all CDM objects. It receives control operations from the
// the render process, and forwards them to corresponding CDM object. Callbacks
// from CDM objects are converted to IPCs and then sent to the render process.
class CONTENT_EXPORT BrowserCdmManager {
 public:
  // Creates a new BrowserCdmManager for |rfh|.
  static BrowserCdmManager* Create(RenderFrameHost* rfh);

  ~BrowserCdmManager();

  media::BrowserCdm* GetCdm(int cdm_id);

  // CDM callbacks.
  void OnSessionCreated(int cdm_id,
                        uint32 session_id,
                        const std::string& web_session_id);
  void OnSessionMessage(int cdm_id,
                        uint32 session_id,
                        const std::vector<uint8>& message,
                        const GURL& destination_url);
  void OnSessionReady(int cdm_id, uint32 session_id);
  void OnSessionClosed(int cdm_id, uint32 session_id);
  void OnSessionError(int cdm_id,
                      uint32 session_id,
                      media::MediaKeys::KeyError error_code,
                      uint32 system_code);

  // Message handlers.
  void OnInitializeCdm(int cdm_id,
                       const std::string& key_system,
                       const GURL& frame_url);
  void OnCreateSession(int cdm_id,
                       uint32 session_id,
                       CdmHostMsg_CreateSession_ContentType content_type,
                       const std::vector<uint8>& init_data);
  void OnUpdateSession(int cdm_id,
                       uint32 session_id,
                       const std::vector<uint8>& response);
  void OnReleaseSession(int cdm_id, uint32 session_id);
  void OnSetCdm(int player_id, int cdm_id);
  void OnDestroyCdm(int cdm_id);

 private:
  // Clients must use Create() or subclass constructor.
  explicit BrowserCdmManager(RenderFrameHost* render_frame_host);

  // Cancels all pending session creations associated with |cdm_id|.
  void CancelAllPendingSessionCreations(int cdm_id);

  // Adds a new CDM identified by |cdm_id| for the given |key_system| and
  // |security_origin|.
  void AddCdm(int cdm_id,
              const std::string& key_system,
              const GURL& security_origin);

  // Removes the CDM with the specified id.
  void RemoveCdm(int cdm_id);

  int RoutingID();

  // Helper function to send messages to RenderFrameObserver.
  bool Send(IPC::Message* msg);

  // If |permitted| is false, it does nothing but send
  // |CdmMsg_SessionError| IPC message.
  // The primary use case is infobar permission callback, i.e., when infobar
  // can decide user's intention either from interacting with the actual info
  // bar or from the saved preference.
  void CreateSessionIfPermitted(int cdm_id,
                                uint32 session_id,
                                const std::string& content_type,
                                const std::vector<uint8>& init_data,
                                bool permitted);

  RenderFrameHost* const render_frame_host_;

  WebContents* const web_contents_;

  // A map from CDM IDs to managed CDMs.
  typedef base::ScopedPtrHashMap<int, media::BrowserCdm> CdmMap;
  CdmMap cdm_map_;

  // Map from CDM ID to CDM's security origin.
  std::map<int, GURL> cdm_security_origin_map_;

  // Map from CDM ID to a callback to cancel the permission request.
  std::map<int, base::Closure> cdm_cancel_permision_map_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<BrowserCdmManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserCdmManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CDM_BROWSER_CDM_MANAGER_H_
