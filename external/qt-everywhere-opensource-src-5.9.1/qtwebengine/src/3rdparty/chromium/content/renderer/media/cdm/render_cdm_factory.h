// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CDM_RENDER_CDM_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_CDM_RENDER_CDM_FACTORY_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "media/base/cdm_factory.h"
#include "media/base/media_keys.h"
#include "ppapi/features/features.h"

#if BUILDFLAG(ENABLE_PEPPER_CDMS)
#include "content/renderer/media/cdm/pepper_cdm_wrapper.h"
#endif

class GURL;

namespace media {
struct CdmConfig;
}  // namespace media

namespace content {

// CdmFactory implementation in content/renderer. This class is not thread safe
// and should only be used on one thread.
class RenderCdmFactory : public media::CdmFactory {
 public:
#if BUILDFLAG(ENABLE_PEPPER_CDMS)
  explicit RenderCdmFactory(const CreatePepperCdmCB& create_pepper_cdm_cb);
#else
  RenderCdmFactory();
#endif  // BUILDFLAG(ENABLE_PEPPER_CDMS)

  ~RenderCdmFactory() override;

  // CdmFactory implementation.
  void Create(
      const std::string& key_system,
      const GURL& security_origin,
      const media::CdmConfig& cdm_config,
      const media::SessionMessageCB& session_message_cb,
      const media::SessionClosedCB& session_closed_cb,
      const media::SessionKeysChangeCB& session_keys_change_cb,
      const media::SessionExpirationUpdateCB& session_expiration_update_cb,
      const media::CdmCreatedCB& cdm_created_cb) override;

 private:
#if BUILDFLAG(ENABLE_PEPPER_CDMS)
  CreatePepperCdmCB create_pepper_cdm_cb_;
#endif

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(RenderCdmFactory);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CDM_RENDER_CDM_FACTORY_H_
