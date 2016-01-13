// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/numerics/safe_conversions.h"
#include "content/common/gpu/media/v4l2_video_decode_accelerator.h"
#include "media/base/media_switches.h"
#include "media/filters/h264_parser.h"
#include "ui/gl/scoped_binders.h"

#define NOTIFY_ERROR(x)                            \
  do {                                             \
    SetDecoderState(kError);                       \
    DLOG(ERROR) << "calling NotifyError(): " << x; \
    NotifyError(x);                                \
  } while (0)

#define IOCTL_OR_ERROR_RETURN_VALUE(type, arg, value)              \
  do {                                                             \
    if (device_->Ioctl(type, arg) != 0) {                          \
      DPLOG(ERROR) << __func__ << "(): ioctl() failed: " << #type; \
      NOTIFY_ERROR(PLATFORM_FAILURE);                              \
      return value;                                                \
    }                                                              \
  } while (0)

#define IOCTL_OR_ERROR_RETURN(type, arg) \
  IOCTL_OR_ERROR_RETURN_VALUE(type, arg, ((void)0))

#define IOCTL_OR_ERROR_RETURN_FALSE(type, arg) \
  IOCTL_OR_ERROR_RETURN_VALUE(type, arg, false)

#define IOCTL_OR_LOG_ERROR(type, arg)                              \
  do {                                                             \
    if (device_->Ioctl(type, arg) != 0)                            \
      DPLOG(ERROR) << __func__ << "(): ioctl() failed: " << #type; \
  } while (0)

namespace content {

namespace {

// TODO(posciak): remove once we update linux-headers.
#ifndef V4L2_EVENT_RESOLUTION_CHANGE
#define V4L2_EVENT_RESOLUTION_CHANGE 5
#endif

}  // anonymous namespace

struct V4L2VideoDecodeAccelerator::BitstreamBufferRef {
  BitstreamBufferRef(
      base::WeakPtr<Client>& client,
      scoped_refptr<base::MessageLoopProxy>& client_message_loop_proxy,
      base::SharedMemory* shm,
      size_t size,
      int32 input_id);
  ~BitstreamBufferRef();
  const base::WeakPtr<Client> client;
  const scoped_refptr<base::MessageLoopProxy> client_message_loop_proxy;
  const scoped_ptr<base::SharedMemory> shm;
  const size_t size;
  off_t bytes_used;
  const int32 input_id;
};

struct V4L2VideoDecodeAccelerator::EGLSyncKHRRef {
  EGLSyncKHRRef(EGLDisplay egl_display, EGLSyncKHR egl_sync);
  ~EGLSyncKHRRef();
  EGLDisplay const egl_display;
  EGLSyncKHR egl_sync;
};

struct V4L2VideoDecodeAccelerator::PictureRecord {
  PictureRecord(bool cleared, const media::Picture& picture);
  ~PictureRecord();
  bool cleared;  // Whether the texture is cleared and safe to render from.
  media::Picture picture;  // The decoded picture.
};

V4L2VideoDecodeAccelerator::BitstreamBufferRef::BitstreamBufferRef(
    base::WeakPtr<Client>& client,
    scoped_refptr<base::MessageLoopProxy>& client_message_loop_proxy,
    base::SharedMemory* shm, size_t size, int32 input_id)
    : client(client),
      client_message_loop_proxy(client_message_loop_proxy),
      shm(shm),
      size(size),
      bytes_used(0),
      input_id(input_id) {
}

V4L2VideoDecodeAccelerator::BitstreamBufferRef::~BitstreamBufferRef() {
  if (input_id >= 0) {
    client_message_loop_proxy->PostTask(FROM_HERE, base::Bind(
        &Client::NotifyEndOfBitstreamBuffer, client, input_id));
  }
}

V4L2VideoDecodeAccelerator::EGLSyncKHRRef::EGLSyncKHRRef(
    EGLDisplay egl_display, EGLSyncKHR egl_sync)
    : egl_display(egl_display),
      egl_sync(egl_sync) {
}

V4L2VideoDecodeAccelerator::EGLSyncKHRRef::~EGLSyncKHRRef() {
  // We don't check for eglDestroySyncKHR failures, because if we get here
  // with a valid sync object, something went wrong and we are getting
  // destroyed anyway.
  if (egl_sync != EGL_NO_SYNC_KHR)
    eglDestroySyncKHR(egl_display, egl_sync);
}

V4L2VideoDecodeAccelerator::InputRecord::InputRecord()
    : at_device(false),
      address(NULL),
      length(0),
      bytes_used(0),
      input_id(-1) {
}

V4L2VideoDecodeAccelerator::InputRecord::~InputRecord() {
}

V4L2VideoDecodeAccelerator::OutputRecord::OutputRecord()
    : at_device(false),
      at_client(false),
      egl_image(EGL_NO_IMAGE_KHR),
      egl_sync(EGL_NO_SYNC_KHR),
      picture_id(-1),
      cleared(false) {
}

V4L2VideoDecodeAccelerator::OutputRecord::~OutputRecord() {}

V4L2VideoDecodeAccelerator::PictureRecord::PictureRecord(
    bool cleared,
    const media::Picture& picture)
    : cleared(cleared), picture(picture) {}

V4L2VideoDecodeAccelerator::PictureRecord::~PictureRecord() {}

V4L2VideoDecodeAccelerator::V4L2VideoDecodeAccelerator(
    EGLDisplay egl_display,
    EGLContext egl_context,
    const base::WeakPtr<Client>& io_client,
    const base::Callback<bool(void)>& make_context_current,
    scoped_ptr<V4L2Device> device,
    const scoped_refptr<base::MessageLoopProxy>& io_message_loop_proxy)
    : child_message_loop_proxy_(base::MessageLoopProxy::current()),
      io_message_loop_proxy_(io_message_loop_proxy),
      io_client_(io_client),
      decoder_thread_("V4L2DecoderThread"),
      decoder_state_(kUninitialized),
      device_(device.Pass()),
      decoder_delay_bitstream_buffer_id_(-1),
      decoder_current_input_buffer_(-1),
      decoder_decode_buffer_tasks_scheduled_(0),
      decoder_frames_at_client_(0),
      decoder_flushing_(false),
      resolution_change_pending_(false),
      resolution_change_reset_pending_(false),
      decoder_partial_frame_pending_(false),
      input_streamon_(false),
      input_buffer_queued_count_(0),
      output_streamon_(false),
      output_buffer_queued_count_(0),
      output_dpb_size_(0),
      output_planes_count_(0),
      picture_clearing_count_(0),
      pictures_assigned_(false, false),
      device_poll_thread_("V4L2DevicePollThread"),
      make_context_current_(make_context_current),
      egl_display_(egl_display),
      egl_context_(egl_context),
      video_profile_(media::VIDEO_CODEC_PROFILE_UNKNOWN),
      weak_this_factory_(this) {
  weak_this_ = weak_this_factory_.GetWeakPtr();
}

V4L2VideoDecodeAccelerator::~V4L2VideoDecodeAccelerator() {
  DCHECK(!decoder_thread_.IsRunning());
  DCHECK(!device_poll_thread_.IsRunning());

  DestroyInputBuffers();
  DestroyOutputBuffers();

  // These maps have members that should be manually destroyed, e.g. file
  // descriptors, mmap() segments, etc.
  DCHECK(input_buffer_map_.empty());
  DCHECK(output_buffer_map_.empty());
}

bool V4L2VideoDecodeAccelerator::Initialize(media::VideoCodecProfile profile,
                                            Client* client) {
  DVLOG(3) << "Initialize()";
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK_EQ(decoder_state_, kUninitialized);

  client_ptr_factory_.reset(new base::WeakPtrFactory<Client>(client));
  client_ = client_ptr_factory_->GetWeakPtr();

  switch (profile) {
    case media::H264PROFILE_BASELINE:
      DVLOG(2) << "Initialize(): profile H264PROFILE_BASELINE";
      break;
    case media::H264PROFILE_MAIN:
      DVLOG(2) << "Initialize(): profile H264PROFILE_MAIN";
      break;
    case media::H264PROFILE_HIGH:
      DVLOG(2) << "Initialize(): profile H264PROFILE_HIGH";
      break;
    case media::VP8PROFILE_MAIN:
      DVLOG(2) << "Initialize(): profile VP8PROFILE_MAIN";
      break;
    default:
      DLOG(ERROR) << "Initialize(): unsupported profile=" << profile;
      return false;
  };
  video_profile_ = profile;

  if (egl_display_ == EGL_NO_DISPLAY) {
    DLOG(ERROR) << "Initialize(): could not get EGLDisplay";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }

  // We need the context to be initialized to query extensions.
  if (!make_context_current_.Run()) {
    DLOG(ERROR) << "Initialize(): could not make context current";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }

  if (!gfx::g_driver_egl.ext.b_EGL_KHR_fence_sync) {
    DLOG(ERROR) << "Initialize(): context does not have EGL_KHR_fence_sync";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }

  // Capabilities check.
  struct v4l2_capability caps;
  const __u32 kCapsRequired =
      V4L2_CAP_VIDEO_CAPTURE_MPLANE |
      V4L2_CAP_VIDEO_OUTPUT_MPLANE |
      V4L2_CAP_STREAMING;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QUERYCAP, &caps);
  if ((caps.capabilities & kCapsRequired) != kCapsRequired) {
    DLOG(ERROR) << "Initialize(): ioctl() failed: VIDIOC_QUERYCAP"
        ", caps check failed: 0x" << std::hex << caps.capabilities;
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }

  if (!CreateInputBuffers())
    return false;

  // Output format has to be setup before streaming starts.
  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  uint32 output_format_fourcc = device_->PreferredOutputFormat();
  if (output_format_fourcc == 0) {
    // TODO(posciak): We should enumerate available output formats, as well as
    // take into account formats that the client is ready to accept.
    return false;
  }
  format.fmt.pix_mp.pixelformat = output_format_fourcc;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_FMT, &format);

  // Subscribe to the resolution change event.
  struct v4l2_event_subscription sub;
  memset(&sub, 0, sizeof(sub));
  sub.type = V4L2_EVENT_RESOLUTION_CHANGE;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_SUBSCRIBE_EVENT, &sub);

  // Initialize format-specific bits.
  if (video_profile_ >= media::H264PROFILE_MIN &&
      video_profile_ <= media::H264PROFILE_MAX) {
    decoder_h264_parser_.reset(new media::H264Parser());
  }

  if (!decoder_thread_.Start()) {
    DLOG(ERROR) << "Initialize(): decoder thread failed to start";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }

  // StartDevicePoll will NOTIFY_ERROR on failure, so IgnoreResult is fine here.
  decoder_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(
          base::IgnoreResult(&V4L2VideoDecodeAccelerator::StartDevicePoll),
          base::Unretained(this)));

  SetDecoderState(kInitialized);
  return true;
}

void V4L2VideoDecodeAccelerator::Decode(
    const media::BitstreamBuffer& bitstream_buffer) {
  DVLOG(1) << "Decode(): input_id=" << bitstream_buffer.id()
           << ", size=" << bitstream_buffer.size();
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  // DecodeTask() will take care of running a DecodeBufferTask().
  decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &V4L2VideoDecodeAccelerator::DecodeTask, base::Unretained(this),
      bitstream_buffer));
}

void V4L2VideoDecodeAccelerator::AssignPictureBuffers(
    const std::vector<media::PictureBuffer>& buffers) {
  DVLOG(3) << "AssignPictureBuffers(): buffer_count=" << buffers.size();
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());

  if (buffers.size() != output_buffer_map_.size()) {
    DLOG(ERROR) << "AssignPictureBuffers(): Failed to provide requested picture"
                   " buffers. (Got " << buffers.size()
                << ", requested " << output_buffer_map_.size() << ")";
    NOTIFY_ERROR(INVALID_ARGUMENT);
    return;
  }

  if (!make_context_current_.Run()) {
    DLOG(ERROR) << "AssignPictureBuffers(): could not make context current";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  gfx::ScopedTextureBinder bind_restore(GL_TEXTURE_EXTERNAL_OES, 0);

  // It's safe to manipulate all the buffer state here, because the decoder
  // thread is waiting on pictures_assigned_.
  DCHECK(free_output_buffers_.empty());
  for (size_t i = 0; i < output_buffer_map_.size(); ++i) {
    DCHECK(buffers[i].size() == frame_buffer_size_);

    OutputRecord& output_record = output_buffer_map_[i];
    DCHECK(!output_record.at_device);
    DCHECK(!output_record.at_client);
    DCHECK_EQ(output_record.egl_image, EGL_NO_IMAGE_KHR);
    DCHECK_EQ(output_record.egl_sync, EGL_NO_SYNC_KHR);
    DCHECK_EQ(output_record.picture_id, -1);
    DCHECK_EQ(output_record.cleared, false);

    EGLImageKHR egl_image = device_->CreateEGLImage(egl_display_,
                                                    egl_context_,
                                                    buffers[i].texture_id(),
                                                    frame_buffer_size_,
                                                    i,
                                                    output_planes_count_);
    if (egl_image == EGL_NO_IMAGE_KHR) {
      DLOG(ERROR) << "AssignPictureBuffers(): could not create EGLImageKHR";
      // Ownership of EGLImages allocated in previous iterations of this loop
      // has been transferred to output_buffer_map_. After we error-out here
      // the destructor will handle their cleanup.
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    }

    output_record.egl_image = egl_image;
    output_record.picture_id = buffers[i].id();
    free_output_buffers_.push(i);
    DVLOG(3) << "AssignPictureBuffers(): buffer[" << i
             << "]: picture_id=" << output_record.picture_id;
  }

  pictures_assigned_.Signal();
}

void V4L2VideoDecodeAccelerator::ReusePictureBuffer(int32 picture_buffer_id) {
  DVLOG(3) << "ReusePictureBuffer(): picture_buffer_id=" << picture_buffer_id;
  // Must be run on child thread, as we'll insert a sync in the EGL context.
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());

  if (!make_context_current_.Run()) {
    DLOG(ERROR) << "ReusePictureBuffer(): could not make context current";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  EGLSyncKHR egl_sync =
      eglCreateSyncKHR(egl_display_, EGL_SYNC_FENCE_KHR, NULL);
  if (egl_sync == EGL_NO_SYNC_KHR) {
    DLOG(ERROR) << "ReusePictureBuffer(): eglCreateSyncKHR() failed";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  scoped_ptr<EGLSyncKHRRef> egl_sync_ref(new EGLSyncKHRRef(
      egl_display_, egl_sync));
  decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &V4L2VideoDecodeAccelerator::ReusePictureBufferTask,
      base::Unretained(this), picture_buffer_id, base::Passed(&egl_sync_ref)));
}

void V4L2VideoDecodeAccelerator::Flush() {
  DVLOG(3) << "Flush()";
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &V4L2VideoDecodeAccelerator::FlushTask, base::Unretained(this)));
}

void V4L2VideoDecodeAccelerator::Reset() {
  DVLOG(3) << "Reset()";
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &V4L2VideoDecodeAccelerator::ResetTask, base::Unretained(this)));
}

void V4L2VideoDecodeAccelerator::Destroy() {
  DVLOG(3) << "Destroy()";
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());

  // We're destroying; cancel all callbacks.
  client_ptr_factory_.reset();
  weak_this_factory_.InvalidateWeakPtrs();

  // If the decoder thread is running, destroy using posted task.
  if (decoder_thread_.IsRunning()) {
    decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
        &V4L2VideoDecodeAccelerator::DestroyTask, base::Unretained(this)));
    pictures_assigned_.Signal();
    // DestroyTask() will cause the decoder_thread_ to flush all tasks.
    decoder_thread_.Stop();
  } else {
    // Otherwise, call the destroy task directly.
    DestroyTask();
  }

  // Set to kError state just in case.
  SetDecoderState(kError);

  delete this;
}

bool V4L2VideoDecodeAccelerator::CanDecodeOnIOThread() { return true; }

void V4L2VideoDecodeAccelerator::DecodeTask(
    const media::BitstreamBuffer& bitstream_buffer) {
  DVLOG(3) << "DecodeTask(): input_id=" << bitstream_buffer.id();
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  DCHECK_NE(decoder_state_, kUninitialized);
  TRACE_EVENT1("Video Decoder", "V4L2VDA::DecodeTask", "input_id",
               bitstream_buffer.id());

  scoped_ptr<BitstreamBufferRef> bitstream_record(new BitstreamBufferRef(
      io_client_, io_message_loop_proxy_,
      new base::SharedMemory(bitstream_buffer.handle(), true),
      bitstream_buffer.size(), bitstream_buffer.id()));
  if (!bitstream_record->shm->Map(bitstream_buffer.size())) {
    DLOG(ERROR) << "Decode(): could not map bitstream_buffer";
    NOTIFY_ERROR(UNREADABLE_INPUT);
    return;
  }
  DVLOG(3) << "DecodeTask(): mapped at=" << bitstream_record->shm->memory();

  if (decoder_state_ == kResetting || decoder_flushing_) {
    // In the case that we're resetting or flushing, we need to delay decoding
    // the BitstreamBuffers that come after the Reset() or Flush() call.  When
    // we're here, we know that this DecodeTask() was scheduled by a Decode()
    // call that came after (in the client thread) the Reset() or Flush() call;
    // thus set up the delay if necessary.
    if (decoder_delay_bitstream_buffer_id_ == -1)
      decoder_delay_bitstream_buffer_id_ = bitstream_record->input_id;
  } else if (decoder_state_ == kError) {
    DVLOG(2) << "DecodeTask(): early out: kError state";
    return;
  }

  decoder_input_queue_.push(
      linked_ptr<BitstreamBufferRef>(bitstream_record.release()));
  decoder_decode_buffer_tasks_scheduled_++;
  DecodeBufferTask();
}

void V4L2VideoDecodeAccelerator::DecodeBufferTask() {
  DVLOG(3) << "DecodeBufferTask()";
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  DCHECK_NE(decoder_state_, kUninitialized);
  TRACE_EVENT0("Video Decoder", "V4L2VDA::DecodeBufferTask");

  decoder_decode_buffer_tasks_scheduled_--;

  if (decoder_state_ == kResetting) {
    DVLOG(2) << "DecodeBufferTask(): early out: kResetting state";
    return;
  } else if (decoder_state_ == kError) {
    DVLOG(2) << "DecodeBufferTask(): early out: kError state";
    return;
  } else if (decoder_state_ == kChangingResolution) {
    DVLOG(2) << "DecodeBufferTask(): early out: resolution change pending";
    return;
  }

  if (decoder_current_bitstream_buffer_ == NULL) {
    if (decoder_input_queue_.empty()) {
      // We're waiting for a new buffer -- exit without scheduling a new task.
      return;
    }
    linked_ptr<BitstreamBufferRef>& buffer_ref = decoder_input_queue_.front();
    if (decoder_delay_bitstream_buffer_id_ == buffer_ref->input_id) {
      // We're asked to delay decoding on this and subsequent buffers.
      return;
    }

    // Setup to use the next buffer.
    decoder_current_bitstream_buffer_.reset(buffer_ref.release());
    decoder_input_queue_.pop();
    DVLOG(3) << "DecodeBufferTask(): reading input_id="
             << decoder_current_bitstream_buffer_->input_id
             << ", addr=" << (decoder_current_bitstream_buffer_->shm ?
                              decoder_current_bitstream_buffer_->shm->memory() :
                              NULL)
             << ", size=" << decoder_current_bitstream_buffer_->size;
  }
  bool schedule_task = false;
  const size_t size = decoder_current_bitstream_buffer_->size;
  size_t decoded_size = 0;
  if (size == 0) {
    const int32 input_id = decoder_current_bitstream_buffer_->input_id;
    if (input_id >= 0) {
      // This is a buffer queued from the client that has zero size.  Skip.
      schedule_task = true;
    } else {
      // This is a buffer of zero size, queued to flush the pipe.  Flush.
      DCHECK_EQ(decoder_current_bitstream_buffer_->shm.get(),
                static_cast<base::SharedMemory*>(NULL));
      // Enqueue a buffer guaranteed to be empty.  To do that, we flush the
      // current input, enqueue no data to the next frame, then flush that down.
      schedule_task = true;
      if (decoder_current_input_buffer_ != -1 &&
          input_buffer_map_[decoder_current_input_buffer_].input_id !=
              kFlushBufferId)
        schedule_task = FlushInputFrame();

      if (schedule_task && AppendToInputFrame(NULL, 0) && FlushInputFrame()) {
        DVLOG(2) << "DecodeBufferTask(): enqueued flush buffer";
        decoder_partial_frame_pending_ = false;
        schedule_task = true;
      } else {
        // If we failed to enqueue the empty buffer (due to pipeline
        // backpressure), don't advance the bitstream buffer queue, and don't
        // schedule the next task.  This bitstream buffer queue entry will get
        // reprocessed when the pipeline frees up.
        schedule_task = false;
      }
    }
  } else {
    // This is a buffer queued from the client, with actual contents.  Decode.
    const uint8* const data =
        reinterpret_cast<const uint8*>(
            decoder_current_bitstream_buffer_->shm->memory()) +
        decoder_current_bitstream_buffer_->bytes_used;
    const size_t data_size =
        decoder_current_bitstream_buffer_->size -
        decoder_current_bitstream_buffer_->bytes_used;
    if (!AdvanceFrameFragment(data, data_size, &decoded_size)) {
      NOTIFY_ERROR(UNREADABLE_INPUT);
      return;
    }
    // AdvanceFrameFragment should not return a size larger than the buffer
    // size, even on invalid data.
    CHECK_LE(decoded_size, data_size);

    switch (decoder_state_) {
      case kInitialized:
      case kAfterReset:
        schedule_task = DecodeBufferInitial(data, decoded_size, &decoded_size);
        break;
      case kDecoding:
        schedule_task = DecodeBufferContinue(data, decoded_size);
        break;
      default:
        NOTIFY_ERROR(ILLEGAL_STATE);
        return;
    }
  }
  if (decoder_state_ == kError) {
    // Failed during decode.
    return;
  }

  if (schedule_task) {
    decoder_current_bitstream_buffer_->bytes_used += decoded_size;
    if (decoder_current_bitstream_buffer_->bytes_used ==
        decoder_current_bitstream_buffer_->size) {
      // Our current bitstream buffer is done; return it.
      int32 input_id = decoder_current_bitstream_buffer_->input_id;
      DVLOG(3) << "DecodeBufferTask(): finished input_id=" << input_id;
      // BitstreamBufferRef destructor calls NotifyEndOfBitstreamBuffer().
      decoder_current_bitstream_buffer_.reset();
    }
    ScheduleDecodeBufferTaskIfNeeded();
  }
}

bool V4L2VideoDecodeAccelerator::AdvanceFrameFragment(
    const uint8* data,
    size_t size,
    size_t* endpos) {
  if (video_profile_ >= media::H264PROFILE_MIN &&
      video_profile_ <= media::H264PROFILE_MAX) {
    // For H264, we need to feed HW one frame at a time.  This is going to take
    // some parsing of our input stream.
    decoder_h264_parser_->SetStream(data, size);
    media::H264NALU nalu;
    media::H264Parser::Result result;
    *endpos = 0;

    // Keep on peeking the next NALs while they don't indicate a frame
    // boundary.
    for (;;) {
      bool end_of_frame = false;
      result = decoder_h264_parser_->AdvanceToNextNALU(&nalu);
      if (result == media::H264Parser::kInvalidStream ||
          result == media::H264Parser::kUnsupportedStream)
        return false;
      if (result == media::H264Parser::kEOStream) {
        // We've reached the end of the buffer before finding a frame boundary.
        decoder_partial_frame_pending_ = true;
        return true;
      }
      switch (nalu.nal_unit_type) {
        case media::H264NALU::kNonIDRSlice:
        case media::H264NALU::kIDRSlice:
          if (nalu.size < 1)
            return false;
          // For these two, if the "first_mb_in_slice" field is zero, start a
          // new frame and return.  This field is Exp-Golomb coded starting on
          // the eighth data bit of the NAL; a zero value is encoded with a
          // leading '1' bit in the byte, which we can detect as the byte being
          // (unsigned) greater than or equal to 0x80.
          if (nalu.data[1] >= 0x80) {
            end_of_frame = true;
            break;
          }
          break;
        case media::H264NALU::kSEIMessage:
        case media::H264NALU::kSPS:
        case media::H264NALU::kPPS:
        case media::H264NALU::kAUD:
        case media::H264NALU::kEOSeq:
        case media::H264NALU::kEOStream:
        case media::H264NALU::kReserved14:
        case media::H264NALU::kReserved15:
        case media::H264NALU::kReserved16:
        case media::H264NALU::kReserved17:
        case media::H264NALU::kReserved18:
          // These unconditionally signal a frame boundary.
          end_of_frame = true;
          break;
        default:
          // For all others, keep going.
          break;
      }
      if (end_of_frame) {
        if (!decoder_partial_frame_pending_ && *endpos == 0) {
          // The frame was previously restarted, and we haven't filled the
          // current frame with any contents yet.  Start the new frame here and
          // continue parsing NALs.
        } else {
          // The frame wasn't previously restarted and/or we have contents for
          // the current frame; signal the start of a new frame here: we don't
          // have a partial frame anymore.
          decoder_partial_frame_pending_ = false;
          return true;
        }
      }
      *endpos = (nalu.data + nalu.size) - data;
    }
    NOTREACHED();
    return false;
  } else {
    DCHECK_GE(video_profile_, media::VP8PROFILE_MIN);
    DCHECK_LE(video_profile_, media::VP8PROFILE_MAX);
    // For VP8, we can just dump the entire buffer.  No fragmentation needed,
    // and we never return a partial frame.
    *endpos = size;
    decoder_partial_frame_pending_ = false;
    return true;
  }
}

void V4L2VideoDecodeAccelerator::ScheduleDecodeBufferTaskIfNeeded() {
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());

  // If we're behind on tasks, schedule another one.
  int buffers_to_decode = decoder_input_queue_.size();
  if (decoder_current_bitstream_buffer_ != NULL)
    buffers_to_decode++;
  if (decoder_decode_buffer_tasks_scheduled_ < buffers_to_decode) {
    decoder_decode_buffer_tasks_scheduled_++;
    decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
        &V4L2VideoDecodeAccelerator::DecodeBufferTask,
        base::Unretained(this)));
  }
}

bool V4L2VideoDecodeAccelerator::DecodeBufferInitial(
    const void* data, size_t size, size_t* endpos) {
  DVLOG(3) << "DecodeBufferInitial(): data=" << data << ", size=" << size;
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  DCHECK_NE(decoder_state_, kUninitialized);
  DCHECK_NE(decoder_state_, kDecoding);
  // Initial decode.  We haven't been able to get output stream format info yet.
  // Get it, and start decoding.

  // Copy in and send to HW.
  if (!AppendToInputFrame(data, size))
    return false;

  // If we only have a partial frame, don't flush and process yet.
  if (decoder_partial_frame_pending_)
    return true;

  if (!FlushInputFrame())
    return false;

  // Recycle buffers.
  Dequeue();

  // Check and see if we have format info yet.
  struct v4l2_format format;
  bool again = false;
  if (!GetFormatInfo(&format, &again))
    return false;

  if (again) {
    // Need more stream to decode format, return true and schedule next buffer.
    *endpos = size;
    return true;
  }

  // Run this initialization only on first startup.
  if (decoder_state_ == kInitialized) {
    DVLOG(3) << "DecodeBufferInitial(): running initialization";
    // Success! Setup our parameters.
    if (!CreateBuffersForFormat(format))
      return false;

    // We expect to process the initial buffer once during stream init to
    // configure stream parameters, but will not consume the steam data on that
    // iteration.  Subsequent iterations (including after reset) do not require
    // the stream init step.
    *endpos = 0;
  } else {
    *endpos = size;
  }

  decoder_state_ = kDecoding;
  ScheduleDecodeBufferTaskIfNeeded();
  return true;
}

bool V4L2VideoDecodeAccelerator::DecodeBufferContinue(
    const void* data, size_t size) {
  DVLOG(3) << "DecodeBufferContinue(): data=" << data << ", size=" << size;
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  DCHECK_EQ(decoder_state_, kDecoding);

  // Both of these calls will set kError state if they fail.
  // Only flush the frame if it's complete.
  return (AppendToInputFrame(data, size) &&
          (decoder_partial_frame_pending_ || FlushInputFrame()));
}

bool V4L2VideoDecodeAccelerator::AppendToInputFrame(
    const void* data, size_t size) {
  DVLOG(3) << "AppendToInputFrame()";
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  DCHECK_NE(decoder_state_, kUninitialized);
  DCHECK_NE(decoder_state_, kResetting);
  DCHECK_NE(decoder_state_, kError);
  // This routine can handle data == NULL and size == 0, which occurs when
  // we queue an empty buffer for the purposes of flushing the pipe.

  // Flush if we're too big
  if (decoder_current_input_buffer_ != -1) {
    InputRecord& input_record =
        input_buffer_map_[decoder_current_input_buffer_];
    if (input_record.bytes_used + size > input_record.length) {
      if (!FlushInputFrame())
        return false;
      decoder_current_input_buffer_ = -1;
    }
  }

  // Try to get an available input buffer
  if (decoder_current_input_buffer_ == -1) {
    if (free_input_buffers_.empty()) {
      // See if we can get more free buffers from HW
      Dequeue();
      if (free_input_buffers_.empty()) {
        // Nope!
        DVLOG(2) << "AppendToInputFrame(): stalled for input buffers";
        return false;
      }
    }
    decoder_current_input_buffer_ = free_input_buffers_.back();
    free_input_buffers_.pop_back();
    InputRecord& input_record =
        input_buffer_map_[decoder_current_input_buffer_];
    DCHECK_EQ(input_record.bytes_used, 0);
    DCHECK_EQ(input_record.input_id, -1);
    DCHECK(decoder_current_bitstream_buffer_ != NULL);
    input_record.input_id = decoder_current_bitstream_buffer_->input_id;
  }

  DCHECK(data != NULL || size == 0);
  if (size == 0) {
    // If we asked for an empty buffer, return now.  We return only after
    // getting the next input buffer, since we might actually want an empty
    // input buffer for flushing purposes.
    return true;
  }

  // Copy in to the buffer.
  InputRecord& input_record =
      input_buffer_map_[decoder_current_input_buffer_];
  if (size > input_record.length - input_record.bytes_used) {
    LOG(ERROR) << "AppendToInputFrame(): over-size frame, erroring";
    NOTIFY_ERROR(UNREADABLE_INPUT);
    return false;
  }
  memcpy(
      reinterpret_cast<uint8*>(input_record.address) + input_record.bytes_used,
      data,
      size);
  input_record.bytes_used += size;

  return true;
}

bool V4L2VideoDecodeAccelerator::FlushInputFrame() {
  DVLOG(3) << "FlushInputFrame()";
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  DCHECK_NE(decoder_state_, kUninitialized);
  DCHECK_NE(decoder_state_, kResetting);
  DCHECK_NE(decoder_state_, kError);

  if (decoder_current_input_buffer_ == -1)
    return true;

  InputRecord& input_record =
      input_buffer_map_[decoder_current_input_buffer_];
  DCHECK_NE(input_record.input_id, -1);
  DCHECK(input_record.input_id != kFlushBufferId ||
         input_record.bytes_used == 0);
  // * if input_id >= 0, this input buffer was prompted by a bitstream buffer we
  //   got from the client.  We can skip it if it is empty.
  // * if input_id < 0 (should be kFlushBufferId in this case), this input
  //   buffer was prompted by a flush buffer, and should be queued even when
  //   empty.
  if (input_record.input_id >= 0 && input_record.bytes_used == 0) {
    input_record.input_id = -1;
    free_input_buffers_.push_back(decoder_current_input_buffer_);
    decoder_current_input_buffer_ = -1;
    return true;
  }

  // Queue it.
  input_ready_queue_.push(decoder_current_input_buffer_);
  decoder_current_input_buffer_ = -1;
  DVLOG(3) << "FlushInputFrame(): submitting input_id="
           << input_record.input_id;
  // Enqueue once since there's new available input for it.
  Enqueue();

  return (decoder_state_ != kError);
}

void V4L2VideoDecodeAccelerator::ServiceDeviceTask(bool event_pending) {
  DVLOG(3) << "ServiceDeviceTask()";
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  DCHECK_NE(decoder_state_, kUninitialized);
  TRACE_EVENT0("Video Decoder", "V4L2VDA::ServiceDeviceTask");

  if (decoder_state_ == kResetting) {
    DVLOG(2) << "ServiceDeviceTask(): early out: kResetting state";
    return;
  } else if (decoder_state_ == kError) {
    DVLOG(2) << "ServiceDeviceTask(): early out: kError state";
    return;
  } else if (decoder_state_ == kChangingResolution) {
    DVLOG(2) << "ServiceDeviceTask(): early out: kChangingResolution state";
    return;
  }

  if (event_pending)
    DequeueEvents();
  Dequeue();
  Enqueue();

  // Clear the interrupt fd.
  if (!device_->ClearDevicePollInterrupt()) {
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  bool poll_device = false;
  // Add fd, if we should poll on it.
  // Can be polled as soon as either input or output buffers are queued.
  if (input_buffer_queued_count_ + output_buffer_queued_count_ > 0)
    poll_device = true;

  // ServiceDeviceTask() should only ever be scheduled from DevicePollTask(),
  // so either:
  // * device_poll_thread_ is running normally
  // * device_poll_thread_ scheduled us, but then a ResetTask() or DestroyTask()
  //   shut it down, in which case we're either in kResetting or kError states
  //   respectively, and we should have early-outed already.
  DCHECK(device_poll_thread_.message_loop());
  // Queue the DevicePollTask() now.
  device_poll_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&V4L2VideoDecodeAccelerator::DevicePollTask,
                 base::Unretained(this),
                 poll_device));

  DVLOG(1) << "ServiceDeviceTask(): buffer counts: DEC["
           << decoder_input_queue_.size() << "->"
           << input_ready_queue_.size() << "] => DEVICE["
           << free_input_buffers_.size() << "+"
           << input_buffer_queued_count_ << "/"
           << input_buffer_map_.size() << "->"
           << free_output_buffers_.size() << "+"
           << output_buffer_queued_count_ << "/"
           << output_buffer_map_.size() << "] => VDA["
           << decoder_frames_at_client_ << "]";

  ScheduleDecodeBufferTaskIfNeeded();
  StartResolutionChangeIfNeeded();
}

void V4L2VideoDecodeAccelerator::Enqueue() {
  DVLOG(3) << "Enqueue()";
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  DCHECK_NE(decoder_state_, kUninitialized);
  TRACE_EVENT0("Video Decoder", "V4L2VDA::Enqueue");

  // Drain the pipe of completed decode buffers.
  const int old_inputs_queued = input_buffer_queued_count_;
  while (!input_ready_queue_.empty()) {
    if (!EnqueueInputRecord())
      return;
  }
  if (old_inputs_queued == 0 && input_buffer_queued_count_ != 0) {
    // We just started up a previously empty queue.
    // Queue state changed; signal interrupt.
    if (!device_->SetDevicePollInterrupt()) {
      DPLOG(ERROR) << "SetDevicePollInterrupt(): failed";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    }
    // Start VIDIOC_STREAMON if we haven't yet.
    if (!input_streamon_) {
      __u32 type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
      IOCTL_OR_ERROR_RETURN(VIDIOC_STREAMON, &type);
      input_streamon_ = true;
    }
  }

  // Enqueue all the outputs we can.
  const int old_outputs_queued = output_buffer_queued_count_;
  while (!free_output_buffers_.empty()) {
    if (!EnqueueOutputRecord())
      return;
  }
  if (old_outputs_queued == 0 && output_buffer_queued_count_ != 0) {
    // We just started up a previously empty queue.
    // Queue state changed; signal interrupt.
    if (!device_->SetDevicePollInterrupt()) {
      DPLOG(ERROR) << "SetDevicePollInterrupt(): failed";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    }
    // Start VIDIOC_STREAMON if we haven't yet.
    if (!output_streamon_) {
      __u32 type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
      IOCTL_OR_ERROR_RETURN(VIDIOC_STREAMON, &type);
      output_streamon_ = true;
    }
  }
}

void V4L2VideoDecodeAccelerator::DequeueEvents() {
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  DCHECK_NE(decoder_state_, kUninitialized);
  DVLOG(3) << "DequeueEvents()";

  struct v4l2_event ev;
  memset(&ev, 0, sizeof(ev));

  while (device_->Ioctl(VIDIOC_DQEVENT, &ev) == 0) {
    if (ev.type == V4L2_EVENT_RESOLUTION_CHANGE) {
      DVLOG(3) << "DequeueEvents(): got resolution change event.";
      DCHECK(!resolution_change_pending_);
      resolution_change_pending_ = IsResolutionChangeNecessary();
    } else {
      DLOG(FATAL) << "DequeueEvents(): got an event (" << ev.type
                  << ") we haven't subscribed to.";
    }
  }
}

void V4L2VideoDecodeAccelerator::Dequeue() {
  DVLOG(3) << "Dequeue()";
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  DCHECK_NE(decoder_state_, kUninitialized);
  TRACE_EVENT0("Video Decoder", "V4L2VDA::Dequeue");

  // Dequeue completed input (VIDEO_OUTPUT) buffers, and recycle to the free
  // list.
  while (input_buffer_queued_count_ > 0) {
    DCHECK(input_streamon_);
    struct v4l2_buffer dqbuf;
    struct v4l2_plane planes[1];
    memset(&dqbuf, 0, sizeof(dqbuf));
    memset(planes, 0, sizeof(planes));
    dqbuf.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    dqbuf.memory = V4L2_MEMORY_MMAP;
    dqbuf.m.planes = planes;
    dqbuf.length = 1;
    if (device_->Ioctl(VIDIOC_DQBUF, &dqbuf) != 0) {
      if (errno == EAGAIN) {
        // EAGAIN if we're just out of buffers to dequeue.
        break;
      }
      DPLOG(ERROR) << "Dequeue(): ioctl() failed: VIDIOC_DQBUF";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    }
    InputRecord& input_record = input_buffer_map_[dqbuf.index];
    DCHECK(input_record.at_device);
    free_input_buffers_.push_back(dqbuf.index);
    input_record.at_device = false;
    input_record.bytes_used = 0;
    input_record.input_id = -1;
    input_buffer_queued_count_--;
  }

  // Dequeue completed output (VIDEO_CAPTURE) buffers, and queue to the
  // completed queue.
  while (output_buffer_queued_count_ > 0) {
    DCHECK(output_streamon_);
    struct v4l2_buffer dqbuf;
    scoped_ptr<struct v4l2_plane[]> planes(
        new v4l2_plane[output_planes_count_]);
    memset(&dqbuf, 0, sizeof(dqbuf));
    memset(planes.get(), 0, sizeof(struct v4l2_plane) * output_planes_count_);
    dqbuf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    dqbuf.memory = V4L2_MEMORY_MMAP;
    dqbuf.m.planes = planes.get();
    dqbuf.length = output_planes_count_;
    if (device_->Ioctl(VIDIOC_DQBUF, &dqbuf) != 0) {
      if (errno == EAGAIN) {
        // EAGAIN if we're just out of buffers to dequeue.
        break;
      }
      DPLOG(ERROR) << "Dequeue(): ioctl() failed: VIDIOC_DQBUF";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    }
    OutputRecord& output_record = output_buffer_map_[dqbuf.index];
    DCHECK(output_record.at_device);
    DCHECK(!output_record.at_client);
    DCHECK_NE(output_record.egl_image, EGL_NO_IMAGE_KHR);
    DCHECK_NE(output_record.picture_id, -1);
    output_record.at_device = false;
    if (dqbuf.m.planes[0].bytesused + dqbuf.m.planes[1].bytesused == 0) {
      // This is an empty output buffer returned as part of a flush.
      free_output_buffers_.push(dqbuf.index);
    } else {
      DCHECK_GE(dqbuf.timestamp.tv_sec, 0);
      output_record.at_client = true;
      DVLOG(3) << "Dequeue(): returning input_id=" << dqbuf.timestamp.tv_sec
               << " as picture_id=" << output_record.picture_id;
      const media::Picture& picture =
          media::Picture(output_record.picture_id, dqbuf.timestamp.tv_sec);
      pending_picture_ready_.push(
          PictureRecord(output_record.cleared, picture));
      SendPictureReady();
      output_record.cleared = true;
      decoder_frames_at_client_++;
    }
    output_buffer_queued_count_--;
  }

  NotifyFlushDoneIfNeeded();
}

bool V4L2VideoDecodeAccelerator::EnqueueInputRecord() {
  DVLOG(3) << "EnqueueInputRecord()";
  DCHECK(!input_ready_queue_.empty());

  // Enqueue an input (VIDEO_OUTPUT) buffer.
  const int buffer = input_ready_queue_.front();
  InputRecord& input_record = input_buffer_map_[buffer];
  DCHECK(!input_record.at_device);
  struct v4l2_buffer qbuf;
  struct v4l2_plane qbuf_plane;
  memset(&qbuf, 0, sizeof(qbuf));
  memset(&qbuf_plane, 0, sizeof(qbuf_plane));
  qbuf.index                 = buffer;
  qbuf.type                  = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  qbuf.timestamp.tv_sec      = input_record.input_id;
  qbuf.memory                = V4L2_MEMORY_MMAP;
  qbuf.m.planes              = &qbuf_plane;
  qbuf.m.planes[0].bytesused = input_record.bytes_used;
  qbuf.length                = 1;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QBUF, &qbuf);
  input_ready_queue_.pop();
  input_record.at_device = true;
  input_buffer_queued_count_++;
  DVLOG(3) << "EnqueueInputRecord(): enqueued input_id="
           << input_record.input_id << " size="  << input_record.bytes_used;
  return true;
}

bool V4L2VideoDecodeAccelerator::EnqueueOutputRecord() {
  DVLOG(3) << "EnqueueOutputRecord()";
  DCHECK(!free_output_buffers_.empty());

  // Enqueue an output (VIDEO_CAPTURE) buffer.
  const int buffer = free_output_buffers_.front();
  OutputRecord& output_record = output_buffer_map_[buffer];
  DCHECK(!output_record.at_device);
  DCHECK(!output_record.at_client);
  DCHECK_NE(output_record.egl_image, EGL_NO_IMAGE_KHR);
  DCHECK_NE(output_record.picture_id, -1);
  if (output_record.egl_sync != EGL_NO_SYNC_KHR) {
    TRACE_EVENT0("Video Decoder",
                 "V4L2VDA::EnqueueOutputRecord: eglClientWaitSyncKHR");
    // If we have to wait for completion, wait.  Note that
    // free_output_buffers_ is a FIFO queue, so we always wait on the
    // buffer that has been in the queue the longest.
    if (eglClientWaitSyncKHR(egl_display_, output_record.egl_sync, 0,
                             EGL_FOREVER_KHR) == EGL_FALSE) {
      // This will cause tearing, but is safe otherwise.
      DVLOG(1) << __func__ << " eglClientWaitSyncKHR failed!";
    }
    if (eglDestroySyncKHR(egl_display_, output_record.egl_sync) != EGL_TRUE) {
      DLOG(FATAL) << __func__ << " eglDestroySyncKHR failed!";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return false;
    }
    output_record.egl_sync = EGL_NO_SYNC_KHR;
  }
  struct v4l2_buffer qbuf;
  scoped_ptr<struct v4l2_plane[]> qbuf_planes(
      new v4l2_plane[output_planes_count_]);
  memset(&qbuf, 0, sizeof(qbuf));
  memset(
      qbuf_planes.get(), 0, sizeof(struct v4l2_plane) * output_planes_count_);
  qbuf.index    = buffer;
  qbuf.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  qbuf.memory   = V4L2_MEMORY_MMAP;
  qbuf.m.planes = qbuf_planes.get();
  qbuf.length = output_planes_count_;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QBUF, &qbuf);
  free_output_buffers_.pop();
  output_record.at_device = true;
  output_buffer_queued_count_++;
  return true;
}

void V4L2VideoDecodeAccelerator::ReusePictureBufferTask(
    int32 picture_buffer_id, scoped_ptr<EGLSyncKHRRef> egl_sync_ref) {
  DVLOG(3) << "ReusePictureBufferTask(): picture_buffer_id="
           << picture_buffer_id;
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  TRACE_EVENT0("Video Decoder", "V4L2VDA::ReusePictureBufferTask");

  // We run ReusePictureBufferTask even if we're in kResetting.
  if (decoder_state_ == kError) {
    DVLOG(2) << "ReusePictureBufferTask(): early out: kError state";
    return;
  }

  if (decoder_state_ == kChangingResolution) {
    DVLOG(2) << "ReusePictureBufferTask(): early out: kChangingResolution";
    return;
  }

  size_t index;
  for (index = 0; index < output_buffer_map_.size(); ++index)
    if (output_buffer_map_[index].picture_id == picture_buffer_id)
      break;

  if (index >= output_buffer_map_.size()) {
    // It's possible that we've already posted a DismissPictureBuffer for this
    // picture, but it has not yet executed when this ReusePictureBuffer was
    // posted to us by the client. In that case just ignore this (we've already
    // dismissed it and accounted for that) and let the sync object get
    // destroyed.
    DVLOG(4) << "ReusePictureBufferTask(): got picture id= "
             << picture_buffer_id << " not in use (anymore?).";
    return;
  }

  OutputRecord& output_record = output_buffer_map_[index];
  if (output_record.at_device || !output_record.at_client) {
    DLOG(ERROR) << "ReusePictureBufferTask(): picture_buffer_id not reusable";
    NOTIFY_ERROR(INVALID_ARGUMENT);
    return;
  }

  DCHECK_EQ(output_record.egl_sync, EGL_NO_SYNC_KHR);
  DCHECK(!output_record.at_device);
  output_record.at_client = false;
  output_record.egl_sync = egl_sync_ref->egl_sync;
  free_output_buffers_.push(index);
  decoder_frames_at_client_--;
  // Take ownership of the EGLSync.
  egl_sync_ref->egl_sync = EGL_NO_SYNC_KHR;
  // We got a buffer back, so enqueue it back.
  Enqueue();
}

void V4L2VideoDecodeAccelerator::FlushTask() {
  DVLOG(3) << "FlushTask()";
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  TRACE_EVENT0("Video Decoder", "V4L2VDA::FlushTask");

  // Flush outstanding buffers.
  if (decoder_state_ == kInitialized || decoder_state_ == kAfterReset) {
    // There's nothing in the pipe, so return done immediately.
    DVLOG(3) << "FlushTask(): returning flush";
    child_message_loop_proxy_->PostTask(
        FROM_HERE, base::Bind(&Client::NotifyFlushDone, client_));
    return;
  } else if (decoder_state_ == kError) {
    DVLOG(2) << "FlushTask(): early out: kError state";
    return;
  }

  // We don't support stacked flushing.
  DCHECK(!decoder_flushing_);

  // Queue up an empty buffer -- this triggers the flush.
  decoder_input_queue_.push(
      linked_ptr<BitstreamBufferRef>(new BitstreamBufferRef(
          io_client_, io_message_loop_proxy_, NULL, 0, kFlushBufferId)));
  decoder_flushing_ = true;
  SendPictureReady();  // Send all pending PictureReady.

  ScheduleDecodeBufferTaskIfNeeded();
}

void V4L2VideoDecodeAccelerator::NotifyFlushDoneIfNeeded() {
  if (!decoder_flushing_)
    return;

  // Pipeline is empty when:
  // * Decoder input queue is empty of non-delayed buffers.
  // * There is no currently filling input buffer.
  // * Input holding queue is empty.
  // * All input (VIDEO_OUTPUT) buffers are returned.
  if (!decoder_input_queue_.empty()) {
    if (decoder_input_queue_.front()->input_id !=
        decoder_delay_bitstream_buffer_id_)
      return;
  }
  if (decoder_current_input_buffer_ != -1)
    return;
  if ((input_ready_queue_.size() + input_buffer_queued_count_) != 0)
    return;

  // TODO(posciak): crbug.com/270039. Exynos requires a streamoff-streamon
  // sequence after flush to continue, even if we are not resetting. This would
  // make sense, because we don't really want to resume from a non-resume point
  // (e.g. not from an IDR) if we are flushed.
  // MSE player however triggers a Flush() on chunk end, but never Reset(). One
  // could argue either way, or even say that Flush() is not needed/harmful when
  // transitioning to next chunk.
  // For now, do the streamoff-streamon cycle to satisfy Exynos and not freeze
  // when doing MSE. This should be harmless otherwise.
  if (!StopDevicePoll(false))
    return;

  if (!StartDevicePoll())
    return;

  decoder_delay_bitstream_buffer_id_ = -1;
  decoder_flushing_ = false;
  DVLOG(3) << "NotifyFlushDoneIfNeeded(): returning flush";
  child_message_loop_proxy_->PostTask(
      FROM_HERE, base::Bind(&Client::NotifyFlushDone, client_));

  // While we were flushing, we early-outed DecodeBufferTask()s.
  ScheduleDecodeBufferTaskIfNeeded();
}

void V4L2VideoDecodeAccelerator::ResetTask() {
  DVLOG(3) << "ResetTask()";
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  TRACE_EVENT0("Video Decoder", "V4L2VDA::ResetTask");

  if (decoder_state_ == kError) {
    DVLOG(2) << "ResetTask(): early out: kError state";
    return;
  }

  // If we are in the middle of switching resolutions, postpone reset until
  // it's done. We don't have to worry about timing of this wrt to decoding,
  // because input pipe is already stopped if we are changing resolution.
  // We will come back here after we are done with the resolution change.
  DCHECK(!resolution_change_reset_pending_);
  if (resolution_change_pending_ || decoder_state_ == kChangingResolution) {
    resolution_change_reset_pending_ = true;
    return;
  }

  // We stop streaming and clear buffer tracking info (not preserving inputs).
  // StopDevicePoll() unconditionally does _not_ destroy buffers, however.
  if (!StopDevicePoll(false))
    return;

  decoder_current_bitstream_buffer_.reset();
  while (!decoder_input_queue_.empty())
    decoder_input_queue_.pop();

  decoder_current_input_buffer_ = -1;

  // If we were flushing, we'll never return any more BitstreamBuffers or
  // PictureBuffers; they have all been dropped and returned by now.
  NotifyFlushDoneIfNeeded();

  // Mark that we're resetting, then enqueue a ResetDoneTask().  All intervening
  // jobs will early-out in the kResetting state.
  decoder_state_ = kResetting;
  SendPictureReady();  // Send all pending PictureReady.
  decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &V4L2VideoDecodeAccelerator::ResetDoneTask, base::Unretained(this)));
}

void V4L2VideoDecodeAccelerator::ResetDoneTask() {
  DVLOG(3) << "ResetDoneTask()";
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  TRACE_EVENT0("Video Decoder", "V4L2VDA::ResetDoneTask");

  if (decoder_state_ == kError) {
    DVLOG(2) << "ResetDoneTask(): early out: kError state";
    return;
  }

  if (!StartDevicePoll())
    return;

  // We might have received a resolution change event while we were waiting
  // for the reset to finish. The codec will not post another event if the
  // resolution after reset remains the same as the one to which were just
  // about to switch, so preserve the event across reset so we can address
  // it after resuming.

  // Reset format-specific bits.
  if (video_profile_ >= media::H264PROFILE_MIN &&
      video_profile_ <= media::H264PROFILE_MAX) {
    decoder_h264_parser_.reset(new media::H264Parser());
  }

  // Jobs drained, we're finished resetting.
  DCHECK_EQ(decoder_state_, kResetting);
  if (output_buffer_map_.empty()) {
    // We must have gotten Reset() before we had a chance to request buffers
    // from the client.
    decoder_state_ = kInitialized;
  } else {
    decoder_state_ = kAfterReset;
  }

  decoder_partial_frame_pending_ = false;
  decoder_delay_bitstream_buffer_id_ = -1;
  child_message_loop_proxy_->PostTask(FROM_HERE, base::Bind(
      &Client::NotifyResetDone, client_));

  // While we were resetting, we early-outed DecodeBufferTask()s.
  ScheduleDecodeBufferTaskIfNeeded();
}

void V4L2VideoDecodeAccelerator::DestroyTask() {
  DVLOG(3) << "DestroyTask()";
  TRACE_EVENT0("Video Decoder", "V4L2VDA::DestroyTask");

  // DestroyTask() should run regardless of decoder_state_.

  // Stop streaming and the device_poll_thread_.
  StopDevicePoll(false);

  decoder_current_bitstream_buffer_.reset();
  decoder_current_input_buffer_ = -1;
  decoder_decode_buffer_tasks_scheduled_ = 0;
  decoder_frames_at_client_ = 0;
  while (!decoder_input_queue_.empty())
    decoder_input_queue_.pop();
  decoder_flushing_ = false;

  // Set our state to kError.  Just in case.
  decoder_state_ = kError;
}

bool V4L2VideoDecodeAccelerator::StartDevicePoll() {
  DVLOG(3) << "StartDevicePoll()";
  DCHECK(!device_poll_thread_.IsRunning());
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());

  // Start up the device poll thread and schedule its first DevicePollTask().
  if (!device_poll_thread_.Start()) {
    DLOG(ERROR) << "StartDevicePoll(): Device thread failed to start";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }
  device_poll_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &V4L2VideoDecodeAccelerator::DevicePollTask,
      base::Unretained(this),
      0));

  return true;
}

bool V4L2VideoDecodeAccelerator::StopDevicePoll(bool keep_input_state) {
  DVLOG(3) << "StopDevicePoll()";
  if (decoder_thread_.IsRunning())
    DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());

  // Signal the DevicePollTask() to stop, and stop the device poll thread.
  if (!device_->SetDevicePollInterrupt()) {
    DPLOG(ERROR) << "SetDevicePollInterrupt(): failed";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }
  device_poll_thread_.Stop();
  // Clear the interrupt now, to be sure.
  if (!device_->ClearDevicePollInterrupt()) {
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }

  // Stop streaming.
  if (!keep_input_state) {
    if (input_streamon_) {
      __u32 type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
      IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_STREAMOFF, &type);
    }
    input_streamon_ = false;
  }
  if (output_streamon_) {
    __u32 type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_STREAMOFF, &type);
  }
  output_streamon_ = false;

  // Reset all our accounting info.
  if (!keep_input_state) {
    while (!input_ready_queue_.empty())
      input_ready_queue_.pop();
    free_input_buffers_.clear();
    for (size_t i = 0; i < input_buffer_map_.size(); ++i) {
      free_input_buffers_.push_back(i);
      input_buffer_map_[i].at_device = false;
      input_buffer_map_[i].bytes_used = 0;
      input_buffer_map_[i].input_id = -1;
    }
    input_buffer_queued_count_ = 0;
  }

  while (!free_output_buffers_.empty())
    free_output_buffers_.pop();

  for (size_t i = 0; i < output_buffer_map_.size(); ++i) {
    OutputRecord& output_record = output_buffer_map_[i];
    DCHECK(!(output_record.at_client && output_record.at_device));

    // After streamoff, the device drops ownership of all buffers, even if
    // we don't dequeue them explicitly.
    output_buffer_map_[i].at_device = false;
    // Some of them may still be owned by the client however.
    // Reuse only those that aren't.
    if (!output_record.at_client) {
      DCHECK_EQ(output_record.egl_sync, EGL_NO_SYNC_KHR);
      free_output_buffers_.push(i);
    }
  }
  output_buffer_queued_count_ = 0;

  DVLOG(3) << "StopDevicePoll(): device poll stopped";
  return true;
}

void V4L2VideoDecodeAccelerator::StartResolutionChangeIfNeeded() {
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  DCHECK_NE(decoder_state_, kUninitialized);
  DCHECK_NE(decoder_state_, kResetting);

  if (!resolution_change_pending_)
    return;

  DVLOG(3) << "No more work, initiate resolution change";

  // Keep input queue.
  if (!StopDevicePoll(true))
    return;

  decoder_state_ = kChangingResolution;
  DCHECK(resolution_change_pending_);
  resolution_change_pending_ = false;

  // Post a task to clean up buffers on child thread. This will also ensure
  // that we won't accept ReusePictureBuffer() anymore after that.
  child_message_loop_proxy_->PostTask(FROM_HERE, base::Bind(
      &V4L2VideoDecodeAccelerator::ResolutionChangeDestroyBuffers,
      weak_this_));
}

void V4L2VideoDecodeAccelerator::FinishResolutionChange() {
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  DCHECK_EQ(decoder_state_, kChangingResolution);
  DVLOG(3) << "FinishResolutionChange()";

  if (decoder_state_ == kError) {
    DVLOG(2) << "FinishResolutionChange(): early out: kError state";
    return;
  }

  struct v4l2_format format;
  bool again;
  bool ret = GetFormatInfo(&format, &again);
  if (!ret || again) {
    DVLOG(3) << "Couldn't get format information after resolution change";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  if (!CreateBuffersForFormat(format)) {
    DVLOG(3) << "Couldn't reallocate buffers after resolution change";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  decoder_state_ = kDecoding;

  if (resolution_change_reset_pending_) {
    resolution_change_reset_pending_ = false;
    ResetTask();
    return;
  }

  if (!StartDevicePoll())
    return;

  Enqueue();
  ScheduleDecodeBufferTaskIfNeeded();
}

void V4L2VideoDecodeAccelerator::DevicePollTask(bool poll_device) {
  DVLOG(3) << "DevicePollTask()";
  DCHECK_EQ(device_poll_thread_.message_loop(), base::MessageLoop::current());
  TRACE_EVENT0("Video Decoder", "V4L2VDA::DevicePollTask");

  bool event_pending = false;

  if (!device_->Poll(poll_device, &event_pending)) {
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  // All processing should happen on ServiceDeviceTask(), since we shouldn't
  // touch decoder state from this thread.
  decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &V4L2VideoDecodeAccelerator::ServiceDeviceTask,
      base::Unretained(this), event_pending));
}

void V4L2VideoDecodeAccelerator::NotifyError(Error error) {
  DVLOG(2) << "NotifyError()";

  if (!child_message_loop_proxy_->BelongsToCurrentThread()) {
    child_message_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &V4L2VideoDecodeAccelerator::NotifyError, weak_this_, error));
    return;
  }

  if (client_) {
    client_->NotifyError(error);
    client_ptr_factory_.reset();
  }
}

void V4L2VideoDecodeAccelerator::SetDecoderState(State state) {
  DVLOG(3) << "SetDecoderState(): state=" << state;

  // We can touch decoder_state_ only if this is the decoder thread or the
  // decoder thread isn't running.
  if (decoder_thread_.message_loop() != NULL &&
      decoder_thread_.message_loop() != base::MessageLoop::current()) {
    decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
        &V4L2VideoDecodeAccelerator::SetDecoderState,
        base::Unretained(this), state));
  } else {
    decoder_state_ = state;
  }
}

bool V4L2VideoDecodeAccelerator::GetFormatInfo(struct v4l2_format* format,
                                                 bool* again) {
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());

  *again = false;
  memset(format, 0, sizeof(*format));
  format->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  if (device_->Ioctl(VIDIOC_G_FMT, format) != 0) {
    if (errno == EINVAL) {
      // EINVAL means we haven't seen sufficient stream to decode the format.
      *again = true;
      return true;
    } else {
      DPLOG(ERROR) << __func__ << "(): ioctl() failed: VIDIOC_G_FMT";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return false;
    }
  }

  return true;
}

bool V4L2VideoDecodeAccelerator::CreateBuffersForFormat(
    const struct v4l2_format& format) {
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  output_planes_count_ = format.fmt.pix_mp.num_planes;
  frame_buffer_size_.SetSize(
      format.fmt.pix_mp.width, format.fmt.pix_mp.height);
  DVLOG(3) << "CreateBuffersForFormat(): new resolution: "
           << frame_buffer_size_.ToString();

  if (!CreateOutputBuffers())
    return false;

  return true;
}

bool V4L2VideoDecodeAccelerator::CreateInputBuffers() {
  DVLOG(3) << "CreateInputBuffers()";
  // We always run this as we prepare to initialize.
  DCHECK_EQ(decoder_state_, kUninitialized);
  DCHECK(!input_streamon_);
  DCHECK(input_buffer_map_.empty());

  __u32 pixelformat = V4L2Device::VideoCodecProfileToV4L2PixFmt(video_profile_);
  if (!pixelformat) {
    NOTREACHED();
    return false;
  }

  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type                              = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  format.fmt.pix_mp.pixelformat            = pixelformat;
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kIgnoreResolutionLimitsForAcceleratedVideoDecode))
    format.fmt.pix_mp.plane_fmt[0].sizeimage = kInputBufferMaxSizeFor4k;
  else
    format.fmt.pix_mp.plane_fmt[0].sizeimage = kInputBufferMaxSizeFor1080p;
  format.fmt.pix_mp.num_planes             = 1;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_FMT, &format);

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count  = kInputBufferCount;
  reqbufs.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_REQBUFS, &reqbufs);
  input_buffer_map_.resize(reqbufs.count);
  for (size_t i = 0; i < input_buffer_map_.size(); ++i) {
    free_input_buffers_.push_back(i);

    // Query for the MEMORY_MMAP pointer.
    struct v4l2_plane planes[1];
    struct v4l2_buffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    memset(planes, 0, sizeof(planes));
    buffer.index    = i;
    buffer.type     = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    buffer.memory   = V4L2_MEMORY_MMAP;
    buffer.m.planes = planes;
    buffer.length   = 1;
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QUERYBUF, &buffer);
    void* address = device_->Mmap(NULL,
                                  buffer.m.planes[0].length,
                                  PROT_READ | PROT_WRITE,
                                  MAP_SHARED,
                                  buffer.m.planes[0].m.mem_offset);
    if (address == MAP_FAILED) {
      DPLOG(ERROR) << "CreateInputBuffers(): mmap() failed";
      return false;
    }
    input_buffer_map_[i].address = address;
    input_buffer_map_[i].length = buffer.m.planes[0].length;
  }

  return true;
}

bool V4L2VideoDecodeAccelerator::CreateOutputBuffers() {
  DVLOG(3) << "CreateOutputBuffers()";
  DCHECK(decoder_state_ == kInitialized ||
         decoder_state_ == kChangingResolution);
  DCHECK(!output_streamon_);
  DCHECK(output_buffer_map_.empty());

  // Number of output buffers we need.
  struct v4l2_control ctrl;
  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_G_CTRL, &ctrl);
  output_dpb_size_ = ctrl.value;

  // Output format setup in Initialize().

  // Allocate the output buffers.
  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count  = output_dpb_size_ + kDpbOutputBufferExtraCount;
  reqbufs.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_REQBUFS, &reqbufs);

  output_buffer_map_.resize(reqbufs.count);

  DVLOG(3) << "CreateOutputBuffers(): ProvidePictureBuffers(): "
           << "buffer_count=" << output_buffer_map_.size()
           << ", width=" << frame_buffer_size_.width()
           << ", height=" << frame_buffer_size_.height();
  child_message_loop_proxy_->PostTask(FROM_HERE,
                                      base::Bind(&Client::ProvidePictureBuffers,
                                                 client_,
                                                 output_buffer_map_.size(),
                                                 frame_buffer_size_,
                                                 device_->GetTextureTarget()));

  // Wait for the client to call AssignPictureBuffers() on the Child thread.
  // We do this, because if we continue decoding without finishing buffer
  // allocation, we may end up Resetting before AssignPictureBuffers arrives,
  // resulting in unnecessary complications and subtle bugs.
  // For example, if the client calls Decode(Input1), Reset(), Decode(Input2)
  // in a sequence, and Decode(Input1) results in us getting here and exiting
  // without waiting, we might end up running Reset{,Done}Task() before
  // AssignPictureBuffers is scheduled, thus cleaning up and pushing buffers
  // to the free_output_buffers_ map twice. If we somehow marked buffers as
  // not ready, we'd need special handling for restarting the second Decode
  // task and delaying it anyway.
  // Waiting here is not very costly and makes reasoning about different
  // situations much simpler.
  pictures_assigned_.Wait();

  Enqueue();
  return true;
}

void V4L2VideoDecodeAccelerator::DestroyInputBuffers() {
  DVLOG(3) << "DestroyInputBuffers()";
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(!input_streamon_);

  for (size_t i = 0; i < input_buffer_map_.size(); ++i) {
    if (input_buffer_map_[i].address != NULL) {
      device_->Munmap(input_buffer_map_[i].address,
                      input_buffer_map_[i].length);
    }
  }

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  IOCTL_OR_LOG_ERROR(VIDIOC_REQBUFS, &reqbufs);

  input_buffer_map_.clear();
  free_input_buffers_.clear();
}

bool V4L2VideoDecodeAccelerator::DestroyOutputBuffers() {
  DVLOG(3) << "DestroyOutputBuffers()";
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(!output_streamon_);
  bool success = true;

  for (size_t i = 0; i < output_buffer_map_.size(); ++i) {
    OutputRecord& output_record = output_buffer_map_[i];

    if (output_record.egl_image != EGL_NO_IMAGE_KHR) {
      if (device_->DestroyEGLImage(egl_display_, output_record.egl_image) !=
          EGL_TRUE) {
        DVLOG(1) << __func__ << " DestroyEGLImage failed.";
        success = false;
      }
    }

    if (output_record.egl_sync != EGL_NO_SYNC_KHR) {
      if (eglDestroySyncKHR(egl_display_, output_record.egl_sync) != EGL_TRUE) {
        DVLOG(1) << __func__ << " eglDestroySyncKHR failed.";
        success = false;
      }
    }

    DVLOG(1) << "DestroyOutputBuffers(): dismissing PictureBuffer id="
             << output_record.picture_id;
    child_message_loop_proxy_->PostTask(
        FROM_HERE,
        base::Bind(
            &Client::DismissPictureBuffer, client_, output_record.picture_id));
  }

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  if (device_->Ioctl(VIDIOC_REQBUFS, &reqbufs) != 0) {
    DPLOG(ERROR) << "DestroyOutputBuffers() ioctl() failed: VIDIOC_REQBUFS";
    success = false;
  }

  output_buffer_map_.clear();
  while (!free_output_buffers_.empty())
    free_output_buffers_.pop();

  return success;
}

void V4L2VideoDecodeAccelerator::ResolutionChangeDestroyBuffers() {
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DVLOG(3) << "ResolutionChangeDestroyBuffers()";

  if (!DestroyOutputBuffers()) {
    DLOG(FATAL) << __func__ << " Failed destroying output buffers.";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  // Finish resolution change on decoder thread.
  decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &V4L2VideoDecodeAccelerator::FinishResolutionChange,
      base::Unretained(this)));
}

void V4L2VideoDecodeAccelerator::SendPictureReady() {
  DVLOG(3) << "SendPictureReady()";
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  bool resetting_or_flushing =
      (decoder_state_ == kResetting || decoder_flushing_);
  while (pending_picture_ready_.size() > 0) {
    bool cleared = pending_picture_ready_.front().cleared;
    const media::Picture& picture = pending_picture_ready_.front().picture;
    if (cleared && picture_clearing_count_ == 0) {
      // This picture is cleared. Post it to IO thread to reduce latency. This
      // should be the case after all pictures are cleared at the beginning.
      io_message_loop_proxy_->PostTask(
          FROM_HERE, base::Bind(&Client::PictureReady, io_client_, picture));
      pending_picture_ready_.pop();
    } else if (!cleared || resetting_or_flushing) {
      DVLOG(3) << "SendPictureReady()"
               << ". cleared=" << pending_picture_ready_.front().cleared
               << ", decoder_state_=" << decoder_state_
               << ", decoder_flushing_=" << decoder_flushing_
               << ", picture_clearing_count_=" << picture_clearing_count_;
      // If the picture is not cleared, post it to the child thread because it
      // has to be cleared in the child thread. A picture only needs to be
      // cleared once. If the decoder is resetting or flushing, send all
      // pictures to ensure PictureReady arrive before reset or flush done.
      child_message_loop_proxy_->PostTaskAndReply(
          FROM_HERE,
          base::Bind(&Client::PictureReady, client_, picture),
          // Unretained is safe. If Client::PictureReady gets to run, |this| is
          // alive. Destroy() will wait the decode thread to finish.
          base::Bind(&V4L2VideoDecodeAccelerator::PictureCleared,
                     base::Unretained(this)));
      picture_clearing_count_++;
      pending_picture_ready_.pop();
    } else {
      // This picture is cleared. But some pictures are about to be cleared on
      // the child thread. To preserve the order, do not send this until those
      // pictures are cleared.
      break;
    }
  }
}

void V4L2VideoDecodeAccelerator::PictureCleared() {
  DVLOG(3) << "PictureCleared(). clearing count=" << picture_clearing_count_;
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  DCHECK_GT(picture_clearing_count_, 0);
  picture_clearing_count_--;
  SendPictureReady();
}

bool V4L2VideoDecodeAccelerator::IsResolutionChangeNecessary() {
  DVLOG(3) << "IsResolutionChangeNecessary() ";

  struct v4l2_control ctrl;
  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_G_CTRL, &ctrl);
  if (ctrl.value != output_dpb_size_) {
    DVLOG(3)
        << "IsResolutionChangeNecessary(): Returning true since DPB mismatch ";
    return true;
  }
  struct v4l2_format format;
  bool again = false;
  bool ret = GetFormatInfo(&format, &again);
  if (!ret || again) {
    DVLOG(3) << "IsResolutionChangeNecessary(): GetFormatInfo() failed";
    return false;
  }
  gfx::Size new_size(base::checked_cast<int>(format.fmt.pix_mp.width),
                     base::checked_cast<int>(format.fmt.pix_mp.height));
  if (frame_buffer_size_ != new_size) {
    DVLOG(3) << "IsResolutionChangeNecessary(): Resolution change detected";
    return true;
  }
  return false;
}

}  // namespace content
