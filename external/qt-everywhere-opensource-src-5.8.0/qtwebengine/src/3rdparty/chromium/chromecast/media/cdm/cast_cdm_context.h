// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CDM_CAST_CDM_CONTEXT_H_
#define CHROMECAST_MEDIA_CDM_CAST_CDM_CONTEXT_H_

#include <memory>
#include <string>

#include "chromecast/public/media/cast_key_status.h"
#include "media/base/cdm_context.h"

namespace media {
class Decryptor;
}

namespace chromecast {
namespace media {

class CastCdm;
class DecryptContextImpl;

// This class exposes only what's needed by CastRenderer.
class CastCdmContext : public ::media::CdmContext {
 public:
  explicit CastCdmContext(CastCdm* cast_cdm);
  ~CastCdmContext() override;

  // ::media::CdmContext implementation.
  ::media::Decryptor* GetDecryptor() override;
  int GetCdmId() const override;

  // Register a player with this CDM. |new_key_cb| will be called when a new
  // key is available. |cdm_unset_cb| will be called when the CDM is destroyed.
  int RegisterPlayer(const base::Closure& new_key_cb,
                     const base::Closure& cdm_unset_cb);

  // Unregiester a player with this CDM. |registration_id| should be the id
  // returned by RegisterPlayer().
  void UnregisterPlayer(int registration_id);

  // Returns the decryption context needed to decrypt frames encrypted with
  // |key_id|. Returns null if |key_id| is not available.
  std::unique_ptr<DecryptContextImpl> GetDecryptContext(
      const std::string& key_id);

  // Notifies that key status has changed (e.g. if expiry is detected by
  // hardware decoder).
  void SetKeyStatus(const std::string& key_id,
                    CastKeyStatus key_status,
                    uint32_t system_code);

 private:
  // The CastCdm object which owns |this|.
  CastCdm* const cast_cdm_;

  DISALLOW_COPY_AND_ASSIGN(CastCdmContext);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CDM_CAST_CDM_CONTEXT_H_