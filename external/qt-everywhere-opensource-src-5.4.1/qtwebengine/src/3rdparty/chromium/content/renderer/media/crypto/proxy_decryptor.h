// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CRYPTO_PROXY_DECRYPTOR_H_
#define CONTENT_RENDERER_MEDIA_CRYPTO_PROXY_DECRYPTOR_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "media/base/decryptor.h"
#include "media/base/media_keys.h"

#if defined(ENABLE_PEPPER_CDMS)
#include "content/renderer/media/crypto/pepper_cdm_wrapper.h"
#endif

class GURL;

namespace content {

#if defined(ENABLE_BROWSER_CDMS)
class RendererCdmManager;
#endif  // defined(ENABLE_BROWSER_CDMS)

// ProxyDecryptor is for EME v0.1b only. It should not be used for the WD API.
// A decryptor proxy that creates a real decryptor object on demand and
// forwards decryptor calls to it.
//
// TODO(xhwang): Currently we don't support run-time switching among decryptor
// objects. Fix this when needed.
// TODO(xhwang): The ProxyDecryptor is not a Decryptor. Find a better name!
class ProxyDecryptor {
 public:
  // These are similar to the callbacks in media_keys.h, but pass back the
  // web session ID rather than the internal session ID.
  typedef base::Callback<void(const std::string& session_id)> KeyAddedCB;
  typedef base::Callback<void(const std::string& session_id,
                              media::MediaKeys::KeyError error_code,
                              uint32 system_code)> KeyErrorCB;
  typedef base::Callback<void(const std::string& session_id,
                              const std::vector<uint8>& message,
                              const GURL& destination_url)> KeyMessageCB;

  ProxyDecryptor(
#if defined(ENABLE_PEPPER_CDMS)
      const CreatePepperCdmCB& create_pepper_cdm_cb,
#elif defined(ENABLE_BROWSER_CDMS)
      RendererCdmManager* manager,
#endif  // defined(ENABLE_PEPPER_CDMS)
      const KeyAddedCB& key_added_cb,
      const KeyErrorCB& key_error_cb,
      const KeyMessageCB& key_message_cb);
  virtual ~ProxyDecryptor();

  // Returns the Decryptor associated with this object. May be NULL if no
  // Decryptor is associated.
  media::Decryptor* GetDecryptor();

#if defined(ENABLE_BROWSER_CDMS)
  // Returns the CDM ID associated with this object. May be kInvalidCdmId if no
  // CDM ID is associated, such as when Clear Key is used.
  int GetCdmId();
#endif

  // Only call this once.
  bool InitializeCDM(const std::string& key_system,
                     const GURL& security_origin);

  // May only be called after InitializeCDM() succeeds.
  bool GenerateKeyRequest(const std::string& type,
                          const uint8* init_data,
                          int init_data_length);
  void AddKey(const uint8* key, int key_length,
              const uint8* init_data, int init_data_length,
              const std::string& session_id);
  void CancelKeyRequest(const std::string& session_id);

 private:
  // Helper function to create MediaKeys to handle the given |key_system|.
  scoped_ptr<media::MediaKeys> CreateMediaKeys(const std::string& key_system,
                                               const GURL& security_origin);

  // Callbacks for firing session events.
  void OnSessionMessage(const std::string& web_session_id,
                        const std::vector<uint8>& message,
                        const GURL& default_url);
  void OnSessionReady(const std::string& web_session_id);
  void OnSessionClosed(const std::string& web_session_id);
  void OnSessionError(const std::string& web_session_id,
                      media::MediaKeys::Exception exception_code,
                      uint32 system_code,
                      const std::string& error_message);

  // Called when a session is actually created or loaded.
  void SetSessionId(bool persistent, const std::string& web_session_id);

#if defined(ENABLE_PEPPER_CDMS)
  // Callback to create the Pepper plugin.
  CreatePepperCdmCB create_pepper_cdm_cb_;
#elif defined(ENABLE_BROWSER_CDMS)
  RendererCdmManager* manager_;
  int cdm_id_;
#endif  // defined(ENABLE_PEPPER_CDMS)

  // The real MediaKeys that manages key operations for the ProxyDecryptor.
  scoped_ptr<media::MediaKeys> media_keys_;

  // Callbacks for firing key events.
  KeyAddedCB key_added_cb_;
  KeyErrorCB key_error_cb_;
  KeyMessageCB key_message_cb_;

  // Keep track of both persistent and non-persistent sessions.
  base::hash_map<std::string, bool> active_sessions_;

  bool is_clear_key_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<ProxyDecryptor> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProxyDecryptor);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CRYPTO_PROXY_DECRYPTOR_H_
