// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CRYPTO_RENDERER_CDM_MANAGER_H_
#define CONTENT_RENDERER_MEDIA_CRYPTO_RENDERER_CDM_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "content/common/media/cdm_messages_enums.h"
#include "content/public/renderer/render_frame_observer.h"
#include "media/base/media_keys.h"
#include "url/gurl.h"

namespace blink {
class WebFrame;
}

namespace content {

class ProxyMediaKeys;

// Class for managing all the CDM objects in the same RenderFrame.
class RendererCdmManager : public RenderFrameObserver {
 public:
  static const int kInvalidCdmId = 0;

  // Constructs a RendererCdmManager object for the |render_frame|.
  explicit RendererCdmManager(RenderFrame* render_frame);
  virtual ~RendererCdmManager();

  // RenderFrameObserver overrides.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  // Encrypted media related methods.
  void InitializeCdm(int cdm_id,
                     ProxyMediaKeys* media_keys,
                     const std::string& key_system,
                     const GURL& security_origin);
  void CreateSession(int cdm_id,
                     uint32 session_id,
                     CdmHostMsg_CreateSession_ContentType conent_type,
                     const std::vector<uint8>& init_data);
  void UpdateSession(int cdm_id,
                     uint32 session_id,
                     const std::vector<uint8>& response);
  void ReleaseSession(int cdm_id, uint32 session_id);
  void DestroyCdm(int cdm_id);

  // Registers a ProxyMediaKeys object. Returns allocated CDM ID.
  int RegisterMediaKeys(ProxyMediaKeys* media_keys);

  // Unregisters a ProxyMediaKeys object identified by |cdm_id|.
  void UnregisterMediaKeys(int cdm_id);

 private:
  // Gets the pointer to ProxyMediaKeys given the |cdm_id|.
  ProxyMediaKeys* GetMediaKeys(int cdm_id);

  // Message handlers.
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

  // CDM ID should be unique per renderer frame.
  // TODO(xhwang): Use uint32 to prevent undefined overflow behavior.
  int next_cdm_id_;

  // CDM ID to ProxyMediaKeys mapping.
  std::map<int, ProxyMediaKeys*> proxy_media_keys_map_;

  DISALLOW_COPY_AND_ASSIGN(RendererCdmManager);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CRYPTO_RENDERER_CDM_MANAGER_H_
