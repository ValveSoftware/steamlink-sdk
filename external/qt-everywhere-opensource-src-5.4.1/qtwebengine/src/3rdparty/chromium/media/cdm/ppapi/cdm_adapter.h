// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_PPAPI_CDM_ADAPTER_H_
#define MEDIA_CDM_PPAPI_CDM_ADAPTER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "media/cdm/ppapi/api/content_decryption_module.h"
#include "media/cdm/ppapi/cdm_helpers.h"
#include "media/cdm/ppapi/cdm_wrapper.h"
#include "media/cdm/ppapi/linked_ptr.h"
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

#if defined(GetCurrentTime)
// winbase.h defines this which messes up calls to Host_5::GetCurrentTime.
#undef GetCurrentTime
#endif

namespace media {

// GetCdmHostFunc implementation.
void* GetCdmHost(int host_interface_version, void* user_data);

// An adapter class for abstracting away PPAPI interaction and threading for a
// Content Decryption Module (CDM).
class CdmAdapter : public pp::Instance,
                   public pp::ContentDecryptor_Private,
                   public cdm::Host_4,
                   public cdm::Host_5 {
 public:
  CdmAdapter(PP_Instance instance, pp::Module* module);
  virtual ~CdmAdapter();

  // pp::Instance implementation.
  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    return true;
  }

  // PPP_ContentDecryptor_Private implementation.
  // Note: Results of calls to these methods must be reported through the
  // PPB_ContentDecryptor_Private interface.
  virtual void Initialize(const std::string& key_system) OVERRIDE;
  virtual void CreateSession(uint32_t promise_id,
                             const std::string& init_data_type,
                             pp::VarArrayBuffer init_data,
                             PP_SessionType session_type) OVERRIDE;
  virtual void LoadSession(uint32_t promise_id,
                           const std::string& web_session_id) OVERRIDE;
  virtual void UpdateSession(uint32_t promise_id,
                             const std::string& web_session_id,
                             pp::VarArrayBuffer response) OVERRIDE;
  virtual void ReleaseSession(uint32_t promise_id,
                              const std::string& web_session_id) OVERRIDE;
  virtual void Decrypt(
      pp::Buffer_Dev encrypted_buffer,
      const PP_EncryptedBlockInfo& encrypted_block_info) OVERRIDE;
  virtual void InitializeAudioDecoder(
      const PP_AudioDecoderConfig& decoder_config,
      pp::Buffer_Dev extra_data_buffer) OVERRIDE;
  virtual void InitializeVideoDecoder(
      const PP_VideoDecoderConfig& decoder_config,
      pp::Buffer_Dev extra_data_buffer) OVERRIDE;
  virtual void DeinitializeDecoder(PP_DecryptorStreamType decoder_type,
                                   uint32_t request_id) OVERRIDE;
  virtual void ResetDecoder(PP_DecryptorStreamType decoder_type,
                            uint32_t request_id) OVERRIDE;
  virtual void DecryptAndDecode(
      PP_DecryptorStreamType decoder_type,
      pp::Buffer_Dev encrypted_buffer,
      const PP_EncryptedBlockInfo& encrypted_block_info) OVERRIDE;

  // cdm::Host_4 and cdm::Host_5 implementation.
  virtual cdm::Buffer* Allocate(uint32_t capacity) OVERRIDE;
  virtual void SetTimer(int64_t delay_ms, void* context) OVERRIDE;

  // cdm::Host_4 implementation.
  virtual double GetCurrentWallTimeInSeconds() OVERRIDE;
  virtual void OnSessionCreated(uint32_t session_id,
                                const char* web_session_id,
                                uint32_t web_session_id_length) OVERRIDE;
  virtual void OnSessionMessage(uint32_t session_id,
                                const char* message,
                                uint32_t message_length,
                                const char* destination_url,
                                uint32_t destination_url_length) OVERRIDE;
  virtual void OnSessionReady(uint32_t session_id) OVERRIDE;
  virtual void OnSessionClosed(uint32_t session_id) OVERRIDE;
  virtual void OnSessionError(uint32_t session_id,
                              cdm::MediaKeyError error_code,
                              uint32_t system_code) OVERRIDE;

  // cdm::Host_5 implementation.
  virtual cdm::Time GetCurrentTime() OVERRIDE;
  virtual void OnResolveNewSessionPromise(
      uint32_t promise_id,
      const char* web_session_id,
      uint32_t web_session_id_length) OVERRIDE;
  virtual void OnResolvePromise(uint32_t promise_id) OVERRIDE;
  virtual void OnRejectPromise(uint32_t promise_id,
                               cdm::Error error,
                               uint32_t system_code,
                               const char* error_message,
                               uint32_t error_message_length) OVERRIDE;
  virtual void OnSessionMessage(const char* web_session_id,
                                uint32_t web_session_id_length,
                                const char* message,
                                uint32_t message_length,
                                const char* destination_url,
                                uint32_t destination_url_length) OVERRIDE;
  virtual void OnSessionKeysChange(const char* web_session_id,
                                   uint32_t web_session_id_length,
                                   bool has_additional_usable_key);
  virtual void OnExpirationChange(const char* web_session_id,
                                  uint32_t web_session_id_length,
                                  cdm::Time new_expiry_time);
  virtual void OnSessionReady(const char* web_session_id,
                              uint32_t web_session_id_length) OVERRIDE;
  virtual void OnSessionClosed(const char* web_session_id,
                               uint32_t web_session_id_length) OVERRIDE;
  virtual void OnSessionError(const char* web_session_id,
                              uint32_t web_session_id_length,
                              cdm::Error error,
                              uint32_t system_code,
                              const char* error_message,
                              uint32_t error_message_length) OVERRIDE;

  // cdm::Host_4 and cdm::Host_5 implementation.
  virtual void SendPlatformChallenge(const char* service_id,
                                     uint32_t service_id_length,
                                     const char* challenge,
                                     uint32_t challenge_length) OVERRIDE;
  virtual void EnableOutputProtection(
      uint32_t desired_protection_mask) OVERRIDE;
  virtual void QueryOutputProtectionStatus() OVERRIDE;
  virtual void OnDeferredInitializationDone(
      cdm::StreamType stream_type,
      cdm::Status decoder_status) OVERRIDE;
  virtual cdm::FileIO* CreateFileIO(cdm::FileIOClient* client) OVERRIDE;

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
                 std::string error_description);
    cdm::Error error;
    uint32_t system_code;
    std::string error_description;
  };

  bool CreateCdmInstance(const std::string& key_system);

  // <code>PPB_ContentDecryptor_Private</code> dispatchers. These are passed to
  // <code>callback_factory_</code> to ensure that calls into
  // <code>PPP_ContentDecryptor_Private</code> are asynchronous.
  void SendPromiseResolvedInternal(int32_t result, uint32_t promise_id);
  void SendPromiseResolvedWithSessionInternal(
      int32_t result,
      uint32_t promise_id,
      const std::string& web_session_id);
  void SendPromiseRejectedInternal(int32_t result,
                                   uint32_t promise_id,
                                   const SessionError& error);
  void SendSessionMessageInternal(int32_t result,
                                  const std::string& web_session_id,
                                  const std::vector<uint8>& message,
                                  const std::string& destination_url);
  void SendSessionReadyInternal(int32_t result,
                                const std::string& web_session_id);
  void SendSessionClosedInternal(int32_t result,
                                 const std::string& web_session_id);
  void SendSessionErrorInternal(int32_t result,
                                const std::string& web_session_id,
                                const SessionError& error);
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

  void SendPlatformChallengeDone(int32_t result);
  void EnableProtectionDone(int32_t result);
  void QueryOutputProtectionStatusDone(int32_t result);

  pp::OutputProtection_Private output_protection_;
  pp::PlatformVerification platform_verification_;

  // Since PPAPI doesn't provide handlers for CompletionCallbacks with more than
  // one output we need to manage our own.  These values are only read by
  // SendPlatformChallengeDone().
  pp::Var signed_data_output_;
  pp::Var signed_data_signature_output_;
  pp::Var platform_key_certificate_output_;
  bool challenge_in_progress_;

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
  pp::CompletionCallbackFactory<CdmAdapter> callback_factory_;
  linked_ptr<CdmWrapper> cdm_;
  std::string key_system_;

  // If the CDM returned kDeferredInitialization during InitializeAudioDecoder()
  // or InitializeVideoDecoder(), the (Audio|Video)DecoderConfig.request_id is
  // saved for the future call to OnDeferredInitializationDone().
  bool deferred_initialize_audio_decoder_;
  uint32_t deferred_audio_decoder_config_id_;
  bool deferred_initialize_video_decoder_;
  uint32_t deferred_video_decoder_config_id_;

  DISALLOW_COPY_AND_ASSIGN(CdmAdapter);
};

}  // namespace media

#endif  // MEDIA_CDM_PPAPI_CDM_ADAPTER_H_
