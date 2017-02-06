// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cdm/cast_cdm_context.h"

#include "base/logging.h"
#include "chromecast/media/base/decrypt_context_impl.h"
#include "chromecast/media/cdm/cast_cdm.h"
#include "chromecast/public/media/decrypt_context.h"

namespace chromecast {
namespace media {

CastCdmContext::CastCdmContext(CastCdm* cast_cdm) : cast_cdm_(cast_cdm) {
  DCHECK(cast_cdm_);
}

CastCdmContext::~CastCdmContext() {}

::media::Decryptor* CastCdmContext::GetDecryptor() {
  // Subclasses providing CdmContext for a ClearKey CDM implementation must
  // override this method to provide the Decryptor. Subclasses providing DRM
  // implementations should return nullptr here.
  return nullptr;
}

int CastCdmContext::GetCdmId() const {
  // This is a local CDM module.
  return ::media::CdmContext::kInvalidCdmId;
}

int CastCdmContext::RegisterPlayer(const base::Closure& new_key_cb,
                                   const base::Closure& cdm_unset_cb) {
  return cast_cdm_->RegisterPlayer(new_key_cb, cdm_unset_cb);
}

void CastCdmContext::UnregisterPlayer(int registration_id) {
  cast_cdm_->UnregisterPlayer(registration_id);
}

std::unique_ptr<DecryptContextImpl> CastCdmContext::GetDecryptContext(
    const std::string& key_id) {
  return cast_cdm_->GetDecryptContext(key_id);
}

void CastCdmContext::SetKeyStatus(const std::string& key_id,
                                  CastKeyStatus key_status,
                                  uint32_t system_code) {
  cast_cdm_->SetKeyStatus(key_id, key_status, system_code);
}

}  // namespace media
}  // namespace chromecast
