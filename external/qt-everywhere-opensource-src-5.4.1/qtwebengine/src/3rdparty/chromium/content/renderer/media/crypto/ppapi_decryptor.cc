// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/crypto/ppapi_decryptor.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "content/renderer/media/crypto/key_systems.h"
#include "content/renderer/pepper/content_decryptor_delegate.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/cdm_promise.h"
#include "media/base/data_buffer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"

namespace content {

// This class is needed so that resolving an Update() promise triggers playback
// of the stream. It intercepts the resolve() call to invoke an additional
// callback.
class SessionUpdatedPromise : public media::SimpleCdmPromise {
 public:
  SessionUpdatedPromise(scoped_ptr<media::SimpleCdmPromise> caller_promise,
                        base::Closure additional_resolve_cb)
      : caller_promise_(caller_promise.Pass()),
        additional_resolve_cb_(additional_resolve_cb) {}

  virtual void resolve() OVERRIDE {
    DCHECK(is_pending_);
    is_pending_ = false;
    additional_resolve_cb_.Run();
    caller_promise_->resolve();
  }

  virtual void reject(media::MediaKeys::Exception exception_code,
                      uint32 system_code,
                      const std::string& error_message) OVERRIDE {
    DCHECK(is_pending_);
    is_pending_ = false;
    caller_promise_->reject(exception_code, system_code, error_message);
  }

 protected:
  scoped_ptr<media::SimpleCdmPromise> caller_promise_;
  base::Closure additional_resolve_cb_;
};

scoped_ptr<PpapiDecryptor> PpapiDecryptor::Create(
    const std::string& key_system,
    const GURL& security_origin,
    const CreatePepperCdmCB& create_pepper_cdm_cb,
    const media::SessionMessageCB& session_message_cb,
    const media::SessionReadyCB& session_ready_cb,
    const media::SessionClosedCB& session_closed_cb,
    const media::SessionErrorCB& session_error_cb) {
  std::string plugin_type = GetPepperType(key_system);
  DCHECK(!plugin_type.empty());
  scoped_ptr<PepperCdmWrapper> pepper_cdm_wrapper =
      create_pepper_cdm_cb.Run(plugin_type, security_origin);
  if (!pepper_cdm_wrapper) {
    DLOG(ERROR) << "Plugin instance creation failed.";
    return scoped_ptr<PpapiDecryptor>();
  }

  return scoped_ptr<PpapiDecryptor>(
      new PpapiDecryptor(key_system,
                         pepper_cdm_wrapper.Pass(),
                         session_message_cb,
                         session_ready_cb,
                         session_closed_cb,
                         session_error_cb));
}

PpapiDecryptor::PpapiDecryptor(
    const std::string& key_system,
    scoped_ptr<PepperCdmWrapper> pepper_cdm_wrapper,
    const media::SessionMessageCB& session_message_cb,
    const media::SessionReadyCB& session_ready_cb,
    const media::SessionClosedCB& session_closed_cb,
    const media::SessionErrorCB& session_error_cb)
    : pepper_cdm_wrapper_(pepper_cdm_wrapper.Pass()),
      session_message_cb_(session_message_cb),
      session_ready_cb_(session_ready_cb),
      session_closed_cb_(session_closed_cb),
      session_error_cb_(session_error_cb),
      render_loop_proxy_(base::MessageLoopProxy::current()),
      weak_ptr_factory_(this) {
  DCHECK(pepper_cdm_wrapper_.get());
  DCHECK(!session_message_cb_.is_null());
  DCHECK(!session_ready_cb_.is_null());
  DCHECK(!session_closed_cb_.is_null());
  DCHECK(!session_error_cb_.is_null());

  base::WeakPtr<PpapiDecryptor> weak_this = weak_ptr_factory_.GetWeakPtr();
  CdmDelegate()->Initialize(
      key_system,
      base::Bind(&PpapiDecryptor::OnSessionMessage, weak_this),
      base::Bind(&PpapiDecryptor::OnSessionReady, weak_this),
      base::Bind(&PpapiDecryptor::OnSessionClosed, weak_this),
      base::Bind(&PpapiDecryptor::OnSessionError, weak_this),
      base::Bind(&PpapiDecryptor::OnFatalPluginError, weak_this));
}

PpapiDecryptor::~PpapiDecryptor() {
  pepper_cdm_wrapper_.reset();
}

void PpapiDecryptor::CreateSession(
    const std::string& init_data_type,
    const uint8* init_data,
    int init_data_length,
    SessionType session_type,
    scoped_ptr<media::NewSessionCdmPromise> promise) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(render_loop_proxy_->BelongsToCurrentThread());

  if (!CdmDelegate()) {
    promise->reject(INVALID_STATE_ERROR, 0, "CdmDelegate() does not exist.");
    return;
  }

  CdmDelegate()->CreateSession(init_data_type,
                               init_data,
                               init_data_length,
                               session_type,
                               promise.Pass());
}

void PpapiDecryptor::LoadSession(
    const std::string& web_session_id,
    scoped_ptr<media::NewSessionCdmPromise> promise) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(render_loop_proxy_->BelongsToCurrentThread());

  if (!CdmDelegate()) {
    promise->reject(INVALID_STATE_ERROR, 0, "CdmDelegate() does not exist.");
    return;
  }

  CdmDelegate()->LoadSession(web_session_id, promise.Pass());
}

void PpapiDecryptor::UpdateSession(
    const std::string& web_session_id,
    const uint8* response,
    int response_length,
    scoped_ptr<media::SimpleCdmPromise> promise) {
  DCHECK(render_loop_proxy_->BelongsToCurrentThread());

  if (!CdmDelegate()) {
    promise->reject(INVALID_STATE_ERROR, 0, "CdmDelegate() does not exist.");
    return;
  }

  scoped_ptr<SessionUpdatedPromise> session_updated_promise(
      new SessionUpdatedPromise(promise.Pass(),
                                base::Bind(&PpapiDecryptor::ResumePlayback,
                                           weak_ptr_factory_.GetWeakPtr())));
  CdmDelegate()->UpdateSession(
      web_session_id,
      response,
      response_length,
      session_updated_promise.PassAs<media::SimpleCdmPromise>());
}

void PpapiDecryptor::ReleaseSession(
    const std::string& web_session_id,
    scoped_ptr<media::SimpleCdmPromise> promise) {
  DCHECK(render_loop_proxy_->BelongsToCurrentThread());

  if (!CdmDelegate()) {
    promise->reject(INVALID_STATE_ERROR, 0, "CdmDelegate() does not exist.");
    return;
  }

  CdmDelegate()->ReleaseSession(web_session_id, promise.Pass());
}

media::Decryptor* PpapiDecryptor::GetDecryptor() {
  return this;
}

void PpapiDecryptor::RegisterNewKeyCB(StreamType stream_type,
                                      const NewKeyCB& new_key_cb) {
  if (!render_loop_proxy_->BelongsToCurrentThread()) {
    render_loop_proxy_->PostTask(FROM_HERE,
                                 base::Bind(&PpapiDecryptor::RegisterNewKeyCB,
                                            weak_ptr_factory_.GetWeakPtr(),
                                            stream_type,
                                            new_key_cb));
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
  if (!render_loop_proxy_->BelongsToCurrentThread()) {
    render_loop_proxy_->PostTask(FROM_HERE,
                                 base::Bind(&PpapiDecryptor::Decrypt,
                                            weak_ptr_factory_.GetWeakPtr(),
                                            stream_type,
                                            encrypted,
                                            decrypt_cb));
    return;
  }

  DVLOG(3) << __FUNCTION__ << " - stream_type: " << stream_type;
  if (!CdmDelegate() ||
      !CdmDelegate()->Decrypt(stream_type, encrypted, decrypt_cb)) {
    decrypt_cb.Run(kError, NULL);
  }
}

void PpapiDecryptor::CancelDecrypt(StreamType stream_type) {
  if (!render_loop_proxy_->BelongsToCurrentThread()) {
    render_loop_proxy_->PostTask(FROM_HERE,
                                 base::Bind(&PpapiDecryptor::CancelDecrypt,
                                            weak_ptr_factory_.GetWeakPtr(),
                                            stream_type));
    return;
  }

  DVLOG(1) << __FUNCTION__ << " - stream_type: " << stream_type;
  if (CdmDelegate())
    CdmDelegate()->CancelDecrypt(stream_type);
}

void PpapiDecryptor::InitializeAudioDecoder(
      const media::AudioDecoderConfig& config,
      const DecoderInitCB& init_cb) {
  if (!render_loop_proxy_->BelongsToCurrentThread()) {
    render_loop_proxy_->PostTask(
        FROM_HERE,
        base::Bind(&PpapiDecryptor::InitializeAudioDecoder,
                   weak_ptr_factory_.GetWeakPtr(),
                   config,
                   init_cb));
    return;
  }

  DVLOG(2) << __FUNCTION__;
  DCHECK(config.is_encrypted());
  DCHECK(config.IsValidConfig());

  audio_decoder_init_cb_ = init_cb;
  if (!CdmDelegate() || !CdmDelegate()->InitializeAudioDecoder(
                            config,
                            base::Bind(&PpapiDecryptor::OnDecoderInitialized,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       kAudio))) {
    base::ResetAndReturn(&audio_decoder_init_cb_).Run(false);
    return;
  }
}

void PpapiDecryptor::InitializeVideoDecoder(
    const media::VideoDecoderConfig& config,
    const DecoderInitCB& init_cb) {
  if (!render_loop_proxy_->BelongsToCurrentThread()) {
    render_loop_proxy_->PostTask(
        FROM_HERE,
        base::Bind(&PpapiDecryptor::InitializeVideoDecoder,
                   weak_ptr_factory_.GetWeakPtr(),
                   config,
                   init_cb));
    return;
  }

  DVLOG(2) << __FUNCTION__;
  DCHECK(config.is_encrypted());
  DCHECK(config.IsValidConfig());

  video_decoder_init_cb_ = init_cb;
  if (!CdmDelegate() || !CdmDelegate()->InitializeVideoDecoder(
                            config,
                            base::Bind(&PpapiDecryptor::OnDecoderInitialized,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       kVideo))) {
    base::ResetAndReturn(&video_decoder_init_cb_).Run(false);
    return;
  }
}

void PpapiDecryptor::DecryptAndDecodeAudio(
    const scoped_refptr<media::DecoderBuffer>& encrypted,
    const AudioDecodeCB& audio_decode_cb) {
  if (!render_loop_proxy_->BelongsToCurrentThread()) {
    render_loop_proxy_->PostTask(
        FROM_HERE,
        base::Bind(&PpapiDecryptor::DecryptAndDecodeAudio,
                   weak_ptr_factory_.GetWeakPtr(),
                   encrypted,
                   audio_decode_cb));
    return;
  }

  DVLOG(3) << __FUNCTION__;
  if (!CdmDelegate() ||
      !CdmDelegate()->DecryptAndDecodeAudio(encrypted, audio_decode_cb)) {
    audio_decode_cb.Run(kError, AudioBuffers());
  }
}

void PpapiDecryptor::DecryptAndDecodeVideo(
    const scoped_refptr<media::DecoderBuffer>& encrypted,
    const VideoDecodeCB& video_decode_cb) {
  if (!render_loop_proxy_->BelongsToCurrentThread()) {
    render_loop_proxy_->PostTask(
        FROM_HERE,
        base::Bind(&PpapiDecryptor::DecryptAndDecodeVideo,
                   weak_ptr_factory_.GetWeakPtr(),
                   encrypted,
                   video_decode_cb));
    return;
  }

  DVLOG(3) << __FUNCTION__;
  if (!CdmDelegate() ||
      !CdmDelegate()->DecryptAndDecodeVideo(encrypted, video_decode_cb)) {
    video_decode_cb.Run(kError, NULL);
  }
}

void PpapiDecryptor::ResetDecoder(StreamType stream_type) {
  if (!render_loop_proxy_->BelongsToCurrentThread()) {
    render_loop_proxy_->PostTask(FROM_HERE,
                                 base::Bind(&PpapiDecryptor::ResetDecoder,
                                            weak_ptr_factory_.GetWeakPtr(),
                                            stream_type));
    return;
  }

  DVLOG(2) << __FUNCTION__ << " - stream_type: " << stream_type;
  if (CdmDelegate())
    CdmDelegate()->ResetDecoder(stream_type);
}

void PpapiDecryptor::DeinitializeDecoder(StreamType stream_type) {
  if (!render_loop_proxy_->BelongsToCurrentThread()) {
    render_loop_proxy_->PostTask(
        FROM_HERE,
        base::Bind(&PpapiDecryptor::DeinitializeDecoder,
                   weak_ptr_factory_.GetWeakPtr(),
                   stream_type));
    return;
  }

  DVLOG(2) << __FUNCTION__ << " - stream_type: " << stream_type;
  if (CdmDelegate())
    CdmDelegate()->DeinitializeDecoder(stream_type);
}

void PpapiDecryptor::OnDecoderInitialized(StreamType stream_type,
                                          bool success) {
  DCHECK(render_loop_proxy_->BelongsToCurrentThread());
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

void PpapiDecryptor::OnSessionMessage(const std::string& web_session_id,
                                      const std::vector<uint8>& message,
                                      const GURL& destination_url) {
  DCHECK(render_loop_proxy_->BelongsToCurrentThread());
  session_message_cb_.Run(web_session_id, message, destination_url);
}

void PpapiDecryptor::OnSessionReady(const std::string& web_session_id) {
  DCHECK(render_loop_proxy_->BelongsToCurrentThread());

  ResumePlayback();
  session_ready_cb_.Run(web_session_id);
}

void PpapiDecryptor::OnSessionClosed(const std::string& web_session_id) {
  DCHECK(render_loop_proxy_->BelongsToCurrentThread());
  session_closed_cb_.Run(web_session_id);
}

void PpapiDecryptor::OnSessionError(const std::string& web_session_id,
                                    MediaKeys::Exception exception_code,
                                    uint32 system_code,
                                    const std::string& error_description) {
  DCHECK(render_loop_proxy_->BelongsToCurrentThread());
  session_error_cb_.Run(
      web_session_id, exception_code, system_code, error_description);
}

void PpapiDecryptor::ResumePlayback() {
  // Based on the spec, we need to resume playback when update() completes
  // successfully, or when a session is successfully loaded (triggered by
  // OnSessionReady()). So we choose to call the NewKeyCBs here.
  if (!new_audio_key_cb_.is_null())
    new_audio_key_cb_.Run();

  if (!new_video_key_cb_.is_null())
    new_video_key_cb_.Run();
}

void PpapiDecryptor::OnFatalPluginError() {
  DCHECK(render_loop_proxy_->BelongsToCurrentThread());
  pepper_cdm_wrapper_.reset();
}

ContentDecryptorDelegate* PpapiDecryptor::CdmDelegate() {
  DCHECK(render_loop_proxy_->BelongsToCurrentThread());
  return (pepper_cdm_wrapper_) ? pepper_cdm_wrapper_->GetCdmDelegate() : NULL;
}

}  // namespace content
