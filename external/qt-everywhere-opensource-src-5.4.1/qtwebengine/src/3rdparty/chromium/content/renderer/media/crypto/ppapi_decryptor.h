// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CRYPTO_PPAPI_DECRYPTOR_H_
#define CONTENT_RENDERER_MEDIA_CRYPTO_PPAPI_DECRYPTOR_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/renderer/media/crypto/pepper_cdm_wrapper.h"
#include "media/base/decryptor.h"
#include "media/base/media_keys.h"
#include "media/base/video_decoder_config.h"

class GURL;

namespace base {
class MessageLoopProxy;
}

namespace content {
class ContentDecryptorDelegate;
class PepperPluginInstanceImpl;

// PpapiDecryptor implements media::MediaKeys and media::Decryptor and forwards
// all calls to the PluginInstance.
// This class should always be created & destroyed on the main renderer thread.
class PpapiDecryptor : public media::MediaKeys, public media::Decryptor {
 public:
  static scoped_ptr<PpapiDecryptor> Create(
      const std::string& key_system,
      const GURL& security_origin,
      const CreatePepperCdmCB& create_pepper_cdm_cb,
      const media::SessionMessageCB& session_message_cb,
      const media::SessionReadyCB& session_ready_cb,
      const media::SessionClosedCB& session_closed_cb,
      const media::SessionErrorCB& session_error_cb);

  virtual ~PpapiDecryptor();

  // media::MediaKeys implementation.
  virtual void CreateSession(
      const std::string& init_data_type,
      const uint8* init_data,
      int init_data_length,
      SessionType session_type,
      scoped_ptr<media::NewSessionCdmPromise> promise) OVERRIDE;
  virtual void LoadSession(
      const std::string& web_session_id,
      scoped_ptr<media::NewSessionCdmPromise> promise) OVERRIDE;
  virtual void UpdateSession(
      const std::string& web_session_id,
      const uint8* response,
      int response_length,
      scoped_ptr<media::SimpleCdmPromise> promise) OVERRIDE;
  virtual void ReleaseSession(
      const std::string& web_session_id,
      scoped_ptr<media::SimpleCdmPromise> promise) OVERRIDE;
  virtual Decryptor* GetDecryptor() OVERRIDE;

  // media::Decryptor implementation.
  virtual void RegisterNewKeyCB(StreamType stream_type,
                                const NewKeyCB& key_added_cb) OVERRIDE;
  virtual void Decrypt(StreamType stream_type,
                       const scoped_refptr<media::DecoderBuffer>& encrypted,
                       const DecryptCB& decrypt_cb) OVERRIDE;
  virtual void CancelDecrypt(StreamType stream_type) OVERRIDE;
  virtual void InitializeAudioDecoder(const media::AudioDecoderConfig& config,
                                      const DecoderInitCB& init_cb) OVERRIDE;
  virtual void InitializeVideoDecoder(const media::VideoDecoderConfig& config,
                                      const DecoderInitCB& init_cb) OVERRIDE;
  virtual void DecryptAndDecodeAudio(
      const scoped_refptr<media::DecoderBuffer>& encrypted,
      const AudioDecodeCB& audio_decode_cb) OVERRIDE;
  virtual void DecryptAndDecodeVideo(
      const scoped_refptr<media::DecoderBuffer>& encrypted,
      const VideoDecodeCB& video_decode_cb) OVERRIDE;
  virtual void ResetDecoder(StreamType stream_type) OVERRIDE;
  virtual void DeinitializeDecoder(StreamType stream_type) OVERRIDE;

 private:
  PpapiDecryptor(const std::string& key_system,
                 scoped_ptr<PepperCdmWrapper> pepper_cdm_wrapper,
                 const media::SessionMessageCB& session_message_cb,
                 const media::SessionReadyCB& session_ready_cb,
                 const media::SessionClosedCB& session_closed_cb,
                 const media::SessionErrorCB& session_error_cb);

  void OnDecoderInitialized(StreamType stream_type, bool success);

  // Callbacks for |plugin_cdm_delegate_| to fire session events.
  void OnSessionMessage(const std::string& web_session_id,
                        const std::vector<uint8>& message,
                        const GURL& destination_url);
  void OnSessionReady(const std::string& web_session_id);
  void OnSessionClosed(const std::string& web_session_id);
  void OnSessionError(const std::string& web_session_id,
                      MediaKeys::Exception exception_code,
                      uint32 system_code,
                      const std::string& error_description);

  // On a successful Update() or SessionReady event, trigger playback to resume.
  void ResumePlayback();

  // Callback to notify that a fatal error happened in |plugin_cdm_delegate_|.
  // The error is terminal and |plugin_cdm_delegate_| should not be used after
  // this call.
  void OnFatalPluginError();

  ContentDecryptorDelegate* CdmDelegate();

  // Hold a reference of the Pepper CDM wrapper to make sure the plugin lives
  // as long as needed.
  scoped_ptr<PepperCdmWrapper> pepper_cdm_wrapper_;

  // Callbacks for firing session events.
  media::SessionMessageCB session_message_cb_;
  media::SessionReadyCB session_ready_cb_;
  media::SessionClosedCB session_closed_cb_;
  media::SessionErrorCB session_error_cb_;

  scoped_refptr<base::MessageLoopProxy> render_loop_proxy_;

  DecoderInitCB audio_decoder_init_cb_;
  DecoderInitCB video_decoder_init_cb_;
  NewKeyCB new_audio_key_cb_;
  NewKeyCB new_video_key_cb_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<PpapiDecryptor> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PpapiDecryptor);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CRYPTO_PPAPI_DECRYPTOR_H_
