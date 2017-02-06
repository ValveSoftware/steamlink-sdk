// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cdm_adapter.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/cdm_initialized_promise.h"
#include "media/base/cdm_key_information.h"
#include "media/base/channel_layout.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/limits.h"
#include "media/base/sample_format.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/cdm/cdm_allocator.h"
#include "media/cdm/cdm_file_io.h"
#include "media/cdm/cdm_helpers.h"
#include "media/cdm/cdm_wrapper.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

namespace {

cdm::SessionType ToCdmSessionType(MediaKeys::SessionType session_type) {
  switch (session_type) {
    case MediaKeys::TEMPORARY_SESSION:
      return cdm::kTemporary;
    case MediaKeys::PERSISTENT_LICENSE_SESSION:
      return cdm::kPersistentLicense;
    case MediaKeys::PERSISTENT_RELEASE_MESSAGE_SESSION:
      return cdm::kPersistentKeyRelease;
  }

  NOTREACHED() << "Unexpected SessionType " << session_type;
  return cdm::kTemporary;
}

cdm::InitDataType ToCdmInitDataType(EmeInitDataType init_data_type) {
  switch (init_data_type) {
    case EmeInitDataType::CENC:
      return cdm::kCenc;
    case EmeInitDataType::KEYIDS:
      return cdm::kKeyIds;
    case EmeInitDataType::WEBM:
      return cdm::kWebM;
    case EmeInitDataType::UNKNOWN:
      break;
  }

  NOTREACHED();
  return cdm::kKeyIds;
}

MediaKeys::Exception ToMediaExceptionType(cdm::Error error) {
  switch (error) {
    case cdm::kNotSupportedError:
      return MediaKeys::NOT_SUPPORTED_ERROR;
    case cdm::kInvalidStateError:
      return MediaKeys::INVALID_STATE_ERROR;
    case cdm::kInvalidAccessError:
      return MediaKeys::INVALID_ACCESS_ERROR;
    case cdm::kQuotaExceededError:
      return MediaKeys::QUOTA_EXCEEDED_ERROR;
    case cdm::kUnknownError:
      return MediaKeys::UNKNOWN_ERROR;
    case cdm::kClientError:
      return MediaKeys::CLIENT_ERROR;
    case cdm::kOutputError:
      return MediaKeys::OUTPUT_ERROR;
  }

  NOTREACHED() << "Unexpected cdm::Error " << error;
  return MediaKeys::UNKNOWN_ERROR;
}

MediaKeys::MessageType ToMediaMessageType(cdm::MessageType message_type) {
  switch (message_type) {
    case cdm::kLicenseRequest:
      return MediaKeys::LICENSE_REQUEST;
    case cdm::kLicenseRenewal:
      return MediaKeys::LICENSE_RENEWAL;
    case cdm::kLicenseRelease:
      return MediaKeys::LICENSE_RELEASE;
  }

  NOTREACHED() << "Unexpected cdm::MessageType " << message_type;
  return MediaKeys::LICENSE_REQUEST;
}

CdmKeyInformation::KeyStatus ToCdmKeyInformationKeyStatus(
    cdm::KeyStatus status) {
  switch (status) {
    case cdm::kUsable:
      return CdmKeyInformation::USABLE;
    case cdm::kInternalError:
      return CdmKeyInformation::INTERNAL_ERROR;
    case cdm::kExpired:
      return CdmKeyInformation::EXPIRED;
    case cdm::kOutputRestricted:
      return CdmKeyInformation::OUTPUT_RESTRICTED;
    case cdm::kOutputDownscaled:
      return CdmKeyInformation::OUTPUT_DOWNSCALED;
    case cdm::kStatusPending:
      return CdmKeyInformation::KEY_STATUS_PENDING;
    case cdm::kReleased:
      return CdmKeyInformation::RELEASED;
  }

  NOTREACHED() << "Unexpected cdm::KeyStatus " << status;
  return CdmKeyInformation::INTERNAL_ERROR;
}

cdm::AudioDecoderConfig::AudioCodec ToCdmAudioCodec(AudioCodec codec) {
  switch (codec) {
    case kCodecVorbis:
      return cdm::AudioDecoderConfig::kCodecVorbis;
    case kCodecAAC:
      return cdm::AudioDecoderConfig::kCodecAac;
    default:
      DVLOG(1) << "Unsupported AudioCodec " << codec;
      return cdm::AudioDecoderConfig::kUnknownAudioCodec;
  }
}

cdm::VideoDecoderConfig::VideoCodec ToCdmVideoCodec(VideoCodec codec) {
  switch (codec) {
    case kCodecVP8:
      return cdm::VideoDecoderConfig::kCodecVp8;
    case kCodecH264:
      return cdm::VideoDecoderConfig::kCodecH264;
    case kCodecVP9:
      return cdm::VideoDecoderConfig::kCodecVp9;
    default:
      DVLOG(1) << "Unsupported VideoCodec " << codec;
      return cdm::VideoDecoderConfig::kUnknownVideoCodec;
  }
}

cdm::VideoDecoderConfig::VideoCodecProfile ToCdmVideoCodecProfile(
    VideoCodecProfile profile) {
  switch (profile) {
    case VP8PROFILE_ANY:
    // TODO(servolk): See crbug.com/592074. We'll need to update this code to
    // handle different VP9 profiles properly after adding VP9 profiles in
    // media/cdm/api/content_decryption_module.h in a separate CL.
    // For now return kProfileNotNeeded to avoid breaking unit tests.
    case VP9PROFILE_PROFILE0:
    case VP9PROFILE_PROFILE1:
    case VP9PROFILE_PROFILE2:
    case VP9PROFILE_PROFILE3:
      return cdm::VideoDecoderConfig::kProfileNotNeeded;
    case H264PROFILE_BASELINE:
      return cdm::VideoDecoderConfig::kH264ProfileBaseline;
    case H264PROFILE_MAIN:
      return cdm::VideoDecoderConfig::kH264ProfileMain;
    case H264PROFILE_EXTENDED:
      return cdm::VideoDecoderConfig::kH264ProfileExtended;
    case H264PROFILE_HIGH:
      return cdm::VideoDecoderConfig::kH264ProfileHigh;
    case H264PROFILE_HIGH10PROFILE:
      return cdm::VideoDecoderConfig::kH264ProfileHigh10;
    case H264PROFILE_HIGH422PROFILE:
      return cdm::VideoDecoderConfig::kH264ProfileHigh422;
    case H264PROFILE_HIGH444PREDICTIVEPROFILE:
      return cdm::VideoDecoderConfig::kH264ProfileHigh444Predictive;
    default:
      DVLOG(1) << "Unsupported VideoCodecProfile " << profile;
      return cdm::VideoDecoderConfig::kUnknownVideoCodecProfile;
  }
}

cdm::VideoFormat ToCdmVideoFormat(VideoPixelFormat format) {
  switch (format) {
    case PIXEL_FORMAT_YV12:
      return cdm::kYv12;
    case PIXEL_FORMAT_I420:
      return cdm::kI420;
    default:
      DVLOG(1) << "Unsupported VideoPixelFormat " << format;
      return cdm::kUnknownVideoFormat;
  }
}

cdm::StreamType ToCdmStreamType(Decryptor::StreamType stream_type) {
  switch (stream_type) {
    case Decryptor::kAudio:
      return cdm::kStreamTypeAudio;
    case Decryptor::kVideo:
      return cdm::kStreamTypeVideo;
  }

  NOTREACHED() << "Unexpected Decryptor::StreamType " << stream_type;
  return cdm::kStreamTypeVideo;
}

Decryptor::Status ToMediaDecryptorStatus(cdm::Status status) {
  switch (status) {
    case cdm::kSuccess:
      return Decryptor::kSuccess;
    case cdm::kNoKey:
      return Decryptor::kNoKey;
    case cdm::kNeedMoreData:
      return Decryptor::kNeedMoreData;
    case cdm::kDecryptError:
      return Decryptor::kError;
    case cdm::kDecodeError:
      return Decryptor::kError;
    case cdm::kSessionError:
    case cdm::kDeferredInitialization:
      break;
  }

  NOTREACHED() << "Unexpected cdm::Status " << status;
  return Decryptor::kError;
}

SampleFormat ToMediaSampleFormat(cdm::AudioFormat format) {
  switch (format) {
    case cdm::kAudioFormatU8:
      return kSampleFormatU8;
    case cdm::kAudioFormatS16:
      return kSampleFormatS16;
    case cdm::kAudioFormatS32:
      return kSampleFormatS32;
    case cdm::kAudioFormatF32:
      return kSampleFormatF32;
    case cdm::kAudioFormatPlanarS16:
      return kSampleFormatPlanarS16;
    case cdm::kAudioFormatPlanarF32:
      return kSampleFormatPlanarF32;
    case cdm::kUnknownAudioFormat:
      return kUnknownSampleFormat;
  }

  NOTREACHED() << "Unexpected cdm::AudioFormat " << format;
  return kUnknownSampleFormat;
}

// Fill |input_buffer| based on the values in |encrypted|. |subsamples|
// is used to hold some of the data. |input_buffer| will contain pointers
// to data contained in |encrypted| and |subsamples|, so the lifetime of
// |input_buffer| must be <= the lifetime of |encrypted| and |subsamples|.
void ToCdmInputBuffer(const scoped_refptr<DecoderBuffer>& encrypted_buffer,
                      std::vector<cdm::SubsampleEntry>* subsamples,
                      cdm::InputBuffer* input_buffer) {
  // End of stream buffers are represented as empty resources.
  DCHECK(!input_buffer->data);
  if (encrypted_buffer->end_of_stream())
    return;

  input_buffer->data = encrypted_buffer->data();
  input_buffer->data_size = encrypted_buffer->data_size();

  const DecryptConfig* decrypt_config = encrypted_buffer->decrypt_config();
  input_buffer->key_id =
      reinterpret_cast<const uint8_t*>(decrypt_config->key_id().data());
  input_buffer->key_id_size = decrypt_config->key_id().size();
  input_buffer->iv =
      reinterpret_cast<const uint8_t*>(decrypt_config->iv().data());
  input_buffer->iv_size = decrypt_config->iv().size();

  DCHECK(subsamples->empty());
  size_t num_subsamples = decrypt_config->subsamples().size();
  if (num_subsamples > 0) {
    subsamples->reserve(num_subsamples);
    for (const auto& sample : decrypt_config->subsamples()) {
      subsamples->push_back(
          cdm::SubsampleEntry(sample.clear_bytes, sample.cypher_bytes));
    }
  }

  input_buffer->subsamples = subsamples->data();
  input_buffer->num_subsamples = num_subsamples;
  input_buffer->timestamp = encrypted_buffer->timestamp().InMicroseconds();
}

void* GetCdmHost(int host_interface_version, void* user_data) {
  if (!host_interface_version || !user_data)
    return nullptr;

  static_assert(
      cdm::ContentDecryptionModule::Host::kVersion == cdm::Host_8::kVersion,
      "update the code below");

  // Ensure IsSupportedCdmHostVersion matches implementation of this function.
  // Always update this DCHECK when updating this function.
  // If this check fails, update this function and DCHECK or update
  // IsSupportedCdmHostVersion.

  DCHECK(
      // Future version is not supported.
      !IsSupportedCdmHostVersion(cdm::Host_8::kVersion + 1) &&
      // Current version is supported.
      IsSupportedCdmHostVersion(cdm::Host_8::kVersion) &&
      // Include all previous supported versions (if any) here.
      IsSupportedCdmHostVersion(cdm::Host_7::kVersion) &&
      // One older than the oldest supported version is not supported.
      !IsSupportedCdmHostVersion(cdm::Host_7::kVersion - 1));
  DCHECK(IsSupportedCdmHostVersion(host_interface_version));

  CdmAdapter* cdm_adapter = static_cast<CdmAdapter*>(user_data);
  DVLOG(1) << "Create CDM Host with version " << host_interface_version;
  switch (host_interface_version) {
    case cdm::Host_8::kVersion:
      return static_cast<cdm::Host_8*>(cdm_adapter);
    case cdm::Host_7::kVersion:
      return static_cast<cdm::Host_7*>(cdm_adapter);
    default:
      NOTREACHED() << "Unexpected host interface version "
                   << host_interface_version;
      return nullptr;
  }
}

}  // namespace

// static
void CdmAdapter::Create(
    const std::string& key_system,
    const base::FilePath& cdm_path,
    const CdmConfig& cdm_config,
    std::unique_ptr<CdmAllocator> allocator,
    const CreateCdmFileIOCB& create_cdm_file_io_cb,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const LegacySessionErrorCB& legacy_session_error_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb,
    const CdmCreatedCB& cdm_created_cb) {
  DCHECK(!key_system.empty());
  DCHECK(!session_message_cb.is_null());
  DCHECK(!session_closed_cb.is_null());
  DCHECK(!legacy_session_error_cb.is_null());
  DCHECK(!session_keys_change_cb.is_null());
  DCHECK(!session_expiration_update_cb.is_null());

  scoped_refptr<CdmAdapter> cdm = new CdmAdapter(
      key_system, cdm_config, std::move(allocator), create_cdm_file_io_cb,
      session_message_cb, session_closed_cb, legacy_session_error_cb,
      session_keys_change_cb, session_expiration_update_cb);

  // |cdm| ownership passed to the promise.
  std::unique_ptr<CdmInitializedPromise> cdm_created_promise(
      new CdmInitializedPromise(cdm_created_cb, cdm));

  cdm->Initialize(cdm_path, std::move(cdm_created_promise));
}

CdmAdapter::CdmAdapter(
    const std::string& key_system,
    const CdmConfig& cdm_config,
    std::unique_ptr<CdmAllocator> allocator,
    const CreateCdmFileIOCB& create_cdm_file_io_cb,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const LegacySessionErrorCB& legacy_session_error_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb)
    : key_system_(key_system),
      cdm_config_(cdm_config),
      session_message_cb_(session_message_cb),
      session_closed_cb_(session_closed_cb),
      legacy_session_error_cb_(legacy_session_error_cb),
      session_keys_change_cb_(session_keys_change_cb),
      session_expiration_update_cb_(session_expiration_update_cb),
      audio_samples_per_second_(0),
      audio_channel_layout_(CHANNEL_LAYOUT_NONE),
      allocator_(std::move(allocator)),
      create_cdm_file_io_cb_(create_cdm_file_io_cb),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_factory_(this) {
  DCHECK(!key_system_.empty());
  DCHECK(!session_message_cb_.is_null());
  DCHECK(!session_closed_cb_.is_null());
  DCHECK(!legacy_session_error_cb_.is_null());
  DCHECK(!session_keys_change_cb_.is_null());
  DCHECK(!session_expiration_update_cb_.is_null());
  DCHECK(allocator_);
}

CdmAdapter::~CdmAdapter() {}

CdmWrapper* CdmAdapter::CreateCdmInstance(const std::string& key_system,
                                          const base::FilePath& cdm_path) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // TODO(jrummell): We need to call INITIALIZE_CDM_MODULE() and
  // DeinitializeCdmModule(). However, that should only be done once for the
  // library.
  base::NativeLibraryLoadError error;
  library_.Reset(base::LoadNativeLibrary(cdm_path, &error));
  if (!library_.is_valid()) {
    DVLOG(1) << "CDM instance for " + key_system + " could not be created. "
             << error.ToString();
    return nullptr;
  }

  CreateCdmFunc create_cdm_func = reinterpret_cast<CreateCdmFunc>(
      library_.GetFunctionPointer("CreateCdmInstance"));
  if (!create_cdm_func) {
    DVLOG(1) << "No CreateCdmInstance() in library for " + key_system;
    return nullptr;
  }

  CdmWrapper* cdm = CdmWrapper::Create(create_cdm_func, key_system.data(),
                                       key_system.size(), GetCdmHost, this);

  DVLOG(1) << "CDM instance for " + key_system + (cdm ? "" : " could not be") +
                  " created.";
  return cdm;
}

void CdmAdapter::Initialize(const base::FilePath& cdm_path,
                            std::unique_ptr<media::SimpleCdmPromise> promise) {
  cdm_.reset(CreateCdmInstance(key_system_, cdm_path));
  if (!cdm_) {
    promise->reject(MediaKeys::INVALID_ACCESS_ERROR, 0,
                    "Unable to create CDM.");
    return;
  }

  cdm_->Initialize(cdm_config_.allow_distinctive_identifier,
                   cdm_config_.allow_persistent_state);
  promise->resolve();
}

void CdmAdapter::SetServerCertificate(
    const std::vector<uint8_t>& certificate,
    std::unique_ptr<SimpleCdmPromise> promise) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (certificate.size() < limits::kMinCertificateLength ||
      certificate.size() > limits::kMaxCertificateLength) {
    promise->reject(MediaKeys::INVALID_ACCESS_ERROR, 0,
                    "Incorrect certificate.");
    return;
  }

  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  cdm_->SetServerCertificate(promise_id, certificate.data(),
                             certificate.size());
}

void CdmAdapter::CreateSessionAndGenerateRequest(
    SessionType session_type,
    EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data,
    std::unique_ptr<NewSessionCdmPromise> promise) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  cdm_->CreateSessionAndGenerateRequest(
      promise_id, ToCdmSessionType(session_type),
      ToCdmInitDataType(init_data_type), init_data.data(), init_data.size());
}

void CdmAdapter::LoadSession(SessionType session_type,
                             const std::string& session_id,
                             std::unique_ptr<NewSessionCdmPromise> promise) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  cdm_->LoadSession(promise_id, ToCdmSessionType(session_type),
                    session_id.data(), session_id.size());
}

void CdmAdapter::UpdateSession(const std::string& session_id,
                               const std::vector<uint8_t>& response,
                               std::unique_ptr<SimpleCdmPromise> promise) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!session_id.empty());
  DCHECK(!response.empty());

  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  cdm_->UpdateSession(promise_id, session_id.data(), session_id.size(),
                      response.data(), response.size());
}

void CdmAdapter::CloseSession(const std::string& session_id,
                              std::unique_ptr<SimpleCdmPromise> promise) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!session_id.empty());

  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  cdm_->CloseSession(promise_id, session_id.data(), session_id.size());
}

void CdmAdapter::RemoveSession(const std::string& session_id,
                               std::unique_ptr<SimpleCdmPromise> promise) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!session_id.empty());

  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  cdm_->RemoveSession(promise_id, session_id.data(), session_id.size());
}

CdmContext* CdmAdapter::GetCdmContext() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return this;
}

Decryptor* CdmAdapter::GetDecryptor() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return this;
}

int CdmAdapter::GetCdmId() const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return kInvalidCdmId;
}

void CdmAdapter::RegisterNewKeyCB(StreamType stream_type,
                                  const NewKeyCB& key_added_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  switch (stream_type) {
    case kAudio:
      new_audio_key_cb_ = key_added_cb;
      return;
    case kVideo:
      new_video_key_cb_ = key_added_cb;
      return;
  }

  NOTREACHED() << "Unexpected StreamType " << stream_type;
}

void CdmAdapter::Decrypt(StreamType stream_type,
                         const scoped_refptr<DecoderBuffer>& encrypted,
                         const DecryptCB& decrypt_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  cdm::InputBuffer input_buffer;
  std::vector<cdm::SubsampleEntry> subsamples;
  std::unique_ptr<DecryptedBlockImpl> decrypted_block(new DecryptedBlockImpl());

  ToCdmInputBuffer(encrypted, &subsamples, &input_buffer);
  cdm::Status status = cdm_->Decrypt(input_buffer, decrypted_block.get());

  if (status != cdm::kSuccess) {
    DVLOG(1) << __FUNCTION__ << " failed with cdm::Error " << status;
    decrypt_cb.Run(ToMediaDecryptorStatus(status), nullptr);
    return;
  }

  scoped_refptr<DecoderBuffer> decrypted_buffer(
      DecoderBuffer::CopyFrom(decrypted_block->DecryptedBuffer()->Data(),
                              decrypted_block->DecryptedBuffer()->Size()));
  decrypted_buffer->set_timestamp(
      base::TimeDelta::FromMicroseconds(decrypted_block->Timestamp()));
  decrypt_cb.Run(Decryptor::kSuccess, decrypted_buffer);
}

void CdmAdapter::CancelDecrypt(StreamType stream_type) {
  // As the Decrypt methods are synchronous, nothing can be done here.
  DCHECK(task_runner_->BelongsToCurrentThread());
}

void CdmAdapter::InitializeAudioDecoder(const AudioDecoderConfig& config,
                                        const DecoderInitCB& init_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(audio_init_cb_.is_null());

  cdm::AudioDecoderConfig cdm_decoder_config;
  cdm_decoder_config.codec = ToCdmAudioCodec(config.codec());
  cdm_decoder_config.channel_count =
      ChannelLayoutToChannelCount(config.channel_layout());
  cdm_decoder_config.bits_per_channel = config.bits_per_channel();
  cdm_decoder_config.samples_per_second = config.samples_per_second();
  cdm_decoder_config.extra_data =
      const_cast<uint8_t*>(config.extra_data().data());
  cdm_decoder_config.extra_data_size = config.extra_data().size();

  cdm::Status status = cdm_->InitializeAudioDecoder(cdm_decoder_config);
  if (status != cdm::kSuccess && status != cdm::kDeferredInitialization) {
    // DCHECK(status == cdm::kSessionError); http://crbug.com/570486
    DVLOG(1) << __FUNCTION__ << " failed with cdm::Error " << status;
    init_cb.Run(false);
    return;
  }

  audio_samples_per_second_ = config.samples_per_second();
  audio_channel_layout_ = config.channel_layout();

  if (status == cdm::kDeferredInitialization) {
    DVLOG(1) << "Deferred initialization in " << __FUNCTION__;
    audio_init_cb_ = init_cb;
    return;
  }

  init_cb.Run(true);
}

void CdmAdapter::InitializeVideoDecoder(const VideoDecoderConfig& config,
                                        const DecoderInitCB& init_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(video_init_cb_.is_null());

  cdm::VideoDecoderConfig cdm_decoder_config;
  cdm_decoder_config.codec = ToCdmVideoCodec(config.codec());
  cdm_decoder_config.profile = ToCdmVideoCodecProfile(config.profile());
  cdm_decoder_config.format = ToCdmVideoFormat(config.format());
  cdm_decoder_config.coded_size.width = config.coded_size().width();
  cdm_decoder_config.coded_size.height = config.coded_size().height();
  cdm_decoder_config.extra_data =
      const_cast<uint8_t*>(config.extra_data().data());
  cdm_decoder_config.extra_data_size = config.extra_data().size();

  cdm::Status status = cdm_->InitializeVideoDecoder(cdm_decoder_config);
  if (status != cdm::kSuccess && status != cdm::kDeferredInitialization) {
    // DCHECK(status == cdm::kSessionError); http://crbug.com/570486
    DVLOG(1) << __FUNCTION__ << " failed with cdm::Error " << status;
    init_cb.Run(false);
    return;
  }

  natural_size_ = config.natural_size();

  if (status == cdm::kDeferredInitialization) {
    DVLOG(1) << "Deferred initialization in " << __FUNCTION__;
    video_init_cb_ = init_cb;
    return;
  }

  init_cb.Run(true);
}

void CdmAdapter::DecryptAndDecodeAudio(
    const scoped_refptr<DecoderBuffer>& encrypted,
    const AudioDecodeCB& audio_decode_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  cdm::InputBuffer input_buffer;
  std::vector<cdm::SubsampleEntry> subsamples;
  std::unique_ptr<AudioFramesImpl> audio_frames(new AudioFramesImpl());

  ToCdmInputBuffer(encrypted, &subsamples, &input_buffer);
  cdm::Status status =
      cdm_->DecryptAndDecodeSamples(input_buffer, audio_frames.get());

  const Decryptor::AudioFrames empty_frames;
  if (status != cdm::kSuccess) {
    DVLOG(1) << __FUNCTION__ << " failed with cdm::Error " << status;
    audio_decode_cb.Run(ToMediaDecryptorStatus(status), empty_frames);
    return;
  }

  Decryptor::AudioFrames audio_frame_list;
  DCHECK(audio_frames->FrameBuffer());
  if (!AudioFramesDataToAudioFrames(std::move(audio_frames),
                                    &audio_frame_list)) {
    DVLOG(1) << __FUNCTION__ << " unable to convert Audio Frames";
    audio_decode_cb.Run(Decryptor::kError, empty_frames);
    return;
  }

  audio_decode_cb.Run(Decryptor::kSuccess, audio_frame_list);
}

void CdmAdapter::DecryptAndDecodeVideo(
    const scoped_refptr<DecoderBuffer>& encrypted,
    const VideoDecodeCB& video_decode_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(3) << __FUNCTION__
           << " encrypted: " << encrypted->AsHumanReadableString();

  cdm::InputBuffer input_buffer;
  std::vector<cdm::SubsampleEntry> subsamples;
  std::unique_ptr<VideoFrameImpl> video_frame =
      allocator_->CreateCdmVideoFrame();

  ToCdmInputBuffer(encrypted, &subsamples, &input_buffer);
  cdm::Status status =
      cdm_->DecryptAndDecodeFrame(input_buffer, video_frame.get());

  if (status != cdm::kSuccess) {
    DVLOG(1) << __FUNCTION__ << " failed with cdm::Error " << status;
    video_decode_cb.Run(ToMediaDecryptorStatus(status), nullptr);
    return;
  }

  scoped_refptr<VideoFrame> decoded_frame =
      video_frame->TransformToVideoFrame(natural_size_);
  video_decode_cb.Run(Decryptor::kSuccess, decoded_frame);
}

void CdmAdapter::ResetDecoder(StreamType stream_type) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  cdm_->ResetDecoder(ToCdmStreamType(stream_type));
}

void CdmAdapter::DeinitializeDecoder(StreamType stream_type) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  cdm_->DeinitializeDecoder(ToCdmStreamType(stream_type));

  // Reset the saved values from initializing the decoder.
  switch (stream_type) {
    case Decryptor::kAudio:
      audio_samples_per_second_ = 0;
      audio_channel_layout_ = CHANNEL_LAYOUT_NONE;
      break;
    case Decryptor::kVideo:
      natural_size_ = gfx::Size();
      break;
  }
}

cdm::Buffer* CdmAdapter::Allocate(uint32_t capacity) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return allocator_->CreateCdmBuffer(capacity);
}

void CdmAdapter::SetTimer(int64_t delay_ms, void* context) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  task_runner_->PostDelayedTask(FROM_HERE,
                                base::Bind(&CdmAdapter::TimerExpired,
                                           weak_factory_.GetWeakPtr(), context),
                                base::TimeDelta::FromMilliseconds(delay_ms));
}

void CdmAdapter::TimerExpired(void* context) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  cdm_->TimerExpired(context);
}

cdm::Time CdmAdapter::GetCurrentWallTime() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return base::Time::Now().ToDoubleT();
}

void CdmAdapter::OnResolvePromise(uint32_t promise_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  cdm_promise_adapter_.ResolvePromise(promise_id);
}

void CdmAdapter::OnResolveNewSessionPromise(uint32_t promise_id,
                                            const char* session_id,
                                            uint32_t session_id_size) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  cdm_promise_adapter_.ResolvePromise(promise_id,
                                      std::string(session_id, session_id_size));
}

void CdmAdapter::OnRejectPromise(uint32_t promise_id,
                                 cdm::Error error,
                                 uint32_t system_code,
                                 const char* error_message,
                                 uint32_t error_message_size) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  cdm_promise_adapter_.RejectPromise(
      promise_id, ToMediaExceptionType(error), system_code,
      std::string(error_message, error_message_size));
}

void CdmAdapter::OnSessionMessage(const char* session_id,
                                  uint32_t session_id_size,
                                  cdm::MessageType message_type,
                                  const char* message,
                                  uint32_t message_size,
                                  const char* legacy_destination_url,
                                  uint32_t legacy_destination_url_size) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(legacy_destination_url_size == 0 ||
         message_type != cdm::MessageType::kLicenseRequest);

  GURL verified_gurl =
      GURL(std::string(legacy_destination_url, legacy_destination_url_size));
  if (!verified_gurl.is_valid()) {
    DLOG(WARNING) << "SessionMessage legacy_destination_url is invalid : "
                  << verified_gurl.possibly_invalid_spec();
    verified_gurl = GURL::EmptyGURL();  // Replace invalid destination_url.
  }

  const uint8_t* message_ptr = reinterpret_cast<const uint8_t*>(message);
  session_message_cb_.Run(
      std::string(session_id, session_id_size),
      ToMediaMessageType(message_type),
      std::vector<uint8_t>(message_ptr, message_ptr + message_size),
      verified_gurl);
}

void CdmAdapter::OnSessionKeysChange(const char* session_id,
                                     uint32_t session_id_size,
                                     bool has_additional_usable_key,
                                     const cdm::KeyInformation* keys_info,
                                     uint32_t keys_info_count) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  CdmKeysInfo keys;
  keys.reserve(keys_info_count);
  for (uint32_t i = 0; i < keys_info_count; ++i) {
    const auto& info = keys_info[i];
    keys.push_back(new CdmKeyInformation(
        info.key_id, info.key_id_size,
        ToCdmKeyInformationKeyStatus(info.status), info.system_code));
  }

  // TODO(jrummell): Handling resume playback should be done in the media
  // player, not in the Decryptors. http://crbug.com/413413.
  if (has_additional_usable_key) {
    if (!new_audio_key_cb_.is_null())
      new_audio_key_cb_.Run();
    if (!new_video_key_cb_.is_null())
      new_video_key_cb_.Run();
  }

  session_keys_change_cb_.Run(std::string(session_id, session_id_size),
                              has_additional_usable_key, std::move(keys));
}

void CdmAdapter::OnExpirationChange(const char* session_id,
                                    uint32_t session_id_size,
                                    cdm::Time new_expiry_time) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  session_expiration_update_cb_.Run(std::string(session_id, session_id_size),
                                    base::Time::FromDoubleT(new_expiry_time));
}

void CdmAdapter::OnSessionClosed(const char* session_id,
                                 uint32_t session_id_size) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  session_closed_cb_.Run(std::string(session_id, session_id_size));
}

void CdmAdapter::OnLegacySessionError(const char* session_id,
                                      uint32_t session_id_size,
                                      cdm::Error error,
                                      uint32_t system_code,
                                      const char* error_message,
                                      uint32_t error_message_size) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  legacy_session_error_cb_.Run(std::string(session_id, session_id_size),
                               ToMediaExceptionType(error), system_code,
                               std::string(error_message, error_message_size));
}

void CdmAdapter::SendPlatformChallenge(const char* service_id,
                                       uint32_t service_id_size,
                                       const char* challenge,
                                       uint32_t challenge_size) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // TODO(jrummell): If platform verification is available, use it.
  NOTIMPLEMENTED();
  cdm::PlatformChallengeResponse platform_challenge_response = {};
  cdm_->OnPlatformChallengeResponse(platform_challenge_response);
}

void CdmAdapter::EnableOutputProtection(uint32_t desired_protection_mask) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // TODO(jrummell): If output protection is available, use it.
  NOTIMPLEMENTED();
}

void CdmAdapter::QueryOutputProtectionStatus() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // TODO(jrummell): If output protection is available, use it.
  NOTIMPLEMENTED();
  cdm_->OnQueryOutputProtectionStatus(cdm::kQueryFailed, 0, 0);
}

void CdmAdapter::OnDeferredInitializationDone(cdm::StreamType stream_type,
                                              cdm::Status decoder_status) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG_IF(1, decoder_status != cdm::kSuccess)
      << __FUNCTION__ << " failed with cdm::Error " << decoder_status;

  switch (stream_type) {
    case cdm::kStreamTypeAudio:
      base::ResetAndReturn(&audio_init_cb_)
          .Run(decoder_status == cdm::kSuccess);
      return;
    case cdm::kStreamTypeVideo:
      base::ResetAndReturn(&video_init_cb_)
          .Run(decoder_status == cdm::kSuccess);
      return;
  }

  NOTREACHED() << "Unexpected cdm::StreamType " << stream_type;
}

cdm::FileIO* CdmAdapter::CreateFileIO(cdm::FileIOClient* client) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  std::unique_ptr<CdmFileIO> file_io = create_cdm_file_io_cb_.Run(client);

  // The CDM owns the returned object and must call FileIO::Close()
  // to release it.
  return file_io.release();
}

bool CdmAdapter::AudioFramesDataToAudioFrames(
    std::unique_ptr<AudioFramesImpl> audio_frames,
    Decryptor::AudioFrames* result_frames) {
  const uint8_t* data = audio_frames->FrameBuffer()->Data();
  const size_t data_size = audio_frames->FrameBuffer()->Size();
  size_t bytes_left = data_size;
  const SampleFormat sample_format =
      ToMediaSampleFormat(audio_frames->Format());
  const int audio_channel_count =
      ChannelLayoutToChannelCount(audio_channel_layout_);
  const int audio_bytes_per_frame =
      SampleFormatToBytesPerChannel(sample_format) * audio_channel_count;
  if (audio_bytes_per_frame <= 0)
    return false;

  // Allocate space for the channel pointers given to AudioBuffer.
  std::vector<const uint8_t*> channel_ptrs(audio_channel_count, nullptr);
  do {
    // AudioFrames can contain multiple audio output buffers, which are
    // serialized into this format:
    // |<------------------- serialized audio buffer ------------------->|
    // | int64_t timestamp | int64_t length | length bytes of audio data |
    int64_t timestamp = 0;
    int64_t frame_size = -1;
    const size_t kHeaderSize = sizeof(timestamp) + sizeof(frame_size);
    if (bytes_left < kHeaderSize)
      return false;

    memcpy(&timestamp, data, sizeof(timestamp));
    memcpy(&frame_size, data + sizeof(timestamp), sizeof(frame_size));
    data += kHeaderSize;
    bytes_left -= kHeaderSize;

    // We should *not* have empty frames in the list.
    if (frame_size <= 0 ||
        bytes_left < base::checked_cast<size_t>(frame_size)) {
      return false;
    }

    // Setup channel pointers.  AudioBuffer::CopyFrom() will only use the first
    // one in the case of interleaved data.
    const int size_per_channel = frame_size / audio_channel_count;
    for (int i = 0; i < audio_channel_count; ++i)
      channel_ptrs[i] = data + i * size_per_channel;

    const int frame_count = frame_size / audio_bytes_per_frame;
    scoped_refptr<media::AudioBuffer> frame = media::AudioBuffer::CopyFrom(
        sample_format, audio_channel_layout_, audio_channel_count,
        audio_samples_per_second_, frame_count, &channel_ptrs[0],
        base::TimeDelta::FromMicroseconds(timestamp));
    result_frames->push_back(frame);

    data += frame_size;
    bytes_left -= frame_size;
  } while (bytes_left > 0);

  return true;
}

}  // namespace media
