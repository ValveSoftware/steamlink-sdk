// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/ppapi/ppapi_cdm_adapter.h"

#include <string.h>

#include "base/macros.h"
#include "build/build_config.h"
#include "media/base/limits.h"
#include "media/cdm/ppapi/cdm_file_io_impl.h"
#include "media/cdm/ppapi/cdm_logging.h"
#include "media/cdm/supported_cdm_versions.h"
#include "ppapi/c/ppb_console.h"
#include "ppapi/cpp/private/uma_private.h"

#if defined(CHECK_DOCUMENT_URL)
#include "ppapi/cpp/dev/url_util_dev.h"
#include "ppapi/cpp/instance_handle.h"
#endif  // defined(CHECK_DOCUMENT_URL)

namespace {

// Constants for UMA reporting of file size (in KB) via HistogramCustomCounts().
// Note that the histogram is log-scaled (rather than linear).
const uint32_t kSizeKBMin = 1;
const uint32_t kSizeKBMax = 512 * 1024;  // 512MB
const uint32_t kSizeKBBuckets = 100;

#if !defined(NDEBUG)
#define DLOG_TO_CONSOLE(message) LogToConsole(message);
#else
#define DLOG_TO_CONSOLE(message) (void)(message);
#endif

bool IsMainThread() {
  return pp::Module::Get()->core()->IsMainThread();
}

// Posts a task to run |cb| on the main thread. The task is posted even if the
// current thread is the main thread.
void PostOnMain(pp::CompletionCallback cb) {
  pp::Module::Get()->core()->CallOnMainThread(0, cb, PP_OK);
}

// Ensures |cb| is called on the main thread, either because the current thread
// is the main thread or by posting it to the main thread.
void CallOnMain(pp::CompletionCallback cb) {
  // TODO(tomfinegan): This is only necessary because PPAPI doesn't allow calls
  // off the main thread yet. Remove this once the change lands.
  if (IsMainThread())
    cb.Run(PP_OK);
  else
    PostOnMain(cb);
}

// Configures a cdm::InputBuffer. |subsamples| must exist as long as
// |input_buffer| is in use.
void ConfigureInputBuffer(const pp::Buffer_Dev& encrypted_buffer,
                          const PP_EncryptedBlockInfo& encrypted_block_info,
                          std::vector<cdm::SubsampleEntry>* subsamples,
                          cdm::InputBuffer* input_buffer) {
  PP_DCHECK(subsamples);
  PP_DCHECK(!encrypted_buffer.is_null());

  input_buffer->data = static_cast<uint8_t*>(encrypted_buffer.data());
  input_buffer->data_size = encrypted_block_info.data_size;
  PP_DCHECK(encrypted_buffer.size() >= input_buffer->data_size);

  PP_DCHECK(encrypted_block_info.key_id_size <=
            arraysize(encrypted_block_info.key_id));
  input_buffer->key_id_size = encrypted_block_info.key_id_size;
  input_buffer->key_id =
      input_buffer->key_id_size > 0 ? encrypted_block_info.key_id : NULL;

  PP_DCHECK(encrypted_block_info.iv_size <= arraysize(encrypted_block_info.iv));
  input_buffer->iv_size = encrypted_block_info.iv_size;
  input_buffer->iv =
      encrypted_block_info.iv_size > 0 ? encrypted_block_info.iv : NULL;

  input_buffer->num_subsamples = encrypted_block_info.num_subsamples;
  if (encrypted_block_info.num_subsamples > 0) {
    subsamples->reserve(encrypted_block_info.num_subsamples);

    for (uint32_t i = 0; i < encrypted_block_info.num_subsamples; ++i) {
      subsamples->push_back(
          cdm::SubsampleEntry(encrypted_block_info.subsamples[i].clear_bytes,
                              encrypted_block_info.subsamples[i].cipher_bytes));
    }

    input_buffer->subsamples = &(*subsamples)[0];
  }

  input_buffer->timestamp = encrypted_block_info.tracking_info.timestamp;
}

PP_DecryptResult CdmStatusToPpDecryptResult(cdm::Status status) {
  switch (status) {
    case cdm::kSuccess:
      return PP_DECRYPTRESULT_SUCCESS;
    case cdm::kNoKey:
      return PP_DECRYPTRESULT_DECRYPT_NOKEY;
    case cdm::kNeedMoreData:
      return PP_DECRYPTRESULT_NEEDMOREDATA;
    case cdm::kDecryptError:
      return PP_DECRYPTRESULT_DECRYPT_ERROR;
    case cdm::kDecodeError:
      return PP_DECRYPTRESULT_DECODE_ERROR;
    case cdm::kSessionError:
    case cdm::kDeferredInitialization:
      // kSessionError and kDeferredInitialization are only used by the
      // Initialize* methods internally and never returned. Deliver*
      // methods should never use these values.
      break;
  }

  PP_NOTREACHED();
  return PP_DECRYPTRESULT_DECRYPT_ERROR;
}

PP_DecryptedFrameFormat CdmVideoFormatToPpDecryptedFrameFormat(
    cdm::VideoFormat format) {
  switch (format) {
    case cdm::kYv12:
      return PP_DECRYPTEDFRAMEFORMAT_YV12;
    case cdm::kI420:
      return PP_DECRYPTEDFRAMEFORMAT_I420;
    default:
      return PP_DECRYPTEDFRAMEFORMAT_UNKNOWN;
  }
}

PP_DecryptedSampleFormat CdmAudioFormatToPpDecryptedSampleFormat(
    cdm::AudioFormat format) {
  switch (format) {
    case cdm::kAudioFormatU8:
      return PP_DECRYPTEDSAMPLEFORMAT_U8;
    case cdm::kAudioFormatS16:
      return PP_DECRYPTEDSAMPLEFORMAT_S16;
    case cdm::kAudioFormatS32:
      return PP_DECRYPTEDSAMPLEFORMAT_S32;
    case cdm::kAudioFormatF32:
      return PP_DECRYPTEDSAMPLEFORMAT_F32;
    case cdm::kAudioFormatPlanarS16:
      return PP_DECRYPTEDSAMPLEFORMAT_PLANAR_S16;
    case cdm::kAudioFormatPlanarF32:
      return PP_DECRYPTEDSAMPLEFORMAT_PLANAR_F32;
    default:
      return PP_DECRYPTEDSAMPLEFORMAT_UNKNOWN;
  }
}

cdm::AudioDecoderConfig::AudioCodec PpAudioCodecToCdmAudioCodec(
    PP_AudioCodec codec) {
  switch (codec) {
    case PP_AUDIOCODEC_VORBIS:
      return cdm::AudioDecoderConfig::kCodecVorbis;
    case PP_AUDIOCODEC_AAC:
      return cdm::AudioDecoderConfig::kCodecAac;
    default:
      return cdm::AudioDecoderConfig::kUnknownAudioCodec;
  }
}

cdm::VideoDecoderConfig::VideoCodec PpVideoCodecToCdmVideoCodec(
    PP_VideoCodec codec) {
  switch (codec) {
    case PP_VIDEOCODEC_VP8:
      return cdm::VideoDecoderConfig::kCodecVp8;
    case PP_VIDEOCODEC_H264:
      return cdm::VideoDecoderConfig::kCodecH264;
    case PP_VIDEOCODEC_VP9:
      return cdm::VideoDecoderConfig::kCodecVp9;
    default:
      return cdm::VideoDecoderConfig::kUnknownVideoCodec;
  }
}

cdm::VideoDecoderConfig::VideoCodecProfile PpVCProfileToCdmVCProfile(
    PP_VideoCodecProfile profile) {
  switch (profile) {
    case PP_VIDEOCODECPROFILE_NOT_NEEDED:
      return cdm::VideoDecoderConfig::kProfileNotNeeded;
    case PP_VIDEOCODECPROFILE_H264_BASELINE:
      return cdm::VideoDecoderConfig::kH264ProfileBaseline;
    case PP_VIDEOCODECPROFILE_H264_MAIN:
      return cdm::VideoDecoderConfig::kH264ProfileMain;
    case PP_VIDEOCODECPROFILE_H264_EXTENDED:
      return cdm::VideoDecoderConfig::kH264ProfileExtended;
    case PP_VIDEOCODECPROFILE_H264_HIGH:
      return cdm::VideoDecoderConfig::kH264ProfileHigh;
    case PP_VIDEOCODECPROFILE_H264_HIGH_10:
      return cdm::VideoDecoderConfig::kH264ProfileHigh10;
    case PP_VIDEOCODECPROFILE_H264_HIGH_422:
      return cdm::VideoDecoderConfig::kH264ProfileHigh422;
    case PP_VIDEOCODECPROFILE_H264_HIGH_444_PREDICTIVE:
      return cdm::VideoDecoderConfig::kH264ProfileHigh444Predictive;
    default:
      return cdm::VideoDecoderConfig::kUnknownVideoCodecProfile;
  }
}

cdm::VideoFormat PpDecryptedFrameFormatToCdmVideoFormat(
    PP_DecryptedFrameFormat format) {
  switch (format) {
    case PP_DECRYPTEDFRAMEFORMAT_YV12:
      return cdm::kYv12;
    case PP_DECRYPTEDFRAMEFORMAT_I420:
      return cdm::kI420;
    default:
      return cdm::kUnknownVideoFormat;
  }
}

cdm::StreamType PpDecryptorStreamTypeToCdmStreamType(
    PP_DecryptorStreamType stream_type) {
  switch (stream_type) {
    case PP_DECRYPTORSTREAMTYPE_AUDIO:
      return cdm::kStreamTypeAudio;
    case PP_DECRYPTORSTREAMTYPE_VIDEO:
      return cdm::kStreamTypeVideo;
  }

  PP_NOTREACHED();
  return cdm::kStreamTypeVideo;
}

cdm::SessionType PpSessionTypeToCdmSessionType(PP_SessionType session_type) {
  switch (session_type) {
    case PP_SESSIONTYPE_TEMPORARY:
      return cdm::kTemporary;
    case PP_SESSIONTYPE_PERSISTENT_LICENSE:
      return cdm::kPersistentLicense;
    case PP_SESSIONTYPE_PERSISTENT_RELEASE:
      return cdm::kPersistentKeyRelease;
  }

  PP_NOTREACHED();
  return cdm::kTemporary;
}

cdm::InitDataType PpInitDataTypeToCdmInitDataType(
    PP_InitDataType init_data_type) {
  switch (init_data_type) {
    case PP_INITDATATYPE_CENC:
      return cdm::kCenc;
    case PP_INITDATATYPE_KEYIDS:
      return cdm::kKeyIds;
    case PP_INITDATATYPE_WEBM:
      return cdm::kWebM;
  }

  PP_NOTREACHED();
  return cdm::kKeyIds;
}

PP_CdmExceptionCode CdmExceptionTypeToPpCdmExceptionType(cdm::Error error) {
  switch (error) {
    case cdm::kNotSupportedError:
      return PP_CDMEXCEPTIONCODE_NOTSUPPORTEDERROR;
    case cdm::kInvalidStateError:
      return PP_CDMEXCEPTIONCODE_INVALIDSTATEERROR;
    case cdm::kInvalidAccessError:
      return PP_CDMEXCEPTIONCODE_INVALIDACCESSERROR;
    case cdm::kQuotaExceededError:
      return PP_CDMEXCEPTIONCODE_QUOTAEXCEEDEDERROR;
    case cdm::kUnknownError:
      return PP_CDMEXCEPTIONCODE_UNKNOWNERROR;
    case cdm::kClientError:
      return PP_CDMEXCEPTIONCODE_CLIENTERROR;
    case cdm::kOutputError:
      return PP_CDMEXCEPTIONCODE_OUTPUTERROR;
  }

  PP_NOTREACHED();
  return PP_CDMEXCEPTIONCODE_UNKNOWNERROR;
}

PP_CdmMessageType CdmMessageTypeToPpMessageType(cdm::MessageType message) {
  switch (message) {
    case cdm::kLicenseRequest:
      return PP_CDMMESSAGETYPE_LICENSE_REQUEST;
    case cdm::kLicenseRenewal:
      return PP_CDMMESSAGETYPE_LICENSE_RENEWAL;
    case cdm::kLicenseRelease:
      return PP_CDMMESSAGETYPE_LICENSE_RELEASE;
  }

  PP_NOTREACHED();
  return PP_CDMMESSAGETYPE_LICENSE_REQUEST;
}

PP_CdmKeyStatus CdmKeyStatusToPpKeyStatus(cdm::KeyStatus status) {
  switch (status) {
    case cdm::kUsable:
      return PP_CDMKEYSTATUS_USABLE;
    case cdm::kInternalError:
      return PP_CDMKEYSTATUS_INVALID;
    case cdm::kExpired:
      return PP_CDMKEYSTATUS_EXPIRED;
    case cdm::kOutputRestricted:
      return PP_CDMKEYSTATUS_OUTPUTRESTRICTED;
    case cdm::kOutputDownscaled:
      return PP_CDMKEYSTATUS_OUTPUTDOWNSCALED;
    case cdm::kStatusPending:
      return PP_CDMKEYSTATUS_STATUSPENDING;
    case cdm::kReleased:
      return PP_CDMKEYSTATUS_RELEASED;
  }

  PP_NOTREACHED();
  return PP_CDMKEYSTATUS_INVALID;
}

}  // namespace

namespace media {

PpapiCdmAdapter::PpapiCdmAdapter(PP_Instance instance, pp::Module* module)
    : pp::Instance(instance),
      pp::ContentDecryptor_Private(this),
#if defined(OS_CHROMEOS)
      output_protection_(this),
      platform_verification_(this),
      output_link_mask_(0),
      output_protection_mask_(0),
      query_output_protection_in_progress_(false),
      uma_for_output_protection_query_reported_(false),
      uma_for_output_protection_positive_result_reported_(false),
#endif
      allocator_(this),
      cdm_(NULL),
      allow_distinctive_identifier_(false),
      allow_persistent_state_(false),
      deferred_initialize_audio_decoder_(false),
      deferred_audio_decoder_config_id_(0),
      deferred_initialize_video_decoder_(false),
      deferred_video_decoder_config_id_(0),
      last_read_file_size_kb_(0),
      file_size_uma_reported_(false) {
  callback_factory_.Initialize(this);
}

PpapiCdmAdapter::~PpapiCdmAdapter() {}

CdmWrapper* PpapiCdmAdapter::CreateCdmInstance(const std::string& key_system) {
  // The Pepper plugin will be staticly linked to the CDM, so pass the plugin's
  // CreateCdmInstance() to CdmWrapper.
  CdmWrapper* cdm = CdmWrapper::Create(::CreateCdmInstance, key_system.data(),
                                       key_system.size(), GetCdmHost, this);

  const std::string message = "CDM instance for " + key_system +
                              (cdm ? "" : " could not be") + " created.";
  DLOG_TO_CONSOLE(message);
  CDM_DLOG() << message;

  return cdm;
}

void PpapiCdmAdapter::Initialize(uint32_t promise_id,
                                 const std::string& key_system,
                                 bool allow_distinctive_identifier,
                                 bool allow_persistent_state) {
  PP_DCHECK(!key_system.empty());
  PP_DCHECK(!cdm_);

#if defined(CHECK_DOCUMENT_URL)
  PP_URLComponents_Dev url_components = {};
  const pp::URLUtil_Dev* url_util = pp::URLUtil_Dev::Get();
  if (!url_util) {
    RejectPromise(promise_id, cdm::kUnknownError, 0,
                  "Unable to determine origin.");
    return;
  }

  pp::Var href = url_util->GetDocumentURL(pp::InstanceHandle(pp_instance()),
                                          &url_components);
  PP_DCHECK(href.is_string());
  std::string url = href.AsString();
  PP_DCHECK(!url.empty());
  std::string url_scheme =
      url.substr(url_components.scheme.begin, url_components.scheme.len);
  if (url_scheme != "file") {
    // Skip this check for file:// URLs as they don't have a host component.
    PP_DCHECK(url_components.host.begin);
    PP_DCHECK(0 < url_components.host.len);
  }
#endif  // defined(CHECK_DOCUMENT_URL)

  cdm_ = make_linked_ptr(CreateCdmInstance(key_system));
  if (!cdm_) {
    RejectPromise(promise_id, cdm::kInvalidAccessError, 0,
                  "Unable to create CDM.");
    return;
  }

  key_system_ = key_system;
  allow_distinctive_identifier_ = allow_distinctive_identifier;
  allow_persistent_state_ = allow_persistent_state;
  cdm_->Initialize(allow_distinctive_identifier, allow_persistent_state);
  OnResolvePromise(promise_id);
}

void PpapiCdmAdapter::SetServerCertificate(
    uint32_t promise_id,
    pp::VarArrayBuffer server_certificate) {
  const uint8_t* server_certificate_ptr =
      static_cast<const uint8_t*>(server_certificate.Map());
  const uint32_t server_certificate_size = server_certificate.ByteLength();

  if (!server_certificate_ptr ||
      server_certificate_size < media::limits::kMinCertificateLength ||
      server_certificate_size > media::limits::kMaxCertificateLength) {
    RejectPromise(promise_id, cdm::kInvalidAccessError, 0,
                  "Incorrect certificate.");
    return;
  }

  cdm_->SetServerCertificate(promise_id, server_certificate_ptr,
                             server_certificate_size);
}

void PpapiCdmAdapter::CreateSessionAndGenerateRequest(
    uint32_t promise_id,
    PP_SessionType session_type,
    PP_InitDataType init_data_type,
    pp::VarArrayBuffer init_data) {
  cdm_->CreateSessionAndGenerateRequest(
      promise_id, PpSessionTypeToCdmSessionType(session_type),
      PpInitDataTypeToCdmInitDataType(init_data_type),
      static_cast<const uint8_t*>(init_data.Map()), init_data.ByteLength());
}

void PpapiCdmAdapter::LoadSession(uint32_t promise_id,
                                  PP_SessionType session_type,
                                  const std::string& session_id) {
  cdm_->LoadSession(promise_id, PpSessionTypeToCdmSessionType(session_type),
                    session_id.data(), session_id.size());
}

void PpapiCdmAdapter::UpdateSession(uint32_t promise_id,
                                    const std::string& session_id,
                                    pp::VarArrayBuffer response) {
  const uint8_t* response_ptr = static_cast<const uint8_t*>(response.Map());
  const uint32_t response_size = response.ByteLength();

  PP_DCHECK(!session_id.empty());
  PP_DCHECK(response_ptr);
  PP_DCHECK(response_size > 0);

  cdm_->UpdateSession(promise_id, session_id.data(), session_id.length(),
                      response_ptr, response_size);
}

void PpapiCdmAdapter::CloseSession(uint32_t promise_id,
                                   const std::string& session_id) {
  cdm_->CloseSession(promise_id, session_id.data(), session_id.length());
}

void PpapiCdmAdapter::RemoveSession(uint32_t promise_id,
                                    const std::string& session_id) {
  cdm_->RemoveSession(promise_id, session_id.data(), session_id.length());
}

// Note: In the following decryption/decoding related functions, errors are NOT
// reported via KeyError, but are reported via corresponding PPB calls.

void PpapiCdmAdapter::Decrypt(
    pp::Buffer_Dev encrypted_buffer,
    const PP_EncryptedBlockInfo& encrypted_block_info) {
  PP_DCHECK(!encrypted_buffer.is_null());

  // Release a buffer that the caller indicated it is finished with.
  allocator_.Release(encrypted_block_info.tracking_info.buffer_id);

  cdm::Status status = cdm::kDecryptError;
  LinkedDecryptedBlock decrypted_block(new DecryptedBlockImpl());

  if (cdm_) {
    cdm::InputBuffer input_buffer;
    std::vector<cdm::SubsampleEntry> subsamples;
    ConfigureInputBuffer(encrypted_buffer, encrypted_block_info, &subsamples,
                         &input_buffer);
    status = cdm_->Decrypt(input_buffer, decrypted_block.get());
    PP_DCHECK(status != cdm::kSuccess ||
              (decrypted_block->DecryptedBuffer() &&
               decrypted_block->DecryptedBuffer()->Size()));
  }

  CallOnMain(callback_factory_.NewCallback(&PpapiCdmAdapter::DeliverBlock,
                                           status, decrypted_block,
                                           encrypted_block_info.tracking_info));
}

void PpapiCdmAdapter::InitializeAudioDecoder(
    const PP_AudioDecoderConfig& decoder_config,
    pp::Buffer_Dev extra_data_buffer) {
  PP_DCHECK(!deferred_initialize_audio_decoder_);
  PP_DCHECK(deferred_audio_decoder_config_id_ == 0);
  cdm::Status status = cdm::kSessionError;
  if (cdm_) {
    cdm::AudioDecoderConfig cdm_decoder_config;
    cdm_decoder_config.codec =
        PpAudioCodecToCdmAudioCodec(decoder_config.codec);
    cdm_decoder_config.channel_count = decoder_config.channel_count;
    cdm_decoder_config.bits_per_channel = decoder_config.bits_per_channel;
    cdm_decoder_config.samples_per_second = decoder_config.samples_per_second;
    cdm_decoder_config.extra_data =
        static_cast<uint8_t*>(extra_data_buffer.data());
    cdm_decoder_config.extra_data_size = extra_data_buffer.size();
    status = cdm_->InitializeAudioDecoder(cdm_decoder_config);
  }

  if (status == cdm::kDeferredInitialization) {
    deferred_initialize_audio_decoder_ = true;
    deferred_audio_decoder_config_id_ = decoder_config.request_id;
    return;
  }

  CallOnMain(callback_factory_.NewCallback(
      &PpapiCdmAdapter::DecoderInitializeDone, PP_DECRYPTORSTREAMTYPE_AUDIO,
      decoder_config.request_id, status == cdm::kSuccess));
}

void PpapiCdmAdapter::InitializeVideoDecoder(
    const PP_VideoDecoderConfig& decoder_config,
    pp::Buffer_Dev extra_data_buffer) {
  PP_DCHECK(!deferred_initialize_video_decoder_);
  PP_DCHECK(deferred_video_decoder_config_id_ == 0);
  cdm::Status status = cdm::kSessionError;
  if (cdm_) {
    cdm::VideoDecoderConfig cdm_decoder_config;
    cdm_decoder_config.codec =
        PpVideoCodecToCdmVideoCodec(decoder_config.codec);
    cdm_decoder_config.profile =
        PpVCProfileToCdmVCProfile(decoder_config.profile);
    cdm_decoder_config.format =
        PpDecryptedFrameFormatToCdmVideoFormat(decoder_config.format);
    cdm_decoder_config.coded_size.width = decoder_config.width;
    cdm_decoder_config.coded_size.height = decoder_config.height;
    cdm_decoder_config.extra_data =
        static_cast<uint8_t*>(extra_data_buffer.data());
    cdm_decoder_config.extra_data_size = extra_data_buffer.size();
    status = cdm_->InitializeVideoDecoder(cdm_decoder_config);
  }

  if (status == cdm::kDeferredInitialization) {
    deferred_initialize_video_decoder_ = true;
    deferred_video_decoder_config_id_ = decoder_config.request_id;
    return;
  }

  CallOnMain(callback_factory_.NewCallback(
      &PpapiCdmAdapter::DecoderInitializeDone, PP_DECRYPTORSTREAMTYPE_VIDEO,
      decoder_config.request_id, status == cdm::kSuccess));
}

void PpapiCdmAdapter::DeinitializeDecoder(PP_DecryptorStreamType decoder_type,
                                          uint32_t request_id) {
  PP_DCHECK(cdm_);  // InitializeXxxxxDecoder should have succeeded.
  if (cdm_) {
    cdm_->DeinitializeDecoder(
        PpDecryptorStreamTypeToCdmStreamType(decoder_type));
  }

  CallOnMain(callback_factory_.NewCallback(
      &PpapiCdmAdapter::DecoderDeinitializeDone, decoder_type, request_id));
}

void PpapiCdmAdapter::ResetDecoder(PP_DecryptorStreamType decoder_type,
                                   uint32_t request_id) {
  PP_DCHECK(cdm_);  // InitializeXxxxxDecoder should have succeeded.
  if (cdm_)
    cdm_->ResetDecoder(PpDecryptorStreamTypeToCdmStreamType(decoder_type));

  CallOnMain(callback_factory_.NewCallback(&PpapiCdmAdapter::DecoderResetDone,
                                           decoder_type, request_id));
}

void PpapiCdmAdapter::DecryptAndDecode(
    PP_DecryptorStreamType decoder_type,
    pp::Buffer_Dev encrypted_buffer,
    const PP_EncryptedBlockInfo& encrypted_block_info) {
  PP_DCHECK(cdm_);  // InitializeXxxxxDecoder should have succeeded.
  // Release a buffer that the caller indicated it is finished with.
  allocator_.Release(encrypted_block_info.tracking_info.buffer_id);

  cdm::InputBuffer input_buffer;
  std::vector<cdm::SubsampleEntry> subsamples;
  if (cdm_ && !encrypted_buffer.is_null()) {
    ConfigureInputBuffer(encrypted_buffer, encrypted_block_info, &subsamples,
                         &input_buffer);
  }

  cdm::Status status = cdm::kDecodeError;

  switch (decoder_type) {
    case PP_DECRYPTORSTREAMTYPE_VIDEO: {
      LinkedVideoFrame video_frame(new VideoFrameImpl());
      if (cdm_)
        status = cdm_->DecryptAndDecodeFrame(input_buffer, video_frame.get());
      CallOnMain(callback_factory_.NewCallback(
          &PpapiCdmAdapter::DeliverFrame, status, video_frame,
          encrypted_block_info.tracking_info));
      return;
    }

    case PP_DECRYPTORSTREAMTYPE_AUDIO: {
      LinkedAudioFrames audio_frames(new AudioFramesImpl());
      if (cdm_) {
        status =
            cdm_->DecryptAndDecodeSamples(input_buffer, audio_frames.get());
      }
      CallOnMain(callback_factory_.NewCallback(
          &PpapiCdmAdapter::DeliverSamples, status, audio_frames,
          encrypted_block_info.tracking_info));
      return;
    }

    default:
      PP_NOTREACHED();
      return;
  }
}

cdm::Buffer* PpapiCdmAdapter::Allocate(uint32_t capacity) {
  return allocator_.Allocate(capacity);
}

void PpapiCdmAdapter::SetTimer(int64_t delay_ms, void* context) {
  // NOTE: doesn't really need to run on the main thread; could just as well run
  // on a helper thread if |cdm_| were thread-friendly and care was taken.  We
  // only use CallOnMainThread() here to get delayed-execution behavior.
  pp::Module::Get()->core()->CallOnMainThread(
      delay_ms,
      callback_factory_.NewCallback(&PpapiCdmAdapter::TimerExpired, context),
      PP_OK);
}

void PpapiCdmAdapter::TimerExpired(int32_t result, void* context) {
  PP_DCHECK(result == PP_OK);
  cdm_->TimerExpired(context);
}

cdm::Time PpapiCdmAdapter::GetCurrentWallTime() {
  return pp::Module::Get()->core()->GetTime();
}

void PpapiCdmAdapter::OnResolveNewSessionPromise(uint32_t promise_id,
                                                 const char* session_id,
                                                 uint32_t session_id_size) {
  PostOnMain(callback_factory_.NewCallback(
      &PpapiCdmAdapter::SendPromiseResolvedWithSessionInternal, promise_id,
      std::string(session_id, session_id_size)));
}

void PpapiCdmAdapter::OnResolvePromise(uint32_t promise_id) {
  PostOnMain(callback_factory_.NewCallback(
      &PpapiCdmAdapter::SendPromiseResolvedInternal, promise_id));
}

void PpapiCdmAdapter::OnRejectPromise(uint32_t promise_id,
                                      cdm::Error error,
                                      uint32_t system_code,
                                      const char* error_message,
                                      uint32_t error_message_size) {
  // UMA to investigate http://crbug.com/410630
  // TODO(xhwang): Remove after bug is fixed.
  if (system_code == 0x27) {
    pp::UMAPrivate uma_interface(this);
    uma_interface.HistogramCustomCounts("Media.EME.CdmFileIO.FileSizeKBOnError",
                                        last_read_file_size_kb_, kSizeKBMin,
                                        kSizeKBMax, kSizeKBBuckets);
  }

  RejectPromise(promise_id, error, system_code,
                std::string(error_message, error_message_size));
}

void PpapiCdmAdapter::RejectPromise(uint32_t promise_id,
                                    cdm::Error error,
                                    uint32_t system_code,
                                    const std::string& error_message) {
  PostOnMain(callback_factory_.NewCallback(
      &PpapiCdmAdapter::SendPromiseRejectedInternal, promise_id,
      SessionError(error, system_code, error_message)));
}

void PpapiCdmAdapter::OnSessionMessage(const char* session_id,
                                       uint32_t session_id_size,
                                       cdm::MessageType message_type,
                                       const char* message,
                                       uint32_t message_size,
                                       const char* legacy_destination_url,
                                       uint32_t legacy_destination_url_size) {
  // License requests should not specify |legacy_destination_url|.
  // |legacy_destination_url| is not passed to unprefixed EME applications,
  // so it can be removed when the prefixed API is removed.
  PP_DCHECK(legacy_destination_url_size == 0 ||
            message_type != cdm::MessageType::kLicenseRequest);

  PostOnMain(callback_factory_.NewCallback(
      &PpapiCdmAdapter::SendSessionMessageInternal,
      SessionMessage(
          std::string(session_id, session_id_size), message_type, message,
          message_size,
          std::string(legacy_destination_url, legacy_destination_url_size))));
}

void PpapiCdmAdapter::OnSessionKeysChange(const char* session_id,
                                          uint32_t session_id_size,
                                          bool has_additional_usable_key,
                                          const cdm::KeyInformation* keys_info,
                                          uint32_t keys_info_count) {
  std::vector<PP_KeyInformation> key_information;
  for (uint32_t i = 0; i < keys_info_count; ++i) {
    const auto& key_info = keys_info[i];
    PP_KeyInformation next_key = {};

    if (key_info.key_id_size > sizeof(next_key.key_id)) {
      PP_NOTREACHED();
      continue;
    }

    // Copy key_id into |next_key|.
    memcpy(next_key.key_id, key_info.key_id, key_info.key_id_size);

    // Set remaining fields on |next_key|.
    next_key.key_id_size = key_info.key_id_size;
    next_key.key_status = CdmKeyStatusToPpKeyStatus(key_info.status);
    next_key.system_code = key_info.system_code;
    key_information.push_back(next_key);
  }

  PostOnMain(callback_factory_.NewCallback(
      &PpapiCdmAdapter::SendSessionKeysChangeInternal,
      std::string(session_id, session_id_size), has_additional_usable_key,
      key_information));
}

void PpapiCdmAdapter::OnExpirationChange(const char* session_id,
                                         uint32_t session_id_size,
                                         cdm::Time new_expiry_time) {
  PostOnMain(callback_factory_.NewCallback(
      &PpapiCdmAdapter::SendExpirationChangeInternal,
      std::string(session_id, session_id_size), new_expiry_time));
}

void PpapiCdmAdapter::OnSessionClosed(const char* session_id,
                                      uint32_t session_id_size) {
  PostOnMain(
      callback_factory_.NewCallback(&PpapiCdmAdapter::SendSessionClosedInternal,
                                    std::string(session_id, session_id_size)));
}

void PpapiCdmAdapter::OnLegacySessionError(const char* session_id,
                                           uint32_t session_id_size,
                                           cdm::Error error,
                                           uint32_t system_code,
                                           const char* error_message,
                                           uint32_t error_message_size) {
  PostOnMain(callback_factory_.NewCallback(
      &PpapiCdmAdapter::SendSessionErrorInternal,
      std::string(session_id, session_id_size),
      SessionError(error, system_code,
                   std::string(error_message, error_message_size))));
}

// Helpers to pass the event to Pepper.

void PpapiCdmAdapter::SendPromiseResolvedInternal(int32_t result,
                                                  uint32_t promise_id) {
  PP_DCHECK(result == PP_OK);
  pp::ContentDecryptor_Private::PromiseResolved(promise_id);
}

void PpapiCdmAdapter::SendPromiseResolvedWithSessionInternal(
    int32_t result,
    uint32_t promise_id,
    const std::string& session_id) {
  PP_DCHECK(result == PP_OK);
  pp::ContentDecryptor_Private::PromiseResolvedWithSession(promise_id,
                                                           session_id);
}

void PpapiCdmAdapter::SendPromiseRejectedInternal(int32_t result,
                                                  uint32_t promise_id,
                                                  const SessionError& error) {
  PP_DCHECK(result == PP_OK);
  pp::ContentDecryptor_Private::PromiseRejected(
      promise_id, CdmExceptionTypeToPpCdmExceptionType(error.error),
      error.system_code, error.error_description);
}

void PpapiCdmAdapter::SendSessionMessageInternal(
    int32_t result,
    const SessionMessage& message) {
  PP_DCHECK(result == PP_OK);

  pp::VarArrayBuffer message_array_buffer(message.message.size());
  if (message.message.size() > 0) {
    memcpy(message_array_buffer.Map(), message.message.data(),
           message.message.size());
  }

  pp::ContentDecryptor_Private::SessionMessage(
      message.session_id, CdmMessageTypeToPpMessageType(message.message_type),
      message_array_buffer, message.legacy_destination_url);
}

void PpapiCdmAdapter::SendSessionClosedInternal(int32_t result,
                                                const std::string& session_id) {
  PP_DCHECK(result == PP_OK);
  pp::ContentDecryptor_Private::SessionClosed(session_id);
}

void PpapiCdmAdapter::SendSessionErrorInternal(int32_t result,
                                               const std::string& session_id,
                                               const SessionError& error) {
  PP_DCHECK(result == PP_OK);
  pp::ContentDecryptor_Private::LegacySessionError(
      session_id, CdmExceptionTypeToPpCdmExceptionType(error.error),
      error.system_code, error.error_description);
}

void PpapiCdmAdapter::SendSessionKeysChangeInternal(
    int32_t result,
    const std::string& session_id,
    bool has_additional_usable_key,
    const std::vector<PP_KeyInformation>& key_info) {
  PP_DCHECK(result == PP_OK);
  pp::ContentDecryptor_Private::SessionKeysChange(
      session_id, has_additional_usable_key, key_info);
}

void PpapiCdmAdapter::SendExpirationChangeInternal(
    int32_t result,
    const std::string& session_id,
    cdm::Time new_expiry_time) {
  PP_DCHECK(result == PP_OK);
  pp::ContentDecryptor_Private::SessionExpirationChange(session_id,
                                                        new_expiry_time);
}

void PpapiCdmAdapter::DeliverBlock(
    int32_t result,
    const cdm::Status& status,
    const LinkedDecryptedBlock& decrypted_block,
    const PP_DecryptTrackingInfo& tracking_info) {
  PP_DCHECK(result == PP_OK);
  PP_DecryptedBlockInfo decrypted_block_info = {};
  decrypted_block_info.tracking_info = tracking_info;
  decrypted_block_info.tracking_info.timestamp = decrypted_block->Timestamp();
  decrypted_block_info.tracking_info.buffer_id = 0;
  decrypted_block_info.data_size = 0;
  decrypted_block_info.result = CdmStatusToPpDecryptResult(status);

  pp::Buffer_Dev buffer;

  if (decrypted_block_info.result == PP_DECRYPTRESULT_SUCCESS) {
    PP_DCHECK(decrypted_block.get() && decrypted_block->DecryptedBuffer());
    if (!decrypted_block.get() || !decrypted_block->DecryptedBuffer()) {
      PP_NOTREACHED();
      decrypted_block_info.result = PP_DECRYPTRESULT_DECRYPT_ERROR;
    } else {
      PpbBuffer* ppb_buffer =
          static_cast<PpbBuffer*>(decrypted_block->DecryptedBuffer());
      decrypted_block_info.tracking_info.buffer_id = ppb_buffer->buffer_id();
      decrypted_block_info.data_size = ppb_buffer->Size();

      buffer = ppb_buffer->TakeBuffer();
    }
  }

  pp::ContentDecryptor_Private::DeliverBlock(buffer, decrypted_block_info);
}

void PpapiCdmAdapter::DecoderInitializeDone(int32_t result,
                                            PP_DecryptorStreamType decoder_type,
                                            uint32_t request_id,
                                            bool success) {
  PP_DCHECK(result == PP_OK);
  pp::ContentDecryptor_Private::DecoderInitializeDone(decoder_type, request_id,
                                                      success);
}

void PpapiCdmAdapter::DecoderDeinitializeDone(
    int32_t result,
    PP_DecryptorStreamType decoder_type,
    uint32_t request_id) {
  pp::ContentDecryptor_Private::DecoderDeinitializeDone(decoder_type,
                                                        request_id);
}

void PpapiCdmAdapter::DecoderResetDone(int32_t result,
                                       PP_DecryptorStreamType decoder_type,
                                       uint32_t request_id) {
  pp::ContentDecryptor_Private::DecoderResetDone(decoder_type, request_id);
}

void PpapiCdmAdapter::DeliverFrame(
    int32_t result,
    const cdm::Status& status,
    const LinkedVideoFrame& video_frame,
    const PP_DecryptTrackingInfo& tracking_info) {
  PP_DCHECK(result == PP_OK);
  PP_DecryptedFrameInfo decrypted_frame_info = {};
  decrypted_frame_info.tracking_info.request_id = tracking_info.request_id;
  decrypted_frame_info.tracking_info.buffer_id = 0;
  decrypted_frame_info.result = CdmStatusToPpDecryptResult(status);

  pp::Buffer_Dev buffer;

  if (decrypted_frame_info.result == PP_DECRYPTRESULT_SUCCESS) {
    if (!IsValidVideoFrame(video_frame)) {
      PP_NOTREACHED();
      decrypted_frame_info.result = PP_DECRYPTRESULT_DECODE_ERROR;
    } else {
      PpbBuffer* ppb_buffer =
          static_cast<PpbBuffer*>(video_frame->FrameBuffer());

      decrypted_frame_info.tracking_info.timestamp = video_frame->Timestamp();
      decrypted_frame_info.tracking_info.buffer_id = ppb_buffer->buffer_id();
      decrypted_frame_info.format =
          CdmVideoFormatToPpDecryptedFrameFormat(video_frame->Format());
      decrypted_frame_info.width = video_frame->Size().width;
      decrypted_frame_info.height = video_frame->Size().height;
      decrypted_frame_info.plane_offsets[PP_DECRYPTEDFRAMEPLANES_Y] =
          video_frame->PlaneOffset(cdm::VideoFrame::kYPlane);
      decrypted_frame_info.plane_offsets[PP_DECRYPTEDFRAMEPLANES_U] =
          video_frame->PlaneOffset(cdm::VideoFrame::kUPlane);
      decrypted_frame_info.plane_offsets[PP_DECRYPTEDFRAMEPLANES_V] =
          video_frame->PlaneOffset(cdm::VideoFrame::kVPlane);
      decrypted_frame_info.strides[PP_DECRYPTEDFRAMEPLANES_Y] =
          video_frame->Stride(cdm::VideoFrame::kYPlane);
      decrypted_frame_info.strides[PP_DECRYPTEDFRAMEPLANES_U] =
          video_frame->Stride(cdm::VideoFrame::kUPlane);
      decrypted_frame_info.strides[PP_DECRYPTEDFRAMEPLANES_V] =
          video_frame->Stride(cdm::VideoFrame::kVPlane);

      buffer = ppb_buffer->TakeBuffer();
    }
  }

  pp::ContentDecryptor_Private::DeliverFrame(buffer, decrypted_frame_info);
}

void PpapiCdmAdapter::DeliverSamples(
    int32_t result,
    const cdm::Status& status,
    const LinkedAudioFrames& audio_frames,
    const PP_DecryptTrackingInfo& tracking_info) {
  PP_DCHECK(result == PP_OK);

  PP_DecryptedSampleInfo decrypted_sample_info = {};
  decrypted_sample_info.tracking_info = tracking_info;
  decrypted_sample_info.tracking_info.timestamp = 0;
  decrypted_sample_info.tracking_info.buffer_id = 0;
  decrypted_sample_info.data_size = 0;
  decrypted_sample_info.result = CdmStatusToPpDecryptResult(status);

  pp::Buffer_Dev buffer;

  if (decrypted_sample_info.result == PP_DECRYPTRESULT_SUCCESS) {
    PP_DCHECK(audio_frames.get() && audio_frames->FrameBuffer());
    if (!audio_frames.get() || !audio_frames->FrameBuffer()) {
      PP_NOTREACHED();
      decrypted_sample_info.result = PP_DECRYPTRESULT_DECRYPT_ERROR;
    } else {
      PpbBuffer* ppb_buffer =
          static_cast<PpbBuffer*>(audio_frames->FrameBuffer());

      decrypted_sample_info.tracking_info.buffer_id = ppb_buffer->buffer_id();
      decrypted_sample_info.data_size = ppb_buffer->Size();
      decrypted_sample_info.format =
          CdmAudioFormatToPpDecryptedSampleFormat(audio_frames->Format());

      buffer = ppb_buffer->TakeBuffer();
    }
  }

  pp::ContentDecryptor_Private::DeliverSamples(buffer, decrypted_sample_info);
}

bool PpapiCdmAdapter::IsValidVideoFrame(const LinkedVideoFrame& video_frame) {
  if (!video_frame.get() || !video_frame->FrameBuffer() ||
      (video_frame->Format() != cdm::kI420 &&
       video_frame->Format() != cdm::kYv12)) {
    CDM_DLOG() << "Invalid video frame!";
    return false;
  }

  PpbBuffer* ppb_buffer = static_cast<PpbBuffer*>(video_frame->FrameBuffer());

  for (uint32_t i = 0; i < cdm::VideoFrame::kMaxPlanes; ++i) {
    int plane_height = (i == cdm::VideoFrame::kYPlane)
                           ? video_frame->Size().height
                           : (video_frame->Size().height + 1) / 2;
    cdm::VideoFrame::VideoPlane plane =
        static_cast<cdm::VideoFrame::VideoPlane>(i);
    if (ppb_buffer->Size() < video_frame->PlaneOffset(plane) +
                                 plane_height * video_frame->Stride(plane)) {
      return false;
    }
  }

  return true;
}

void PpapiCdmAdapter::OnFirstFileRead(int32_t file_size_bytes) {
  PP_DCHECK(IsMainThread());
  PP_DCHECK(file_size_bytes >= 0);

  last_read_file_size_kb_ = file_size_bytes / 1024;

  if (file_size_uma_reported_)
    return;

  pp::UMAPrivate uma_interface(this);
  uma_interface.HistogramCustomCounts(
      "Media.EME.CdmFileIO.FileSizeKBOnFirstRead", last_read_file_size_kb_,
      kSizeKBMin, kSizeKBMax, kSizeKBBuckets);
  file_size_uma_reported_ = true;
}

#if !defined(NDEBUG)
void PpapiCdmAdapter::LogToConsole(const pp::Var& value) {
  PP_DCHECK(IsMainThread());
  const PPB_Console* console = reinterpret_cast<const PPB_Console*>(
      pp::Module::Get()->GetBrowserInterface(PPB_CONSOLE_INTERFACE));
  console->Log(pp_instance(), PP_LOGLEVEL_LOG, value.pp_var());
}
#endif  // !defined(NDEBUG)

void PpapiCdmAdapter::SendPlatformChallenge(const char* service_id,
                                            uint32_t service_id_size,
                                            const char* challenge,
                                            uint32_t challenge_size) {
#if defined(OS_CHROMEOS)
  // If access to a distinctive identifier is not allowed, block platform
  // verification to prevent access to such an identifier.
  if (allow_distinctive_identifier_) {
    pp::VarArrayBuffer challenge_var(challenge_size);
    uint8_t* var_data = static_cast<uint8_t*>(challenge_var.Map());
    memcpy(var_data, challenge, challenge_size);

    std::string service_id_str(service_id, service_id_size);

    linked_ptr<PepperPlatformChallengeResponse> response(
        new PepperPlatformChallengeResponse());

    int32_t result = platform_verification_.ChallengePlatform(
        pp::Var(service_id_str), challenge_var, &response->signed_data,
        &response->signed_data_signature, &response->platform_key_certificate,
        callback_factory_.NewCallback(
            &PpapiCdmAdapter::SendPlatformChallengeDone, response));
    challenge_var.Unmap();
    if (result == PP_OK_COMPLETIONPENDING)
      return;

    // Fall through on error and issue an empty OnPlatformChallengeResponse().
    PP_DCHECK(result != PP_OK);
  }
#endif

  cdm::PlatformChallengeResponse platform_challenge_response = {};
  cdm_->OnPlatformChallengeResponse(platform_challenge_response);
}

void PpapiCdmAdapter::EnableOutputProtection(uint32_t desired_protection_mask) {
#if defined(OS_CHROMEOS)
  int32_t result = output_protection_.EnableProtection(
      desired_protection_mask,
      callback_factory_.NewCallback(&PpapiCdmAdapter::EnableProtectionDone));

  // Errors are ignored since clients must call QueryOutputProtectionStatus() to
  // inspect the protection status on a regular basis.

  if (result != PP_OK && result != PP_OK_COMPLETIONPENDING)
    CDM_DLOG() << __FUNCTION__ << " failed!";
#endif
}

void PpapiCdmAdapter::QueryOutputProtectionStatus() {
#if defined(OS_CHROMEOS)
  PP_DCHECK(!query_output_protection_in_progress_);

  output_link_mask_ = output_protection_mask_ = 0;
  const int32_t result = output_protection_.QueryStatus(
      &output_link_mask_, &output_protection_mask_,
      callback_factory_.NewCallback(
          &PpapiCdmAdapter::QueryOutputProtectionStatusDone));
  if (result == PP_OK_COMPLETIONPENDING) {
    query_output_protection_in_progress_ = true;
    ReportOutputProtectionQuery();
    return;
  }

  // Fall through on error and issue an empty OnQueryOutputProtectionStatus().
  PP_DCHECK(result != PP_OK);
  CDM_DLOG() << __FUNCTION__ << " failed, result = " << result;
#endif
  cdm_->OnQueryOutputProtectionStatus(cdm::kQueryFailed, 0, 0);
}

void PpapiCdmAdapter::OnDeferredInitializationDone(cdm::StreamType stream_type,
                                                   cdm::Status decoder_status) {
  switch (stream_type) {
    case cdm::kStreamTypeAudio:
      PP_DCHECK(deferred_initialize_audio_decoder_);
      CallOnMain(callback_factory_.NewCallback(
          &PpapiCdmAdapter::DecoderInitializeDone, PP_DECRYPTORSTREAMTYPE_AUDIO,
          deferred_audio_decoder_config_id_, decoder_status == cdm::kSuccess));
      deferred_initialize_audio_decoder_ = false;
      deferred_audio_decoder_config_id_ = 0;
      break;
    case cdm::kStreamTypeVideo:
      PP_DCHECK(deferred_initialize_video_decoder_);
      CallOnMain(callback_factory_.NewCallback(
          &PpapiCdmAdapter::DecoderInitializeDone, PP_DECRYPTORSTREAMTYPE_VIDEO,
          deferred_video_decoder_config_id_, decoder_status == cdm::kSuccess));
      deferred_initialize_video_decoder_ = false;
      deferred_video_decoder_config_id_ = 0;
      break;
  }
}

// The CDM owns the returned object and must call FileIO::Close() to release it.
cdm::FileIO* PpapiCdmAdapter::CreateFileIO(cdm::FileIOClient* client) {
  if (!allow_persistent_state_) {
    CDM_DLOG()
        << "Cannot create FileIO because persistent state is not allowed.";
    return nullptr;
  }

  return new CdmFileIOImpl(
      client, pp_instance(),
      callback_factory_.NewCallback(&PpapiCdmAdapter::OnFirstFileRead));
}

#if defined(OS_CHROMEOS)
void PpapiCdmAdapter::ReportOutputProtectionUMA(OutputProtectionStatus status) {
  pp::UMAPrivate uma_interface(this);
  uma_interface.HistogramEnumeration("Media.EME.OutputProtection", status,
                                     OUTPUT_PROTECTION_MAX);
}

void PpapiCdmAdapter::ReportOutputProtectionQuery() {
  if (uma_for_output_protection_query_reported_)
    return;

  ReportOutputProtectionUMA(OUTPUT_PROTECTION_QUERIED);
  uma_for_output_protection_query_reported_ = true;
}

void PpapiCdmAdapter::ReportOutputProtectionQueryResult() {
  if (uma_for_output_protection_positive_result_reported_)
    return;

  // Report UMAs for output protection query result.
  uint32_t external_links = (output_link_mask_ & ~cdm::kLinkTypeInternal);

  if (!external_links) {
    ReportOutputProtectionUMA(OUTPUT_PROTECTION_NO_EXTERNAL_LINK);
    uma_for_output_protection_positive_result_reported_ = true;
    return;
  }

  const uint32_t kProtectableLinks =
      cdm::kLinkTypeHDMI | cdm::kLinkTypeDVI | cdm::kLinkTypeDisplayPort;
  bool is_unprotectable_link_connected = external_links & ~kProtectableLinks;
  bool is_hdcp_enabled_on_all_protectable_links =
      output_protection_mask_ & cdm::kProtectionHDCP;

  if (!is_unprotectable_link_connected &&
      is_hdcp_enabled_on_all_protectable_links) {
    ReportOutputProtectionUMA(OUTPUT_PROTECTION_ALL_EXTERNAL_LINKS_PROTECTED);
    uma_for_output_protection_positive_result_reported_ = true;
    return;
  }

  // Do not report a negative result because it could be a false negative.
  // Instead, we will calculate number of negatives using the total number of
  // queries and success results.
}

void PpapiCdmAdapter::SendPlatformChallengeDone(
    int32_t result,
    const linked_ptr<PepperPlatformChallengeResponse>& response) {
  if (result != PP_OK) {
    CDM_DLOG() << __FUNCTION__ << ": Platform challenge failed!";
    cdm::PlatformChallengeResponse platform_challenge_response = {};
    cdm_->OnPlatformChallengeResponse(platform_challenge_response);
    return;
  }

  pp::VarArrayBuffer signed_data_var(response->signed_data);
  pp::VarArrayBuffer signed_data_signature_var(response->signed_data_signature);
  std::string platform_key_certificate_string =
      response->platform_key_certificate.AsString();

  cdm::PlatformChallengeResponse platform_challenge_response = {
      static_cast<uint8_t*>(signed_data_var.Map()),
      signed_data_var.ByteLength(),
      static_cast<uint8_t*>(signed_data_signature_var.Map()),
      signed_data_signature_var.ByteLength(),
      reinterpret_cast<const uint8_t*>(platform_key_certificate_string.data()),
      static_cast<uint32_t>(platform_key_certificate_string.length())};
  cdm_->OnPlatformChallengeResponse(platform_challenge_response);

  signed_data_var.Unmap();
  signed_data_signature_var.Unmap();
}

void PpapiCdmAdapter::EnableProtectionDone(int32_t result) {
  // Does nothing since clients must call QueryOutputProtectionStatus() to
  // inspect the protection status on a regular basis.
  CDM_DLOG() << __FUNCTION__ << " : " << result;
}

void PpapiCdmAdapter::QueryOutputProtectionStatusDone(int32_t result) {
  PP_DCHECK(query_output_protection_in_progress_);
  query_output_protection_in_progress_ = false;

  // Return a query status of failed on error.
  cdm::QueryResult query_result;
  if (result != PP_OK) {
    CDM_DLOG() << __FUNCTION__ << " failed, result = " << result;
    output_link_mask_ = output_protection_mask_ = 0;
    query_result = cdm::kQueryFailed;
  } else {
    query_result = cdm::kQuerySucceeded;
    ReportOutputProtectionQueryResult();
  }

  cdm_->OnQueryOutputProtectionStatus(query_result, output_link_mask_,
                                      output_protection_mask_);
}
#endif

PpapiCdmAdapter::SessionError::SessionError(
    cdm::Error error,
    uint32_t system_code,
    const std::string& error_description)
    : error(error),
      system_code(system_code),
      error_description(error_description) {}

PpapiCdmAdapter::SessionMessage::SessionMessage(
    const std::string& session_id,
    cdm::MessageType message_type,
    const char* message,
    uint32_t message_size,
    const std::string& legacy_destination_url)
    : session_id(session_id),
      message_type(message_type),
      message(message, message + message_size),
      legacy_destination_url(legacy_destination_url) {}

void* GetCdmHost(int host_interface_version, void* user_data) {
  if (!host_interface_version || !user_data)
    return NULL;

  static_assert(
      cdm::ContentDecryptionModule::Host::kVersion == cdm::Host_8::kVersion,
      "update the code below");

  // Ensure IsSupportedCdmHostVersion matches implementation of this function.
  // Always update this DCHECK when updating this function.
  // If this check fails, update this function and DCHECK or update
  // IsSupportedCdmHostVersion.

  PP_DCHECK(
      // Future version is not supported.
      !IsSupportedCdmHostVersion(cdm::Host_8::kVersion + 1) &&
      // Current version is supported.
      IsSupportedCdmHostVersion(cdm::Host_8::kVersion) &&
      // Include all previous supported versions (if any) here.
      IsSupportedCdmHostVersion(cdm::Host_7::kVersion) &&
      // One older than the oldest supported version is not supported.
      !IsSupportedCdmHostVersion(cdm::Host_7::kVersion - 1));
  PP_DCHECK(IsSupportedCdmHostVersion(host_interface_version));

  PpapiCdmAdapter* cdm_adapter = static_cast<PpapiCdmAdapter*>(user_data);
  CDM_DLOG() << "Create CDM Host with version " << host_interface_version;
  switch (host_interface_version) {
    case cdm::Host_8::kVersion:
      return static_cast<cdm::Host_8*>(cdm_adapter);
    case cdm::Host_7::kVersion:
      return static_cast<cdm::Host_7*>(cdm_adapter);
    default:
      PP_NOTREACHED();
      return NULL;
  }
}

// This object is the global object representing this plugin library as long
// as it is loaded.
class PpapiCdmAdapterModule : public pp::Module {
 public:
  PpapiCdmAdapterModule() : pp::Module() {
    // This function blocks the renderer thread (PluginInstance::Initialize()).
    // Move this call to other places if this may be a concern in the future.
    INITIALIZE_CDM_MODULE();
  }
  virtual ~PpapiCdmAdapterModule() { DeinitializeCdmModule(); }

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new PpapiCdmAdapter(instance, this);
  }

 private:
  CdmFileIOImpl::ResourceTracker cdm_file_io_impl_resource_tracker;
};

}  // namespace media

namespace pp {

// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new media::PpapiCdmAdapterModule();
}

}  // namespace pp
