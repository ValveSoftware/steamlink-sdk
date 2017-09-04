// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/stub/stub_cdm.h"

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"

// Version number for this stub. The third number represents the
// cdm::ContentDecryptionModule version.
const char kStubCdmVersion[] = "1.4.8.0";

void INITIALIZE_CDM_MODULE() {
}

void DeinitializeCdmModule() {
}

void* CreateCdmInstance(int cdm_interface_version,
                        const char* /* key_system */,
                        uint32_t /* key_system_size */,
                        GetCdmHostFunc get_cdm_host_func,
                        void* user_data) {
  DVLOG(1) << "CreateCdmInstance()";

  if (cdm_interface_version != media::StubCdmInterface::kVersion)
    return nullptr;

  media::StubCdmInterface::Host* host =
      static_cast<media::StubCdmInterface::Host*>(get_cdm_host_func(
          media::StubCdmInterface::Host::kVersion, user_data));
  if (!host)
    return nullptr;

  return new media::StubCdm(host);
}

const char* GetCdmVersion() {
  return kStubCdmVersion;
}

namespace media {

StubCdm::StubCdm(Host* host) : host_(host), next_session_id_(0) {
}

StubCdm::~StubCdm() {
}

void StubCdm::Initialize(bool /* allow_distinctive_identifier */,
                         bool /* allow_persistent_state */) {
}

void StubCdm::CreateSessionAndGenerateRequest(
    uint32_t promise_id,
    cdm::SessionType /* session_type */,
    cdm::InitDataType /* init_data_type */,
    const uint8_t* /* init_data */,
    uint32_t /* init_data_size */) {
  // Provide a dummy message (with a trivial session ID) to enable some testing
  // and be consistent with existing testing without a license server.
  std::string session_id(base::UintToString(next_session_id_++));
  host_->OnResolveNewSessionPromise(
      promise_id, session_id.data(),
      base::checked_cast<uint32_t>(session_id.length()));
  host_->OnSessionMessage(session_id.data(),
                          base::checked_cast<uint32_t>(session_id.length()),
                          cdm::kLicenseRequest, nullptr, 0, nullptr, 0);
}

void StubCdm::LoadSession(uint32_t promise_id,
                          cdm::SessionType /* session_type */,
                          const char* /* session_id */,
                          uint32_t /* session_id_length */) {
  FailRequest(promise_id);
}

void StubCdm::UpdateSession(uint32_t promise_id,
                            const char* /* session_id */,
                            uint32_t /* session_id_length */,
                            const uint8_t* /* response */,
                            uint32_t /* response_size */) {
  FailRequest(promise_id);
}

void StubCdm::CloseSession(uint32_t promise_id,
                           const char* /* session_id */,
                           uint32_t /* session_id_length */) {
  FailRequest(promise_id);
}

void StubCdm::RemoveSession(uint32_t promise_id,
                            const char* /* session_id */,
                            uint32_t /* session_id_length */) {
  FailRequest(promise_id);
}

void StubCdm::SetServerCertificate(
    uint32_t promise_id,
    const uint8_t* /* server_certificate_data */,
    uint32_t /* server_certificate_data_size */) {
  FailRequest(promise_id);
}

void StubCdm::TimerExpired(void* /* context */) {
}

cdm::Status StubCdm::Decrypt(const cdm::InputBuffer& /* encrypted_buffer */,
                             cdm::DecryptedBlock* /* decrypted_block */) {
  return cdm::kDecryptError;
}

cdm::Status StubCdm::InitializeAudioDecoder(
    const cdm::AudioDecoderConfig& /* audio_decoder_config */) {
  return cdm::kDecryptError;
}

cdm::Status StubCdm::InitializeVideoDecoder(
    const cdm::VideoDecoderConfig& /* video_decoder_config */) {
  return cdm::kDecryptError;
}

void StubCdm::ResetDecoder(cdm::StreamType /* decoder_type */) {
}

void StubCdm::DeinitializeDecoder(cdm::StreamType /* decoder_type */) {
}

cdm::Status StubCdm::DecryptAndDecodeFrame(
    const cdm::InputBuffer& /* encrypted_buffer */,
    cdm::VideoFrame* /* decoded_frame */) {
  return cdm::kDecryptError;
}

cdm::Status StubCdm::DecryptAndDecodeSamples(
    const cdm::InputBuffer& /* encrypted_buffer */,
    cdm::AudioFrames* /* audio_frames */) {
  return cdm::kDecryptError;
}

void StubCdm::Destroy() {
  delete this;
}

void StubCdm::OnPlatformChallengeResponse(
    const cdm::PlatformChallengeResponse& /* response */) {
  NOTREACHED();
}

void StubCdm::OnQueryOutputProtectionStatus(
    cdm::QueryResult /* result */,
    uint32_t /* link_mask */,
    uint32_t /* output_protection_mask */) {
  NOTREACHED();
};

void StubCdm::FailRequest(uint32_t promise_id) {
  std::string message("Operation not supported by stub CDM.");
  host_->OnRejectPromise(promise_id, cdm::kInvalidAccessError, 0,
                         message.data(),
                         base::checked_cast<uint32_t>(message.length()));
}

}  // namespace media
