// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CRYPTO_PROXY_MEDIA_KEYS_H_
#define CONTENT_RENDERER_MEDIA_CRYPTO_PROXY_MEDIA_KEYS_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "media/base/cdm_promise.h"
#include "media/base/media_keys.h"

class GURL;

namespace content {

class RendererCdmManager;

// A MediaKeys proxy that wraps the EME part of RendererCdmManager.
class ProxyMediaKeys : public media::MediaKeys {
 public:
  static scoped_ptr<ProxyMediaKeys> Create(
      const std::string& key_system,
      const GURL& security_origin,
      RendererCdmManager* manager,
      const media::SessionMessageCB& session_message_cb,
      const media::SessionReadyCB& session_ready_cb,
      const media::SessionClosedCB& session_closed_cb,
      const media::SessionErrorCB& session_error_cb);

  virtual ~ProxyMediaKeys();

  // MediaKeys implementation.
  virtual void CreateSession(
      const std::string& init_data_type,
      const uint8* init_data,
      int init_data_length,
      SessionType session_type,
      scoped_ptr<media::NewSessionCdmPromise> promise) OVERRIDE;
  virtual void LoadSession(
      const std::string& web_session_id,
      scoped_ptr<media::NewSessionCdmPromise> promise) OVERRIDE;
  virtual void UpdateSession(
      const std::string& web_session_id,
      const uint8* response,
      int response_length,
      scoped_ptr<media::SimpleCdmPromise> promise) OVERRIDE;
  virtual void ReleaseSession(
      const std::string& web_session_id,
      scoped_ptr<media::SimpleCdmPromise> promise) OVERRIDE;

  // Callbacks.
  void OnSessionCreated(uint32 session_id, const std::string& web_session_id);
  void OnSessionMessage(uint32 session_id,
                        const std::vector<uint8>& message,
                        const GURL& destination_url);
  void OnSessionReady(uint32 session_id);
  void OnSessionClosed(uint32 session_id);
  void OnSessionError(uint32 session_id,
                      media::MediaKeys::KeyError error_code,
                      uint32 system_code);

  int GetCdmId() const;

 private:
  // The Android-specific code that handles sessions uses integer session ids
  // (basically a reference id), but media::MediaKeys bases everything on
  // web_session_id (a string representing the actual session id as generated
  // by the CDM). SessionIdMap is used to map between the web_session_id and
  // the session_id used by the Android-specific code.
  typedef base::hash_map<std::string, uint32_t> SessionIdMap;

  // The following types keep track of Promises. The index is the
  // Android-specific session_id, so that returning results can be matched to
  // the corresponding promise.
  typedef base::ScopedPtrHashMap<uint32_t, media::CdmPromise> PromiseMap;

  ProxyMediaKeys(RendererCdmManager* manager,
                 const media::SessionMessageCB& session_message_cb,
                 const media::SessionReadyCB& session_ready_cb,
                 const media::SessionClosedCB& session_closed_cb,
                 const media::SessionErrorCB& session_error_cb);

  void InitializeCdm(const std::string& key_system,
                     const GURL& security_origin);

  // These functions keep track of Android-specific session_ids <->
  // web_session_ids mappings.
  // TODO(jrummell): Remove this once the Android-specific code changes to
  // support string web session ids.
  uint32_t CreateSessionId();
  void AssignWebSessionId(uint32_t session_id,
                          const std::string& web_session_id);
  uint32_t LookupSessionId(const std::string& web_session_id) const;
  std::string LookupWebSessionId(uint32_t session_id) const;
  void DropWebSessionId(const std::string& web_session_id);

  // Helper function to keep track of promises. Adding takes ownership of the
  // promise, transferred back to caller on take.
  void SavePromise(uint32_t session_id, scoped_ptr<media::CdmPromise> promise);
  scoped_ptr<media::CdmPromise> TakePromise(uint32_t session_id);

  RendererCdmManager* manager_;
  int cdm_id_;

  media::SessionMessageCB session_message_cb_;
  media::SessionReadyCB session_ready_cb_;
  media::SessionClosedCB session_closed_cb_;
  media::SessionErrorCB session_error_cb_;

  // Android-specific. See comment above CreateSessionId().
  uint32_t next_session_id_;
  SessionIdMap web_session_to_session_id_map_;

  // Keep track of outstanding promises. This map owns the promise object.
  PromiseMap session_id_to_promise_map_;

  DISALLOW_COPY_AND_ASSIGN(ProxyMediaKeys);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CRYPTO_PROXY_MEDIA_KEYS_H_
