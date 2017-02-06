// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_PPAPI_EXTERNAL_CLEAR_KEY_CLEAR_KEY_CDM_H_
#define MEDIA_CDM_PPAPI_EXTERNAL_CLEAR_KEY_CLEAR_KEY_CDM_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "media/base/cdm_key_information.h"
#include "media/cdm/aes_decryptor.h"
#include "media/cdm/ppapi/external_clear_key/clear_key_cdm_common.h"

// Enable this to use the fake decoder for testing.
// TODO(tomfinegan): Move fake audio decoder into a separate class.
#if 0
#define CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER
#endif

class GURL;

namespace media {
class FileIOTestRunner;
class CdmVideoDecoder;
class DecoderBuffer;
class FFmpegCdmAudioDecoder;

// Clear key implementation of the cdm::ContentDecryptionModule interface.
class ClearKeyCdm : public ClearKeyCdmInterface {
 public:
  ClearKeyCdm(Host* host, const std::string& key_system, const GURL& origin);
  ~ClearKeyCdm() override;

  // ContentDecryptionModule implementation.
  void Initialize(bool allow_distinctive_identifier,
                  bool allow_persistent_state) override;
  void CreateSessionAndGenerateRequest(uint32_t promise_id,
                                       cdm::SessionType session_type,
                                       cdm::InitDataType init_data_type,
                                       const uint8_t* init_data,
                                       uint32_t init_data_size) override;
  void LoadSession(uint32_t promise_id,
                   cdm::SessionType session_type,
                   const char* session_id,
                   uint32_t session_id_length) override;
  void UpdateSession(uint32_t promise_id,
                     const char* session_id,
                     uint32_t session_id_length,
                     const uint8_t* response,
                     uint32_t response_size) override;
  void CloseSession(uint32_t promise_id,
                    const char* session_id,
                    uint32_t session_id_length) override;
  void RemoveSession(uint32_t promise_id,
                     const char* session_id,
                     uint32_t session_id_length) override;
  void SetServerCertificate(uint32_t promise_id,
                            const uint8_t* server_certificate_data,
                            uint32_t server_certificate_data_size) override;
  void TimerExpired(void* context) override;
  cdm::Status Decrypt(const cdm::InputBuffer& encrypted_buffer,
                      cdm::DecryptedBlock* decrypted_block) override;
  cdm::Status InitializeAudioDecoder(
      const cdm::AudioDecoderConfig& audio_decoder_config) override;
  cdm::Status InitializeVideoDecoder(
      const cdm::VideoDecoderConfig& video_decoder_config) override;
  void DeinitializeDecoder(cdm::StreamType decoder_type) override;
  void ResetDecoder(cdm::StreamType decoder_type) override;
  cdm::Status DecryptAndDecodeFrame(const cdm::InputBuffer& encrypted_buffer,
                                    cdm::VideoFrame* video_frame) override;
  cdm::Status DecryptAndDecodeSamples(const cdm::InputBuffer& encrypted_buffer,
                                      cdm::AudioFrames* audio_frames) override;
  void Destroy() override;
  void OnPlatformChallengeResponse(
      const cdm::PlatformChallengeResponse& response) override;
  void OnQueryOutputProtectionStatus(cdm::QueryResult result,
                                     uint32_t link_mask,
                                     uint32_t output_protection_mask) override;

 private:
  // Emulates a session stored for |session_id_for_emulated_loadsession_|. This
  // is necessary since aes_decryptor.cc does not support storing sessions.
  void LoadLoadableSession();
  void OnLoadSessionUpdated();

  // ContentDecryptionModule callbacks.
  void OnSessionMessage(const std::string& session_id,
                        MediaKeys::MessageType message_type,
                        const std::vector<uint8_t>& message,
                        const GURL& legacy_destination_url);
  void OnSessionKeysChange(const std::string& session_id,
                           bool has_additional_usable_key,
                           CdmKeysInfo keys_info);
  void OnSessionClosed(const std::string& session_id);

  // Handle the success/failure of a promise. These methods are responsible for
  // calling |host_| to resolve or reject the promise.
  void OnSessionCreated(uint32_t promise_id, const std::string& session_id);
  void OnSessionLoaded(uint32_t promise_id, const std::string& session_id);
  void OnPromiseResolved(uint32_t promise_id);
  void OnPromiseFailed(uint32_t promise_id,
                       MediaKeys::Exception exception_code,
                       uint32_t system_code,
                       const std::string& error_message);

  // Prepares next renewal message and sets a timer for it.
  void ScheduleNextRenewal();

  // Decrypts the |encrypted_buffer| and puts the result in |decrypted_buffer|.
  // Returns cdm::kSuccess if decryption succeeded. The decrypted result is
  // put in |decrypted_buffer|. If |encrypted_buffer| is empty, the
  // |decrypted_buffer| is set to an empty (EOS) buffer.
  // Returns cdm::kNoKey if no decryption key was available. In this case
  // |decrypted_buffer| should be ignored by the caller.
  // Returns cdm::kDecryptError if any decryption error occurred. In this case
  // |decrypted_buffer| should be ignored by the caller.
  cdm::Status DecryptToMediaDecoderBuffer(
      const cdm::InputBuffer& encrypted_buffer,
      scoped_refptr<DecoderBuffer>* decrypted_buffer);

#if defined(CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER)
  int64_t CurrentTimeStampInMicroseconds() const;

  // Generates fake video frames with |duration_in_microseconds|.
  // Returns the number of samples generated in the |audio_frames|.
  int GenerateFakeAudioFramesFromDuration(int64_t duration_in_microseconds,
                                          cdm::AudioFrames* audio_frames) const;

  // Generates fake video frames given |input_timestamp|.
  // Returns cdm::kSuccess if any audio frame is successfully generated.
  cdm::Status GenerateFakeAudioFrames(int64_t timestamp_in_microseconds,
                                      cdm::AudioFrames* audio_frames);
#endif  // CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER

  void StartFileIOTest();

  // Callback for CDM File IO test.
  void OnFileIOTestComplete(bool success);

  // Keep track of the last session created.
  void SetSessionId(const std::string& session_id);

  scoped_refptr<AesDecryptor> decryptor_;

  ClearKeyCdmHost* host_;

  const std::string key_system_;

  std::string last_session_id_;
  std::string next_renewal_message_;

  // In order to simulate LoadSession(), CreateSession() and then
  // UpdateSession() will be called to create a session with known keys.
  // |session_id_for_emulated_loadsession_| is used to keep track of the
  // session_id allocated by aes_decryptor, as the session_id will be returned
  // as |kLoadableSessionId|. Future requests for this simulated session
  // need to use |session_id_for_emulated_loadsession_| for all calls
  // to aes_decryptor.
  // |promise_id_for_emulated_loadsession_| is used to keep track of the
  // original LoadSession() promise, as it is not resolved until the
  // UpdateSession() call succeeds.
  // |has_received_keys_change_event_for_emulated_loadsession_| is used to keep
  // track of whether a keyschange event has been received for the loadable
  // session in case it happens before the emulated session is fully created.
  // |keys_info_for_emulated_loadsession_| is used to keep track of the list
  // of keys provided as a result of calling UpdateSession() if it happens,
  // since they can't be forwarded on until the LoadSession() promise is
  // resolved.
  // TODO(xhwang): Extract testing code from main implementation.
  // See http://crbug.com/341751
  // TODO(jrummell): Once the order of events is fixed,
  // |has_received_keys_change_event_for_emulated_loadsession_| should be
  // removed (the event should have either happened or never happened).
  // |keys_info_for_emulated_loadsession_| may also go away if the event is
  // not expected. See http://crbug.com/448225
  std::string session_id_for_emulated_loadsession_;
  uint32_t promise_id_for_emulated_loadsession_;
  bool has_received_keys_change_event_for_emulated_loadsession_;
  CdmKeysInfo keys_info_for_emulated_loadsession_;

  // Timer delay in milliseconds for the next host_->SetTimer() call.
  int64_t timer_delay_ms_;

  // Indicates whether a renewal timer has been set to prevent multiple timers
  // from running.
  bool renewal_timer_set_;

#if defined(CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER)
  int channel_count_;
  int bits_per_channel_;
  int samples_per_second_;
  int64_t output_timestamp_base_in_microseconds_;
  int total_samples_generated_;
#endif  // CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER

#if defined(CLEAR_KEY_CDM_USE_FFMPEG_DECODER)
  std::unique_ptr<FFmpegCdmAudioDecoder> audio_decoder_;
#endif  // CLEAR_KEY_CDM_USE_FFMPEG_DECODER

  std::unique_ptr<CdmVideoDecoder> video_decoder_;

  std::unique_ptr<FileIOTestRunner> file_io_test_runner_;

  DISALLOW_COPY_AND_ASSIGN(ClearKeyCdm);
};

}  // namespace media

#endif  // MEDIA_CDM_PPAPI_EXTERNAL_CLEAR_KEY_CLEAR_KEY_CDM_H_
