// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_cdm_service.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/lock.h"
#include "media/base/cdm_config.h"
#include "media/base/cdm_context.h"
#include "media/base/cdm_factory.h"
#include "media/base/cdm_key_information.h"
#include "media/base/key_systems.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "mojo/common/common_type_converters.h"
#include "url/gurl.h"

namespace media {

namespace {

// Manages all CDMs created by MojoCdmService. Can only have one instance per
// process so use a LazyInstance to ensure this.
class CdmManager {
 public:
  CdmManager() {}
  ~CdmManager() {}

  // Returns the CDM associated with |cdm_id|. Can be called on any thread.
  scoped_refptr<MediaKeys> GetCdm(int cdm_id) {
    base::AutoLock lock(lock_);
    auto iter = cdm_map_.find(cdm_id);
    return iter == cdm_map_.end() ? nullptr : iter->second;
  }

  // Registers the |cdm| for |cdm_id|.
  void RegisterCdm(int cdm_id, const scoped_refptr<MediaKeys>& cdm) {
    base::AutoLock lock(lock_);
    DCHECK(!cdm_map_.count(cdm_id));
    cdm_map_[cdm_id] = cdm;
  }

  // Unregisters the CDM associated with |cdm_id|.
  void UnregisterCdm(int cdm_id) {
    base::AutoLock lock(lock_);
    DCHECK(cdm_map_.count(cdm_id));
    cdm_map_.erase(cdm_id);
  }

 private:
  // Lock to protect |cdm_map_|.
  base::Lock lock_;
  std::map<int, scoped_refptr<MediaKeys>> cdm_map_;

  DISALLOW_COPY_AND_ASSIGN(CdmManager);
};

base::LazyInstance<CdmManager>::Leaky g_cdm_manager = LAZY_INSTANCE_INITIALIZER;

}  // namespace

using SimpleMojoCdmPromise = MojoCdmPromise<>;
using NewSessionMojoCdmPromise = MojoCdmPromise<std::string>;

int MojoCdmService::next_cdm_id_ = CdmContext::kInvalidCdmId + 1;

// static
scoped_refptr<MediaKeys> MojoCdmService::LegacyGetCdm(int cdm_id) {
  DVLOG(1) << __FUNCTION__ << ": " << cdm_id;
  return g_cdm_manager.Get().GetCdm(cdm_id);
}

MojoCdmService::MojoCdmService(
    base::WeakPtr<MojoCdmServiceContext> context,
    CdmFactory* cdm_factory,
    mojo::InterfaceRequest<mojom::ContentDecryptionModule> request)
    : binding_(this, std::move(request)),
      context_(context),
      cdm_factory_(cdm_factory),
      cdm_id_(CdmContext::kInvalidCdmId),
      weak_factory_(this) {
  DCHECK(context_);
  DCHECK(cdm_factory_);
}

MojoCdmService::~MojoCdmService() {
  if (cdm_id_ == CdmContext::kInvalidCdmId)
    return;

  g_cdm_manager.Get().UnregisterCdm(cdm_id_);

  if (context_)
    context_->UnregisterCdm(cdm_id_);
}

void MojoCdmService::SetClient(mojom::ContentDecryptionModuleClientPtr client) {
  client_ = std::move(client);
}

void MojoCdmService::Initialize(const mojo::String& key_system,
                                const mojo::String& security_origin,
                                mojom::CdmConfigPtr cdm_config,
                                const InitializeCallback& callback) {
  DVLOG(1) << __FUNCTION__ << ": " << key_system;
  DCHECK(!cdm_);

  auto weak_this = weak_factory_.GetWeakPtr();
  cdm_factory_->Create(
      key_system, GURL(security_origin.get()), cdm_config.To<CdmConfig>(),
      base::Bind(&MojoCdmService::OnSessionMessage, weak_this),
      base::Bind(&MojoCdmService::OnSessionClosed, weak_this),
      base::Bind(&MojoCdmService::OnLegacySessionError, weak_this),
      base::Bind(&MojoCdmService::OnSessionKeysChange, weak_this),
      base::Bind(&MojoCdmService::OnSessionExpirationUpdate, weak_this),
      base::Bind(&MojoCdmService::OnCdmCreated, weak_this, callback));
}

void MojoCdmService::SetServerCertificate(
    mojo::Array<uint8_t> certificate_data,
    const SetServerCertificateCallback& callback) {
  DVLOG(2) << __FUNCTION__;
  cdm_->SetServerCertificate(
      certificate_data.storage(),
      base::WrapUnique(new SimpleMojoCdmPromise(callback)));
}

void MojoCdmService::CreateSessionAndGenerateRequest(
    mojom::ContentDecryptionModule::SessionType session_type,
    mojom::ContentDecryptionModule::InitDataType init_data_type,
    mojo::Array<uint8_t> init_data,
    const CreateSessionAndGenerateRequestCallback& callback) {
  DVLOG(2) << __FUNCTION__;
  cdm_->CreateSessionAndGenerateRequest(
      static_cast<MediaKeys::SessionType>(session_type),
      static_cast<EmeInitDataType>(init_data_type), init_data.storage(),
      base::WrapUnique(new NewSessionMojoCdmPromise(callback)));
}

void MojoCdmService::LoadSession(
    mojom::ContentDecryptionModule::SessionType session_type,
    const mojo::String& session_id,
    const LoadSessionCallback& callback) {
  DVLOG(2) << __FUNCTION__;
  cdm_->LoadSession(static_cast<MediaKeys::SessionType>(session_type),
                    session_id.To<std::string>(),
                    base::WrapUnique(new NewSessionMojoCdmPromise(callback)));
}

void MojoCdmService::UpdateSession(const mojo::String& session_id,
                                   mojo::Array<uint8_t> response,
                                   const UpdateSessionCallback& callback) {
  DVLOG(2) << __FUNCTION__;
  cdm_->UpdateSession(
      session_id.To<std::string>(), response.storage(),
      std::unique_ptr<SimpleCdmPromise>(new SimpleMojoCdmPromise(callback)));
}

void MojoCdmService::CloseSession(const mojo::String& session_id,
                                  const CloseSessionCallback& callback) {
  DVLOG(2) << __FUNCTION__;
  cdm_->CloseSession(session_id.To<std::string>(),
                     base::WrapUnique(new SimpleMojoCdmPromise(callback)));
}

void MojoCdmService::RemoveSession(const mojo::String& session_id,
                                   const RemoveSessionCallback& callback) {
  DVLOG(2) << __FUNCTION__;
  cdm_->RemoveSession(session_id.To<std::string>(),
                      base::WrapUnique(new SimpleMojoCdmPromise(callback)));
}

scoped_refptr<MediaKeys> MojoCdmService::GetCdm() {
  return cdm_;
}

void MojoCdmService::OnCdmCreated(const InitializeCallback& callback,
                                  const scoped_refptr<MediaKeys>& cdm,
                                  const std::string& error_message) {
  mojom::CdmPromiseResultPtr cdm_promise_result(mojom::CdmPromiseResult::New());

  // TODO(xhwang): This should not happen when KeySystemInfo is properly
  // populated. See http://crbug.com/469366
  if (!cdm || !context_) {
    cdm_promise_result->success = false;
    cdm_promise_result->exception = mojom::CdmException::NOT_SUPPORTED_ERROR;
    cdm_promise_result->system_code = 0;
    cdm_promise_result->error_message = error_message;
    callback.Run(std::move(cdm_promise_result), 0, nullptr);
    return;
  }

  cdm_ = cdm;
  cdm_id_ = next_cdm_id_++;

  context_->RegisterCdm(cdm_id_, this);
  g_cdm_manager.Get().RegisterCdm(cdm_id_, cdm);

  // If |cdm| has a decryptor, create the MojoDecryptorService
  // and pass the connection back to the client.
  mojom::DecryptorPtr decryptor_service;
  CdmContext* const cdm_context = cdm_->GetCdmContext();
  if (cdm_context && cdm_context->GetDecryptor()) {
    // MojoDecryptorService takes a reference to the CDM, but it is still owned
    // by |this|.
    decryptor_.reset(new MojoDecryptorService(
        cdm_, GetProxy(&decryptor_service),
        base::Bind(&MojoCdmService::OnDecryptorConnectionError, weak_this_)));
  }

  DVLOG(1) << __FUNCTION__ << ": CDM successfully created with ID " << cdm_id_;
  cdm_promise_result->success = true;
  callback.Run(std::move(cdm_promise_result), cdm_id_,
               std::move(decryptor_service));
}

void MojoCdmService::OnSessionMessage(const std::string& session_id,
                                      MediaKeys::MessageType message_type,
                                      const std::vector<uint8_t>& message,
                                      const GURL& legacy_destination_url) {
  DVLOG(2) << __FUNCTION__ << "(" << message_type << ")";
  client_->OnSessionMessage(
      session_id, static_cast<mojom::CdmMessageType>(message_type),
      mojo::Array<uint8_t>::From(message), legacy_destination_url);
}

void MojoCdmService::OnSessionKeysChange(const std::string& session_id,
                                         bool has_additional_usable_key,
                                         CdmKeysInfo keys_info) {
  DVLOG(2) << __FUNCTION__
           << " has_additional_usable_key=" << has_additional_usable_key;

  mojo::Array<mojom::CdmKeyInformationPtr> keys_data;
  for (auto* key : keys_info)
    keys_data.push_back(mojom::CdmKeyInformation::From(*key));
  client_->OnSessionKeysChange(session_id, has_additional_usable_key,
                               std::move(keys_data));
}

void MojoCdmService::OnSessionExpirationUpdate(
    const std::string& session_id,
    const base::Time& new_expiry_time_sec) {
  DVLOG(2) << __FUNCTION__ << " expiry=" << new_expiry_time_sec;
  client_->OnSessionExpirationUpdate(session_id,
                                     new_expiry_time_sec.ToDoubleT());
}

void MojoCdmService::OnSessionClosed(const std::string& session_id) {
  DVLOG(2) << __FUNCTION__;
  client_->OnSessionClosed(session_id);
}

void MojoCdmService::OnLegacySessionError(const std::string& session_id,
                                          MediaKeys::Exception exception,
                                          uint32_t system_code,
                                          const std::string& error_message) {
  DVLOG(2) << __FUNCTION__ << "(" << exception << ") " << error_message;
  client_->OnLegacySessionError(session_id,
                                static_cast<mojom::CdmException>(exception),
                                system_code, error_message);
}

void MojoCdmService::OnDecryptorConnectionError() {
  DVLOG(2) << __FUNCTION__;

  // MojoDecryptorService has lost connectivity to it's client, so it can be
  // freed.
  decryptor_.reset();
}

}  // namespace media
