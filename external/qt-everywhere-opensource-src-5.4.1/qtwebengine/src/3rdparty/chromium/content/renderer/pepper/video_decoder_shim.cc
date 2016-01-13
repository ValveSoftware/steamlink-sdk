// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/video_decoder_shim.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include "base/bind.h"
#include "base/numerics/safe_conversions.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/pepper/pepper_video_decoder_host.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "media/base/decoder_buffer.h"
#include "media/base/limits.h"
#include "media/base/video_decoder.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/filters/vpx_video_decoder.h"
#include "media/video/picture.h"
#include "media/video/video_decode_accelerator.h"
#include "ppapi/c/pp_errors.h"
#include "third_party/libyuv/include/libyuv.h"
#include "webkit/common/gpu/context_provider_web_context.h"

namespace content {

struct VideoDecoderShim::PendingDecode {
  PendingDecode(uint32_t decode_id,
                const scoped_refptr<media::DecoderBuffer>& buffer);
  ~PendingDecode();

  const uint32_t decode_id;
  const scoped_refptr<media::DecoderBuffer> buffer;
};

VideoDecoderShim::PendingDecode::PendingDecode(
    uint32_t decode_id,
    const scoped_refptr<media::DecoderBuffer>& buffer)
    : decode_id(decode_id), buffer(buffer) {
}

VideoDecoderShim::PendingDecode::~PendingDecode() {
}

struct VideoDecoderShim::PendingFrame {
  explicit PendingFrame(uint32_t decode_id);
  PendingFrame(uint32_t decode_id, const gfx::Size& size);
  ~PendingFrame();

  const uint32_t decode_id;
  const gfx::Size size;
  std::vector<uint8_t> argb_pixels;

 private:
  // This could be expensive to copy, so guard against that.
  DISALLOW_COPY_AND_ASSIGN(PendingFrame);
};

VideoDecoderShim::PendingFrame::PendingFrame(uint32_t decode_id)
    : decode_id(decode_id) {
}

VideoDecoderShim::PendingFrame::PendingFrame(uint32_t decode_id,
                                             const gfx::Size& size)
    : decode_id(decode_id),
      size(size),
      argb_pixels(size.width() * size.height() * 4) {
}

VideoDecoderShim::PendingFrame::~PendingFrame() {
}

// DecoderImpl runs the underlying VideoDecoder on the media thread, receiving
// calls from the VideoDecodeShim on the main thread and sending results back.
// This class is constructed on the main thread, but used and destructed on the
// media thread.
class VideoDecoderShim::DecoderImpl {
 public:
  explicit DecoderImpl(const base::WeakPtr<VideoDecoderShim>& proxy);
  ~DecoderImpl();

  void Initialize(media::VideoDecoderConfig config);
  void Decode(uint32_t decode_id, scoped_refptr<media::DecoderBuffer> buffer);
  void Reset();
  void Stop();

 private:
  void OnPipelineStatus(media::PipelineStatus status);
  void DoDecode();
  void OnDecodeComplete(uint32_t decode_id, media::VideoDecoder::Status status);
  void OnOutputComplete(const scoped_refptr<media::VideoFrame>& frame);
  void OnResetComplete();

  // WeakPtr is bound to main_message_loop_. Use only in shim callbacks.
  base::WeakPtr<VideoDecoderShim> shim_;
  scoped_ptr<media::VideoDecoder> decoder_;
  scoped_refptr<base::MessageLoopProxy> main_message_loop_;
  // Queue of decodes waiting for the decoder.
  typedef std::queue<PendingDecode> PendingDecodeQueue;
  PendingDecodeQueue pending_decodes_;
  int max_decodes_at_decoder_;
  int num_decodes_at_decoder_;
  // VideoDecoder returns pictures without information about the decode buffer
  // that generated it. Save the decode_id from the last decode that completed,
  // which is close for most decoders, which only decode one buffer at a time.
  uint32_t decode_id_;
};

VideoDecoderShim::DecoderImpl::DecoderImpl(
    const base::WeakPtr<VideoDecoderShim>& proxy)
    : shim_(proxy),
      main_message_loop_(base::MessageLoopProxy::current()),
      max_decodes_at_decoder_(0),
      num_decodes_at_decoder_(0),
      decode_id_(0) {
}

VideoDecoderShim::DecoderImpl::~DecoderImpl() {
  DCHECK(pending_decodes_.empty());
}

void VideoDecoderShim::DecoderImpl::Initialize(
    media::VideoDecoderConfig config) {
  DCHECK(!decoder_);
  if (config.codec() == media::kCodecVP9) {
    decoder_.reset(
        new media::VpxVideoDecoder(base::MessageLoopProxy::current()));
  } else {
    scoped_ptr<media::FFmpegVideoDecoder> ffmpeg_video_decoder(
        new media::FFmpegVideoDecoder(base::MessageLoopProxy::current()));
    ffmpeg_video_decoder->set_decode_nalus(true);
    decoder_ = ffmpeg_video_decoder.Pass();
  }
  max_decodes_at_decoder_ = decoder_->GetMaxDecodeRequests();
  // We can use base::Unretained() safely in decoder callbacks because we call
  // VideoDecoder::Stop() before deletion. Stop() guarantees there will be no
  // outstanding callbacks after it returns.
  decoder_->Initialize(
      config,
      true /* low_delay */,
      base::Bind(&VideoDecoderShim::DecoderImpl::OnPipelineStatus,
                 base::Unretained(this)),
      base::Bind(&VideoDecoderShim::DecoderImpl::OnOutputComplete,
                 base::Unretained(this)));
}

void VideoDecoderShim::DecoderImpl::Decode(
    uint32_t decode_id,
    scoped_refptr<media::DecoderBuffer> buffer) {
  DCHECK(decoder_);
  pending_decodes_.push(PendingDecode(decode_id, buffer));
  DoDecode();
}

void VideoDecoderShim::DecoderImpl::Reset() {
  DCHECK(decoder_);
  // Abort all pending decodes.
  while (!pending_decodes_.empty()) {
    const PendingDecode& decode = pending_decodes_.front();
    scoped_ptr<PendingFrame> pending_frame(new PendingFrame(decode.decode_id));
    main_message_loop_->PostTask(FROM_HERE,
                                 base::Bind(&VideoDecoderShim::OnDecodeComplete,
                                            shim_,
                                            media::VideoDecoder::kAborted,
                                            decode.decode_id));
    pending_decodes_.pop();
  }
  decoder_->Reset(base::Bind(&VideoDecoderShim::DecoderImpl::OnResetComplete,
                             base::Unretained(this)));
}

void VideoDecoderShim::DecoderImpl::Stop() {
  DCHECK(decoder_);
  // Clear pending decodes now. We don't want OnDecodeComplete to call DoDecode
  // again.
  while (!pending_decodes_.empty())
    pending_decodes_.pop();
  decoder_->Stop();
  // This instance is deleted once we exit this scope.
}

void VideoDecoderShim::DecoderImpl::OnPipelineStatus(
    media::PipelineStatus status) {
  int32_t result;
  switch (status) {
    case media::PIPELINE_OK:
      result = PP_OK;
      break;
    case media::DECODER_ERROR_NOT_SUPPORTED:
      result = PP_ERROR_NOTSUPPORTED;
      break;
    default:
      result = PP_ERROR_FAILED;
      break;
  }

  // Calculate how many textures the shim should create.
  uint32_t shim_texture_pool_size =
      max_decodes_at_decoder_ + media::limits::kMaxVideoFrames;
  main_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&VideoDecoderShim::OnInitializeComplete,
                 shim_,
                 result,
                 shim_texture_pool_size));
}

void VideoDecoderShim::DecoderImpl::DoDecode() {
  while (!pending_decodes_.empty() &&
         num_decodes_at_decoder_ < max_decodes_at_decoder_) {
    num_decodes_at_decoder_++;
    const PendingDecode& decode = pending_decodes_.front();
    decoder_->Decode(
        decode.buffer,
        base::Bind(&VideoDecoderShim::DecoderImpl::OnDecodeComplete,
                   base::Unretained(this),
                   decode.decode_id));
    pending_decodes_.pop();
  }
}

void VideoDecoderShim::DecoderImpl::OnDecodeComplete(
    uint32_t decode_id,
    media::VideoDecoder::Status status) {
  num_decodes_at_decoder_--;
  decode_id_ = decode_id;

  int32_t result;
  switch (status) {
    case media::VideoDecoder::kOk:
    case media::VideoDecoder::kAborted:
      result = PP_OK;
      break;
    case media::VideoDecoder::kDecodeError:
      result = PP_ERROR_RESOURCE_FAILED;
      break;
    default:
      NOTREACHED();
      result = PP_ERROR_FAILED;
      break;
  }

  main_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(
          &VideoDecoderShim::OnDecodeComplete, shim_, result, decode_id));

  DoDecode();
}

void VideoDecoderShim::DecoderImpl::OnOutputComplete(
    const scoped_refptr<media::VideoFrame>& frame) {
  scoped_ptr<PendingFrame> pending_frame;
  if (!frame->end_of_stream()) {
    pending_frame.reset(new PendingFrame(decode_id_, frame->coded_size()));
    // Convert the VideoFrame pixels to ABGR to match VideoDecodeAccelerator.
    libyuv::I420ToABGR(frame->data(media::VideoFrame::kYPlane),
                       frame->stride(media::VideoFrame::kYPlane),
                       frame->data(media::VideoFrame::kUPlane),
                       frame->stride(media::VideoFrame::kUPlane),
                       frame->data(media::VideoFrame::kVPlane),
                       frame->stride(media::VideoFrame::kVPlane),
                       &pending_frame->argb_pixels.front(),
                       frame->coded_size().width() * 4,
                       frame->coded_size().width(),
                       frame->coded_size().height());
  } else {
    pending_frame.reset(new PendingFrame(decode_id_));
  }

  main_message_loop_->PostTask(FROM_HERE,
                               base::Bind(&VideoDecoderShim::OnOutputComplete,
                                          shim_,
                                          base::Passed(&pending_frame)));
}

void VideoDecoderShim::DecoderImpl::OnResetComplete() {
  main_message_loop_->PostTask(
      FROM_HERE, base::Bind(&VideoDecoderShim::OnResetComplete, shim_));
}

VideoDecoderShim::VideoDecoderShim(PepperVideoDecoderHost* host)
    : state_(UNINITIALIZED),
      host_(host),
      media_message_loop_(
          RenderThreadImpl::current()->GetMediaThreadMessageLoopProxy()),
      context_provider_(
          RenderThreadImpl::current()->SharedMainThreadContextProvider()),
      texture_pool_size_(0),
      num_pending_decodes_(0),
      weak_ptr_factory_(this) {
  DCHECK(host_);
  DCHECK(media_message_loop_);
  DCHECK(context_provider_);
  decoder_impl_.reset(new DecoderImpl(weak_ptr_factory_.GetWeakPtr()));
}

VideoDecoderShim::~VideoDecoderShim() {
  DCHECK(RenderThreadImpl::current());
  // Delete any remaining textures.
  TextureIdMap::iterator it = texture_id_map_.begin();
  for (; it != texture_id_map_.end(); ++it)
    DeleteTexture(it->second);
  texture_id_map_.clear();

  FlushCommandBuffer();

  weak_ptr_factory_.InvalidateWeakPtrs();
  // No more callbacks from the delegate will be received now.

  // The callback now holds the only reference to the DecoderImpl, which will be
  // deleted when Stop completes.
  media_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&VideoDecoderShim::DecoderImpl::Stop,
                 base::Owned(decoder_impl_.release())));
}

bool VideoDecoderShim::Initialize(
    media::VideoCodecProfile profile,
    media::VideoDecodeAccelerator::Client* client) {
  DCHECK_EQ(client, host_);
  DCHECK(RenderThreadImpl::current());
  DCHECK_EQ(state_, UNINITIALIZED);
  media::VideoCodec codec = media::kUnknownVideoCodec;
  if (profile <= media::H264PROFILE_MAX)
    codec = media::kCodecH264;
  else if (profile <= media::VP8PROFILE_MAX)
    codec = media::kCodecVP8;
  else if (profile <= media::VP9PROFILE_MAX)
    codec = media::kCodecVP9;
  DCHECK_NE(codec, media::kUnknownVideoCodec);

  media::VideoDecoderConfig config(
      codec,
      profile,
      media::VideoFrame::YV12,
      gfx::Size(32, 24),  // Small sizes that won't fail.
      gfx::Rect(32, 24),
      gfx::Size(32, 24),
      NULL /* extra_data */,  // TODO(bbudge) Verify this isn't needed.
      0 /* extra_data_size */,
      false /* decryption */);

  media_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&VideoDecoderShim::DecoderImpl::Initialize,
                 base::Unretained(decoder_impl_.get()),
                 config));
  // Return success, even though we are asynchronous, to mimic
  // media::VideoDecodeAccelerator.
  return true;
}

void VideoDecoderShim::Decode(const media::BitstreamBuffer& bitstream_buffer) {
  DCHECK(RenderThreadImpl::current());
  DCHECK_EQ(state_, DECODING);

  // We need the address of the shared memory, so we can copy the buffer.
  const uint8_t* buffer = host_->DecodeIdToAddress(bitstream_buffer.id());
  DCHECK(buffer);

  media_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(
          &VideoDecoderShim::DecoderImpl::Decode,
          base::Unretained(decoder_impl_.get()),
          bitstream_buffer.id(),
          media::DecoderBuffer::CopyFrom(buffer, bitstream_buffer.size())));
  num_pending_decodes_++;
}

void VideoDecoderShim::AssignPictureBuffers(
    const std::vector<media::PictureBuffer>& buffers) {
  DCHECK(RenderThreadImpl::current());
  DCHECK_EQ(state_, DECODING);
  if (buffers.empty()) {
    NOTREACHED();
    return;
  }
  DCHECK_EQ(buffers.size(), pending_texture_mailboxes_.size());
  GLuint num_textures = base::checked_cast<GLuint>(buffers.size());
  std::vector<uint32_t> local_texture_ids(num_textures);
  gpu::gles2::GLES2Interface* gles2 = context_provider_->ContextGL();
  gles2->GenTextures(num_textures, &local_texture_ids.front());
  for (uint32_t i = 0; i < num_textures; i++) {
    gles2->ActiveTexture(GL_TEXTURE0);
    gles2->BindTexture(GL_TEXTURE_2D, local_texture_ids[i]);
    gles2->ConsumeTextureCHROMIUM(GL_TEXTURE_2D,
                                  pending_texture_mailboxes_[i].name);
    // Map the plugin texture id to the local texture id.
    uint32_t plugin_texture_id = buffers[i].texture_id();
    texture_id_map_[plugin_texture_id] = local_texture_ids[i];
    available_textures_.insert(plugin_texture_id);
  }
  pending_texture_mailboxes_.clear();
  SendPictures();
}

void VideoDecoderShim::ReusePictureBuffer(int32 picture_buffer_id) {
  DCHECK(RenderThreadImpl::current());
  uint32_t texture_id = static_cast<uint32_t>(picture_buffer_id);
  if (textures_to_dismiss_.find(texture_id) != textures_to_dismiss_.end()) {
    DismissTexture(texture_id);
  } else if (texture_id_map_.find(texture_id) != texture_id_map_.end()) {
    available_textures_.insert(texture_id);
    SendPictures();
  } else {
    NOTREACHED();
  }
}

void VideoDecoderShim::Flush() {
  DCHECK(RenderThreadImpl::current());
  DCHECK_EQ(state_, DECODING);
  state_ = FLUSHING;
}

void VideoDecoderShim::Reset() {
  DCHECK(RenderThreadImpl::current());
  DCHECK_EQ(state_, DECODING);
  state_ = RESETTING;
  media_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&VideoDecoderShim::DecoderImpl::Reset,
                 base::Unretained(decoder_impl_.get())));
}

void VideoDecoderShim::Destroy() {
  // This will be called, but our destructor does the actual work.
}

void VideoDecoderShim::OnInitializeComplete(int32_t result,
                                            uint32_t texture_pool_size) {
  DCHECK(RenderThreadImpl::current());
  DCHECK(host_);

  if (result == PP_OK) {
    state_ = DECODING;
    texture_pool_size_ = texture_pool_size;
  }

  host_->OnInitializeComplete(result);
}

void VideoDecoderShim::OnDecodeComplete(int32_t result, uint32_t decode_id) {
  DCHECK(RenderThreadImpl::current());
  DCHECK(host_);

  if (result == PP_ERROR_RESOURCE_FAILED) {
    host_->NotifyError(media::VideoDecodeAccelerator::PLATFORM_FAILURE);
    return;
  }

  num_pending_decodes_--;
  completed_decodes_.push(decode_id);

  // If frames are being queued because we're out of textures, don't notify
  // the host that decode has completed. This exerts "back pressure" to keep
  // the host from sending buffers that will cause pending_frames_ to grow.
  if (pending_frames_.empty())
    NotifyCompletedDecodes();
}

void VideoDecoderShim::OnOutputComplete(scoped_ptr<PendingFrame> frame) {
  DCHECK(RenderThreadImpl::current());
  DCHECK(host_);

  if (!frame->argb_pixels.empty()) {
    if (texture_size_ != frame->size) {
      // If the size has changed, all current textures must be dismissed. Add
      // all textures to |textures_to_dismiss_| and dismiss any that aren't in
      // use by the plugin. We will dismiss the rest as they are recycled.
      for (TextureIdMap::const_iterator it = texture_id_map_.begin();
           it != texture_id_map_.end();
           ++it) {
        textures_to_dismiss_.insert(it->second);
      }
      for (TextureIdSet::const_iterator it = available_textures_.begin();
           it != available_textures_.end();
           ++it) {
        DismissTexture(*it);
      }
      available_textures_.clear();
      FlushCommandBuffer();

      DCHECK(pending_texture_mailboxes_.empty());
      for (uint32_t i = 0; i < texture_pool_size_; i++)
        pending_texture_mailboxes_.push_back(gpu::Mailbox::Generate());

      host_->RequestTextures(texture_pool_size_,
                             frame->size,
                             GL_TEXTURE_2D,
                             pending_texture_mailboxes_);
      texture_size_ = frame->size;
    }

    pending_frames_.push(linked_ptr<PendingFrame>(frame.release()));
    SendPictures();
  }
}

void VideoDecoderShim::SendPictures() {
  DCHECK(RenderThreadImpl::current());
  DCHECK(host_);
  while (!pending_frames_.empty() && !available_textures_.empty()) {
    const linked_ptr<PendingFrame>& frame = pending_frames_.front();

    TextureIdSet::iterator it = available_textures_.begin();
    uint32_t texture_id = *it;
    available_textures_.erase(it);

    uint32_t local_texture_id = texture_id_map_[texture_id];
    gpu::gles2::GLES2Interface* gles2 = context_provider_->ContextGL();
    gles2->ActiveTexture(GL_TEXTURE0);
    gles2->BindTexture(GL_TEXTURE_2D, local_texture_id);
    gles2->TexImage2D(GL_TEXTURE_2D,
                      0,
                      GL_RGBA,
                      texture_size_.width(),
                      texture_size_.height(),
                      0,
                      GL_RGBA,
                      GL_UNSIGNED_BYTE,
                      &frame->argb_pixels.front());

    host_->PictureReady(media::Picture(texture_id, frame->decode_id));
    pending_frames_.pop();
  }

  FlushCommandBuffer();

  if (pending_frames_.empty()) {
    // If frames aren't backing up, notify the host of any completed decodes so
    // it can send more buffers.
    NotifyCompletedDecodes();

    if (state_ == FLUSHING && !num_pending_decodes_) {
      state_ = DECODING;
      host_->NotifyFlushDone();
    }
  }
}

void VideoDecoderShim::OnResetComplete() {
  DCHECK(RenderThreadImpl::current());
  DCHECK(host_);

  while (!pending_frames_.empty())
    pending_frames_.pop();
  NotifyCompletedDecodes();

  // Dismiss any old textures now.
  while (!textures_to_dismiss_.empty())
    DismissTexture(*textures_to_dismiss_.begin());

  state_ = DECODING;
  host_->NotifyResetDone();
}

void VideoDecoderShim::NotifyCompletedDecodes() {
  while (!completed_decodes_.empty()) {
    host_->NotifyEndOfBitstreamBuffer(completed_decodes_.front());
    completed_decodes_.pop();
  }
}

void VideoDecoderShim::DismissTexture(uint32_t texture_id) {
  DCHECK(host_);
  textures_to_dismiss_.erase(texture_id);
  DCHECK(texture_id_map_.find(texture_id) != texture_id_map_.end());
  DeleteTexture(texture_id_map_[texture_id]);
  texture_id_map_.erase(texture_id);
  host_->DismissPictureBuffer(texture_id);
}

void VideoDecoderShim::DeleteTexture(uint32_t texture_id) {
  gpu::gles2::GLES2Interface* gles2 = context_provider_->ContextGL();
  gles2->DeleteTextures(1, &texture_id);
}

void VideoDecoderShim::FlushCommandBuffer() {
  context_provider_->ContextGL()->Flush();
}

}  // namespace content
