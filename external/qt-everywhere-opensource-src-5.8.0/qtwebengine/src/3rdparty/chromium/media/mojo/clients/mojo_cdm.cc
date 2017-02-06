// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_cdm.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/cdm_context.h"
#include "media/base/cdm_key_information.h"
#include "media/base/cdm_promise.h"
#include "media/mojo/clients/mojo_decryptor.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/interfaces/decryptor.mojom.h"
#include "services/shell/public/cpp/connect.h"
#include "services/shell/public/interfaces/interface_provider.mojom.h"
#include "url/gurl.h"

namespace media {

template <typename PromiseType>
static void RejectPromise(std::unique_ptr<PromiseType> promise,
                          mojom::CdmPromiseResultPtr result) {
  promise->reject(static_cast<MediaKeys::Exception>(result->exception),
                  result->system_code, result->error_message);
}

// static
void MojoCdm::Create(
    const std::string& key_system,
    const GURL& security_origin,
    const media::CdmConfig& cdm_config,
    mojom::ContentDecryptionModulePtr remote_cdm,
    const media::SessionMessageCB& session_message_cb,
    const media::SessionClosedCB& session_closed_cb,
    const media::LegacySessionErrorCB& legacy_session_error_cb,
    const media::SessionKeysChangeCB& session_keys_change_cb,
    const media::SessionExpirationUpdateCB& session_expiration_update_cb,
    const media::CdmCreatedCB& cdm_created_cb) {
  scoped_refptr<MojoCdm> mojo_cdm(
      new MojoCdm(std::move(remote_cdm), session_message_cb, session_closed_cb,
                  legacy_session_error_cb, session_keys_change_cb,
                  session_expiration_update_cb));

  // |mojo_cdm| ownership is passed to the promise.
  std::unique_ptr<CdmInitializedPromise> promise(
      new CdmInitializedPromise(cdm_created_cb, mojo_cdm));

  mojo_cdm->InitializeCdm(key_system, security_origin, cdm_config,
                          std::move(promise));
}

MojoCdm::MojoCdm(mojom::ContentDecryptionModulePtr remote_cdm,
                 const SessionMessageCB& session_message_cb,
                 const SessionClosedCB& session_closed_cb,
                 const LegacySessionErrorCB& legacy_session_error_cb,
                 const SessionKeysChangeCB& session_keys_change_cb,
                 const SessionExpirationUpdateCB& session_expiration_update_cb)
    : remote_cdm_(std::move(remote_cdm)),
      binding_(this),
      cdm_id_(CdmContext::kInvalidCdmId),
      session_message_cb_(session_message_cb),
      session_closed_cb_(session_closed_cb),
      legacy_session_error_cb_(legacy_session_error_cb),
      session_keys_change_cb_(session_keys_change_cb),
      session_expiration_update_cb_(session_expiration_update_cb),
      weak_factory_(this) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(!session_message_cb_.is_null());
  DCHECK(!session_closed_cb_.is_null());
  DCHECK(!legacy_session_error_cb_.is_null());
  DCHECK(!session_keys_change_cb_.is_null());
  DCHECK(!session_expiration_update_cb_.is_null());

  remote_cdm_->SetClient(binding_.CreateInterfacePtrAndBind());
}

MojoCdm::~MojoCdm() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  base::AutoLock auto_lock(lock_);

  // Release |decryptor_| on the correct thread. If GetDecryptor() is never
  // called but |decryptor_ptr_| is not null, it is not bound to any thread and
  // is safe to be released on the current thread.
  if (decryptor_task_runner_ &&
      !decryptor_task_runner_->BelongsToCurrentThread() && decryptor_) {
    decryptor_task_runner_->DeleteSoon(FROM_HERE, decryptor_.release());
  }
}

// Using base::Unretained(this) below is safe because |this| owns |remote_cdm_|,
// and if |this| is destroyed, |remote_cdm_| will be destroyed as well. Then the
// error handler can't be invoked and callbacks won't be dispatched.

void MojoCdm::InitializeCdm(const std::string& key_system,
                            const GURL& security_origin,
                            const media::CdmConfig& cdm_config,
                            std::unique_ptr<CdmInitializedPromise> promise) {
  DVLOG(1) << __FUNCTION__ << ": " << key_system;
  DCHECK(thread_checker_.CalledOnValidThread());

  // If connection error has happened, fail immediately.
  if (remote_cdm_.encountered_error()) {
    LOG(ERROR) << "Remote CDM encountered error.";
    promise->reject(NOT_SUPPORTED_ERROR, 0, "Mojo CDM creation failed.");
    return;
  }

  // Otherwise, set an error handler to catch the connection error.
  remote_cdm_.set_connection_error_handler(
      base::Bind(&MojoCdm::OnConnectionError, base::Unretained(this)));

  pending_init_promise_ = std::move(promise);

  remote_cdm_->Initialize(
      key_system, security_origin.spec(), mojom::CdmConfig::From(cdm_config),
      base::Bind(&MojoCdm::OnCdmInitialized, base::Unretained(this)));
}

void MojoCdm::OnConnectionError() {
  LOG(ERROR) << "Remote CDM connection error.";
  DCHECK(thread_checker_.CalledOnValidThread());

  // We only handle initial connection error.
  if (!pending_init_promise_)
    return;

  pending_init_promise_->reject(NOT_SUPPORTED_ERROR, 0,
                                "Mojo CDM creation failed.");
  pending_init_promise_.reset();
}

void MojoCdm::SetServerCertificate(const std::vector<uint8_t>& certificate,
                                   std::unique_ptr<SimpleCdmPromise> promise) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  remote_cdm_->SetServerCertificate(
      mojo::Array<uint8_t>::From(certificate),
      base::Bind(&MojoCdm::OnPromiseResult<>, base::Unretained(this),
                 base::Passed(&promise)));
}

void MojoCdm::CreateSessionAndGenerateRequest(
    SessionType session_type,
    EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data,
    std::unique_ptr<NewSessionCdmPromise> promise) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  remote_cdm_->CreateSessionAndGenerateRequest(
      static_cast<mojom::ContentDecryptionModule::SessionType>(session_type),
      static_cast<mojom::ContentDecryptionModule::InitDataType>(init_data_type),
      mojo::Array<uint8_t>::From(init_data),
      base::Bind(&MojoCdm::OnPromiseResult<std::string>, base::Unretained(this),
                 base::Passed(&promise)));
}

void MojoCdm::LoadSession(SessionType session_type,
                          const std::string& session_id,
                          std::unique_ptr<NewSessionCdmPromise> promise) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  remote_cdm_->LoadSession(
      static_cast<mojom::ContentDecryptionModule::SessionType>(session_type),
      session_id, base::Bind(&MojoCdm::OnPromiseResult<std::string>,
                             base::Unretained(this), base::Passed(&promise)));
}

void MojoCdm::UpdateSession(const std::string& session_id,
                            const std::vector<uint8_t>& response,
                            std::unique_ptr<SimpleCdmPromise> promise) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  remote_cdm_->UpdateSession(
      session_id, mojo::Array<uint8_t>::From(response),
      base::Bind(&MojoCdm::OnPromiseResult<>, base::Unretained(this),
                 base::Passed(&promise)));
}

void MojoCdm::CloseSession(const std::string& session_id,
                           std::unique_ptr<SimpleCdmPromise> promise) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  remote_cdm_->CloseSession(
      session_id, base::Bind(&MojoCdm::OnPromiseResult<>,
                             base::Unretained(this), base::Passed(&promise)));
}

void MojoCdm::RemoveSession(const std::string& session_id,
                            std::unique_ptr<SimpleCdmPromise> promise) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  remote_cdm_->RemoveSession(
      session_id, base::Bind(&MojoCdm::OnPromiseResult<>,
                             base::Unretained(this), base::Passed(&promise)));
}

CdmContext* MojoCdm::GetCdmContext() {
  DVLOG(2) << __FUNCTION__;
  return this;
}

media::Decryptor* MojoCdm::GetDecryptor() {
  base::AutoLock auto_lock(lock_);

  if (!decryptor_task_runner_)
    decryptor_task_runner_ = base::ThreadTaskRunnerHandle::Get();

  DCHECK(decryptor_task_runner_->BelongsToCurrentThread());

  // Can be called on a different thread.
  if (decryptor_ptr_) {
    DCHECK(!decryptor_);
    decryptor_.reset(new MojoDecryptor(std::move(decryptor_ptr_)));
  }

  return decryptor_.get();
}

int MojoCdm::GetCdmId() const {
  base::AutoLock auto_lock(lock_);
  // Can be called on a different thread.
  DCHECK_NE(CdmContext::kInvalidCdmId, cdm_id_);
  return cdm_id_;
}

void MojoCdm::OnSessionMessage(const mojo::String& session_id,
                               mojom::CdmMessageType message_type,
                               mojo::Array<uint8_t> message,
                               const GURL& legacy_destination_url) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  session_message_cb_.Run(session_id,
                          static_cast<MediaKeys::MessageType>(message_type),
                          message.storage(), legacy_destination_url);
}

void MojoCdm::OnSessionClosed(const mojo::String& session_id) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  session_closed_cb_.Run(session_id);
}

void MojoCdm::OnLegacySessionError(const mojo::String& session_id,
                                   mojom::CdmException exception,
                                   uint32_t system_code,
                                   const mojo::String& error_message) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  legacy_session_error_cb_.Run(session_id,
                               static_cast<MediaKeys::Exception>(exception),
                               system_code, error_message);
}

void MojoCdm::OnSessionKeysChange(
    const mojo::String& session_id,
    bool has_additional_usable_key,
    mojo::Array<mojom::CdmKeyInformationPtr> keys_info) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  // TODO(jrummell): Handling resume playback should be done in the media
  // player, not in the Decryptors. http://crbug.com/413413.
  if (has_additional_usable_key) {
    base::AutoLock auto_lock(lock_);
    if (decryptor_) {
      DCHECK(decryptor_task_runner_);
      decryptor_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&MojoCdm::OnKeyAdded, weak_factory_.GetWeakPtr()));
    }
  }

  media::CdmKeysInfo key_data;
  key_data.reserve(keys_info.size());
  for (size_t i = 0; i < keys_info.size(); ++i) {
    key_data.push_back(
        keys_info[i].To<std::unique_ptr<media::CdmKeyInformation>>().release());
  }
  session_keys_change_cb_.Run(session_id, has_additional_usable_key,
                              std::move(key_data));
}

void MojoCdm::OnSessionExpirationUpdate(const mojo::String& session_id,
                                        double new_expiry_time_sec) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  session_expiration_update_cb_.Run(
      session_id, base::Time::FromDoubleT(new_expiry_time_sec));
}

void MojoCdm::OnCdmInitialized(mojom::CdmPromiseResultPtr result,
                               int cdm_id,
                               mojom::DecryptorPtr decryptor) {
  DVLOG(2) << __FUNCTION__ << " cdm_id: " << cdm_id;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(pending_init_promise_);

  if (!result->success) {
    RejectPromise(std::move(pending_init_promise_), std::move(result));
    return;
  }

  {
    base::AutoLock auto_lock(lock_);
    DCHECK_NE(CdmContext::kInvalidCdmId, cdm_id);
    cdm_id_ = cdm_id;
    decryptor_ptr_ = std::move(decryptor);
  }

  pending_init_promise_->resolve();
  pending_init_promise_.reset();
}

void MojoCdm::OnKeyAdded() {
  base::AutoLock auto_lock(lock_);

  DCHECK(decryptor_task_runner_);
  DCHECK(decryptor_task_runner_->BelongsToCurrentThread());
  DCHECK(decryptor_);

  decryptor_->OnKeyAdded();
}

}  // namespace media
