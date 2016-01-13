// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_video_decoder.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/task_runner_util.h"
#include "content/child/child_thread.h"
#include "content/renderer/media/native_handle_impl.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/bind_to_current_loop.h"
#include "media/filters/gpu_video_accelerator_factories.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/webrtc/common_video/interface/texture_video_frame.h"
#include "third_party/webrtc/system_wrappers/interface/ref_count.h"

namespace content {

const int32 RTCVideoDecoder::ID_LAST = 0x3FFFFFFF;
const int32 RTCVideoDecoder::ID_HALF = 0x20000000;
const int32 RTCVideoDecoder::ID_INVALID = -1;

// Maximum number of concurrent VDA::Decode() operations RVD will maintain.
// Higher values allow better pipelining in the GPU, but also require more
// resources.
static const size_t kMaxInFlightDecodes = 8;

// Size of shared-memory segments we allocate.  Since we reuse them we let them
// be on the beefy side.
static const size_t kSharedMemorySegmentBytes = 100 << 10;

// Maximum number of allocated shared-memory segments.
static const int kMaxNumSharedMemorySegments = 16;

// Maximum number of pending WebRTC buffers that are waiting for the shared
// memory. 10 seconds for 30 fps.
static const size_t kMaxNumOfPendingBuffers = 300;

// A shared memory segment and its allocated size. This class has the ownership
// of |shm|.
class RTCVideoDecoder::SHMBuffer {
 public:
  SHMBuffer(base::SharedMemory* shm, size_t size);
  ~SHMBuffer();
  base::SharedMemory* const shm;
  const size_t size;
};

RTCVideoDecoder::SHMBuffer::SHMBuffer(base::SharedMemory* shm, size_t size)
    : shm(shm), size(size) {}

RTCVideoDecoder::SHMBuffer::~SHMBuffer() { shm->Close(); }

RTCVideoDecoder::BufferData::BufferData(int32 bitstream_buffer_id,
                                        uint32_t timestamp,
                                        int width,
                                        int height,
                                        size_t size)
    : bitstream_buffer_id(bitstream_buffer_id),
      timestamp(timestamp),
      width(width),
      height(height),
      size(size) {}

RTCVideoDecoder::BufferData::BufferData() {}

RTCVideoDecoder::BufferData::~BufferData() {}

RTCVideoDecoder::RTCVideoDecoder(
    const scoped_refptr<media::GpuVideoAcceleratorFactories>& factories)
    : factories_(factories),
      decoder_texture_target_(0),
      next_picture_buffer_id_(0),
      state_(UNINITIALIZED),
      decode_complete_callback_(NULL),
      num_shm_buffers_(0),
      next_bitstream_buffer_id_(0),
      reset_bitstream_buffer_id_(ID_INVALID),
      weak_factory_(this) {
  DCHECK(!factories_->GetTaskRunner()->BelongsToCurrentThread());
}

RTCVideoDecoder::~RTCVideoDecoder() {
  DVLOG(2) << "~RTCVideoDecoder";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  DestroyVDA();

  // Delete all shared memories.
  STLDeleteElements(&available_shm_segments_);
  STLDeleteValues(&bitstream_buffers_in_decoder_);
  STLDeleteContainerPairFirstPointers(decode_buffers_.begin(),
                                      decode_buffers_.end());
  decode_buffers_.clear();

  // Delete WebRTC input buffers.
  for (std::deque<std::pair<webrtc::EncodedImage, BufferData> >::iterator it =
           pending_buffers_.begin();
       it != pending_buffers_.end();
       ++it) {
    delete[] it->first._buffer;
  }
}

// static
scoped_ptr<RTCVideoDecoder> RTCVideoDecoder::Create(
    webrtc::VideoCodecType type,
    const scoped_refptr<media::GpuVideoAcceleratorFactories>& factories) {
  scoped_ptr<RTCVideoDecoder> decoder;
  // Convert WebRTC codec type to media codec profile.
  media::VideoCodecProfile profile;
  switch (type) {
    case webrtc::kVideoCodecVP8:
      profile = media::VP8PROFILE_MAIN;
      break;
    default:
      DVLOG(2) << "Video codec not supported:" << type;
      return decoder.Pass();
  }

  base::WaitableEvent waiter(true, false);
  decoder.reset(new RTCVideoDecoder(factories));
  decoder->factories_->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&RTCVideoDecoder::CreateVDA,
                 base::Unretained(decoder.get()),
                 profile,
                 &waiter));
  waiter.Wait();
  // vda can be NULL if VP8 is not supported.
  if (decoder->vda_ != NULL) {
    decoder->state_ = INITIALIZED;
  } else {
    factories->GetTaskRunner()->DeleteSoon(FROM_HERE, decoder.release());
  }
  return decoder.Pass();
}

int32_t RTCVideoDecoder::InitDecode(const webrtc::VideoCodec* codecSettings,
                                    int32_t /*numberOfCores*/) {
  DVLOG(2) << "InitDecode";
  DCHECK_EQ(codecSettings->codecType, webrtc::kVideoCodecVP8);
  if (codecSettings->codecSpecific.VP8.feedbackModeOn) {
    LOG(ERROR) << "Feedback mode not supported";
    return RecordInitDecodeUMA(WEBRTC_VIDEO_CODEC_ERROR);
  }

  base::AutoLock auto_lock(lock_);
  if (state_ == UNINITIALIZED || state_ == DECODE_ERROR) {
    LOG(ERROR) << "VDA is not initialized. state=" << state_;
    return RecordInitDecodeUMA(WEBRTC_VIDEO_CODEC_UNINITIALIZED);
  }
  // Create some shared memory if the queue is empty.
  if (available_shm_segments_.size() == 0) {
    factories_->GetTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&RTCVideoDecoder::CreateSHM,
                   weak_factory_.GetWeakPtr(),
                   kMaxInFlightDecodes,
                   kSharedMemorySegmentBytes));
  }
  return RecordInitDecodeUMA(WEBRTC_VIDEO_CODEC_OK);
}

int32_t RTCVideoDecoder::Decode(
    const webrtc::EncodedImage& inputImage,
    bool missingFrames,
    const webrtc::RTPFragmentationHeader* /*fragmentation*/,
    const webrtc::CodecSpecificInfo* /*codecSpecificInfo*/,
    int64_t /*renderTimeMs*/) {
  DVLOG(3) << "Decode";

  base::AutoLock auto_lock(lock_);

  if (state_ == UNINITIALIZED || decode_complete_callback_ == NULL) {
    LOG(ERROR) << "The decoder has not initialized.";
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  if (state_ == DECODE_ERROR) {
    LOG(ERROR) << "Decoding error occurred.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  if (missingFrames || !inputImage._completeFrame) {
    DLOG(ERROR) << "Missing or incomplete frames.";
    // Unlike the SW decoder in libvpx, hw decoder cannot handle broken frames.
    // Return an error to request a key frame.
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  // Most platforms' VDA implementations support mid-stream resolution change
  // internally.  Platforms whose VDAs fail to support mid-stream resolution
  // change gracefully need to have their clients cover for them, and we do that
  // here.
#ifdef ANDROID
  const bool kVDACanHandleMidstreamResize = false;
#else
  const bool kVDACanHandleMidstreamResize = true;
#endif

  bool need_to_reset_for_midstream_resize = false;
  if (inputImage._frameType == webrtc::kKeyFrame) {
    DVLOG(2) << "Got key frame. size=" << inputImage._encodedWidth << "x"
             << inputImage._encodedHeight;
    gfx::Size prev_frame_size = frame_size_;
    frame_size_.SetSize(inputImage._encodedWidth, inputImage._encodedHeight);
    if (!kVDACanHandleMidstreamResize && !prev_frame_size.IsEmpty() &&
        prev_frame_size != frame_size_) {
      need_to_reset_for_midstream_resize = true;
    }
  } else if (IsFirstBufferAfterReset(next_bitstream_buffer_id_,
                                     reset_bitstream_buffer_id_)) {
    // TODO(wuchengli): VDA should handle it. Remove this when
    // http://crosbug.com/p/21913 is fixed.
    DVLOG(1) << "The first frame should be a key frame. Drop this.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  // Create buffer metadata.
  BufferData buffer_data(next_bitstream_buffer_id_,
                         inputImage._timeStamp,
                         frame_size_.width(),
                         frame_size_.height(),
                         inputImage._length);
  // Mask against 30 bits, to avoid (undefined) wraparound on signed integer.
  next_bitstream_buffer_id_ = (next_bitstream_buffer_id_ + 1) & ID_LAST;

  // If a shared memory segment is available, there are no pending buffers, and
  // this isn't a mid-stream resolution change, then send the buffer for decode
  // immediately. Otherwise, save the buffer in the queue for later decode.
  scoped_ptr<SHMBuffer> shm_buffer;
  if (!need_to_reset_for_midstream_resize && pending_buffers_.size() == 0)
    shm_buffer = GetSHM_Locked(inputImage._length);
  if (!shm_buffer) {
    if (!SaveToPendingBuffers_Locked(inputImage, buffer_data))
      return WEBRTC_VIDEO_CODEC_ERROR;
    if (need_to_reset_for_midstream_resize) {
      base::AutoUnlock auto_unlock(lock_);
      Reset();
    }
    return WEBRTC_VIDEO_CODEC_OK;
  }

  SaveToDecodeBuffers_Locked(inputImage, shm_buffer.Pass(), buffer_data);
  factories_->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&RTCVideoDecoder::RequestBufferDecode,
                 weak_factory_.GetWeakPtr()));
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RTCVideoDecoder::RegisterDecodeCompleteCallback(
    webrtc::DecodedImageCallback* callback) {
  DVLOG(2) << "RegisterDecodeCompleteCallback";
  base::AutoLock auto_lock(lock_);
  decode_complete_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RTCVideoDecoder::Release() {
  DVLOG(2) << "Release";
  // Do not destroy VDA because WebRTC can call InitDecode and start decoding
  // again.
  return Reset();
}

int32_t RTCVideoDecoder::Reset() {
  DVLOG(2) << "Reset";
  base::AutoLock auto_lock(lock_);
  if (state_ == UNINITIALIZED) {
    LOG(ERROR) << "Decoder not initialized.";
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (next_bitstream_buffer_id_ != 0)
    reset_bitstream_buffer_id_ = next_bitstream_buffer_id_ - 1;
  else
    reset_bitstream_buffer_id_ = ID_LAST;
  // If VDA is already resetting, no need to request the reset again.
  if (state_ != RESETTING) {
    state_ = RESETTING;
    factories_->GetTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&RTCVideoDecoder::ResetInternal,
                   weak_factory_.GetWeakPtr()));
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

void RTCVideoDecoder::ProvidePictureBuffers(uint32 count,
                                            const gfx::Size& size,
                                            uint32 texture_target) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  DVLOG(3) << "ProvidePictureBuffers. texture_target=" << texture_target;

  if (!vda_)
    return;

  std::vector<uint32> texture_ids;
  std::vector<gpu::Mailbox> texture_mailboxes;
  decoder_texture_target_ = texture_target;
  if (!factories_->CreateTextures(count,
                                  size,
                                  &texture_ids,
                                  &texture_mailboxes,
                                  decoder_texture_target_)) {
    NotifyError(media::VideoDecodeAccelerator::PLATFORM_FAILURE);
    return;
  }
  DCHECK_EQ(count, texture_ids.size());
  DCHECK_EQ(count, texture_mailboxes.size());

  std::vector<media::PictureBuffer> picture_buffers;
  for (size_t i = 0; i < texture_ids.size(); ++i) {
    picture_buffers.push_back(media::PictureBuffer(
        next_picture_buffer_id_++, size, texture_ids[i], texture_mailboxes[i]));
    bool inserted = assigned_picture_buffers_.insert(std::make_pair(
        picture_buffers.back().id(), picture_buffers.back())).second;
    DCHECK(inserted);
  }
  vda_->AssignPictureBuffers(picture_buffers);
}

void RTCVideoDecoder::DismissPictureBuffer(int32 id) {
  DVLOG(3) << "DismissPictureBuffer. id=" << id;
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  std::map<int32, media::PictureBuffer>::iterator it =
      assigned_picture_buffers_.find(id);
  if (it == assigned_picture_buffers_.end()) {
    NOTREACHED() << "Missing picture buffer: " << id;
    return;
  }

  media::PictureBuffer buffer_to_dismiss = it->second;
  assigned_picture_buffers_.erase(it);

  if (!picture_buffers_at_display_.count(id)) {
    // We can delete the texture immediately as it's not being displayed.
    factories_->DeleteTexture(buffer_to_dismiss.texture_id());
    return;
  }
  // Not destroying a texture in display in |picture_buffers_at_display_|.
  // Postpone deletion until after it's returned to us.
}

void RTCVideoDecoder::PictureReady(const media::Picture& picture) {
  DVLOG(3) << "PictureReady";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  std::map<int32, media::PictureBuffer>::iterator it =
      assigned_picture_buffers_.find(picture.picture_buffer_id());
  if (it == assigned_picture_buffers_.end()) {
    NOTREACHED() << "Missing picture buffer: " << picture.picture_buffer_id();
    NotifyError(media::VideoDecodeAccelerator::PLATFORM_FAILURE);
    return;
  }
  const media::PictureBuffer& pb = it->second;

  // Create a media::VideoFrame.
  uint32_t timestamp = 0, width = 0, height = 0;
  size_t size = 0;
  GetBufferData(
      picture.bitstream_buffer_id(), &timestamp, &width, &height, &size);
  scoped_refptr<media::VideoFrame> frame =
      CreateVideoFrame(picture, pb, timestamp, width, height, size);
  bool inserted =
      picture_buffers_at_display_.insert(std::make_pair(
                                             picture.picture_buffer_id(),
                                             pb.texture_id())).second;
  DCHECK(inserted);

  // Create a WebRTC video frame.
  webrtc::RefCountImpl<NativeHandleImpl>* handle =
      new webrtc::RefCountImpl<NativeHandleImpl>(frame);
  webrtc::TextureVideoFrame decoded_image(handle, width, height, timestamp, 0);

  // Invoke decode callback. WebRTC expects no callback after Reset or Release.
  {
    base::AutoLock auto_lock(lock_);
    DCHECK(decode_complete_callback_ != NULL);
    if (IsBufferAfterReset(picture.bitstream_buffer_id(),
                           reset_bitstream_buffer_id_)) {
      decode_complete_callback_->Decoded(decoded_image);
    }
  }
}

static void ReadPixelsSyncInner(
    const scoped_refptr<media::GpuVideoAcceleratorFactories>& factories,
    uint32 texture_id,
    const gfx::Rect& visible_rect,
    const SkBitmap& pixels,
    base::WaitableEvent* event) {
  factories->ReadPixels(texture_id, visible_rect, pixels);
  event->Signal();
}

static void ReadPixelsSync(
    const scoped_refptr<media::GpuVideoAcceleratorFactories>& factories,
    uint32 texture_id,
    const gfx::Rect& visible_rect,
    const SkBitmap& pixels) {
  base::WaitableEvent event(true, false);
  if (!factories->GetTaskRunner()->PostTask(FROM_HERE,
                                            base::Bind(&ReadPixelsSyncInner,
                                                       factories,
                                                       texture_id,
                                                       visible_rect,
                                                       pixels,
                                                       &event)))
    return;
  event.Wait();
}

scoped_refptr<media::VideoFrame> RTCVideoDecoder::CreateVideoFrame(
    const media::Picture& picture,
    const media::PictureBuffer& pb,
    uint32_t timestamp,
    uint32_t width,
    uint32_t height,
    size_t size) {
  gfx::Rect visible_rect(width, height);
  DCHECK(decoder_texture_target_);
  // Convert timestamp from 90KHz to ms.
  base::TimeDelta timestamp_ms = base::TimeDelta::FromInternalValue(
      base::checked_cast<uint64_t>(timestamp) * 1000 / 90);
  return media::VideoFrame::WrapNativeTexture(
      make_scoped_ptr(new gpu::MailboxHolder(
          pb.texture_mailbox(), decoder_texture_target_, 0)),
      media::BindToCurrentLoop(base::Bind(&RTCVideoDecoder::ReleaseMailbox,
                                          weak_factory_.GetWeakPtr(),
                                          factories_,
                                          picture.picture_buffer_id(),
                                          pb.texture_id())),
      pb.size(),
      visible_rect,
      visible_rect.size(),
      timestamp_ms,
      base::Bind(&ReadPixelsSync, factories_, pb.texture_id(), visible_rect));
}

void RTCVideoDecoder::NotifyEndOfBitstreamBuffer(int32 id) {
  DVLOG(3) << "NotifyEndOfBitstreamBuffer. id=" << id;
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  std::map<int32, SHMBuffer*>::iterator it =
      bitstream_buffers_in_decoder_.find(id);
  if (it == bitstream_buffers_in_decoder_.end()) {
    NotifyError(media::VideoDecodeAccelerator::PLATFORM_FAILURE);
    NOTREACHED() << "Missing bitstream buffer: " << id;
    return;
  }

  {
    base::AutoLock auto_lock(lock_);
    PutSHM_Locked(scoped_ptr<SHMBuffer>(it->second));
  }
  bitstream_buffers_in_decoder_.erase(it);

  RequestBufferDecode();
}

void RTCVideoDecoder::NotifyFlushDone() {
  DVLOG(3) << "NotifyFlushDone";
  NOTREACHED() << "Unexpected flush done notification.";
}

void RTCVideoDecoder::NotifyResetDone() {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  DVLOG(3) << "NotifyResetDone";

  if (!vda_)
    return;

  input_buffer_data_.clear();
  {
    base::AutoLock auto_lock(lock_);
    state_ = INITIALIZED;
  }
  // Send the pending buffers for decoding.
  RequestBufferDecode();
}

void RTCVideoDecoder::NotifyError(media::VideoDecodeAccelerator::Error error) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  if (!vda_)
    return;

  LOG(ERROR) << "VDA Error:" << error;
  UMA_HISTOGRAM_ENUMERATION("Media.RTCVideoDecoderError",
                            error,
                            media::VideoDecodeAccelerator::LARGEST_ERROR_ENUM);
  DestroyVDA();

  base::AutoLock auto_lock(lock_);
  state_ = DECODE_ERROR;
}

void RTCVideoDecoder::RequestBufferDecode() {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  if (!vda_)
    return;

  MovePendingBuffersToDecodeBuffers();

  while (CanMoreDecodeWorkBeDone()) {
    // Get a buffer and data from the queue.
    SHMBuffer* shm_buffer = NULL;
    BufferData buffer_data;
    {
      base::AutoLock auto_lock(lock_);
      // Do not request decode if VDA is resetting.
      if (decode_buffers_.size() == 0 || state_ == RESETTING)
        return;
      shm_buffer = decode_buffers_.front().first;
      buffer_data = decode_buffers_.front().second;
      decode_buffers_.pop_front();
      // Drop the buffers before Reset or Release is called.
      if (!IsBufferAfterReset(buffer_data.bitstream_buffer_id,
                              reset_bitstream_buffer_id_)) {
        PutSHM_Locked(scoped_ptr<SHMBuffer>(shm_buffer));
        continue;
      }
    }

    // Create a BitstreamBuffer and send to VDA to decode.
    media::BitstreamBuffer bitstream_buffer(buffer_data.bitstream_buffer_id,
                                            shm_buffer->shm->handle(),
                                            buffer_data.size);
    bool inserted = bitstream_buffers_in_decoder_
        .insert(std::make_pair(bitstream_buffer.id(), shm_buffer)).second;
    DCHECK(inserted);
    RecordBufferData(buffer_data);
    vda_->Decode(bitstream_buffer);
  }
}

bool RTCVideoDecoder::CanMoreDecodeWorkBeDone() {
  return bitstream_buffers_in_decoder_.size() < kMaxInFlightDecodes;
}

bool RTCVideoDecoder::IsBufferAfterReset(int32 id_buffer, int32 id_reset) {
  if (id_reset == ID_INVALID)
    return true;
  int32 diff = id_buffer - id_reset;
  if (diff <= 0)
    diff += ID_LAST + 1;
  return diff < ID_HALF;
}

bool RTCVideoDecoder::IsFirstBufferAfterReset(int32 id_buffer, int32 id_reset) {
  if (id_reset == ID_INVALID)
    return id_buffer == 0;
  return id_buffer == ((id_reset + 1) & ID_LAST);
}

void RTCVideoDecoder::SaveToDecodeBuffers_Locked(
    const webrtc::EncodedImage& input_image,
    scoped_ptr<SHMBuffer> shm_buffer,
    const BufferData& buffer_data) {
  memcpy(shm_buffer->shm->memory(), input_image._buffer, input_image._length);
  std::pair<SHMBuffer*, BufferData> buffer_pair =
      std::make_pair(shm_buffer.release(), buffer_data);

  // Store the buffer and the metadata to the queue.
  decode_buffers_.push_back(buffer_pair);
}

bool RTCVideoDecoder::SaveToPendingBuffers_Locked(
    const webrtc::EncodedImage& input_image,
    const BufferData& buffer_data) {
  DVLOG(2) << "SaveToPendingBuffers_Locked"
           << ". pending_buffers size=" << pending_buffers_.size()
           << ". decode_buffers_ size=" << decode_buffers_.size()
           << ". available_shm size=" << available_shm_segments_.size();
  // Queued too many buffers. Something goes wrong.
  if (pending_buffers_.size() >= kMaxNumOfPendingBuffers) {
    LOG(WARNING) << "Too many pending buffers!";
    return false;
  }

  // Clone the input image and save it to the queue.
  uint8_t* buffer = new uint8_t[input_image._length];
  // TODO(wuchengli): avoid memcpy. Extend webrtc::VideoDecoder::Decode()
  // interface to take a non-const ptr to the frame and add a method to the
  // frame that will swap buffers with another.
  memcpy(buffer, input_image._buffer, input_image._length);
  webrtc::EncodedImage encoded_image(
      buffer, input_image._length, input_image._length);
  std::pair<webrtc::EncodedImage, BufferData> buffer_pair =
      std::make_pair(encoded_image, buffer_data);

  pending_buffers_.push_back(buffer_pair);
  return true;
}

void RTCVideoDecoder::MovePendingBuffersToDecodeBuffers() {
  base::AutoLock auto_lock(lock_);
  while (pending_buffers_.size() > 0) {
    // Get a pending buffer from the queue.
    const webrtc::EncodedImage& input_image = pending_buffers_.front().first;
    const BufferData& buffer_data = pending_buffers_.front().second;

    // Drop the frame if it comes before Reset or Release.
    if (!IsBufferAfterReset(buffer_data.bitstream_buffer_id,
                            reset_bitstream_buffer_id_)) {
      delete[] input_image._buffer;
      pending_buffers_.pop_front();
      continue;
    }
    // Get shared memory and save it to decode buffers.
    scoped_ptr<SHMBuffer> shm_buffer = GetSHM_Locked(input_image._length);
    if (!shm_buffer)
      return;
    SaveToDecodeBuffers_Locked(input_image, shm_buffer.Pass(), buffer_data);
    delete[] input_image._buffer;
    pending_buffers_.pop_front();
  }
}

void RTCVideoDecoder::ResetInternal() {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  DVLOG(2) << "ResetInternal";
  if (vda_)
    vda_->Reset();
}

// static
void RTCVideoDecoder::ReleaseMailbox(
    base::WeakPtr<RTCVideoDecoder> decoder,
    const scoped_refptr<media::GpuVideoAcceleratorFactories>& factories,
    int64 picture_buffer_id,
    uint32 texture_id,
    const std::vector<uint32>& release_sync_points) {
  DCHECK(factories->GetTaskRunner()->BelongsToCurrentThread());

  for (size_t i = 0; i < release_sync_points.size(); i++)
    factories->WaitSyncPoint(release_sync_points[i]);

  if (decoder) {
    decoder->ReusePictureBuffer(picture_buffer_id);
    return;
  }
  // It's the last chance to delete the texture after display,
  // because RTCVideoDecoder was destructed.
  factories->DeleteTexture(texture_id);
}

void RTCVideoDecoder::ReusePictureBuffer(int64 picture_buffer_id) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  DVLOG(3) << "ReusePictureBuffer. id=" << picture_buffer_id;

  DCHECK(!picture_buffers_at_display_.empty());
  PictureBufferTextureMap::iterator display_iterator =
      picture_buffers_at_display_.find(picture_buffer_id);
  uint32 texture_id = display_iterator->second;
  DCHECK(display_iterator != picture_buffers_at_display_.end());
  picture_buffers_at_display_.erase(display_iterator);

  if (!assigned_picture_buffers_.count(picture_buffer_id)) {
    // This picture was dismissed while in display, so we postponed deletion.
    factories_->DeleteTexture(texture_id);
    return;
  }

  // DestroyVDA() might already have been called.
  if (vda_)
    vda_->ReusePictureBuffer(picture_buffer_id);
}

void RTCVideoDecoder::CreateVDA(media::VideoCodecProfile profile,
                                base::WaitableEvent* waiter) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  vda_ = factories_->CreateVideoDecodeAccelerator();
  if (vda_ && !vda_->Initialize(profile, this))
    vda_.release()->Destroy();
  waiter->Signal();
}

void RTCVideoDecoder::DestroyTextures() {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  // Not destroying PictureBuffers in |picture_buffers_at_display_| yet, since
  // their textures may still be in use by the user of this RTCVideoDecoder.
  for (PictureBufferTextureMap::iterator it =
           picture_buffers_at_display_.begin();
       it != picture_buffers_at_display_.end();
       ++it) {
    assigned_picture_buffers_.erase(it->first);
  }

  for (std::map<int32, media::PictureBuffer>::iterator it =
           assigned_picture_buffers_.begin();
       it != assigned_picture_buffers_.end();
       ++it) {
    factories_->DeleteTexture(it->second.texture_id());
  }
  assigned_picture_buffers_.clear();
}

void RTCVideoDecoder::DestroyVDA() {
  DVLOG(2) << "DestroyVDA";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  if (vda_)
    vda_.release()->Destroy();
  DestroyTextures();
  base::AutoLock auto_lock(lock_);
  state_ = UNINITIALIZED;
}

scoped_ptr<RTCVideoDecoder::SHMBuffer> RTCVideoDecoder::GetSHM_Locked(
    size_t min_size) {
  // Reuse a SHM if possible.
  SHMBuffer* ret = NULL;
  if (!available_shm_segments_.empty() &&
      available_shm_segments_.back()->size >= min_size) {
    ret = available_shm_segments_.back();
    available_shm_segments_.pop_back();
  }
  // Post to vda thread to create shared memory if SHM cannot be reused or the
  // queue is almost empty.
  if (num_shm_buffers_ < kMaxNumSharedMemorySegments &&
      (ret == NULL || available_shm_segments_.size() <= 1)) {
    factories_->GetTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&RTCVideoDecoder::CreateSHM,
                   weak_factory_.GetWeakPtr(),
                   1,
                   min_size));
  }
  return scoped_ptr<SHMBuffer>(ret);
}

void RTCVideoDecoder::PutSHM_Locked(scoped_ptr<SHMBuffer> shm_buffer) {
  available_shm_segments_.push_back(shm_buffer.release());
}

void RTCVideoDecoder::CreateSHM(int number, size_t min_size) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  DVLOG(2) << "CreateSHM. size=" << min_size;
  int number_to_allocate;
  {
    base::AutoLock auto_lock(lock_);
    number_to_allocate =
        std::min(kMaxNumSharedMemorySegments - num_shm_buffers_, number);
  }
  size_t size_to_allocate = std::max(min_size, kSharedMemorySegmentBytes);
  for (int i = 0; i < number_to_allocate; i++) {
    base::SharedMemory* shm = factories_->CreateSharedMemory(size_to_allocate);
    if (shm != NULL) {
      base::AutoLock auto_lock(lock_);
      num_shm_buffers_++;
      PutSHM_Locked(
          scoped_ptr<SHMBuffer>(new SHMBuffer(shm, size_to_allocate)));
    }
  }
  // Kick off the decoding.
  RequestBufferDecode();
}

void RTCVideoDecoder::RecordBufferData(const BufferData& buffer_data) {
  input_buffer_data_.push_front(buffer_data);
  // Why this value?  Because why not.  avformat.h:MAX_REORDER_DELAY is 16, but
  // that's too small for some pathological B-frame test videos.  The cost of
  // using too-high a value is low (192 bits per extra slot).
  static const size_t kMaxInputBufferDataSize = 128;
  // Pop from the back of the list, because that's the oldest and least likely
  // to be useful in the future data.
  if (input_buffer_data_.size() > kMaxInputBufferDataSize)
    input_buffer_data_.pop_back();
}

void RTCVideoDecoder::GetBufferData(int32 bitstream_buffer_id,
                                    uint32_t* timestamp,
                                    uint32_t* width,
                                    uint32_t* height,
                                    size_t* size) {
  for (std::list<BufferData>::iterator it = input_buffer_data_.begin();
       it != input_buffer_data_.end();
       ++it) {
    if (it->bitstream_buffer_id != bitstream_buffer_id)
      continue;
    *timestamp = it->timestamp;
    *width = it->width;
    *height = it->height;
    return;
  }
  NOTREACHED() << "Missing bitstream buffer id: " << bitstream_buffer_id;
}

int32_t RTCVideoDecoder::RecordInitDecodeUMA(int32_t status) {
  // Logging boolean is enough to know if HW decoding has been used. Also,
  // InitDecode is less likely to return an error so enum is not used here.
  bool sample = (status == WEBRTC_VIDEO_CODEC_OK) ? true : false;
  UMA_HISTOGRAM_BOOLEAN("Media.RTCVideoDecoderInitDecodeSuccess", sample);
  return status;
}

void RTCVideoDecoder::DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent()
    const {
  DCHECK(factories_->GetTaskRunner()->BelongsToCurrentThread());
}

}  // namespace content
