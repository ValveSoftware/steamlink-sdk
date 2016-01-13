// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_KEYS_H_
#define MEDIA_BASE_MEDIA_KEYS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"
#include "url/gurl.h"

namespace media {

class Decryptor;

template <typename T>
class CdmPromiseTemplate;

typedef CdmPromiseTemplate<std::string> NewSessionCdmPromise;
typedef CdmPromiseTemplate<void> SimpleCdmPromise;

// Performs media key operations.
//
// All key operations are called on the renderer thread. Therefore, these calls
// should be fast and nonblocking; key events should be fired asynchronously.
class MEDIA_EXPORT MediaKeys {
 public:
  // Reported to UMA, so never reuse a value!
  // Must be kept in sync with blink::WebMediaPlayerClient::MediaKeyErrorCode
  // (enforced in webmediaplayer_impl.cc).
  // TODO(jrummell): Can this be moved to proxy_decryptor as it should only be
  // used by the prefixed EME code?
  enum KeyError {
    kUnknownError = 1,
    kClientError,
    // The commented v0.1b values below have never been used.
    // kServiceError,
    kOutputError = 4,
    // kHardwareChangeError,
    // kDomainError,
    kMaxKeyError  // Must be last and greater than any legit value.
  };

  // Must be a superset of cdm::MediaKeyException.
  enum Exception {
    NOT_SUPPORTED_ERROR,
    INVALID_STATE_ERROR,
    INVALID_ACCESS_ERROR,
    QUOTA_EXCEEDED_ERROR,
    UNKNOWN_ERROR,
    CLIENT_ERROR,
    OUTPUT_ERROR
  };

  // Type of license required when creating/loading a session.
  // Must be consistent with the values specified in the spec:
  // https://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#extensions
  enum SessionType {
    TEMPORARY_SESSION,
    PERSISTENT_SESSION
  };

  const static uint32 kInvalidSessionId = 0;

  MediaKeys();
  virtual ~MediaKeys();

  // Creates a session with the |init_data_type|, |init_data| and |session_type|
  // provided.
  // Note: UpdateSession() and ReleaseSession() should only be called after
  // |promise| is resolved.
  virtual void CreateSession(const std::string& init_data_type,
                             const uint8* init_data,
                             int init_data_length,
                             SessionType session_type,
                             scoped_ptr<NewSessionCdmPromise> promise) = 0;

  // Loads a session with the |web_session_id| provided.
  // Note: UpdateSession() and ReleaseSession() should only be called after
  // |promise| is resolved.
  virtual void LoadSession(const std::string& web_session_id,
                           scoped_ptr<NewSessionCdmPromise> promise) = 0;

  // Updates a session specified by |web_session_id| with |response|.
  virtual void UpdateSession(const std::string& web_session_id,
                             const uint8* response,
                             int response_length,
                             scoped_ptr<SimpleCdmPromise> promise) = 0;

  // Releases the session specified by |web_session_id|.
  virtual void ReleaseSession(const std::string& web_session_id,
                              scoped_ptr<SimpleCdmPromise> promise) = 0;

  // Gets the Decryptor object associated with the MediaKeys. Returns NULL if
  // no Decryptor object is associated. The returned object is only guaranteed
  // to be valid during the MediaKeys' lifetime.
  virtual Decryptor* GetDecryptor();

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaKeys);
};

// Key event callbacks. See the spec for details:
// https://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#event-summary
typedef base::Callback<void(const std::string& web_session_id,
                            const std::vector<uint8>& message,
                            const GURL& destination_url)> SessionMessageCB;

typedef base::Callback<void(const std::string& web_session_id)> SessionReadyCB;

typedef base::Callback<void(const std::string& web_session_id)> SessionClosedCB;

typedef base::Callback<void(const std::string& web_session_id,
                            MediaKeys::Exception exception_code,
                            uint32 system_code,
                            const std::string& error_message)> SessionErrorCB;

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_KEYS_H_
