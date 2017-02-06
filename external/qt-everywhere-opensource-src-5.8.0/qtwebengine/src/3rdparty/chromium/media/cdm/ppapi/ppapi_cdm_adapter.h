// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_PPAPI_PPAPI_CDM_ADAPTER_H_
#define MEDIA_CDM_PPAPI_PPAPI_CDM_ADAPTER_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "base/macros.h"
#include "media/cdm/api/content_decryption_module.h"
#include "media/cdm/cdm_helpers.h"
#include "media/cdm/cdm_wrapper.h"
#include "media/cdm/ppapi/linked_ptr.h"
#include "media/cdm/ppapi/ppapi_cdm_buffer.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/private/pp_content_decryptor.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/private/content_decryptor_private.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_array_buffer.h"
#include "ppapi/utility/completion_callback_factory.h"

#if defined(OS_CHROMEOS)
#include "ppapi/cpp/private/output_protection_private.h"
#include "ppapi/cpp/private/platform_verification.h"
#endif

namespace media {

// GetCdmHostFunc implementation.
void* GetCdmHost(int host_interface_version, void* user_data);

// An adapter class for abstracting away PPAPI interaction and threading for a
// Content Decryption Module (CDM).
class PpapiCdmAdapter : public pp::Instance,
                        public pp::ContentDecryptor_Private,
                        public cdm::Host_7,
                        public cdm::Host_8 {
 public:
  PpapiCdmAdapter(PP_Instance instance, pp::Module* module);
  virtual ~PpapiCdmAdapter();

  // pp::Instance implementation.
  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    return true;
  }

  // PPP_ContentDecryptor_Private implementation.
  // Note: Results of calls to these methods must be reported through the
  // PPB_ContentDecryptor_Private interface.
  void Initialize(uint32_t promise_id,
                  const std::string& key_system,
                  bool allow_distinctive_identifier,
                  bool allow_persistent_state) override;
  void SetServerCertificate(uint32_t promise_id,
                            pp::VarArrayBuffer server_certificate) override;
  void CreateSessionAndGenerateRequest(uint32_t promise_id,
                                       PP_SessionType session_type,
                                       PP_InitDataType init_data_type,
                                       pp::VarArrayBuffer init_data) override;
  void LoadSession(uint32_t promise_id,
                   PP_SessionType session_type,
                   const std::string& session_id) override;
  void UpdateSession(uint32_t promise_id,
                     const std::string& session_id,
                     pp::VarArrayBuffer response) override;
  void CloseSession(uint32_t promise_id, const std::string& session_id);
  void RemoveSession(uint32_t promise_id,
                     const std::string& session_id) override;
  void Decrypt(pp::Buffer_Dev encrypted_buffer,
               const PP_EncryptedBlockInfo& encrypted_block_info) override;
  void InitializeAudioDecoder(const PP_AudioDecoderConfig& decoder_config,
                              pp::Buffer_Dev extra_data_buffer) override;
  void InitializeVideoDecoder(const PP_VideoDecoderConfig& decoder_config,
                              pp::Buffer_Dev extra_data_buffer) override;
  void DeinitializeDecoder(PP_DecryptorStreamType decoder_type,
                           uint32_t request_id) override;
  void ResetDecoder(PP_DecryptorStreamType decoder_type,
                    uint32_t request_id) override;
  void DecryptAndDecode(
      PP_DecryptorStreamType decoder_type,
      pp::Buffer_Dev encrypted_buffer,
      const PP_EncryptedBlockInfo& encrypted_block_info) override;

  // cdm::Host_7 and cdm::Host_8 implementation.
  cdm::Buffer* Allocate(uint32_t capacity) override;
  void SetTimer(int64_t delay_ms, void* context) override;
  cdm::Time GetCurrentWallTime() override;
  void OnResolveNewSessionPromise(uint32_t promise_id,
                                  const char* session_id,
                                  uint32_t session_id_size) override;
  void OnResolvePromise(uint32_t promise_id) override;
  void OnRejectPromise(uint32_t promise_id,
                       cdm::Error error,
                       uint32_t system_code,
                       const char* error_message,
                       uint32_t error_message_size) override;
  void OnSessionMessage(const char* session_id,
                        uint32_t session_id_size,
                        cdm::MessageType message_type,
                        const char* message,
                        uint32_t message_size,
                        const char* legacy_destination_url,
                        uint32_t legacy_destination_url_size) override;
  void OnSessionKeysChange(const char* session_id,
                           uint32_t session_id_size,
                           bool has_additional_usable_key,
                           const cdm::KeyInformation* keys_info,
                           uint32_t keys_info_count) override;
  void OnExpirationChange(const char* session_id,
                          uint32_t session_id_size,
                          cdm::Time new_expiry_time) override;
  void OnSessionClosed(const char* session_id,
                       uint32_t session_id_size) override;
  void OnLegacySessionError(const char* session_id,
                            uint32_t session_id_size,
                            cdm::Error error,
                            uint32_t system_code,
                            const char* error_message,
                            uint32_t error_message_size) override;
  void SendPlatformChallenge(const char* service_id,
                             uint32_t service_id_size,
                             const char* challenge,
                             uint32_t challenge_size) override;
  void EnableOutputProtection(uint32_t desired_protection_mask) override;
  void QueryOutputProtectionStatus() override;
  void OnDeferredInitializationDone(cdm::StreamType stream_type,
                                    cdm::Status decoder_status) override;
  cdm::FileIO* CreateFileIO(cdm::FileIOClient* client) override;

 private:
  // These are reported to UMA server. Do not change the existing values!
  enum OutputProtectionStatus {
    OUTPUT_PROTECTION_QUERIED = 0,
    OUTPUT_PROTECTION_NO_EXTERNAL_LINK = 1,
    OUTPUT_PROTECTION_ALL_EXTERNAL_LINKS_PROTECTED = 2,
    OUTPUT_PROTECTION_MAX = 3
  };

  typedef linked_ptr<DecryptedBlockImpl> LinkedDecryptedBlock;
  typedef linked_ptr<VideoFrameImpl> LinkedVideoFrame;
  typedef linked_ptr<AudioFramesImpl> LinkedAudioFrames;

  struct SessionError {
    SessionError(cdm::Error error,
                 uint32_t system_code,
                 const std::string& error_description);
    cdm::Error error;
    uint32_t system_code;
    std::string error_description;
  };

  struct SessionMessage {
    SessionMessage(const std::string& session_id,
                   cdm::MessageType message_type,
                   const char* message,
                   uint32_t message_size,
                   const std::string& legacy_destination_url);
    std::string session_id;
    cdm::MessageType message_type;
    std::vector<uint8_t> message;
    std::string legacy_destination_url;
  };

  // Create an instance of the |key_system| CDM. Caller owns the returned
  // pointer. On error (unable to load CDM, does not support |key_system|,
  // does not support an supported interface, etc.) NULL will be returned.
  CdmWrapper* CreateCdmInstance(const std::string& key_system);

  // <code>PPB_ContentDecryptor_Private</code> dispatchers. These are passed to
  // <code>callback_factory_</code> to ensure that calls into
  // <code>PPP_ContentDecryptor_Private</code> are asynchronous.
  void SendPromiseResolvedInternal(int32_t result, uint32_t promise_id);
  void SendPromiseResolvedWithSessionInternal(int32_t result,
                                              uint32_t promise_id,
                                              const std::string& session_id);
  void SendPromiseRejectedInternal(int32_t result,
                                   uint32_t promise_id,
                                   const SessionError& error);
  void SendSessionMessageInternal(int32_t result,
                                  const SessionMessage& message);
  void SendSessionClosedInternal(int32_t result, const std::string& session_id);
  void SendSessionErrorInternal(int32_t result,
                                const std::string& session_id,
                                const SessionError& error);
  void SendSessionKeysChangeInternal(
      int32_t result,
      const std::string& session_id,
      bool has_additional_usable_key,
      const std::vector<PP_KeyInformation>& key_info);
  void SendExpirationChangeInternal(int32_t result,
                                    const std::string& session_id,
                                    cdm::Time new_expiry_time);
  void RejectPromise(uint32_t promise_id,
                     cdm::Error error,
                     uint32_t system_code,
                     const std::string& error_message);

  void DeliverBlock(int32_t result,
                    const cdm::Status& status,
                    const LinkedDecryptedBlock& decrypted_block,
                    const PP_DecryptTrackingInfo& tracking_info);
  void DecoderInitializeDone(int32_t result,
                             PP_DecryptorStreamType decoder_type,
                             uint32_t request_id,
                             bool success);
  void DecoderDeinitializeDone(int32_t result,
                               PP_DecryptorStreamType decoder_type,
                               uint32_t request_id);
  void DecoderResetDone(int32_t result,
                        PP_DecryptorStreamType decoder_type,
                        uint32_t request_id);
  void DeliverFrame(int32_t result,
                    const cdm::Status& status,
                    const LinkedVideoFrame& video_frame,
                    const PP_DecryptTrackingInfo& tracking_info);
  void DeliverSamples(int32_t result,
                      const cdm::Status& status,
                      const LinkedAudioFrames& audio_frames,
                      const PP_DecryptTrackingInfo& tracking_info);

  // Helper for SetTimer().
  void TimerExpired(int32_t result, void* context);

  bool IsValidVideoFrame(const LinkedVideoFrame& video_frame);

  // Callback to report |file_size_bytes| of the first file read by FileIO.
  void OnFirstFileRead(int32_t file_size_bytes);

#if !defined(NDEBUG)
  // Logs the given message to the JavaScript console associated with the
  // CDM adapter instance. The name of the CDM adapter issuing the log message
  // will be automatically prepended to the message.
  void LogToConsole(const pp::Var& value);
#endif  // !defined(NDEBUG)

#if defined(OS_CHROMEOS)
  void ReportOutputProtectionUMA(OutputProtectionStatus status);
  void ReportOutputProtectionQuery();
  void ReportOutputProtectionQueryResult();

  struct PepperPlatformChallengeResponse {
    pp::Var signed_data;
    pp::Var signed_data_signature;
    pp::Var platform_key_certificate;
  };

  void SendPlatformChallengeDone(
      int32_t result,
      const linked_ptr<PepperPlatformChallengeResponse>& response);
  void EnableProtectionDone(int32_t result);
  void QueryOutputProtectionStatusDone(int32_t result);

  pp::OutputProtection_Private output_protection_;
  pp::PlatformVerification platform_verification_;

  // Same as above, these are only read by QueryOutputProtectionStatusDone().
  uint32_t output_link_mask_;
  uint32_t output_protection_mask_;
  bool query_output_protection_in_progress_;

  // Tracks whether an output protection query and a positive query result (no
  // unprotected external link) have been reported to UMA.
  bool uma_for_output_protection_query_reported_;
  bool uma_for_output_protection_positive_result_reported_;
#endif

  PpbBufferAllocator allocator_;
  pp::CompletionCallbackFactory<PpapiCdmAdapter> callback_factory_;
  linked_ptr<CdmWrapper> cdm_;
  std::string key_system_;
  bool allow_distinctive_identifier_;
  bool allow_persistent_state_;

  // If the CDM returned kDeferredInitialization during InitializeAudioDecoder()
  // or InitializeVideoDecoder(), the (Audio|Video)DecoderConfig.request_id is
  // saved for the future call to OnDeferredInitializationDone().
  bool deferred_initialize_audio_decoder_;
  uint32_t deferred_audio_decoder_config_id_;
  bool deferred_initialize_video_decoder_;
  uint32_t deferred_video_decoder_config_id_;

  uint32_t last_read_file_size_kb_;
  bool file_size_uma_reported_;

  DISALLOW_COPY_AND_ASSIGN(PpapiCdmAdapter);
};

}  // namespace media

#endif  // MEDIA_CDM_PPAPI_PPAPI_CDM_ADAPTER_H_
