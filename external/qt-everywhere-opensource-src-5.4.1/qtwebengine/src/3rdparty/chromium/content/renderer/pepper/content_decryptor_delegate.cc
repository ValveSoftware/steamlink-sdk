// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/content_decryptor_delegate.h"

#include "base/callback_helpers.h"
#include "base/debug/trace_event.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/numerics/safe_conversions.h"
#include "content/renderer/pepper/ppb_buffer_impl.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/cdm_promise.h"
#include "media/base/channel_layout.h"
#include "media/base/data_buffer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_buffer_api.h"
#include "ui/gfx/rect.h"

using media::CdmPromise;
using media::Decryptor;
using media::MediaKeys;
using media::NewSessionCdmPromise;
using media::SimpleCdmPromise;
using ppapi::ArrayBufferVar;
using ppapi::PpapiGlobals;
using ppapi::ScopedPPResource;
using ppapi::StringVar;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Buffer_API;

namespace content {

namespace {

// Fills |resource| with a PPB_Buffer_Impl and copies |data| into the buffer
// resource. The |*resource|, if valid, will be in the ResourceTracker with a
// reference-count of 0. If |data| is NULL, sets |*resource| to NULL. Returns
// true upon success and false if any error happened.
bool MakeBufferResource(PP_Instance instance,
                        const uint8* data,
                        uint32_t size,
                        scoped_refptr<PPB_Buffer_Impl>* resource) {
  TRACE_EVENT0("media", "ContentDecryptorDelegate - MakeBufferResource");
  DCHECK(resource);

  if (!data || !size) {
    DCHECK(!data && !size);
    resource = NULL;
    return true;
  }

  scoped_refptr<PPB_Buffer_Impl> buffer(
      PPB_Buffer_Impl::CreateResource(instance, size));
  if (!buffer.get())
    return false;

  BufferAutoMapper mapper(buffer.get());
  if (!mapper.data() || mapper.size() < size)
    return false;
  memcpy(mapper.data(), data, size);

  *resource = buffer;
  return true;
}

// Copies the content of |str| into |array|.
// Returns true if copy succeeded. Returns false if copy failed, e.g. if the
// |array_size| is smaller than the |str| length.
template <uint32_t array_size>
bool CopyStringToArray(const std::string& str, uint8 (&array)[array_size]) {
  if (array_size < str.size())
    return false;

  memcpy(array, str.data(), str.size());
  return true;
}

// Fills the |block_info| with information from |encrypted_buffer|.
//
// Returns true if |block_info| is successfully filled. Returns false
// otherwise.
static bool MakeEncryptedBlockInfo(
    const scoped_refptr<media::DecoderBuffer>& encrypted_buffer,
    uint32_t request_id,
    PP_EncryptedBlockInfo* block_info) {
  // TODO(xhwang): Fix initialization of PP_EncryptedBlockInfo here and
  // anywhere else.
  memset(block_info, 0, sizeof(*block_info));
  block_info->tracking_info.request_id = request_id;

  // EOS buffers need a request ID and nothing more.
  if (encrypted_buffer->end_of_stream())
    return true;

  DCHECK(encrypted_buffer->data_size())
      << "DecryptConfig is set on an empty buffer";

  block_info->tracking_info.timestamp =
      encrypted_buffer->timestamp().InMicroseconds();
  block_info->data_size = encrypted_buffer->data_size();

  const media::DecryptConfig* decrypt_config =
      encrypted_buffer->decrypt_config();

  if (!CopyStringToArray(decrypt_config->key_id(), block_info->key_id) ||
      !CopyStringToArray(decrypt_config->iv(), block_info->iv))
    return false;

  block_info->key_id_size = decrypt_config->key_id().size();
  block_info->iv_size = decrypt_config->iv().size();

  if (decrypt_config->subsamples().size() > arraysize(block_info->subsamples))
    return false;

  block_info->num_subsamples = decrypt_config->subsamples().size();
  for (uint32_t i = 0; i < block_info->num_subsamples; ++i) {
    block_info->subsamples[i].clear_bytes =
        decrypt_config->subsamples()[i].clear_bytes;
    block_info->subsamples[i].cipher_bytes =
        decrypt_config->subsamples()[i].cypher_bytes;
  }

  return true;
}

PP_AudioCodec MediaAudioCodecToPpAudioCodec(media::AudioCodec codec) {
  switch (codec) {
    case media::kCodecVorbis:
      return PP_AUDIOCODEC_VORBIS;
    case media::kCodecAAC:
      return PP_AUDIOCODEC_AAC;
    default:
      return PP_AUDIOCODEC_UNKNOWN;
  }
}

PP_VideoCodec MediaVideoCodecToPpVideoCodec(media::VideoCodec codec) {
  switch (codec) {
    case media::kCodecVP8:
      return PP_VIDEOCODEC_VP8;
    case media::kCodecH264:
      return PP_VIDEOCODEC_H264;
    case media::kCodecVP9:
      return PP_VIDEOCODEC_VP9;
    default:
      return PP_VIDEOCODEC_UNKNOWN;
  }
}

PP_VideoCodecProfile MediaVideoCodecProfileToPpVideoCodecProfile(
    media::VideoCodecProfile profile) {
  switch (profile) {
    // TODO(xhwang): VP8 and VP9 do not have profiles. Clean up
    // media::VideoCodecProfile and remove these two cases.
    case media::VP8PROFILE_MAIN:
    case media::VP9PROFILE_MAIN:
      return PP_VIDEOCODECPROFILE_NOT_NEEDED;
    case media::H264PROFILE_BASELINE:
      return PP_VIDEOCODECPROFILE_H264_BASELINE;
    case media::H264PROFILE_MAIN:
      return PP_VIDEOCODECPROFILE_H264_MAIN;
    case media::H264PROFILE_EXTENDED:
      return PP_VIDEOCODECPROFILE_H264_EXTENDED;
    case media::H264PROFILE_HIGH:
      return PP_VIDEOCODECPROFILE_H264_HIGH;
    case media::H264PROFILE_HIGH10PROFILE:
      return PP_VIDEOCODECPROFILE_H264_HIGH_10;
    case media::H264PROFILE_HIGH422PROFILE:
      return PP_VIDEOCODECPROFILE_H264_HIGH_422;
    case media::H264PROFILE_HIGH444PREDICTIVEPROFILE:
      return PP_VIDEOCODECPROFILE_H264_HIGH_444_PREDICTIVE;
    default:
      return PP_VIDEOCODECPROFILE_UNKNOWN;
  }
}

PP_DecryptedFrameFormat MediaVideoFormatToPpDecryptedFrameFormat(
    media::VideoFrame::Format format) {
  switch (format) {
    case media::VideoFrame::YV12:
      return PP_DECRYPTEDFRAMEFORMAT_YV12;
    case media::VideoFrame::I420:
      return PP_DECRYPTEDFRAMEFORMAT_I420;
    default:
      return PP_DECRYPTEDFRAMEFORMAT_UNKNOWN;
  }
}

Decryptor::Status PpDecryptResultToMediaDecryptorStatus(
    PP_DecryptResult result) {
  switch (result) {
    case PP_DECRYPTRESULT_SUCCESS:
      return Decryptor::kSuccess;
    case PP_DECRYPTRESULT_DECRYPT_NOKEY:
      return Decryptor::kNoKey;
    case PP_DECRYPTRESULT_NEEDMOREDATA:
      return Decryptor::kNeedMoreData;
    case PP_DECRYPTRESULT_DECRYPT_ERROR:
      return Decryptor::kError;
    case PP_DECRYPTRESULT_DECODE_ERROR:
      return Decryptor::kError;
    default:
      NOTREACHED();
      return Decryptor::kError;
  }
}

PP_DecryptorStreamType MediaDecryptorStreamTypeToPpStreamType(
    Decryptor::StreamType stream_type) {
  switch (stream_type) {
    case Decryptor::kAudio:
      return PP_DECRYPTORSTREAMTYPE_AUDIO;
    case Decryptor::kVideo:
      return PP_DECRYPTORSTREAMTYPE_VIDEO;
    default:
      NOTREACHED();
      return PP_DECRYPTORSTREAMTYPE_VIDEO;
  }
}

media::SampleFormat PpDecryptedSampleFormatToMediaSampleFormat(
    PP_DecryptedSampleFormat result) {
  switch (result) {
    case PP_DECRYPTEDSAMPLEFORMAT_U8:
      return media::kSampleFormatU8;
    case PP_DECRYPTEDSAMPLEFORMAT_S16:
      return media::kSampleFormatS16;
    case PP_DECRYPTEDSAMPLEFORMAT_S32:
      return media::kSampleFormatS32;
    case PP_DECRYPTEDSAMPLEFORMAT_F32:
      return media::kSampleFormatF32;
    case PP_DECRYPTEDSAMPLEFORMAT_PLANAR_S16:
      return media::kSampleFormatPlanarS16;
    case PP_DECRYPTEDSAMPLEFORMAT_PLANAR_F32:
      return media::kSampleFormatPlanarF32;
    default:
      NOTREACHED();
      return media::kUnknownSampleFormat;
  }
}

PP_SessionType MediaSessionTypeToPpSessionType(
    MediaKeys::SessionType session_type) {
  switch (session_type) {
    case MediaKeys::TEMPORARY_SESSION:
      return PP_SESSIONTYPE_TEMPORARY;
    case MediaKeys::PERSISTENT_SESSION:
      return PP_SESSIONTYPE_PERSISTENT;
    default:
      NOTREACHED();
      return PP_SESSIONTYPE_TEMPORARY;
  }
}

MediaKeys::Exception PpExceptionTypeToMediaException(
    PP_CdmExceptionCode exception_code) {
  switch (exception_code) {
    case PP_CDMEXCEPTIONCODE_NOTSUPPORTEDERROR:
      return MediaKeys::NOT_SUPPORTED_ERROR;
    case PP_CDMEXCEPTIONCODE_INVALIDSTATEERROR:
      return MediaKeys::INVALID_STATE_ERROR;
    case PP_CDMEXCEPTIONCODE_INVALIDACCESSERROR:
      return MediaKeys::INVALID_ACCESS_ERROR;
    case PP_CDMEXCEPTIONCODE_QUOTAEXCEEDEDERROR:
      return MediaKeys::QUOTA_EXCEEDED_ERROR;
    case PP_CDMEXCEPTIONCODE_UNKNOWNERROR:
      return MediaKeys::UNKNOWN_ERROR;
    case PP_CDMEXCEPTIONCODE_CLIENTERROR:
      return MediaKeys::CLIENT_ERROR;
    case PP_CDMEXCEPTIONCODE_OUTPUTERROR:
      return MediaKeys::OUTPUT_ERROR;
    default:
      NOTREACHED();
      return MediaKeys::UNKNOWN_ERROR;
  }
}

}  // namespace

ContentDecryptorDelegate::ContentDecryptorDelegate(
    PP_Instance pp_instance,
    const PPP_ContentDecryptor_Private* plugin_decryption_interface)
    : pp_instance_(pp_instance),
      plugin_decryption_interface_(plugin_decryption_interface),
      next_decryption_request_id_(1),
      audio_samples_per_second_(0),
      audio_channel_count_(0),
      audio_channel_layout_(media::CHANNEL_LAYOUT_NONE),
      next_promise_id_(1),
      weak_ptr_factory_(this) {
  weak_this_ = weak_ptr_factory_.GetWeakPtr();
}

ContentDecryptorDelegate::~ContentDecryptorDelegate() {
  SatisfyAllPendingCallbacksOnError();
}

void ContentDecryptorDelegate::Initialize(
    const std::string& key_system,
    const media::SessionMessageCB& session_message_cb,
    const media::SessionReadyCB& session_ready_cb,
    const media::SessionClosedCB& session_closed_cb,
    const media::SessionErrorCB& session_error_cb,
    const base::Closure& fatal_plugin_error_cb) {
  DCHECK(!key_system.empty());
  DCHECK(key_system_.empty());
  key_system_ = key_system;

  session_message_cb_ = session_message_cb;
  session_ready_cb_ = session_ready_cb;
  session_closed_cb_ = session_closed_cb;
  session_error_cb_ = session_error_cb;
  fatal_plugin_error_cb_ = fatal_plugin_error_cb;

  plugin_decryption_interface_->Initialize(
      pp_instance_, StringVar::StringToPPVar(key_system_));
}

void ContentDecryptorDelegate::InstanceCrashed() {
  fatal_plugin_error_cb_.Run();
  SatisfyAllPendingCallbacksOnError();
}

void ContentDecryptorDelegate::CreateSession(
    const std::string& init_data_type,
    const uint8* init_data,
    int init_data_length,
    MediaKeys::SessionType session_type,
    scoped_ptr<NewSessionCdmPromise> promise) {
  uint32_t promise_id = SavePromise(promise.PassAs<CdmPromise>());
  PP_Var init_data_array =
      PpapiGlobals::Get()->GetVarTracker()->MakeArrayBufferPPVar(
          init_data_length, init_data);
  plugin_decryption_interface_->CreateSession(
      pp_instance_,
      promise_id,
      StringVar::StringToPPVar(init_data_type),
      init_data_array,
      MediaSessionTypeToPpSessionType(session_type));
}

void ContentDecryptorDelegate::LoadSession(
    const std::string& web_session_id,
    scoped_ptr<NewSessionCdmPromise> promise) {
  uint32_t promise_id = SavePromise(promise.PassAs<CdmPromise>());
  plugin_decryption_interface_->LoadSession(
      pp_instance_, promise_id, StringVar::StringToPPVar(web_session_id));
}

void ContentDecryptorDelegate::UpdateSession(
    const std::string& web_session_id,
    const uint8* response,
    int response_length,
    scoped_ptr<SimpleCdmPromise> promise) {
  uint32_t promise_id = SavePromise(promise.PassAs<CdmPromise>());
  PP_Var response_array =
      PpapiGlobals::Get()->GetVarTracker()->MakeArrayBufferPPVar(
          response_length, response);
  plugin_decryption_interface_->UpdateSession(
      pp_instance_,
      promise_id,
      StringVar::StringToPPVar(web_session_id),
      response_array);
}

void ContentDecryptorDelegate::ReleaseSession(
    const std::string& web_session_id,
    scoped_ptr<SimpleCdmPromise> promise) {
  uint32_t promise_id = SavePromise(promise.PassAs<CdmPromise>());
  plugin_decryption_interface_->ReleaseSession(
      pp_instance_, promise_id, StringVar::StringToPPVar(web_session_id));
}

// TODO(xhwang): Remove duplication of code in Decrypt(),
// DecryptAndDecodeAudio() and DecryptAndDecodeVideo().
bool ContentDecryptorDelegate::Decrypt(
    Decryptor::StreamType stream_type,
    const scoped_refptr<media::DecoderBuffer>& encrypted_buffer,
    const Decryptor::DecryptCB& decrypt_cb) {
  DVLOG(3) << "Decrypt() - stream_type: " << stream_type;

  // |{audio|video}_input_resource_| is not being used by the plugin
  // now because there is only one pending audio/video decrypt request at any
  // time. This is enforced by the media pipeline.
  scoped_refptr<PPB_Buffer_Impl> encrypted_resource;
  if (!MakeMediaBufferResource(
          stream_type, encrypted_buffer, &encrypted_resource) ||
      !encrypted_resource.get()) {
    return false;
  }
  ScopedPPResource pp_resource(encrypted_resource.get());

  const uint32_t request_id = next_decryption_request_id_++;
  DVLOG(2) << "Decrypt() - request_id " << request_id;

  PP_EncryptedBlockInfo block_info = {};
  DCHECK(encrypted_buffer->decrypt_config());
  if (!MakeEncryptedBlockInfo(encrypted_buffer, request_id, &block_info)) {
    return false;
  }

  // There is only one pending decrypt request at any time per stream. This is
  // enforced by the media pipeline.
  switch (stream_type) {
    case Decryptor::kAudio:
      audio_decrypt_cb_.Set(request_id, decrypt_cb);
      break;
    case Decryptor::kVideo:
      video_decrypt_cb_.Set(request_id, decrypt_cb);
      break;
    default:
      NOTREACHED();
      return false;
  }

  SetBufferToFreeInTrackingInfo(&block_info.tracking_info);

  plugin_decryption_interface_->Decrypt(pp_instance_, pp_resource, &block_info);
  return true;
}

bool ContentDecryptorDelegate::CancelDecrypt(
    Decryptor::StreamType stream_type) {
  DVLOG(3) << "CancelDecrypt() - stream_type: " << stream_type;

  Decryptor::DecryptCB decrypt_cb;
  switch (stream_type) {
    case Decryptor::kAudio:
      // Release the shared memory as it can still be in use by the plugin.
      // The next Decrypt() call will need to allocate a new shared memory
      // buffer.
      audio_input_resource_ = NULL;
      decrypt_cb = audio_decrypt_cb_.ResetAndReturn();
      break;
    case Decryptor::kVideo:
      // Release the shared memory as it can still be in use by the plugin.
      // The next Decrypt() call will need to allocate a new shared memory
      // buffer.
      video_input_resource_ = NULL;
      decrypt_cb = video_decrypt_cb_.ResetAndReturn();
      break;
    default:
      NOTREACHED();
      return false;
  }

  if (!decrypt_cb.is_null())
    decrypt_cb.Run(Decryptor::kSuccess, NULL);

  return true;
}

bool ContentDecryptorDelegate::InitializeAudioDecoder(
    const media::AudioDecoderConfig& decoder_config,
    const Decryptor::DecoderInitCB& init_cb) {
  PP_AudioDecoderConfig pp_decoder_config;
  pp_decoder_config.codec =
      MediaAudioCodecToPpAudioCodec(decoder_config.codec());
  pp_decoder_config.channel_count =
      media::ChannelLayoutToChannelCount(decoder_config.channel_layout());
  pp_decoder_config.bits_per_channel = decoder_config.bits_per_channel();
  pp_decoder_config.samples_per_second = decoder_config.samples_per_second();
  pp_decoder_config.request_id = next_decryption_request_id_++;

  audio_samples_per_second_ = pp_decoder_config.samples_per_second;
  audio_channel_count_ = pp_decoder_config.channel_count;
  audio_channel_layout_ = decoder_config.channel_layout();

  scoped_refptr<PPB_Buffer_Impl> extra_data_resource;
  if (!MakeBufferResource(pp_instance_,
                          decoder_config.extra_data(),
                          decoder_config.extra_data_size(),
                          &extra_data_resource)) {
    return false;
  }
  ScopedPPResource pp_resource(extra_data_resource.get());

  audio_decoder_init_cb_.Set(pp_decoder_config.request_id, init_cb);
  plugin_decryption_interface_->InitializeAudioDecoder(
      pp_instance_, &pp_decoder_config, pp_resource);
  return true;
}

bool ContentDecryptorDelegate::InitializeVideoDecoder(
    const media::VideoDecoderConfig& decoder_config,
    const Decryptor::DecoderInitCB& init_cb) {
  PP_VideoDecoderConfig pp_decoder_config;
  pp_decoder_config.codec =
      MediaVideoCodecToPpVideoCodec(decoder_config.codec());
  pp_decoder_config.profile =
      MediaVideoCodecProfileToPpVideoCodecProfile(decoder_config.profile());
  pp_decoder_config.format =
      MediaVideoFormatToPpDecryptedFrameFormat(decoder_config.format());
  pp_decoder_config.width = decoder_config.coded_size().width();
  pp_decoder_config.height = decoder_config.coded_size().height();
  pp_decoder_config.request_id = next_decryption_request_id_++;

  scoped_refptr<PPB_Buffer_Impl> extra_data_resource;
  if (!MakeBufferResource(pp_instance_,
                          decoder_config.extra_data(),
                          decoder_config.extra_data_size(),
                          &extra_data_resource)) {
    return false;
  }
  ScopedPPResource pp_resource(extra_data_resource.get());

  video_decoder_init_cb_.Set(pp_decoder_config.request_id, init_cb);
  natural_size_ = decoder_config.natural_size();

  plugin_decryption_interface_->InitializeVideoDecoder(
      pp_instance_, &pp_decoder_config, pp_resource);
  return true;
}

bool ContentDecryptorDelegate::DeinitializeDecoder(
    Decryptor::StreamType stream_type) {
  CancelDecode(stream_type);

  if (stream_type == Decryptor::kVideo)
    natural_size_ = gfx::Size();

  // TODO(tomfinegan): Add decoder deinitialize request tracking, and get
  // stream type from media stack.
  plugin_decryption_interface_->DeinitializeDecoder(
      pp_instance_, MediaDecryptorStreamTypeToPpStreamType(stream_type), 0);
  return true;
}

bool ContentDecryptorDelegate::ResetDecoder(Decryptor::StreamType stream_type) {
  CancelDecode(stream_type);

  // TODO(tomfinegan): Add decoder reset request tracking.
  plugin_decryption_interface_->ResetDecoder(
      pp_instance_, MediaDecryptorStreamTypeToPpStreamType(stream_type), 0);
  return true;
}

bool ContentDecryptorDelegate::DecryptAndDecodeAudio(
    const scoped_refptr<media::DecoderBuffer>& encrypted_buffer,
    const Decryptor::AudioDecodeCB& audio_decode_cb) {
  // |audio_input_resource_| is not being used by the plugin now
  // because there is only one pending audio decode request at any time.
  // This is enforced by the media pipeline.
  scoped_refptr<PPB_Buffer_Impl> encrypted_resource;
  if (!MakeMediaBufferResource(
          Decryptor::kAudio, encrypted_buffer, &encrypted_resource)) {
    return false;
  }

  // The resource should not be NULL for non-EOS buffer.
  if (!encrypted_buffer->end_of_stream() && !encrypted_resource.get())
    return false;

  const uint32_t request_id = next_decryption_request_id_++;
  DVLOG(2) << "DecryptAndDecodeAudio() - request_id " << request_id;

  PP_EncryptedBlockInfo block_info = {};
  if (!MakeEncryptedBlockInfo(encrypted_buffer, request_id, &block_info)) {
    return false;
  }

  SetBufferToFreeInTrackingInfo(&block_info.tracking_info);

  // There is only one pending audio decode request at any time. This is
  // enforced by the media pipeline. If this DCHECK is violated, our buffer
  // reuse policy is not valid, and we may have race problems for the shared
  // buffer.
  audio_decode_cb_.Set(request_id, audio_decode_cb);

  ScopedPPResource pp_resource(encrypted_resource.get());
  plugin_decryption_interface_->DecryptAndDecode(
      pp_instance_, PP_DECRYPTORSTREAMTYPE_AUDIO, pp_resource, &block_info);
  return true;
}

bool ContentDecryptorDelegate::DecryptAndDecodeVideo(
    const scoped_refptr<media::DecoderBuffer>& encrypted_buffer,
    const Decryptor::VideoDecodeCB& video_decode_cb) {
  // |video_input_resource_| is not being used by the plugin now
  // because there is only one pending video decode request at any time.
  // This is enforced by the media pipeline.
  scoped_refptr<PPB_Buffer_Impl> encrypted_resource;
  if (!MakeMediaBufferResource(
          Decryptor::kVideo, encrypted_buffer, &encrypted_resource)) {
    return false;
  }

  // The resource should not be 0 for non-EOS buffer.
  if (!encrypted_buffer->end_of_stream() && !encrypted_resource.get())
    return false;

  const uint32_t request_id = next_decryption_request_id_++;
  DVLOG(2) << "DecryptAndDecodeVideo() - request_id " << request_id;
  TRACE_EVENT_ASYNC_BEGIN0(
      "media", "ContentDecryptorDelegate::DecryptAndDecodeVideo", request_id);

  PP_EncryptedBlockInfo block_info = {};
  if (!MakeEncryptedBlockInfo(encrypted_buffer, request_id, &block_info)) {
    return false;
  }

  SetBufferToFreeInTrackingInfo(&block_info.tracking_info);

  // Only one pending video decode request at any time. This is enforced by the
  // media pipeline. If this DCHECK is violated, our buffer
  // reuse policy is not valid, and we may have race problems for the shared
  // buffer.
  video_decode_cb_.Set(request_id, video_decode_cb);

  // TODO(tomfinegan): Need to get stream type from media stack.
  ScopedPPResource pp_resource(encrypted_resource.get());
  plugin_decryption_interface_->DecryptAndDecode(
      pp_instance_, PP_DECRYPTORSTREAMTYPE_VIDEO, pp_resource, &block_info);
  return true;
}

void ContentDecryptorDelegate::OnPromiseResolved(uint32 promise_id) {
  scoped_ptr<CdmPromise> promise = TakePromise(promise_id);
  if (promise) {
    SimpleCdmPromise* simple_promise(
        static_cast<SimpleCdmPromise*>(promise.get()));
    simple_promise->resolve();
  }
}

void ContentDecryptorDelegate::OnPromiseResolvedWithSession(
    uint32 promise_id,
    PP_Var web_session_id) {
  scoped_ptr<CdmPromise> promise = TakePromise(promise_id);

  StringVar* web_session_id_string = StringVar::FromPPVar(web_session_id);
  DCHECK(web_session_id_string);

  if (promise) {
    NewSessionCdmPromise* session_promise(
        static_cast<NewSessionCdmPromise*>(promise.get()));
    session_promise->resolve(web_session_id_string->value());
  }
}

void ContentDecryptorDelegate::OnPromiseRejected(
    uint32 promise_id,
    PP_CdmExceptionCode exception_code,
    uint32 system_code,
    PP_Var error_description) {
  StringVar* error_description_string = StringVar::FromPPVar(error_description);
  DCHECK(error_description_string);

  scoped_ptr<CdmPromise> promise = TakePromise(promise_id);
  if (promise) {
    promise->reject(PpExceptionTypeToMediaException(exception_code),
                    system_code,
                    error_description_string->value());
  }
}

void ContentDecryptorDelegate::OnSessionMessage(PP_Var web_session_id,
                                                PP_Var message,
                                                PP_Var destination_url) {
  if (session_message_cb_.is_null())
    return;

  StringVar* web_session_id_string = StringVar::FromPPVar(web_session_id);
  DCHECK(web_session_id_string);

  ArrayBufferVar* message_array_buffer = ArrayBufferVar::FromPPVar(message);
  std::vector<uint8> message_vector;
  if (message_array_buffer) {
    const uint8* data = static_cast<const uint8*>(message_array_buffer->Map());
    message_vector.assign(data, data + message_array_buffer->ByteLength());
  }

  StringVar* destination_url_string = StringVar::FromPPVar(destination_url);
  DCHECK(destination_url_string);

  GURL verified_gurl = GURL(destination_url_string->value());
  if (!verified_gurl.is_valid() && !verified_gurl.is_empty()) {
    DLOG(WARNING) << "SessionMessage default_url is invalid : "
                  << verified_gurl.possibly_invalid_spec();
    verified_gurl = GURL::EmptyGURL();  // Replace invalid destination_url.
  }

  session_message_cb_.Run(
      web_session_id_string->value(), message_vector, verified_gurl);
}

void ContentDecryptorDelegate::OnSessionReady(PP_Var web_session_id) {
  if (session_ready_cb_.is_null())
    return;

  StringVar* web_session_id_string = StringVar::FromPPVar(web_session_id);
  DCHECK(web_session_id_string);

  session_ready_cb_.Run(web_session_id_string->value());
}

void ContentDecryptorDelegate::OnSessionClosed(PP_Var web_session_id) {
  if (session_closed_cb_.is_null())
    return;

  StringVar* web_session_id_string = StringVar::FromPPVar(web_session_id);
  DCHECK(web_session_id_string);

  session_closed_cb_.Run(web_session_id_string->value());
}

void ContentDecryptorDelegate::OnSessionError(
    PP_Var web_session_id,
    PP_CdmExceptionCode exception_code,
    uint32 system_code,
    PP_Var error_description) {
  if (session_error_cb_.is_null())
    return;

  StringVar* web_session_id_string = StringVar::FromPPVar(web_session_id);
  DCHECK(web_session_id_string);

  StringVar* error_description_string = StringVar::FromPPVar(error_description);
  DCHECK(error_description_string);

  session_error_cb_.Run(web_session_id_string->value(),
                        PpExceptionTypeToMediaException(exception_code),
                        system_code,
                        error_description_string->value());
}

void ContentDecryptorDelegate::DecoderInitializeDone(
    PP_DecryptorStreamType decoder_type,
    uint32_t request_id,
    PP_Bool success) {
  if (decoder_type == PP_DECRYPTORSTREAMTYPE_AUDIO) {
    // If the request ID is not valid or does not match what's saved, do
    // nothing.
    if (request_id == 0 || !audio_decoder_init_cb_.Matches(request_id))
      return;

    audio_decoder_init_cb_.ResetAndReturn().Run(PP_ToBool(success));
  } else {
    if (request_id == 0 || !video_decoder_init_cb_.Matches(request_id))
      return;

    if (!success)
      natural_size_ = gfx::Size();

    video_decoder_init_cb_.ResetAndReturn().Run(PP_ToBool(success));
  }
}

void ContentDecryptorDelegate::DecoderDeinitializeDone(
    PP_DecryptorStreamType decoder_type,
    uint32_t request_id) {
  // TODO(tomfinegan): Add decoder stop completion handling.
}

void ContentDecryptorDelegate::DecoderResetDone(
    PP_DecryptorStreamType decoder_type,
    uint32_t request_id) {
  // TODO(tomfinegan): Add decoder reset completion handling.
}

void ContentDecryptorDelegate::DeliverBlock(
    PP_Resource decrypted_block,
    const PP_DecryptedBlockInfo* block_info) {
  DCHECK(block_info);

  FreeBuffer(block_info->tracking_info.buffer_id);

  const uint32_t request_id = block_info->tracking_info.request_id;
  DVLOG(2) << "DeliverBlock() - request_id: " << request_id;

  // If the request ID is not valid or does not match what's saved, do nothing.
  if (request_id == 0) {
    DVLOG(1) << "DeliverBlock() - invalid request_id " << request_id;
    return;
  }

  Decryptor::DecryptCB decrypt_cb;
  if (audio_decrypt_cb_.Matches(request_id)) {
    decrypt_cb = audio_decrypt_cb_.ResetAndReturn();
  } else if (video_decrypt_cb_.Matches(request_id)) {
    decrypt_cb = video_decrypt_cb_.ResetAndReturn();
  } else {
    DVLOG(1) << "DeliverBlock() - request_id " << request_id << " not found";
    return;
  }

  Decryptor::Status status =
      PpDecryptResultToMediaDecryptorStatus(block_info->result);
  if (status != Decryptor::kSuccess) {
    decrypt_cb.Run(status, NULL);
    return;
  }

  EnterResourceNoLock<PPB_Buffer_API> enter(decrypted_block, true);
  if (!enter.succeeded()) {
    decrypt_cb.Run(Decryptor::kError, NULL);
    return;
  }
  BufferAutoMapper mapper(enter.object());
  if (!mapper.data() || !mapper.size() ||
      mapper.size() < block_info->data_size) {
    decrypt_cb.Run(Decryptor::kError, NULL);
    return;
  }

  // TODO(tomfinegan): Find a way to take ownership of the shared memory
  // managed by the PPB_Buffer_Dev, and avoid the extra copy.
  scoped_refptr<media::DecoderBuffer> decrypted_buffer(
      media::DecoderBuffer::CopyFrom(static_cast<uint8*>(mapper.data()),
                                     block_info->data_size));
  decrypted_buffer->set_timestamp(
      base::TimeDelta::FromMicroseconds(block_info->tracking_info.timestamp));
  decrypt_cb.Run(Decryptor::kSuccess, decrypted_buffer);
}

// Use a non-class-member function here so that if for some reason
// ContentDecryptorDelegate is destroyed before VideoFrame calls this callback,
// we can still get the shared memory unmapped.
static void BufferNoLongerNeeded(
    const scoped_refptr<PPB_Buffer_Impl>& ppb_buffer,
    base::Closure buffer_no_longer_needed_cb) {
  ppb_buffer->Unmap();
  buffer_no_longer_needed_cb.Run();
}

// Enters |resource|, maps shared memory and returns pointer of mapped data.
// Returns NULL if any error occurs.
static uint8* GetMappedBuffer(PP_Resource resource,
                              scoped_refptr<PPB_Buffer_Impl>* ppb_buffer) {
  EnterResourceNoLock<PPB_Buffer_API> enter(resource, true);
  if (!enter.succeeded())
    return NULL;

  uint8* mapped_data = static_cast<uint8*>(enter.object()->Map());
  if (!enter.object()->IsMapped() || !mapped_data)
    return NULL;

  uint32_t mapped_size = 0;
  if (!enter.object()->Describe(&mapped_size) || !mapped_size) {
    enter.object()->Unmap();
    return NULL;
  }

  *ppb_buffer = static_cast<PPB_Buffer_Impl*>(enter.object());

  return mapped_data;
}

void ContentDecryptorDelegate::DeliverFrame(
    PP_Resource decrypted_frame,
    const PP_DecryptedFrameInfo* frame_info) {
  DCHECK(frame_info);

  const uint32_t request_id = frame_info->tracking_info.request_id;
  DVLOG(2) << "DeliverFrame() - request_id: " << request_id;

  // If the request ID is not valid or does not match what's saved, do nothing.
  if (request_id == 0 || !video_decode_cb_.Matches(request_id)) {
    DVLOG(1) << "DeliverFrame() - request_id " << request_id << " not found";
    FreeBuffer(frame_info->tracking_info.buffer_id);
    return;
  }

  TRACE_EVENT_ASYNC_END0(
      "media", "ContentDecryptorDelegate::DecryptAndDecodeVideo", request_id);

  Decryptor::VideoDecodeCB video_decode_cb = video_decode_cb_.ResetAndReturn();

  Decryptor::Status status =
      PpDecryptResultToMediaDecryptorStatus(frame_info->result);
  if (status != Decryptor::kSuccess) {
    DCHECK(!frame_info->tracking_info.buffer_id);
    video_decode_cb.Run(status, NULL);
    return;
  }

  scoped_refptr<PPB_Buffer_Impl> ppb_buffer;
  uint8* frame_data = GetMappedBuffer(decrypted_frame, &ppb_buffer);
  if (!frame_data) {
    FreeBuffer(frame_info->tracking_info.buffer_id);
    video_decode_cb.Run(Decryptor::kError, NULL);
    return;
  }

  gfx::Size frame_size(frame_info->width, frame_info->height);
  DCHECK_EQ(frame_info->format, PP_DECRYPTEDFRAMEFORMAT_YV12);

  scoped_refptr<media::VideoFrame> decoded_frame =
      media::VideoFrame::WrapExternalYuvData(
          media::VideoFrame::YV12,
          frame_size,
          gfx::Rect(frame_size),
          natural_size_,
          frame_info->strides[PP_DECRYPTEDFRAMEPLANES_Y],
          frame_info->strides[PP_DECRYPTEDFRAMEPLANES_U],
          frame_info->strides[PP_DECRYPTEDFRAMEPLANES_V],
          frame_data + frame_info->plane_offsets[PP_DECRYPTEDFRAMEPLANES_Y],
          frame_data + frame_info->plane_offsets[PP_DECRYPTEDFRAMEPLANES_U],
          frame_data + frame_info->plane_offsets[PP_DECRYPTEDFRAMEPLANES_V],
          base::TimeDelta::FromMicroseconds(
              frame_info->tracking_info.timestamp),
          media::BindToCurrentLoop(
              base::Bind(&BufferNoLongerNeeded,
                         ppb_buffer,
                         base::Bind(&ContentDecryptorDelegate::FreeBuffer,
                                    weak_this_,
                                    frame_info->tracking_info.buffer_id))));

  video_decode_cb.Run(Decryptor::kSuccess, decoded_frame);
}

void ContentDecryptorDelegate::DeliverSamples(
    PP_Resource audio_frames,
    const PP_DecryptedSampleInfo* sample_info) {
  DCHECK(sample_info);

  FreeBuffer(sample_info->tracking_info.buffer_id);

  const uint32_t request_id = sample_info->tracking_info.request_id;
  DVLOG(2) << "DeliverSamples() - request_id: " << request_id;

  // If the request ID is not valid or does not match what's saved, do nothing.
  if (request_id == 0 || !audio_decode_cb_.Matches(request_id)) {
    DVLOG(1) << "DeliverSamples() - request_id " << request_id << " not found";
    return;
  }

  Decryptor::AudioDecodeCB audio_decode_cb = audio_decode_cb_.ResetAndReturn();

  const Decryptor::AudioBuffers empty_frames;

  Decryptor::Status status =
      PpDecryptResultToMediaDecryptorStatus(sample_info->result);
  if (status != Decryptor::kSuccess) {
    audio_decode_cb.Run(status, empty_frames);
    return;
  }

  media::SampleFormat sample_format =
      PpDecryptedSampleFormatToMediaSampleFormat(sample_info->format);

  Decryptor::AudioBuffers audio_frame_list;
  if (!DeserializeAudioFrames(audio_frames,
                              sample_info->data_size,
                              sample_format,
                              &audio_frame_list)) {
    NOTREACHED() << "CDM did not serialize the buffer correctly.";
    audio_decode_cb.Run(Decryptor::kError, empty_frames);
    return;
  }

  audio_decode_cb.Run(Decryptor::kSuccess, audio_frame_list);
}

// TODO(xhwang): Try to remove duplicate logic here and in CancelDecrypt().
void ContentDecryptorDelegate::CancelDecode(Decryptor::StreamType stream_type) {
  switch (stream_type) {
    case Decryptor::kAudio:
      // Release the shared memory as it can still be in use by the plugin.
      // The next DecryptAndDecode() call will need to allocate a new shared
      // memory buffer.
      audio_input_resource_ = NULL;
      if (!audio_decode_cb_.is_null())
        audio_decode_cb_.ResetAndReturn().Run(Decryptor::kSuccess,
                                              Decryptor::AudioBuffers());
      break;
    case Decryptor::kVideo:
      // Release the shared memory as it can still be in use by the plugin.
      // The next DecryptAndDecode() call will need to allocate a new shared
      // memory buffer.
      video_input_resource_ = NULL;
      if (!video_decode_cb_.is_null())
        video_decode_cb_.ResetAndReturn().Run(Decryptor::kSuccess, NULL);
      break;
    default:
      NOTREACHED();
  }
}

bool ContentDecryptorDelegate::MakeMediaBufferResource(
    Decryptor::StreamType stream_type,
    const scoped_refptr<media::DecoderBuffer>& encrypted_buffer,
    scoped_refptr<PPB_Buffer_Impl>* resource) {
  TRACE_EVENT0("media", "ContentDecryptorDelegate::MakeMediaBufferResource");

  // End of stream buffers are represented as null resources.
  if (encrypted_buffer->end_of_stream()) {
    *resource = NULL;
    return true;
  }

  DCHECK(stream_type == Decryptor::kAudio || stream_type == Decryptor::kVideo);
  scoped_refptr<PPB_Buffer_Impl>& media_resource =
      (stream_type == Decryptor::kAudio) ? audio_input_resource_
                                         : video_input_resource_;

  const size_t data_size = static_cast<size_t>(encrypted_buffer->data_size());
  if (!media_resource.get() || media_resource->size() < data_size) {
    // Either the buffer hasn't been created yet, or we have one that isn't big
    // enough to fit |size| bytes.

    // Media resource size starts from |kMinimumMediaBufferSize| and grows
    // exponentially to avoid frequent re-allocation of PPB_Buffer_Impl,
    // which is usually expensive. Since input media buffers are compressed,
    // they are usually small (compared to outputs). The over-allocated memory
    // should be negligible.
    const uint32_t kMinimumMediaBufferSize = 1024;
    uint32_t media_resource_size =
        media_resource.get() ? media_resource->size() : kMinimumMediaBufferSize;
    while (media_resource_size < data_size)
      media_resource_size *= 2;

    DVLOG(2) << "Size of media buffer for "
             << ((stream_type == Decryptor::kAudio) ? "audio" : "video")
             << " stream bumped to " << media_resource_size
             << " bytes to fit input.";
    media_resource =
        PPB_Buffer_Impl::CreateResource(pp_instance_, media_resource_size);
    if (!media_resource.get())
      return false;
  }

  BufferAutoMapper mapper(media_resource.get());
  if (!mapper.data() || mapper.size() < data_size) {
    media_resource = NULL;
    return false;
  }
  memcpy(mapper.data(), encrypted_buffer->data(), data_size);

  *resource = media_resource;
  return true;
}

void ContentDecryptorDelegate::FreeBuffer(uint32_t buffer_id) {
  if (buffer_id)
    free_buffers_.push(buffer_id);
}

void ContentDecryptorDelegate::SetBufferToFreeInTrackingInfo(
    PP_DecryptTrackingInfo* tracking_info) {
  DCHECK_EQ(tracking_info->buffer_id, 0u);

  if (free_buffers_.empty())
    return;

  tracking_info->buffer_id = free_buffers_.front();
  free_buffers_.pop();
}

bool ContentDecryptorDelegate::DeserializeAudioFrames(
    PP_Resource audio_frames,
    size_t data_size,
    media::SampleFormat sample_format,
    Decryptor::AudioBuffers* frames) {
  DCHECK(frames);
  EnterResourceNoLock<PPB_Buffer_API> enter(audio_frames, true);
  if (!enter.succeeded())
    return false;

  BufferAutoMapper mapper(enter.object());
  if (!mapper.data() || !mapper.size() ||
      mapper.size() < static_cast<uint32_t>(data_size))
    return false;

  // TODO(jrummell): Pass ownership of data() directly to AudioBuffer to avoid
  // the copy. Since it is possible to get multiple buffers, it would need to be
  // sliced and ref counted appropriately. http://crbug.com/255576.
  const uint8* cur = static_cast<uint8*>(mapper.data());
  size_t bytes_left = data_size;

  const int audio_bytes_per_frame =
      media::SampleFormatToBytesPerChannel(sample_format) *
      audio_channel_count_;
  if (audio_bytes_per_frame <= 0)
    return false;

  // Allocate space for the channel pointers given to AudioBuffer.
  std::vector<const uint8*> channel_ptrs(audio_channel_count_,
                                         static_cast<const uint8*>(NULL));
  do {
    int64 timestamp = 0;
    int64 frame_size = -1;
    const size_t kHeaderSize = sizeof(timestamp) + sizeof(frame_size);

    if (bytes_left < kHeaderSize)
      return false;

    memcpy(&timestamp, cur, sizeof(timestamp));
    cur += sizeof(timestamp);
    bytes_left -= sizeof(timestamp);

    memcpy(&frame_size, cur, sizeof(frame_size));
    cur += sizeof(frame_size);
    bytes_left -= sizeof(frame_size);

    // We should *not* have empty frames in the list.
    if (frame_size <= 0 ||
        bytes_left < base::checked_cast<size_t>(frame_size)) {
      return false;
    }

    // Setup channel pointers.  AudioBuffer::CopyFrom() will only use the first
    // one in the case of interleaved data.
    const int size_per_channel = frame_size / audio_channel_count_;
    for (int i = 0; i < audio_channel_count_; ++i)
      channel_ptrs[i] = cur + i * size_per_channel;

    const int frame_count = frame_size / audio_bytes_per_frame;
    scoped_refptr<media::AudioBuffer> frame = media::AudioBuffer::CopyFrom(
        sample_format,
        audio_channel_layout_,
        audio_channel_count_,
        audio_samples_per_second_,
        frame_count,
        &channel_ptrs[0],
        base::TimeDelta::FromMicroseconds(timestamp));
    frames->push_back(frame);

    cur += frame_size;
    bytes_left -= frame_size;
  } while (bytes_left > 0);

  return true;
}

void ContentDecryptorDelegate::SatisfyAllPendingCallbacksOnError() {
  if (!audio_decoder_init_cb_.is_null())
    audio_decoder_init_cb_.ResetAndReturn().Run(false);

  if (!video_decoder_init_cb_.is_null())
    video_decoder_init_cb_.ResetAndReturn().Run(false);

  audio_input_resource_ = NULL;
  video_input_resource_ = NULL;

  if (!audio_decrypt_cb_.is_null())
    audio_decrypt_cb_.ResetAndReturn().Run(media::Decryptor::kError, NULL);

  if (!video_decrypt_cb_.is_null())
    video_decrypt_cb_.ResetAndReturn().Run(media::Decryptor::kError, NULL);

  if (!audio_decode_cb_.is_null()) {
    const media::Decryptor::AudioBuffers empty_frames;
    audio_decode_cb_.ResetAndReturn().Run(media::Decryptor::kError,
                                          empty_frames);
  }

  if (!video_decode_cb_.is_null())
    video_decode_cb_.ResetAndReturn().Run(media::Decryptor::kError, NULL);

  // Reject all outstanding promises.
  for (PromiseMap::iterator it = promises_.begin(); it != promises_.end();
       ++it) {
    it->second->reject(
        media::MediaKeys::UNKNOWN_ERROR, 0, "Failure calling plugin.");
  }
  promises_.clear();
}

uint32_t ContentDecryptorDelegate::SavePromise(scoped_ptr<CdmPromise> promise) {
  uint32_t promise_id = next_promise_id_++;
  promises_.add(promise_id, promise.Pass());
  return promise_id;
}

scoped_ptr<CdmPromise> ContentDecryptorDelegate::TakePromise(
    uint32_t promise_id) {
  PromiseMap::iterator it = promises_.find(promise_id);
  if (it == promises_.end())
    return scoped_ptr<CdmPromise>();
  return promises_.take_and_erase(it);
}

}  // namespace content
