// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/cdm/ppapi_decryptor.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "content/renderer/pepper/content_decryptor_delegate.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/cdm_initialized_promise.h"
#include "media/base/cdm_key_information.h"
#include "media/base/data_buffer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/key_systems.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"

namespace content {

void PpapiDecryptor::Create(
    const std::string& key_system,
    const GURL& security_origin,
    bool allow_distinctive_identifier,
    bool allow_persistent_state,
    const CreatePepperCdmCB& create_pepper_cdm_cb,
    const media::SessionMessageCB& session_message_cb,
    const media::SessionClosedCB& session_closed_cb,
    const media::LegacySessionErrorCB& legacy_session_error_cb,
    const media::SessionKeysChangeCB& session_keys_change_cb,
    const media::SessionExpirationUpdateCB& session_expiration_update_cb,
    const media::CdmCreatedCB& cdm_created_cb) {
  std::string plugin_type = media::GetPepperType(key_system);
  DCHECK(!plugin_type.empty());

  std::unique_ptr<PepperCdmWrapper> pepper_cdm_wrapper;
  {
    TRACE_EVENT0("media", "PpapiDecryptor::CreatePepperCDM");
    pepper_cdm_wrapper = create_pepper_cdm_cb.Run(plugin_type, security_origin);
  }

  if (!pepper_cdm_wrapper) {
    std::string message =
        "Unable to create the CDM for the key system " + key_system + ".";
    DLOG(ERROR) << message;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(cdm_created_cb, nullptr, message));
    return;
  }

  scoped_refptr<PpapiDecryptor> ppapi_decryptor(
      new PpapiDecryptor(std::move(pepper_cdm_wrapper), session_message_cb,
                         session_closed_cb, legacy_session_error_cb,
                         session_keys_change_cb, session_expiration_update_cb));

  // |ppapi_decryptor| ownership is passed to the promise.
  std::unique_ptr<media::CdmInitializedPromise> promise(
      new media::CdmInitializedPromise(cdm_created_cb, ppapi_decryptor));

  ppapi_decryptor->InitializeCdm(key_system, allow_distinctive_identifier,
                                 allow_persistent_state, std::move(promise));
}

PpapiDecryptor::PpapiDecryptor(
    std::unique_ptr<PepperCdmWrapper> pepper_cdm_wrapper,
    const media::SessionMessageCB& session_message_cb,
    const media::SessionClosedCB& session_closed_cb,
    const media::LegacySessionErrorCB& legacy_session_error_cb,
    const media::SessionKeysChangeCB& session_keys_change_cb,
    const media::SessionExpirationUpdateCB& session_expiration_update_cb)
    : pepper_cdm_wrapper_(std::move(pepper_cdm_wrapper)),
      session_message_cb_(session_message_cb),
      session_closed_cb_(session_closed_cb),
      legacy_session_error_cb_(legacy_session_error_cb),
      session_keys_change_cb_(session_keys_change_cb),
      session_expiration_update_cb_(session_expiration_update_cb),
      render_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_ptr_factory_(this) {
  DCHECK(pepper_cdm_wrapper_.get());
  DCHECK(!session_message_cb_.is_null());
  DCHECK(!session_closed_cb_.is_null());
  DCHECK(!legacy_session_error_cb_.is_null());
  DCHECK(!session_keys_change_cb.is_null());
  DCHECK(!session_expiration_update_cb.is_null());
}

PpapiDecryptor::~PpapiDecryptor() {
  pepper_cdm_wrapper_.reset();
}

void PpapiDecryptor::InitializeCdm(
    const std::string& key_system,
    bool allow_distinctive_identifier,
    bool allow_persistent_state,
    std::unique_ptr<media::SimpleCdmPromise> promise) {
  base::WeakPtr<PpapiDecryptor> weak_this = weak_ptr_factory_.GetWeakPtr();
  CdmDelegate()->Initialize(
      key_system, allow_distinctive_identifier, allow_persistent_state,
      base::Bind(&PpapiDecryptor::OnSessionMessage, weak_this),
      base::Bind(&PpapiDecryptor::OnSessionClosed, weak_this),
      base::Bind(&PpapiDecryptor::OnLegacySessionError, weak_this),
      base::Bind(&PpapiDecryptor::OnSessionKeysChange, weak_this),
      base::Bind(&PpapiDecryptor::OnSessionExpirationUpdate, weak_this),
      base::Bind(&PpapiDecryptor::OnFatalPluginError, weak_this),
      std::move(promise));
}

void PpapiDecryptor::SetServerCertificate(
    const std::vector<uint8_t>& certificate,
    std::unique_ptr<media::SimpleCdmPromise> promise) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(render_task_runner_->BelongsToCurrentThread());

  if (!CdmDelegate()) {
    promise->reject(INVALID_STATE_ERROR, 0, "CDM has failed.");
    return;
  }

  CdmDelegate()->SetServerCertificate(certificate, std::move(promise));
}

void PpapiDecryptor::CreateSessionAndGenerateRequest(
    SessionType session_type,
    media::EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data,
    std::unique_ptr<media::NewSessionCdmPromise> promise) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(render_task_runner_->BelongsToCurrentThread());

  if (!CdmDelegate()) {
    promise->reject(INVALID_STATE_ERROR, 0, "CDM has failed.");
    return;
  }

  CdmDelegate()->CreateSessionAndGenerateRequest(session_type, init_data_type,
                                                 init_data, std::move(promise));
}

void PpapiDecryptor::LoadSession(
    SessionType session_type,
    const std::string& session_id,
    std::unique_ptr<media::NewSessionCdmPromise> promise) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(render_task_runner_->BelongsToCurrentThread());

  if (!CdmDelegate()) {
    promise->reject(INVALID_STATE_ERROR, 0, "CDM has failed.");
    return;
  }
  CdmDelegate()->LoadSession(session_type, session_id, std::move(promise));
}

void PpapiDecryptor::UpdateSession(
    const std::string& session_id,
    const std::vector<uint8_t>& response,
    std::unique_ptr<media::SimpleCdmPromise> promise) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(render_task_runner_->BelongsToCurrentThread());

  if (!CdmDelegate()) {
    promise->reject(INVALID_STATE_ERROR, 0, "CDM has failed.");
    return;
  }
  CdmDelegate()->UpdateSession(session_id, response, std::move(promise));
}

void PpapiDecryptor::CloseSession(
    const std::string& session_id,
    std::unique_ptr<media::SimpleCdmPromise> promise) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(render_task_runner_->BelongsToCurrentThread());

  if (!CdmDelegate()) {
    promise->reject(INVALID_STATE_ERROR, 0, "CDM has failed.");
    return;
  }

  CdmDelegate()->CloseSession(session_id, std::move(promise));
}

void PpapiDecryptor::RemoveSession(
    const std::string& session_id,
    std::unique_ptr<media::SimpleCdmPromise> promise) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(render_task_runner_->BelongsToCurrentThread());

  if (!CdmDelegate()) {
    promise->reject(INVALID_STATE_ERROR, 0, "CDM has failed.");
    return;
  }

  CdmDelegate()->RemoveSession(session_id, std::move(promise));
}

media::CdmContext* PpapiDecryptor::GetCdmContext() {
  return this;
}

media::Decryptor* PpapiDecryptor::GetDecryptor() {
  return this;
}

int PpapiDecryptor::GetCdmId() const {
  return kInvalidCdmId;
}

void PpapiDecryptor::RegisterNewKeyCB(StreamType stream_type,
                                      const NewKeyCB& new_key_cb) {
  if (!render_task_runner_->BelongsToCurrentThread()) {
    render_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&PpapiDecryptor::RegisterNewKeyCB,
                   weak_ptr_factory_.GetWeakPtr(), stream_type, new_key_cb));
    return;
  }

  DVLOG(3) << __FUNCTION__ << " - stream_type: " << stream_type;
  switch (stream_type) {
    case kAudio:
      new_audio_key_cb_ = new_key_cb;
      break;
    case kVideo:
      new_video_key_cb_ = new_key_cb;
      break;
    default:
      NOTREACHED();
  }
}

void PpapiDecryptor::Decrypt(
    StreamType stream_type,
    const scoped_refptr<media::DecoderBuffer>& encrypted,
    const DecryptCB& decrypt_cb) {
  if (!render_task_runner_->BelongsToCurrentThread()) {
    render_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&PpapiDecryptor::Decrypt, weak_ptr_factory_.GetWeakPtr(),
                   stream_type, encrypted, decrypt_cb));
    return;
  }

  DVLOG(3) << __FUNCTION__ << " - stream_type: " << stream_type;
  if (!CdmDelegate() ||
      !CdmDelegate()->Decrypt(stream_type, encrypted, decrypt_cb)) {
    decrypt_cb.Run(kError, NULL);
  }
}

void PpapiDecryptor::CancelDecrypt(StreamType stream_type) {
  if (!render_task_runner_->BelongsToCurrentThread()) {
    render_task_runner_->PostTask(
        FROM_HERE, base::Bind(&PpapiDecryptor::CancelDecrypt,
                              weak_ptr_factory_.GetWeakPtr(), stream_type));
    return;
  }

  DVLOG(1) << __FUNCTION__ << " - stream_type: " << stream_type;
  if (CdmDelegate())
    CdmDelegate()->CancelDecrypt(stream_type);
}

void PpapiDecryptor::InitializeAudioDecoder(
    const media::AudioDecoderConfig& config,
    const DecoderInitCB& init_cb) {
  if (!render_task_runner_->BelongsToCurrentThread()) {
    render_task_runner_->PostTask(
        FROM_HERE, base::Bind(&PpapiDecryptor::InitializeAudioDecoder,
                              weak_ptr_factory_.GetWeakPtr(), config, init_cb));
    return;
  }

  DVLOG(2) << __FUNCTION__;
  DCHECK(config.is_encrypted());
  DCHECK(config.IsValidConfig());

  audio_decoder_init_cb_ = init_cb;
  if (!CdmDelegate() ||
      !CdmDelegate()->InitializeAudioDecoder(
          config, base::Bind(&PpapiDecryptor::OnDecoderInitialized,
                             weak_ptr_factory_.GetWeakPtr(), kAudio))) {
    base::ResetAndReturn(&audio_decoder_init_cb_).Run(false);
    return;
  }
}

void PpapiDecryptor::InitializeVideoDecoder(
    const media::VideoDecoderConfig& config,
    const DecoderInitCB& init_cb) {
  if (!render_task_runner_->BelongsToCurrentThread()) {
    render_task_runner_->PostTask(
        FROM_HERE, base::Bind(&PpapiDecryptor::InitializeVideoDecoder,
                              weak_ptr_factory_.GetWeakPtr(), config, init_cb));
    return;
  }

  DVLOG(2) << __FUNCTION__;
  DCHECK(config.is_encrypted());
  DCHECK(config.IsValidConfig());

  video_decoder_init_cb_ = init_cb;
  if (!CdmDelegate() ||
      !CdmDelegate()->InitializeVideoDecoder(
          config, base::Bind(&PpapiDecryptor::OnDecoderInitialized,
                             weak_ptr_factory_.GetWeakPtr(), kVideo))) {
    base::ResetAndReturn(&video_decoder_init_cb_).Run(false);
    return;
  }
}

void PpapiDecryptor::DecryptAndDecodeAudio(
    const scoped_refptr<media::DecoderBuffer>& encrypted,
    const AudioDecodeCB& audio_decode_cb) {
  if (!render_task_runner_->BelongsToCurrentThread()) {
    render_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&PpapiDecryptor::DecryptAndDecodeAudio,
                   weak_ptr_factory_.GetWeakPtr(), encrypted, audio_decode_cb));
    return;
  }

  DVLOG(3) << __FUNCTION__;
  if (!CdmDelegate() ||
      !CdmDelegate()->DecryptAndDecodeAudio(encrypted, audio_decode_cb)) {
    audio_decode_cb.Run(kError, AudioFrames());
  }
}

void PpapiDecryptor::DecryptAndDecodeVideo(
    const scoped_refptr<media::DecoderBuffer>& encrypted,
    const VideoDecodeCB& video_decode_cb) {
  if (!render_task_runner_->BelongsToCurrentThread()) {
    render_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&PpapiDecryptor::DecryptAndDecodeVideo,
                   weak_ptr_factory_.GetWeakPtr(), encrypted, video_decode_cb));
    return;
  }

  DVLOG(3) << __FUNCTION__;
  if (!CdmDelegate() ||
      !CdmDelegate()->DecryptAndDecodeVideo(encrypted, video_decode_cb)) {
    video_decode_cb.Run(kError, NULL);
  }
}

void PpapiDecryptor::ResetDecoder(StreamType stream_type) {
  if (!render_task_runner_->BelongsToCurrentThread()) {
    render_task_runner_->PostTask(
        FROM_HERE, base::Bind(&PpapiDecryptor::ResetDecoder,
                              weak_ptr_factory_.GetWeakPtr(), stream_type));
    return;
  }

  DVLOG(2) << __FUNCTION__ << " - stream_type: " << stream_type;
  if (CdmDelegate())
    CdmDelegate()->ResetDecoder(stream_type);
}

void PpapiDecryptor::DeinitializeDecoder(StreamType stream_type) {
  if (!render_task_runner_->BelongsToCurrentThread()) {
    render_task_runner_->PostTask(
        FROM_HERE, base::Bind(&PpapiDecryptor::DeinitializeDecoder,
                              weak_ptr_factory_.GetWeakPtr(), stream_type));
    return;
  }

  DVLOG(2) << __FUNCTION__ << " - stream_type: " << stream_type;
  if (CdmDelegate())
    CdmDelegate()->DeinitializeDecoder(stream_type);
}

void PpapiDecryptor::OnDecoderInitialized(StreamType stream_type,
                                          bool success) {
  DCHECK(render_task_runner_->BelongsToCurrentThread());
  switch (stream_type) {
    case kAudio:
      DCHECK(!audio_decoder_init_cb_.is_null());
      base::ResetAndReturn(&audio_decoder_init_cb_).Run(success);
      break;
    case kVideo:
      DCHECK(!video_decoder_init_cb_.is_null());
      base::ResetAndReturn(&video_decoder_init_cb_).Run(success);
      break;
    default:
      NOTREACHED();
  }
}

void PpapiDecryptor::OnSessionMessage(const std::string& session_id,
                                      MessageType message_type,
                                      const std::vector<uint8_t>& message,
                                      const GURL& legacy_destination_url) {
  DCHECK(render_task_runner_->BelongsToCurrentThread());
  session_message_cb_.Run(session_id, message_type, message,
                          legacy_destination_url);
}

void PpapiDecryptor::OnSessionKeysChange(const std::string& session_id,
                                         bool has_additional_usable_key,
                                         media::CdmKeysInfo keys_info) {
  DVLOG(2) << __FUNCTION__ << ": " << has_additional_usable_key;
  DCHECK(render_task_runner_->BelongsToCurrentThread());

  // TODO(jrummell): Handling resume playback should be done in the media
  // player, not in the Decryptors. http://crbug.com/413413.
  if (has_additional_usable_key)
    AttemptToResumePlayback();

  session_keys_change_cb_.Run(session_id, has_additional_usable_key,
                              std::move(keys_info));
}

void PpapiDecryptor::OnSessionExpirationUpdate(
    const std::string& session_id,
    const base::Time& new_expiry_time) {
  DCHECK(render_task_runner_->BelongsToCurrentThread());
  session_expiration_update_cb_.Run(session_id, new_expiry_time);
}

void PpapiDecryptor::OnSessionClosed(const std::string& session_id) {
  DCHECK(render_task_runner_->BelongsToCurrentThread());
  session_closed_cb_.Run(session_id);
}

void PpapiDecryptor::OnLegacySessionError(
    const std::string& session_id,
    MediaKeys::Exception exception_code,
    uint32_t system_code,
    const std::string& error_description) {
  DCHECK(render_task_runner_->BelongsToCurrentThread());
  legacy_session_error_cb_.Run(session_id, exception_code, system_code,
                               error_description);
}

void PpapiDecryptor::AttemptToResumePlayback() {
  if (!new_audio_key_cb_.is_null())
    new_audio_key_cb_.Run();

  if (!new_video_key_cb_.is_null())
    new_video_key_cb_.Run();
}

void PpapiDecryptor::OnFatalPluginError() {
  DCHECK(render_task_runner_->BelongsToCurrentThread());
  pepper_cdm_wrapper_.reset();
}

ContentDecryptorDelegate* PpapiDecryptor::CdmDelegate() {
  DCHECK(render_task_runner_->BelongsToCurrentThread());
  return (pepper_cdm_wrapper_) ? pepper_cdm_wrapper_->GetCdmDelegate() : NULL;
}

}  // namespace content
