// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/cdm/render_cdm_factory.h"

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/cdm_config.h"
#include "media/base/cdm_promise.h"
#include "media/base/key_systems.h"
#include "media/base/media_keys.h"
#include "media/cdm/aes_decryptor.h"
#include "ppapi/features/features.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_PEPPER_CDMS)
#include "content/renderer/media/cdm/ppapi_decryptor.h"
#endif  // BUILDFLAG(ENABLE_PEPPER_CDMS)

namespace content {

#if BUILDFLAG(ENABLE_PEPPER_CDMS)
RenderCdmFactory::RenderCdmFactory(
    const CreatePepperCdmCB& create_pepper_cdm_cb)
    : create_pepper_cdm_cb_(create_pepper_cdm_cb) {}
#else
RenderCdmFactory::RenderCdmFactory() {}
#endif  // BUILDFLAG(ENABLE_PEPPER_CDMS)

RenderCdmFactory::~RenderCdmFactory() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void RenderCdmFactory::Create(
    const std::string& key_system,
    const GURL& security_origin,
    const media::CdmConfig& cdm_config,
    const media::SessionMessageCB& session_message_cb,
    const media::SessionClosedCB& session_closed_cb,
    const media::SessionKeysChangeCB& session_keys_change_cb,
    const media::SessionExpirationUpdateCB& session_expiration_update_cb,
    const media::CdmCreatedCB& cdm_created_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!security_origin.is_valid()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(cdm_created_cb, nullptr, "Invalid origin."));
    return;
  }

  if (media::CanUseAesDecryptor(key_system)) {
    DCHECK(!cdm_config.allow_distinctive_identifier);
    DCHECK(!cdm_config.allow_persistent_state);
    scoped_refptr<media::MediaKeys> cdm(
        new media::AesDecryptor(security_origin, session_message_cb,
                                session_closed_cb, session_keys_change_cb));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(cdm_created_cb, cdm, ""));
    return;
  }

#if BUILDFLAG(ENABLE_PEPPER_CDMS)
  DCHECK(!cdm_config.use_hw_secure_codecs);
  PpapiDecryptor::Create(
      key_system, security_origin, cdm_config.allow_distinctive_identifier,
      cdm_config.allow_persistent_state, create_pepper_cdm_cb_,
      session_message_cb, session_closed_cb, session_keys_change_cb,
      session_expiration_update_cb, cdm_created_cb);
#else
  // No possible CDM to create, so fail the request.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(cdm_created_cb, nullptr, "Key system not supported."));
#endif  // BUILDFLAG(ENABLE_PEPPER_CDMS)
}

}  // namespace content
