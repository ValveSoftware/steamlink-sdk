// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/android_video_decode_accelerator.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "content/common/gpu/gpu_channel.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/limits.h"
#include "media/video/picture.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"

namespace content {

// Helper macros for dealing with failure.  If |result| evaluates false, emit
// |log| to ERROR, register |error| with the decoder, and return.
#define RETURN_ON_FAILURE(result, log, error)                     \
  do {                                                            \
    if (!(result)) {                                              \
      DLOG(ERROR) << log;                                         \
      base::MessageLoop::current()->PostTask(                     \
          FROM_HERE,                                              \
          base::Bind(&AndroidVideoDecodeAccelerator::NotifyError, \
                     weak_this_factory_.GetWeakPtr(),             \
                     error));                                     \
      state_ = ERROR;                                             \
      return;                                                     \
    }                                                             \
  } while (0)

// TODO(dwkang): We only need kMaxVideoFrames to pass media stack's prerolling
// phase, but 1 is added due to crbug.com/176036. This should be tuned when we
// have actual use case.
enum { kNumPictureBuffers = media::limits::kMaxVideoFrames + 1 };

// Max number of bitstreams notified to the client with
// NotifyEndOfBitstreamBuffer() before getting output from the bitstream.
enum { kMaxBitstreamsNotifiedInAdvance = 32 };

// Because MediaCodec is thread-hostile (must be poked on a single thread) and
// has no callback mechanism (b/11990118), we must drive it by polling for
// complete frames (and available input buffers, when the codec is fully
// saturated).  This function defines the polling delay.  The value used is an
// arbitrary choice that trades off CPU utilization (spinning) against latency.
// Mirrors android_video_encode_accelerator.cc:EncodePollDelay().
static inline const base::TimeDelta DecodePollDelay() {
  // An alternative to this polling scheme could be to dedicate a new thread
  // (instead of using the ChildThread) to run the MediaCodec, and make that
  // thread use the timeout-based flavor of MediaCodec's dequeue methods when it
  // believes the codec should complete "soon" (e.g. waiting for an input
  // buffer, or waiting for a picture when it knows enough complete input
  // pictures have been fed to saturate any internal buffering).  This is
  // speculative and it's unclear that this would be a win (nor that there's a
  // reasonably device-agnostic way to fill in the "believes" above).
  return base::TimeDelta::FromMilliseconds(10);
}

static inline const base::TimeDelta NoWaitTimeOut() {
  return base::TimeDelta::FromMicroseconds(0);
}

AndroidVideoDecodeAccelerator::AndroidVideoDecodeAccelerator(
    const base::WeakPtr<gpu::gles2::GLES2Decoder> decoder,
    const base::Callback<bool(void)>& make_context_current)
    : client_(NULL),
      make_context_current_(make_context_current),
      codec_(media::kCodecH264),
      state_(NO_ERROR),
      surface_texture_id_(0),
      picturebuffers_requested_(false),
      gl_decoder_(decoder),
      weak_this_factory_(this) {}

AndroidVideoDecodeAccelerator::~AndroidVideoDecodeAccelerator() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

bool AndroidVideoDecodeAccelerator::Initialize(media::VideoCodecProfile profile,
                                               Client* client) {
  DCHECK(!media_codec_);
  DCHECK(thread_checker_.CalledOnValidThread());

  client_ = client;

  if (profile == media::VP8PROFILE_MAIN) {
    codec_ = media::kCodecVP8;
  } else {
    // TODO(dwkang): enable H264 once b/8125974 is fixed.
    LOG(ERROR) << "Unsupported profile: " << profile;
    return false;
  }

  // Only consider using MediaCodec if it's likely backed by hardware.
  if (media::VideoCodecBridge::IsKnownUnaccelerated(
          codec_, media::MEDIA_CODEC_DECODER)) {
    return false;
  }

  if (!make_context_current_.Run()) {
    LOG(ERROR) << "Failed to make this decoder's GL context current.";
    return false;
  }

  if (!gl_decoder_) {
    LOG(ERROR) << "Failed to get gles2 decoder instance.";
    return false;
  }
  glGenTextures(1, &surface_texture_id_);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, surface_texture_id_);

  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES,
                  GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES,
                  GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl_decoder_->RestoreTextureUnitBindings(0);
  gl_decoder_->RestoreActiveTexture();

  surface_texture_ = gfx::SurfaceTexture::Create(surface_texture_id_);

  if (!ConfigureMediaCodec()) {
    LOG(ERROR) << "Failed to create MediaCodec instance.";
    return false;
  }

  return true;
}

void AndroidVideoDecodeAccelerator::DoIOTask() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (state_ == ERROR) {
    return;
  }

  QueueInput();
  DequeueOutput();
}

void AndroidVideoDecodeAccelerator::QueueInput() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (bitstreams_notified_in_advance_.size() > kMaxBitstreamsNotifiedInAdvance)
    return;
  if (pending_bitstream_buffers_.empty())
    return;

  int input_buf_index = 0;
  media::MediaCodecStatus status = media_codec_->DequeueInputBuffer(
      NoWaitTimeOut(), &input_buf_index);
  if (status != media::MEDIA_CODEC_OK) {
    DCHECK(status == media::MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER ||
           status == media::MEDIA_CODEC_ERROR);
    return;
  }

  base::Time queued_time = pending_bitstream_buffers_.front().second;
  UMA_HISTOGRAM_TIMES("Media.AVDA.InputQueueTime",
                      base::Time::Now() - queued_time);
  media::BitstreamBuffer bitstream_buffer =
      pending_bitstream_buffers_.front().first;
  pending_bitstream_buffers_.pop();

  if (bitstream_buffer.id() == -1) {
    media_codec_->QueueEOS(input_buf_index);
    return;
  }

  // Abuse the presentation time argument to propagate the bitstream
  // buffer ID to the output, so we can report it back to the client in
  // PictureReady().
  base::TimeDelta timestamp =
      base::TimeDelta::FromMicroseconds(bitstream_buffer.id());

  scoped_ptr<base::SharedMemory> shm(
      new base::SharedMemory(bitstream_buffer.handle(), true));

  RETURN_ON_FAILURE(shm->Map(bitstream_buffer.size()),
                    "Failed to SharedMemory::Map()",
                    UNREADABLE_INPUT);

  status =
      media_codec_->QueueInputBuffer(input_buf_index,
                                     static_cast<const uint8*>(shm->memory()),
                                     bitstream_buffer.size(),
                                     timestamp);
  RETURN_ON_FAILURE(status == media::MEDIA_CODEC_OK,
                    "Failed to QueueInputBuffer: " << status,
                    PLATFORM_FAILURE);

  // We should call NotifyEndOfBitstreamBuffer(), when no more decoded output
  // will be returned from the bitstream buffer. However, MediaCodec API is
  // not enough to guarantee it.
  // So, here, we calls NotifyEndOfBitstreamBuffer() in advance in order to
  // keep getting more bitstreams from the client, and throttle them by using
  // |bitstreams_notified_in_advance_|.
  // TODO(dwkang): check if there is a way to remove this workaround.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&AndroidVideoDecodeAccelerator::NotifyEndOfBitstreamBuffer,
                 weak_this_factory_.GetWeakPtr(),
                 bitstream_buffer.id()));
  bitstreams_notified_in_advance_.push_back(bitstream_buffer.id());
}

void AndroidVideoDecodeAccelerator::DequeueOutput() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (picturebuffers_requested_ && output_picture_buffers_.empty())
    return;

  if (!output_picture_buffers_.empty() && free_picture_ids_.empty()) {
    // Don't have any picture buffer to send. Need to wait more.
    return;
  }

  bool eos = false;
  base::TimeDelta timestamp;
  int32 buf_index = 0;
  do {
    size_t offset = 0;
    size_t size = 0;

    media::MediaCodecStatus status = media_codec_->DequeueOutputBuffer(
        NoWaitTimeOut(), &buf_index, &offset, &size, &timestamp, &eos, NULL);
    switch (status) {
      case media::MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER:
      case media::MEDIA_CODEC_ERROR:
        return;

      case media::MEDIA_CODEC_OUTPUT_FORMAT_CHANGED: {
        int32 width, height;
        media_codec_->GetOutputFormat(&width, &height);

        if (!picturebuffers_requested_) {
          picturebuffers_requested_ = true;
          size_ = gfx::Size(width, height);
          base::MessageLoop::current()->PostTask(
              FROM_HERE,
              base::Bind(&AndroidVideoDecodeAccelerator::RequestPictureBuffers,
                         weak_this_factory_.GetWeakPtr()));
        } else {
          // Dynamic resolution change support is not specified by the Android
          // platform at and before JB-MR1, so it's not possible to smoothly
          // continue playback at this point.  Instead, error out immediately,
          // expecting clients to Reset() as appropriate to avoid this.
          // b/7093648
          RETURN_ON_FAILURE(size_ == gfx::Size(width, height),
                            "Dynamic resolution change is not supported.",
                            PLATFORM_FAILURE);
        }
        return;
      }

      case media::MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED:
        RETURN_ON_FAILURE(media_codec_->GetOutputBuffers(),
                          "Cannot get output buffer from MediaCodec.",
                          PLATFORM_FAILURE);
        break;

      case media::MEDIA_CODEC_OK:
        DCHECK_GE(buf_index, 0);
        break;

      default:
        NOTREACHED();
        break;
    }
  } while (buf_index < 0);

  // This ignores the emitted ByteBuffer and instead relies on rendering to the
  // codec's SurfaceTexture and then copying from that texture to the client's
  // PictureBuffer's texture.  This means that each picture's data is written
  // three times: once to the ByteBuffer, once to the SurfaceTexture, and once
  // to the client's texture.  It would be nicer to either:
  // 1) Render directly to the client's texture from MediaCodec (one write); or
  // 2) Upload the ByteBuffer to the client's texture (two writes).
  // Unfortunately neither is possible:
  // 1) MediaCodec's use of SurfaceTexture is a singleton, and the texture
  //    written to can't change during the codec's lifetime.  b/11990461
  // 2) The ByteBuffer is likely to contain the pixels in a vendor-specific,
  //    opaque/non-standard format.  It's not possible to negotiate the decoder
  //    to emit a specific colorspace, even using HW CSC.  b/10706245
  // So, we live with these two extra copies per picture :(
  media_codec_->ReleaseOutputBuffer(buf_index, true);

  if (eos) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&AndroidVideoDecodeAccelerator::NotifyFlushDone,
                   weak_this_factory_.GetWeakPtr()));
  } else {
    int64 bitstream_buffer_id = timestamp.InMicroseconds();
    SendCurrentSurfaceToClient(static_cast<int32>(bitstream_buffer_id));

    // Removes ids former or equal than the id from decoder. Note that
    // |bitstreams_notified_in_advance_| does not mean bitstream ids in decoder
    // because of frame reordering issue. We just maintain this roughly and use
    // for the throttling purpose.
    std::list<int32>::iterator it;
    for (it = bitstreams_notified_in_advance_.begin();
        it != bitstreams_notified_in_advance_.end();
        ++it) {
      if (*it == bitstream_buffer_id) {
        bitstreams_notified_in_advance_.erase(
            bitstreams_notified_in_advance_.begin(), ++it);
        break;
      }
    }
  }
}

void AndroidVideoDecodeAccelerator::SendCurrentSurfaceToClient(
    int32 bitstream_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(bitstream_id, -1);
  DCHECK(!free_picture_ids_.empty());

  RETURN_ON_FAILURE(make_context_current_.Run(),
                    "Failed to make this decoder's GL context current.",
                    PLATFORM_FAILURE);

  int32 picture_buffer_id = free_picture_ids_.front();
  free_picture_ids_.pop();

  float transfrom_matrix[16];
  surface_texture_->UpdateTexImage();
  surface_texture_->GetTransformMatrix(transfrom_matrix);

  OutputBufferMap::const_iterator i =
      output_picture_buffers_.find(picture_buffer_id);
  RETURN_ON_FAILURE(i != output_picture_buffers_.end(),
                    "Can't find a PictureBuffer for " << picture_buffer_id,
                    PLATFORM_FAILURE);
  uint32 picture_buffer_texture_id = i->second.texture_id();

  RETURN_ON_FAILURE(gl_decoder_.get(),
                    "Failed to get gles2 decoder instance.",
                    ILLEGAL_STATE);
  // Defer initializing the CopyTextureCHROMIUMResourceManager until it is
  // needed because it takes 10s of milliseconds to initialize.
  if (!copier_) {
    copier_.reset(new gpu::CopyTextureCHROMIUMResourceManager());
    copier_->Initialize(gl_decoder_.get());
  }

  // Here, we copy |surface_texture_id_| to the picture buffer instead of
  // setting new texture to |surface_texture_| by calling attachToGLContext()
  // because:
  // 1. Once we call detachFrameGLContext(), it deletes the texture previous
  //    attached.
  // 2. SurfaceTexture requires us to apply a transform matrix when we show
  //    the texture.
  copier_->DoCopyTexture(gl_decoder_.get(), GL_TEXTURE_EXTERNAL_OES,
                         GL_TEXTURE_2D, surface_texture_id_,
                         picture_buffer_texture_id, 0, size_.width(),
                         size_.height(), false, false, false);

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&AndroidVideoDecodeAccelerator::NotifyPictureReady,
                 weak_this_factory_.GetWeakPtr(),
                 media::Picture(picture_buffer_id, bitstream_id)));
}

void AndroidVideoDecodeAccelerator::Decode(
    const media::BitstreamBuffer& bitstream_buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (bitstream_buffer.id() != -1 && bitstream_buffer.size() == 0) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&AndroidVideoDecodeAccelerator::NotifyEndOfBitstreamBuffer,
                   weak_this_factory_.GetWeakPtr(),
                   bitstream_buffer.id()));
    return;
  }

  pending_bitstream_buffers_.push(
      std::make_pair(bitstream_buffer, base::Time::Now()));

  DoIOTask();
}

void AndroidVideoDecodeAccelerator::AssignPictureBuffers(
    const std::vector<media::PictureBuffer>& buffers) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(output_picture_buffers_.empty());
  DCHECK(free_picture_ids_.empty());

  for (size_t i = 0; i < buffers.size(); ++i) {
    RETURN_ON_FAILURE(buffers[i].size() == size_,
                      "Invalid picture buffer size was passed.",
                      INVALID_ARGUMENT);
    int32 id = buffers[i].id();
    output_picture_buffers_.insert(std::make_pair(id, buffers[i]));
    free_picture_ids_.push(id);
    // Since the client might be re-using |picture_buffer_id| values, forget
    // about previously-dismissed IDs now.  See ReusePictureBuffer() comment
    // about "zombies" for why we maintain this set in the first place.
    dismissed_picture_ids_.erase(id);
  }

  RETURN_ON_FAILURE(output_picture_buffers_.size() == kNumPictureBuffers,
                    "Invalid picture buffers were passed.",
                    INVALID_ARGUMENT);

  DoIOTask();
}

void AndroidVideoDecodeAccelerator::ReusePictureBuffer(
    int32 picture_buffer_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // This ReusePictureBuffer() might have been in a pipe somewhere (queued in
  // IPC, or in a PostTask either at the sender or receiver) when we sent a
  // DismissPictureBuffer() for this |picture_buffer_id|.  Account for such
  // potential "zombie" IDs here.
  if (dismissed_picture_ids_.erase(picture_buffer_id))
    return;

  free_picture_ids_.push(picture_buffer_id);

  DoIOTask();
}

void AndroidVideoDecodeAccelerator::Flush() {
  DCHECK(thread_checker_.CalledOnValidThread());

  Decode(media::BitstreamBuffer(-1, base::SharedMemoryHandle(), 0));
}

bool AndroidVideoDecodeAccelerator::ConfigureMediaCodec() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(surface_texture_.get());

  gfx::ScopedJavaSurface surface(surface_texture_.get());

  // Pass a dummy 320x240 canvas size and let the codec signal the real size
  // when it's known from the bitstream.
  media_codec_.reset(media::VideoCodecBridge::CreateDecoder(
      codec_, false, gfx::Size(320, 240), surface.j_surface().obj(), NULL));
  if (!media_codec_)
    return false;

  io_timer_.Start(FROM_HERE,
                  DecodePollDelay(),
                  this,
                  &AndroidVideoDecodeAccelerator::DoIOTask);
  return true;
}

void AndroidVideoDecodeAccelerator::Reset() {
  DCHECK(thread_checker_.CalledOnValidThread());

  while (!pending_bitstream_buffers_.empty()) {
    int32 bitstream_buffer_id = pending_bitstream_buffers_.front().first.id();
    pending_bitstream_buffers_.pop();

    if (bitstream_buffer_id != -1) {
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&AndroidVideoDecodeAccelerator::NotifyEndOfBitstreamBuffer,
                     weak_this_factory_.GetWeakPtr(),
                     bitstream_buffer_id));
    }
  }
  bitstreams_notified_in_advance_.clear();

  for (OutputBufferMap::iterator it = output_picture_buffers_.begin();
       it != output_picture_buffers_.end();
       ++it) {
    client_->DismissPictureBuffer(it->first);
    dismissed_picture_ids_.insert(it->first);
  }
  output_picture_buffers_.clear();
  std::queue<int32> empty;
  std::swap(free_picture_ids_, empty);
  CHECK(free_picture_ids_.empty());
  picturebuffers_requested_ = false;

  // On some devices, and up to at least JB-MR1,
  // - flush() can fail after EOS (b/8125974); and
  // - mid-stream resolution change is unsupported (b/7093648).
  // To cope with these facts, we always stop & restart the codec on Reset().
  io_timer_.Stop();
  media_codec_->Stop();
  ConfigureMediaCodec();
  state_ = NO_ERROR;

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&AndroidVideoDecodeAccelerator::NotifyResetDone,
                 weak_this_factory_.GetWeakPtr()));
}

void AndroidVideoDecodeAccelerator::Destroy() {
  DCHECK(thread_checker_.CalledOnValidThread());

  weak_this_factory_.InvalidateWeakPtrs();
  if (media_codec_) {
    io_timer_.Stop();
    media_codec_->Stop();
  }
  if (surface_texture_id_)
    glDeleteTextures(1, &surface_texture_id_);
  if (copier_)
    copier_->Destroy();
  delete this;
}

bool AndroidVideoDecodeAccelerator::CanDecodeOnIOThread() {
  return false;
}

void AndroidVideoDecodeAccelerator::RequestPictureBuffers() {
  client_->ProvidePictureBuffers(kNumPictureBuffers, size_, GL_TEXTURE_2D);
}

void AndroidVideoDecodeAccelerator::NotifyPictureReady(
    const media::Picture& picture) {
  client_->PictureReady(picture);
}

void AndroidVideoDecodeAccelerator::NotifyEndOfBitstreamBuffer(
    int input_buffer_id) {
  client_->NotifyEndOfBitstreamBuffer(input_buffer_id);
}

void AndroidVideoDecodeAccelerator::NotifyFlushDone() {
  client_->NotifyFlushDone();
}

void AndroidVideoDecodeAccelerator::NotifyResetDone() {
  client_->NotifyResetDone();
}

void AndroidVideoDecodeAccelerator::NotifyError(
    media::VideoDecodeAccelerator::Error error) {
  client_->NotifyError(error);
}

}  // namespace content
