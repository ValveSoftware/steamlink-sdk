// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/crypto/content_decryption_module_factory.h"

#include "base/logging.h"
#include "content/renderer/media/crypto/key_systems.h"
#include "media/cdm/aes_decryptor.h"
#include "url/gurl.h"

#if defined(ENABLE_PEPPER_CDMS)
#include "content/renderer/media/crypto/ppapi_decryptor.h"
#elif defined(ENABLE_BROWSER_CDMS)
#include "content/renderer/media/crypto/proxy_media_keys.h"
#include "content/renderer/media/crypto/renderer_cdm_manager.h"
#endif  // defined(ENABLE_PEPPER_CDMS)

namespace content {

scoped_ptr<media::MediaKeys> ContentDecryptionModuleFactory::Create(
    const std::string& key_system,
    const GURL& security_origin,
#if defined(ENABLE_PEPPER_CDMS)
    const CreatePepperCdmCB& create_pepper_cdm_cb,
#elif defined(ENABLE_BROWSER_CDMS)
    RendererCdmManager* manager,
    int* cdm_id,
#endif  // defined(ENABLE_PEPPER_CDMS)
    const media::SessionMessageCB& session_message_cb,
    const media::SessionReadyCB& session_ready_cb,
    const media::SessionClosedCB& session_closed_cb,
    const media::SessionErrorCB& session_error_cb) {
  // TODO(jrummell): Pass |security_origin| to all constructors.
  // TODO(jrummell): Enable the following line once blink code updated to
  // check the security origin before calling.
  // DCHECK(security_origin.is_valid());

#if defined(ENABLE_BROWSER_CDMS)
  *cdm_id = RendererCdmManager::kInvalidCdmId;
#endif

  if (CanUseAesDecryptor(key_system)) {
    return scoped_ptr<media::MediaKeys>(
        new media::AesDecryptor(session_message_cb, session_closed_cb));
  }
#if defined(ENABLE_PEPPER_CDMS)
  return scoped_ptr<media::MediaKeys>(
      PpapiDecryptor::Create(key_system,
                             security_origin,
                             create_pepper_cdm_cb,
                             session_message_cb,
                             session_ready_cb,
                             session_closed_cb,
                             session_error_cb));
#elif defined(ENABLE_BROWSER_CDMS)
  scoped_ptr<ProxyMediaKeys> proxy_media_keys =
      ProxyMediaKeys::Create(key_system,
                             security_origin,
                             manager,
                             session_message_cb,
                             session_ready_cb,
                             session_closed_cb,
                             session_error_cb);
  if (proxy_media_keys)
    *cdm_id = proxy_media_keys->GetCdmId();
  return proxy_media_keys.PassAs<media::MediaKeys>();
#else
  return scoped_ptr<media::MediaKeys>();
#endif  // defined(ENABLE_PEPPER_CDMS)
}

}  // namespace content
