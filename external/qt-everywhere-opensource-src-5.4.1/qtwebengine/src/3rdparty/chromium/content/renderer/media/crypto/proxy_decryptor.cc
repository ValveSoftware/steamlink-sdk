// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/crypto/proxy_decryptor.h"

#include <cstring>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "content/renderer/media/crypto/content_decryption_module_factory.h"
#include "media/base/cdm_promise.h"
#include "media/cdm/json_web_key.h"
#include "media/cdm/key_system_names.h"

#if defined(ENABLE_PEPPER_CDMS)
#include "content/renderer/media/crypto/pepper_cdm_wrapper.h"
#endif  // defined(ENABLE_PEPPER_CDMS)

#if defined(ENABLE_BROWSER_CDMS)
#include "content/renderer/media/crypto/renderer_cdm_manager.h"
#endif  // defined(ENABLE_BROWSER_CDMS)

namespace content {

// Special system code to signal a closed persistent session in a SessionError()
// call. This is needed because there is no SessionClosed() call in the prefixed
// EME API.
const int kSessionClosedSystemCode = 29127;

ProxyDecryptor::ProxyDecryptor(
#if defined(ENABLE_PEPPER_CDMS)
    const CreatePepperCdmCB& create_pepper_cdm_cb,
#elif defined(ENABLE_BROWSER_CDMS)
    RendererCdmManager* manager,
#endif  // defined(ENABLE_PEPPER_CDMS)
    const KeyAddedCB& key_added_cb,
    const KeyErrorCB& key_error_cb,
    const KeyMessageCB& key_message_cb)
    :
#if defined(ENABLE_PEPPER_CDMS)
      create_pepper_cdm_cb_(create_pepper_cdm_cb),
#elif defined(ENABLE_BROWSER_CDMS)
      manager_(manager),
      cdm_id_(RendererCdmManager::kInvalidCdmId),
#endif  // defined(ENABLE_PEPPER_CDMS)
      key_added_cb_(key_added_cb),
      key_error_cb_(key_error_cb),
      key_message_cb_(key_message_cb),
      is_clear_key_(false),
      weak_ptr_factory_(this) {
#if defined(ENABLE_PEPPER_CDMS)
  DCHECK(!create_pepper_cdm_cb_.is_null());
#endif  // defined(ENABLE_PEPPER_CDMS)
  DCHECK(!key_added_cb_.is_null());
  DCHECK(!key_error_cb_.is_null());
  DCHECK(!key_message_cb_.is_null());
}

ProxyDecryptor::~ProxyDecryptor() {
  // Destroy the decryptor explicitly before destroying the plugin.
  media_keys_.reset();
}

media::Decryptor* ProxyDecryptor::GetDecryptor() {
  return media_keys_ ? media_keys_->GetDecryptor() : NULL;
}

#if defined(ENABLE_BROWSER_CDMS)
int ProxyDecryptor::GetCdmId() {
  return cdm_id_;
}
#endif

bool ProxyDecryptor::InitializeCDM(const std::string& key_system,
                                   const GURL& security_origin) {
  DVLOG(1) << "InitializeCDM: key_system = " << key_system;

  DCHECK(!media_keys_);
  media_keys_ = CreateMediaKeys(key_system, security_origin);
  if (!media_keys_)
    return false;

  is_clear_key_ =
      media::IsClearKey(key_system) || media::IsExternalClearKey(key_system);
  return true;
}

// Returns true if |data| is prefixed with |header| and has data after the
// |header|.
bool HasHeader(const uint8* data, int data_length, const std::string& header) {
  return static_cast<size_t>(data_length) > header.size() &&
         std::equal(data, data + header.size(), header.begin());
}

bool ProxyDecryptor::GenerateKeyRequest(const std::string& content_type,
                                        const uint8* init_data,
                                        int init_data_length) {
  DVLOG(1) << "GenerateKeyRequest()";
  const char kPrefixedApiPersistentSessionHeader[] = "PERSISTENT|";
  const char kPrefixedApiLoadSessionHeader[] = "LOAD_SESSION|";

  bool loadSession =
      HasHeader(init_data, init_data_length, kPrefixedApiLoadSessionHeader);
  bool persistent = HasHeader(
      init_data, init_data_length, kPrefixedApiPersistentSessionHeader);

  scoped_ptr<media::NewSessionCdmPromise> promise(
      new media::NewSessionCdmPromise(
          base::Bind(&ProxyDecryptor::SetSessionId,
                     weak_ptr_factory_.GetWeakPtr(),
                     persistent || loadSession),
          base::Bind(&ProxyDecryptor::OnSessionError,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::string())));  // No session id until created.

  if (loadSession) {
    media_keys_->LoadSession(
        std::string(reinterpret_cast<const char*>(
                        init_data + strlen(kPrefixedApiLoadSessionHeader)),
                    init_data_length - strlen(kPrefixedApiLoadSessionHeader)),
        promise.Pass());
    return true;
  }

  media::MediaKeys::SessionType session_type =
      persistent ? media::MediaKeys::PERSISTENT_SESSION
                 : media::MediaKeys::TEMPORARY_SESSION;
  media_keys_->CreateSession(
      content_type, init_data, init_data_length, session_type, promise.Pass());
  return true;
}

void ProxyDecryptor::AddKey(const uint8* key,
                            int key_length,
                            const uint8* init_data,
                            int init_data_length,
                            const std::string& web_session_id) {
  DVLOG(1) << "AddKey()";

  // In the prefixed API, the session parameter provided to addKey() is
  // optional, so use the single existing session if it exists.
  // TODO(jrummell): remove when the prefixed API is removed.
  std::string session_id(web_session_id);
  if (session_id.empty()) {
    if (active_sessions_.size() == 1) {
      base::hash_map<std::string, bool>::iterator it = active_sessions_.begin();
      session_id = it->first;
    } else {
      OnSessionError(std::string(),
                     media::MediaKeys::NOT_SUPPORTED_ERROR,
                     0,
                     "SessionId not specified.");
      return;
    }
  }

  scoped_ptr<media::SimpleCdmPromise> promise(
      new media::SimpleCdmPromise(base::Bind(&ProxyDecryptor::OnSessionReady,
                                             weak_ptr_factory_.GetWeakPtr(),
                                             web_session_id),
                                  base::Bind(&ProxyDecryptor::OnSessionError,
                                             weak_ptr_factory_.GetWeakPtr(),
                                             web_session_id)));

  // EME WD spec only supports a single array passed to the CDM. For
  // Clear Key using v0.1b, both arrays are used (|init_data| is key_id).
  // Since the EME WD spec supports the key as a JSON Web Key,
  // convert the 2 arrays to a JWK and pass it as the single array.
  if (is_clear_key_) {
    // Decryptor doesn't support empty key ID (see http://crbug.com/123265).
    // So ensure a non-empty value is passed.
    if (!init_data) {
      static const uint8 kDummyInitData[1] = {0};
      init_data = kDummyInitData;
      init_data_length = arraysize(kDummyInitData);
    }

    std::string jwk =
        media::GenerateJWKSet(key, key_length, init_data, init_data_length);
    DCHECK(!jwk.empty());
    media_keys_->UpdateSession(session_id,
                               reinterpret_cast<const uint8*>(jwk.data()),
                               jwk.size(),
                               promise.Pass());
    return;
  }

  media_keys_->UpdateSession(session_id, key, key_length, promise.Pass());
}

void ProxyDecryptor::CancelKeyRequest(const std::string& web_session_id) {
  DVLOG(1) << "CancelKeyRequest()";

  scoped_ptr<media::SimpleCdmPromise> promise(
      new media::SimpleCdmPromise(base::Bind(&ProxyDecryptor::OnSessionClosed,
                                             weak_ptr_factory_.GetWeakPtr(),
                                             web_session_id),
                                  base::Bind(&ProxyDecryptor::OnSessionError,
                                             weak_ptr_factory_.GetWeakPtr(),
                                             web_session_id)));
  media_keys_->ReleaseSession(web_session_id, promise.Pass());
}

scoped_ptr<media::MediaKeys> ProxyDecryptor::CreateMediaKeys(
    const std::string& key_system,
    const GURL& security_origin) {
  return ContentDecryptionModuleFactory::Create(
      key_system,
      security_origin,
#if defined(ENABLE_PEPPER_CDMS)
      create_pepper_cdm_cb_,
#elif defined(ENABLE_BROWSER_CDMS)
      manager_,
      &cdm_id_,
#endif  // defined(ENABLE_PEPPER_CDMS)
      base::Bind(&ProxyDecryptor::OnSessionMessage,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&ProxyDecryptor::OnSessionReady,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&ProxyDecryptor::OnSessionClosed,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&ProxyDecryptor::OnSessionError,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ProxyDecryptor::OnSessionMessage(const std::string& web_session_id,
                                      const std::vector<uint8>& message,
                                      const GURL& destination_url) {
  // Assumes that OnSessionCreated() has been called before this.
  key_message_cb_.Run(web_session_id, message, destination_url);
}

void ProxyDecryptor::OnSessionReady(const std::string& web_session_id) {
  key_added_cb_.Run(web_session_id);
}

void ProxyDecryptor::OnSessionClosed(const std::string& web_session_id) {
  base::hash_map<std::string, bool>::iterator it =
      active_sessions_.find(web_session_id);

  // Latest EME spec separates closing a session ("allows an application to
  // indicate that it no longer needs the session") and actually closing the
  // session (done by the CDM at any point "such as in response to a close()
  // call, when the session is no longer needed, or when system resources are
  // lost.") Thus the CDM may cause 2 close() events -- one to resolve the
  // close() promise, and a second to actually close the session. Prefixed EME
  // only expects 1 close event, so drop the second (and subsequent) events.
  // However, this means we can't tell if the CDM is generating spurious close()
  // events.
  if (it == active_sessions_.end())
    return;

  if (it->second) {
    OnSessionError(web_session_id,
                   media::MediaKeys::NOT_SUPPORTED_ERROR,
                   kSessionClosedSystemCode,
                   "Do not close persistent sessions.");
  }
  active_sessions_.erase(it);
}

void ProxyDecryptor::OnSessionError(const std::string& web_session_id,
                                    media::MediaKeys::Exception exception_code,
                                    uint32 system_code,
                                    const std::string& error_message) {
  // Convert |error_name| back to MediaKeys::KeyError if possible. Prefixed
  // EME has different error message, so all the specific error events will
  // get lost.
  media::MediaKeys::KeyError error_code;
  switch (exception_code) {
    case media::MediaKeys::CLIENT_ERROR:
      error_code = media::MediaKeys::kClientError;
      break;
    case media::MediaKeys::OUTPUT_ERROR:
      error_code = media::MediaKeys::kOutputError;
      break;
    default:
      // This will include all other CDM4 errors and any error generated
      // by CDM5 or later.
      error_code = media::MediaKeys::kUnknownError;
      break;
  }
  key_error_cb_.Run(web_session_id, error_code, system_code);
}

void ProxyDecryptor::SetSessionId(bool persistent,
                                  const std::string& web_session_id) {
  active_sessions_.insert(std::make_pair(web_session_id, persistent));
}

}  // namespace content
