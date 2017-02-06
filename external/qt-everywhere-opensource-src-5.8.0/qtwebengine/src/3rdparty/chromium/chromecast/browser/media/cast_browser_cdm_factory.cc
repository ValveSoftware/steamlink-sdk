// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/media/cast_browser_cdm_factory.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "chromecast/media/cdm/cast_cdm.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/cdm_config.h"
#include "media/base/cdm_key_information.h"

#if !defined(ENABLE_MOJO_MEDIA_IN_BROWSER_PROCESS)
#include "chromecast/media/cdm/cast_cdm_proxy.h"
#endif  // !defined(ENABLE_MOJO_MEDIA_IN_BROWSER_PROCESS)

namespace chromecast {
namespace media {

CastBrowserCdmFactory::CastBrowserCdmFactory(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    MediaResourceTracker* media_resource_tracker)
    : media_resource_tracker_(media_resource_tracker),
      task_runner_(task_runner) {
  DCHECK(media_resource_tracker_);
  DCHECK(task_runner_);
}

CastBrowserCdmFactory::~CastBrowserCdmFactory() {}

void CastBrowserCdmFactory::Create(
    const std::string& key_system,
    const GURL& security_origin,
    const ::media::CdmConfig& cdm_config,
    const ::media::SessionMessageCB& session_message_cb,
    const ::media::SessionClosedCB& session_closed_cb,
    const ::media::LegacySessionErrorCB& legacy_session_error_cb,
    const ::media::SessionKeysChangeCB& session_keys_change_cb,
    const ::media::SessionExpirationUpdateCB& session_expiration_update_cb,
    const ::media::CdmCreatedCB& cdm_created_cb) {
  // Bound |cdm_created_cb| so we always fire it asynchronously.
  ::media::CdmCreatedCB bound_cdm_created_cb =
      ::media::BindToCurrentLoop(cdm_created_cb);

  DCHECK(!cdm_config.use_hw_secure_codecs)
      << "Chromecast does not use |use_hw_secure_codecs|";

  CastKeySystem cast_key_system(GetKeySystemByName(key_system));

  scoped_refptr<chromecast::media::CastCdm> cast_cdm;
  if (cast_key_system == chromecast::media::KEY_SYSTEM_CLEAR_KEY) {
    // TODO(gunsch): handle ClearKey decryption. See crbug.com/441957
  } else {
    cast_cdm = CreatePlatformBrowserCdm(cast_key_system);
  }

  if (!cast_cdm) {
    LOG(INFO) << "No matching key system found: " << cast_key_system;
    bound_cdm_created_cb.Run(nullptr, "No matching key system found.");
    return;
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CastCdm::Initialize, base::Unretained(cast_cdm.get()),
                 ::media::BindToCurrentLoop(session_message_cb),
                 ::media::BindToCurrentLoop(session_closed_cb),
                 ::media::BindToCurrentLoop(legacy_session_error_cb),
                 ::media::BindToCurrentLoop(session_keys_change_cb),
                 ::media::BindToCurrentLoop(session_expiration_update_cb)));

// When using Mojo media, we do not need to proxy calls to the CMA thread. All
// calls are made on that thread already.
#if defined(ENABLE_MOJO_MEDIA_IN_BROWSER_PROCESS)
  bound_cdm_created_cb.Run(cast_cdm, "");
#else
  bound_cdm_created_cb.Run(new CastCdmProxy(cast_cdm, task_runner_), "");
#endif  // defined(ENABLE_MOJO_MEDIA_IN_BROWSER_PROCESS)
}

scoped_refptr<CastCdm> CastBrowserCdmFactory::CreatePlatformBrowserCdm(
    const CastKeySystem& cast_key_system) {
  return nullptr;
}

}  // namespace media
}  // namespace chromecast
