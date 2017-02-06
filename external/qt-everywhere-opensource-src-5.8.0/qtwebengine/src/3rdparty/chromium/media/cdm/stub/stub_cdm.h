// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_STUB_STUB_CDM_H_
#define MEDIA_CDM_STUB_STUB_CDM_H_

#include <stdint.h>

#include "base/macros.h"
#include "media/cdm/api/content_decryption_module.h"

namespace media {

typedef cdm::ContentDecryptionModule_8 StubCdmInterface;

// Dummy implementation of the cdm::ContentDecryptionModule interface.
class StubCdm : public StubCdmInterface {
 public:
  explicit StubCdm(Host* host);
  ~StubCdm() override;

  // StubCdmInterface implementation.
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
  // Helper function that rejects the promise specified by |promise_id|.
  void FailRequest(uint32_t promise_id);

  Host* host_;

  uint32_t next_session_id_;

  DISALLOW_COPY_AND_ASSIGN(StubCdm);
};

}  // namespace media

#endif  // MEDIA_CDM_STUB_STUB_CDM_H_
