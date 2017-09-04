// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_CDM_SERVICE_H_
#define MEDIA_MOJO_SERVICES_MOJO_CDM_SERVICE_H_

#include <stdint.h>

#include <memory>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "media/base/eme_constants.h"
#include "media/base/media_keys.h"
#include "media/mojo/interfaces/content_decryption_module.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "media/mojo/services/mojo_cdm_promise.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "media/mojo/services/mojo_decryptor_service.h"

namespace media {

class CdmFactory;

// A mojom::ContentDecryptionModule implementation backed by a
// media::MediaKeys.
class MEDIA_MOJO_EXPORT MojoCdmService
    : NON_EXPORTED_BASE(public mojom::ContentDecryptionModule) {
 public:
  using SessionType = MediaKeys::SessionType;

  // Get the CDM associated with |cdm_id|, which is unique per process.
  // Can be called on any thread. The returned CDM is not guaranteed to be
  // thread safe.
  // Note: This provides a generic hack to get the CDM in the process where
  // MediaService is running, regardless of which render process or
  // render frame the caller is associated with. In the future, we should move
  // all out-of-process media players into the MediaService so that we can use
  // MojoCdmServiceContext (per render frame) to get the CDM.
  static scoped_refptr<MediaKeys> LegacyGetCdm(int cdm_id);

  // Constructs a MojoCdmService and strongly binds it to the |request|.
  MojoCdmService(base::WeakPtr<MojoCdmServiceContext> context,
                 CdmFactory* cdm_factory);

  ~MojoCdmService() final;

  // mojom::ContentDecryptionModule implementation.
  void SetClient(mojom::ContentDecryptionModuleClientPtr client) final;
  void Initialize(const std::string& key_system,
                  const std::string& security_origin,
                  mojom::CdmConfigPtr cdm_config,
                  const InitializeCallback& callback) final;
  void SetServerCertificate(const std::vector<uint8_t>& certificate_data,
                            const SetServerCertificateCallback& callback) final;
  void CreateSessionAndGenerateRequest(
      SessionType session_type,
      EmeInitDataType init_data_type,
      const std::vector<uint8_t>& init_data,
      const CreateSessionAndGenerateRequestCallback& callback) final;
  void LoadSession(SessionType session_type,
                   const std::string& session_id,
                   const LoadSessionCallback& callback) final;
  void UpdateSession(const std::string& session_id,
                     const std::vector<uint8_t>& response,
                     const UpdateSessionCallback& callback) final;
  void CloseSession(const std::string& session_id,
                    const CloseSessionCallback& callback) final;
  void RemoveSession(const std::string& session_id,
                     const RemoveSessionCallback& callback) final;

  // Get CDM to be used by the media pipeline.
  scoped_refptr<MediaKeys> GetCdm();

 private:
  // Callback for CdmFactory::Create().
  void OnCdmCreated(const InitializeCallback& callback,
                    const scoped_refptr<MediaKeys>& cdm,
                    const std::string& error_message);

  // Callbacks for firing session events.
  void OnSessionMessage(const std::string& session_id,
                        MediaKeys::MessageType message_type,
                        const std::vector<uint8_t>& message);
  void OnSessionKeysChange(const std::string& session_id,
                           bool has_additional_usable_key,
                           CdmKeysInfo keys_info);
  void OnSessionExpirationUpdate(const std::string& session_id,
                                 const base::Time& new_expiry_time);
  void OnSessionClosed(const std::string& session_id);

  // Callback for when |decryptor_| loses connectivity.
  void OnDecryptorConnectionError();

  // CDM ID to be assigned to the next successfully initialized CDM. This ID is
  // unique per process. It will be used to locate the CDM by the media players
  // living in the same process.
  static int next_cdm_id_;

  base::WeakPtr<MojoCdmServiceContext> context_;

  CdmFactory* cdm_factory_;
  scoped_refptr<MediaKeys> cdm_;

  std::unique_ptr<MojoDecryptorService> decryptor_;

  // Set to a valid CDM ID if the |cdm_| is successfully created.
  int cdm_id_;

  mojom::ContentDecryptionModuleClientPtr client_;

  base::WeakPtr<MojoCdmService> weak_this_;
  base::WeakPtrFactory<MojoCdmService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MojoCdmService);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_CDM_SERVICE_H_
