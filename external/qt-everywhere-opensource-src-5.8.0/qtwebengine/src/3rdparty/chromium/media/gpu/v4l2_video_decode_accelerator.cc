// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2_video_decode_accelerator.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_switches.h"
#include "media/filters/h264_parser.h"
#include "media/gpu/shared_memory_region.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/scoped_binders.h"

#define LOGF(level) LOG(level) << __FUNCTION__ << "(): "
#define DLOGF(level) DLOG(level) << __FUNCTION__ << "(): "
#define DVLOGF(level) DVLOG(level) << __FUNCTION__ << "(): "
#define PLOGF(level) PLOG(level) << __FUNCTION__ << "(): "

#define NOTIFY_ERROR(x)                         \
  do {                                          \
    LOGF(ERROR) << "Setting error state:" << x; \
    SetErrorState(x);                           \
  } while (0)

#define IOCTL_OR_ERROR_RETURN_VALUE(type, arg, value, type_str) \
  do {                                                          \
    if (device_->Ioctl(type, arg) != 0) {                       \
      PLOGF(ERROR) << "ioctl() failed: " << type_str;           \
      NOTIFY_ERROR(PLATFORM_FAILURE);                           \
      return value;                                             \
    }                                                           \
  } while (0)

#define IOCTL_OR_ERROR_RETURN(type, arg) \
  IOCTL_OR_ERROR_RETURN_VALUE(type, arg, ((void)0), #type)

#define IOCTL_OR_ERROR_RETURN_FALSE(type, arg) \
  IOCTL_OR_ERROR_RETURN_VALUE(type, arg, false, #type)

#define IOCTL_OR_LOG_ERROR(type, arg)              \
  do {                                             \
    if (device_->Ioctl(type, arg) != 0)            \
      PLOGF(ERROR) << "ioctl() failed: " << #type; \
  } while (0)

namespace media {

// static
const uint32_t V4L2VideoDecodeAccelerator::supported_input_fourccs_[] = {
    V4L2_PIX_FMT_H264, V4L2_PIX_FMT_VP8, V4L2_PIX_FMT_VP9,
};

struct V4L2VideoDecodeAccelerator::BitstreamBufferRef {
  BitstreamBufferRef(
      base::WeakPtr<Client>& client,
      scoped_refptr<base::SingleThreadTaskRunner>& client_task_runner,
      std::unique_ptr<SharedMemoryRegion> shm,
      int32_t input_id);
  ~BitstreamBufferRef();
  const base::WeakPtr<Client> client;
  const scoped_refptr<base::SingleThreadTaskRunner> client_task_runner;
  const std::unique_ptr<SharedMemoryRegion> shm;
  size_t bytes_used;
  const int32_t input_id;
};

struct V4L2VideoDecodeAccelerator::EGLSyncKHRRef {
  EGLSyncKHRRef(EGLDisplay egl_display, EGLSyncKHR egl_sync);
  ~EGLSyncKHRRef();
  EGLDisplay const egl_display;
  EGLSyncKHR egl_sync;
};

struct V4L2VideoDecodeAccelerator::PictureRecord {
  PictureRecord(bool cleared, const Picture& picture);
  ~PictureRecord();
  bool cleared;  // Whether the texture is cleared and safe to render from.
  Picture picture;  // The decoded picture.
};

V4L2VideoDecodeAccelerator::BitstreamBufferRef::BitstreamBufferRef(
    base::WeakPtr<Client>& client,
    scoped_refptr<base::SingleThreadTaskRunner>& client_task_runner,
    std::unique_ptr<SharedMemoryRegion> shm,
    int32_t input_id)
    : client(client),
      client_task_runner(client_task_runner),
      shm(std::move(shm)),
      bytes_used(0),
      input_id(input_id) {}

V4L2VideoDecodeAccelerator::BitstreamBufferRef::~BitstreamBufferRef() {
  if (input_id >= 0) {
    client_task_runner->PostTask(
        FROM_HERE,
        base::Bind(&Client::NotifyEndOfBitstreamBuffer, client, input_id));
  }
}

V4L2VideoDecodeAccelerator::EGLSyncKHRRef::EGLSyncKHRRef(EGLDisplay egl_display,
                                                         EGLSyncKHR egl_sync)
    : egl_display(egl_display), egl_sync(egl_sync) {}

V4L2VideoDecodeAccelerator::EGLSyncKHRRef::~EGLSyncKHRRef() {
  // We don't check for eglDestroySyncKHR failures, because if we get here
  // with a valid sync object, something went wrong and we are getting
  // destroyed anyway.
  if (egl_sync != EGL_NO_SYNC_KHR)
    eglDestroySyncKHR(egl_display, egl_sync);
}

V4L2VideoDecodeAccelerator::InputRecord::InputRecord()
    : at_device(false), address(NULL), length(0), bytes_used(0), input_id(-1) {}

V4L2VideoDecodeAccelerator::InputRecord::~InputRecord() {}

V4L2VideoDecodeAccelerator::OutputRecord::OutputRecord()
    : state(kFree),
      egl_image(EGL_NO_IMAGE_KHR),
      egl_sync(EGL_NO_SYNC_KHR),
      picture_id(-1),
      cleared(false) {}

V4L2VideoDecodeAccelerator::OutputRecord::~OutputRecord() {}

V4L2VideoDecodeAccelerator::PictureRecord::PictureRecord(bool cleared,
                                                         const Picture& picture)
    : cleared(cleared), picture(picture) {}

V4L2VideoDecodeAccelerator::PictureRecord::~PictureRecord() {}

V4L2VideoDecodeAccelerator::V4L2VideoDecodeAccelerator(
    EGLDisplay egl_display,
    const GetGLContextCallback& get_gl_context_cb,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const scoped_refptr<V4L2Device>& device)
    : child_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      decoder_thread_("V4L2DecoderThread"),
      decoder_state_(kUninitialized),
      device_(device),
      decoder_delay_bitstream_buffer_id_(-1),
      decoder_current_input_buffer_(-1),
      decoder_decode_buffer_tasks_scheduled_(0),
      decoder_frames_at_client_(0),
      decoder_flushing_(false),
      resolution_change_reset_pending_(false),
      decoder_partial_frame_pending_(false),
      input_streamon_(false),
      input_buffer_queued_count_(0),
      output_streamon_(false),
      output_buffer_queued_count_(0),
      output_dpb_size_(0),
      output_planes_count_(0),
      picture_clearing_count_(0),
      pictures_assigned_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                         base::WaitableEvent::InitialState::NOT_SIGNALED),
      device_poll_thread_("V4L2DevicePollThread"),
      egl_display_(egl_display),
      get_gl_context_cb_(get_gl_context_cb),
      make_context_current_cb_(make_context_current_cb),
      video_profile_(VIDEO_CODEC_PROFILE_UNKNOWN),
      output_format_fourcc_(0),
      egl_image_format_fourcc_(0),
      egl_image_planes_count_(0),
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

bool V4L2VideoDecodeAccelerator::Initialize(const Config& config,
                                            Client* client) {
  DVLOGF(3) << "profile: " << config.profile;
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(decoder_state_, kUninitialized);

  if (!device_->SupportsDecodeProfileForV4L2PixelFormats(
          config.profile, arraysize(supported_input_fourccs_),
          supported_input_fourccs_)) {
    DVLOGF(1) << "unsupported profile=" << config.profile;
    return false;
  }

  if (config.is_encrypted) {
    NOTREACHED() << "Encrypted streams are not supported for this VDA";
    return false;
  }

  if (config.output_mode != Config::OutputMode::ALLOCATE) {
    NOTREACHED() << "Only ALLOCATE OutputMode is supported by this VDA";
    return false;
  }

  if (get_gl_context_cb_.is_null() || make_context_current_cb_.is_null()) {
    NOTREACHED() << "GL callbacks are required for this VDA";
    return false;
  }

  client_ptr_factory_.reset(new base::WeakPtrFactory<Client>(client));
  client_ = client_ptr_factory_->GetWeakPtr();
  // If we haven't been set up to decode on separate thread via
  // TryToSetupDecodeOnSeparateThread(), use the main thread/client for
  // decode tasks.
  if (!decode_task_runner_) {
    decode_task_runner_ = child_task_runner_;
    DCHECK(!decode_client_);
    decode_client_ = client_;
  }

  video_profile_ = config.profile;

  if (egl_display_ == EGL_NO_DISPLAY) {
    LOGF(ERROR) << "could not get EGLDisplay";
    return false;
  }

  // We need the context to be initialized to query extensions.
  if (!make_context_current_cb_.Run()) {
    LOGF(ERROR) << "could not make context current";
    return false;
  }

// TODO(posciak): crbug.com/450898.
#if defined(ARCH_CPU_ARMEL)
  if (!gl::g_driver_egl.ext.b_EGL_KHR_fence_sync) {
    LOGF(ERROR) << "context does not have EGL_KHR_fence_sync";
    return false;
  }
#endif

  // Capabilities check.
  struct v4l2_capability caps;
  const __u32 kCapsRequired = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QUERYCAP, &caps);
  if ((caps.capabilities & kCapsRequired) != kCapsRequired) {
    LOGF(ERROR) << "ioctl() failed: VIDIOC_QUERYCAP"
                << ", caps check failed: 0x" << std::hex << caps.capabilities;
    return false;
  }

  if (!SetupFormats())
    return false;

  // Subscribe to the resolution change event.
  struct v4l2_event_subscription sub;
  memset(&sub, 0, sizeof(sub));
  sub.type = V4L2_EVENT_SOURCE_CHANGE;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_SUBSCRIBE_EVENT, &sub);

  if (video_profile_ >= H264PROFILE_MIN && video_profile_ <= H264PROFILE_MAX) {
    decoder_h264_parser_.reset(new H264Parser());
  }

  if (!CreateInputBuffers())
    return false;

  if (!decoder_thread_.Start()) {
    LOGF(ERROR) << "decoder thread failed to start";
    return false;
  }

  decoder_state_ = kInitialized;

  // StartDevicePoll will NOTIFY_ERROR on failure, so IgnoreResult is fine here.
  decoder_thread_.message_loop()->PostTask(
      FROM_HERE, base::Bind(base::IgnoreResult(
                                &V4L2VideoDecodeAccelerator::StartDevicePoll),
                            base::Unretained(this)));

  return true;
}

void V4L2VideoDecodeAccelerator::Decode(
    const BitstreamBuffer& bitstream_buffer) {
  DVLOGF(1) << "input_id=" << bitstream_buffer.id()
            << ", size=" << bitstream_buffer.size();
  DCHECK(decode_task_runner_->BelongsToCurrentThread());

  if (bitstream_buffer.id() < 0) {
    LOGF(ERROR) << "Invalid bitstream_buffer, id: " << bitstream_buffer.id();
    if (base::SharedMemory::IsHandleValid(bitstream_buffer.handle()))
      base::SharedMemory::CloseHandle(bitstream_buffer.handle());
    NOTIFY_ERROR(INVALID_ARGUMENT);
    return;
  }

  // DecodeTask() will take care of running a DecodeBufferTask().
  decoder_thread_.message_loop()->PostTask(
      FROM_HERE, base::Bind(&V4L2VideoDecodeAccelerator::DecodeTask,
                            base::Unretained(this), bitstream_buffer));
}

void V4L2VideoDecodeAccelerator::AssignPictureBuffers(
    const std::vector<PictureBuffer>& buffers) {
  DVLOGF(3) << "buffer_count=" << buffers.size();
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  const uint32_t req_buffer_count =
      output_dpb_size_ + kDpbOutputBufferExtraCount;

  if (buffers.size() < req_buffer_count) {
    LOGF(ERROR) << "Failed to provide requested picture buffers. (Got "
                << buffers.size() << ", requested " << req_buffer_count << ")";
    NOTIFY_ERROR(INVALID_ARGUMENT);
    return;
  }

  gl::GLContext* gl_context = get_gl_context_cb_.Run();
  if (!gl_context || !make_context_current_cb_.Run()) {
    LOGF(ERROR) << "could not make context current";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  gl::ScopedTextureBinder bind_restore(GL_TEXTURE_EXTERNAL_OES, 0);

  // It's safe to manipulate all the buffer state here, because the decoder
  // thread is waiting on pictures_assigned_.

  // Allocate the output buffers.
  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = buffers.size();
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  IOCTL_OR_ERROR_RETURN(VIDIOC_REQBUFS, &reqbufs);

  if (reqbufs.count != buffers.size()) {
    DLOGF(ERROR) << "Could not allocate enough output buffers";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  if (image_processor_device_) {
    DCHECK(!image_processor_);
    image_processor_.reset(new V4L2ImageProcessor(image_processor_device_));
    // Unretained is safe because |this| owns image processor and there will be
    // no callbacks after processor destroys.
    if (!image_processor_->Initialize(
            V4L2Device::V4L2PixFmtToVideoPixelFormat(output_format_fourcc_),
            V4L2Device::V4L2PixFmtToVideoPixelFormat(egl_image_format_fourcc_),
            V4L2_MEMORY_DMABUF, visible_size_, coded_size_, visible_size_,
            visible_size_, buffers.size(),
            base::Bind(&V4L2VideoDecodeAccelerator::ImageProcessorError,
                       base::Unretained(this)))) {
      LOGF(ERROR) << "Initialize image processor failed";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    }
    DCHECK(image_processor_->output_allocated_size() == egl_image_size_);
    if (image_processor_->input_allocated_size() != coded_size_) {
      LOGF(ERROR) << "Image processor should be able to take the output coded "
                  << "size of decoder " << coded_size_.ToString()
                  << " without adjusting to "
                  << image_processor_->input_allocated_size().ToString();
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    }
  }

  output_buffer_map_.resize(buffers.size());

  DCHECK(free_output_buffers_.empty());
  for (size_t i = 0; i < output_buffer_map_.size(); ++i) {
    DCHECK(buffers[i].size() == egl_image_size_);

    OutputRecord& output_record = output_buffer_map_[i];
    DCHECK_EQ(output_record.state, kFree);
    DCHECK_EQ(output_record.egl_image, EGL_NO_IMAGE_KHR);
    DCHECK_EQ(output_record.egl_sync, EGL_NO_SYNC_KHR);
    DCHECK_EQ(output_record.picture_id, -1);
    DCHECK_EQ(output_record.cleared, false);
    DCHECK_LE(1u, buffers[i].texture_ids().size());

    gfx::Size egl_image_size;
    if (image_processor_device_) {
      std::vector<base::ScopedFD> fds = device_->GetDmabufsForV4L2Buffer(
          i, output_planes_count_, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
      if (fds.empty()) {
        LOGF(ERROR) << "Failed to get DMABUFs of decoder.";
        NOTIFY_ERROR(PLATFORM_FAILURE);
        return;
      }
      output_record.fds = std::move(fds);
    }

    std::vector<base::ScopedFD> dmabuf_fds;
    dmabuf_fds = egl_image_device_->GetDmabufsForV4L2Buffer(
        i, egl_image_planes_count_, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
    if (dmabuf_fds.empty()) {
      LOGF(ERROR) << "Failed to get DMABUFs for EGLImage.";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    }

    EGLImageKHR egl_image = egl_image_device_->CreateEGLImage(
        egl_display_, gl_context->GetHandle(), buffers[i].texture_ids()[0],
        egl_image_size_, i, egl_image_format_fourcc_, dmabuf_fds);
    if (egl_image == EGL_NO_IMAGE_KHR) {
      LOGF(ERROR) << "could not create EGLImageKHR";
      // Ownership of EGLImages allocated in previous iterations of this loop
      // has been transferred to output_buffer_map_. After we error-out here
      // the destructor will handle their cleanup.
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    }

    output_record.egl_image = egl_image;
    output_record.picture_id = buffers[i].id();
    free_output_buffers_.push(i);
    DVLOGF(3) << "buffer[" << i << "]: picture_id=" << output_record.picture_id;
  }

  pictures_assigned_.Signal();
}

void V4L2VideoDecodeAccelerator::ReusePictureBuffer(int32_t picture_buffer_id) {
  DVLOGF(3) << "picture_buffer_id=" << picture_buffer_id;
  // Must be run on child thread, as we'll insert a sync in the EGL context.
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  if (!make_context_current_cb_.Run()) {
    LOGF(ERROR) << "could not make context current";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  EGLSyncKHR egl_sync = EGL_NO_SYNC_KHR;
// TODO(posciak): crbug.com/450898.
#if defined(ARCH_CPU_ARMEL)
  egl_sync = eglCreateSyncKHR(egl_display_, EGL_SYNC_FENCE_KHR, NULL);
  if (egl_sync == EGL_NO_SYNC_KHR) {
    LOGF(ERROR) << "eglCreateSyncKHR() failed";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }
#endif

  std::unique_ptr<EGLSyncKHRRef> egl_sync_ref(
      new EGLSyncKHRRef(egl_display_, egl_sync));
  decoder_thread_.message_loop()->PostTask(
      FROM_HERE, base::Bind(&V4L2VideoDecodeAccelerator::ReusePictureBufferTask,
                            base::Unretained(this), picture_buffer_id,
                            base::Passed(&egl_sync_ref)));
}

void V4L2VideoDecodeAccelerator::Flush() {
  DVLOGF(3);
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  decoder_thread_.message_loop()->PostTask(
      FROM_HERE, base::Bind(&V4L2VideoDecodeAccelerator::FlushTask,
                            base::Unretained(this)));
}

void V4L2VideoDecodeAccelerator::Reset() {
  DVLOGF(3);
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  decoder_thread_.message_loop()->PostTask(
      FROM_HERE, base::Bind(&V4L2VideoDecodeAccelerator::ResetTask,
                            base::Unretained(this)));
}

void V4L2VideoDecodeAccelerator::Destroy() {
  DVLOGF(3);
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  // We're destroying; cancel all callbacks.
  client_ptr_factory_.reset();
  weak_this_factory_.InvalidateWeakPtrs();

  // If the decoder thread is running, destroy using posted task.
  if (decoder_thread_.IsRunning()) {
    decoder_thread_.message_loop()->PostTask(
        FROM_HERE, base::Bind(&V4L2VideoDecodeAccelerator::DestroyTask,
                              base::Unretained(this)));
    pictures_assigned_.Signal();
    // DestroyTask() will cause the decoder_thread_ to flush all tasks.
    decoder_thread_.Stop();
  } else {
    // Otherwise, call the destroy task directly.
    DestroyTask();
  }

  delete this;
}

bool V4L2VideoDecodeAccelerator::TryToSetupDecodeOnSeparateThread(
    const base::WeakPtr<Client>& decode_client,
    const scoped_refptr<base::SingleThreadTaskRunner>& decode_task_runner) {
  decode_client_ = decode_client;
  decode_task_runner_ = decode_task_runner;
  return true;
}

// static
VideoDecodeAccelerator::SupportedProfiles
V4L2VideoDecodeAccelerator::GetSupportedProfiles() {
  scoped_refptr<V4L2Device> device = V4L2Device::Create(V4L2Device::kDecoder);
  if (!device)
    return SupportedProfiles();

  return device->GetSupportedDecodeProfiles(arraysize(supported_input_fourccs_),
                                            supported_input_fourccs_);
}

void V4L2VideoDecodeAccelerator::DecodeTask(
    const BitstreamBuffer& bitstream_buffer) {
  DVLOGF(3) << "input_id=" << bitstream_buffer.id();
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_NE(decoder_state_, kUninitialized);
  TRACE_EVENT1("Video Decoder", "V4L2VDA::DecodeTask", "input_id",
               bitstream_buffer.id());

  std::unique_ptr<BitstreamBufferRef> bitstream_record(new BitstreamBufferRef(
      decode_client_, decode_task_runner_,
      std::unique_ptr<SharedMemoryRegion>(
          new SharedMemoryRegion(bitstream_buffer, true)),
      bitstream_buffer.id()));

  // Skip empty buffer.
  if (bitstream_buffer.size() == 0)
    return;

  if (!bitstream_record->shm->Map()) {
    LOGF(ERROR) << "could not map bitstream_buffer";
    NOTIFY_ERROR(UNREADABLE_INPUT);
    return;
  }
  DVLOGF(3) << "mapped at=" << bitstream_record->shm->memory();

  if (decoder_state_ == kResetting || decoder_flushing_) {
    // In the case that we're resetting or flushing, we need to delay decoding
    // the BitstreamBuffers that come after the Reset() or Flush() call.  When
    // we're here, we know that this DecodeTask() was scheduled by a Decode()
    // call that came after (in the client thread) the Reset() or Flush() call;
    // thus set up the delay if necessary.
    if (decoder_delay_bitstream_buffer_id_ == -1)
      decoder_delay_bitstream_buffer_id_ = bitstream_record->input_id;
  } else if (decoder_state_ == kError) {
    DVLOGF(2) << "early out: kError state";
    return;
  }

  decoder_input_queue_.push(
      linked_ptr<BitstreamBufferRef>(bitstream_record.release()));
  decoder_decode_buffer_tasks_scheduled_++;
  DecodeBufferTask();
}

void V4L2VideoDecodeAccelerator::DecodeBufferTask() {
  DVLOGF(3);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_NE(decoder_state_, kUninitialized);
  TRACE_EVENT0("Video Decoder", "V4L2VDA::DecodeBufferTask");

  decoder_decode_buffer_tasks_scheduled_--;

  if (decoder_state_ == kResetting) {
    DVLOGF(2) << "early out: kResetting state";
    return;
  } else if (decoder_state_ == kError) {
    DVLOGF(2) << "early out: kError state";
    return;
  } else if (decoder_state_ == kChangingResolution) {
    DVLOGF(2) << "early out: resolution change pending";
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
    const auto& shm = decoder_current_bitstream_buffer_->shm;
    if (shm) {
      DVLOGF(3) << "reading input_id="
                << decoder_current_bitstream_buffer_->input_id
                << ", addr=" << shm->memory() << ", size=" << shm->size();
    } else {
      DCHECK_EQ(decoder_current_bitstream_buffer_->input_id, kFlushBufferId);
      DVLOGF(3) << "reading input_id=kFlushBufferId";
    }
  }
  bool schedule_task = false;
  size_t decoded_size = 0;
  const auto& shm = decoder_current_bitstream_buffer_->shm;
  if (!shm) {
    // This is a dummy buffer, queued to flush the pipe.  Flush.
    DCHECK_EQ(decoder_current_bitstream_buffer_->input_id, kFlushBufferId);
    // Enqueue a buffer guaranteed to be empty.  To do that, we flush the
    // current input, enqueue no data to the next frame, then flush that down.
    schedule_task = true;
    if (decoder_current_input_buffer_ != -1 &&
        input_buffer_map_[decoder_current_input_buffer_].input_id !=
            kFlushBufferId)
      schedule_task = FlushInputFrame();

    if (schedule_task && AppendToInputFrame(NULL, 0) && FlushInputFrame()) {
      DVLOGF(2) << "enqueued flush buffer";
      decoder_partial_frame_pending_ = false;
      schedule_task = true;
    } else {
      // If we failed to enqueue the empty buffer (due to pipeline
      // backpressure), don't advance the bitstream buffer queue, and don't
      // schedule the next task.  This bitstream buffer queue entry will get
      // reprocessed when the pipeline frees up.
      schedule_task = false;
    }
  } else if (shm->size() == 0) {
    // This is a buffer queued from the client that has zero size.  Skip.
    schedule_task = true;
  } else {
    // This is a buffer queued from the client, with actual contents.  Decode.
    const uint8_t* const data =
        reinterpret_cast<const uint8_t*>(shm->memory()) +
        decoder_current_bitstream_buffer_->bytes_used;
    const size_t data_size =
        shm->size() - decoder_current_bitstream_buffer_->bytes_used;
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
    if ((shm ? shm->size() : 0) ==
        decoder_current_bitstream_buffer_->bytes_used) {
      // Our current bitstream buffer is done; return it.
      int32_t input_id = decoder_current_bitstream_buffer_->input_id;
      DVLOGF(3) << "finished input_id=" << input_id;
      // BitstreamBufferRef destructor calls NotifyEndOfBitstreamBuffer().
      decoder_current_bitstream_buffer_.reset();
    }
    ScheduleDecodeBufferTaskIfNeeded();
  }
}

bool V4L2VideoDecodeAccelerator::AdvanceFrameFragment(const uint8_t* data,
                                                      size_t size,
                                                      size_t* endpos) {
  if (video_profile_ >= H264PROFILE_MIN && video_profile_ <= H264PROFILE_MAX) {
    // For H264, we need to feed HW one frame at a time.  This is going to take
    // some parsing of our input stream.
    decoder_h264_parser_->SetStream(data, size);
    H264NALU nalu;
    H264Parser::Result result;
    *endpos = 0;

    // Keep on peeking the next NALs while they don't indicate a frame
    // boundary.
    for (;;) {
      bool end_of_frame = false;
      result = decoder_h264_parser_->AdvanceToNextNALU(&nalu);
      if (result == H264Parser::kInvalidStream ||
          result == H264Parser::kUnsupportedStream)
        return false;
      if (result == H264Parser::kEOStream) {
        // We've reached the end of the buffer before finding a frame boundary.
        decoder_partial_frame_pending_ = true;
        return true;
      }
      switch (nalu.nal_unit_type) {
        case H264NALU::kNonIDRSlice:
        case H264NALU::kIDRSlice:
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
        case H264NALU::kSEIMessage:
        case H264NALU::kSPS:
        case H264NALU::kPPS:
        case H264NALU::kAUD:
        case H264NALU::kEOSeq:
        case H264NALU::kEOStream:
        case H264NALU::kReserved14:
        case H264NALU::kReserved15:
        case H264NALU::kReserved16:
        case H264NALU::kReserved17:
        case H264NALU::kReserved18:
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
    DCHECK_GE(video_profile_, VP8PROFILE_MIN);
    DCHECK_LE(video_profile_, VP9PROFILE_MAX);
    // For VP8/9, we can just dump the entire buffer.  No fragmentation needed,
    // and we never return a partial frame.
    *endpos = size;
    decoder_partial_frame_pending_ = false;
    return true;
  }
}

void V4L2VideoDecodeAccelerator::ScheduleDecodeBufferTaskIfNeeded() {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  // If we're behind on tasks, schedule another one.
  int buffers_to_decode = decoder_input_queue_.size();
  if (decoder_current_bitstream_buffer_ != NULL)
    buffers_to_decode++;
  if (decoder_decode_buffer_tasks_scheduled_ < buffers_to_decode) {
    decoder_decode_buffer_tasks_scheduled_++;
    decoder_thread_.message_loop()->PostTask(
        FROM_HERE, base::Bind(&V4L2VideoDecodeAccelerator::DecodeBufferTask,
                              base::Unretained(this)));
  }
}

bool V4L2VideoDecodeAccelerator::DecodeBufferInitial(const void* data,
                                                     size_t size,
                                                     size_t* endpos) {
  DVLOGF(3) << "data=" << data << ", size=" << size;
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
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
  gfx::Size visible_size;
  bool again = false;
  if (!GetFormatInfo(&format, &visible_size, &again))
    return false;

  *endpos = size;

  if (again) {
    // Need more stream to decode format, return true and schedule next buffer.
    return true;
  }

  // Run this initialization only on first startup.
  if (decoder_state_ == kInitialized) {
    DVLOGF(3) << "running initialization";
    // Success! Setup our parameters.
    if (!CreateBuffersForFormat(format, visible_size))
      return false;
  }

  decoder_state_ = kDecoding;
  ScheduleDecodeBufferTaskIfNeeded();
  return true;
}

bool V4L2VideoDecodeAccelerator::DecodeBufferContinue(const void* data,
                                                      size_t size) {
  DVLOGF(3) << "data=" << data << ", size=" << size;
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_EQ(decoder_state_, kDecoding);

  // Both of these calls will set kError state if they fail.
  // Only flush the frame if it's complete.
  return (AppendToInputFrame(data, size) &&
          (decoder_partial_frame_pending_ || FlushInputFrame()));
}

bool V4L2VideoDecodeAccelerator::AppendToInputFrame(const void* data,
                                                    size_t size) {
  DVLOGF(3);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
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
        DVLOGF(2) << "stalled for input buffers";
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
  InputRecord& input_record = input_buffer_map_[decoder_current_input_buffer_];
  if (size > input_record.length - input_record.bytes_used) {
    LOGF(ERROR) << "over-size frame, erroring";
    NOTIFY_ERROR(UNREADABLE_INPUT);
    return false;
  }
  memcpy(reinterpret_cast<uint8_t*>(input_record.address) +
             input_record.bytes_used,
         data, size);
  input_record.bytes_used += size;

  return true;
}

bool V4L2VideoDecodeAccelerator::FlushInputFrame() {
  DVLOGF(3);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_NE(decoder_state_, kUninitialized);
  DCHECK_NE(decoder_state_, kResetting);
  DCHECK_NE(decoder_state_, kError);

  if (decoder_current_input_buffer_ == -1)
    return true;

  InputRecord& input_record = input_buffer_map_[decoder_current_input_buffer_];
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
  DVLOGF(3) << "submitting input_id=" << input_record.input_id;
  // Enqueue once since there's new available input for it.
  Enqueue();

  return (decoder_state_ != kError);
}

void V4L2VideoDecodeAccelerator::ServiceDeviceTask(bool event_pending) {
  DVLOGF(3);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_NE(decoder_state_, kUninitialized);
  TRACE_EVENT0("Video Decoder", "V4L2VDA::ServiceDeviceTask");

  if (decoder_state_ == kResetting) {
    DVLOGF(2) << "early out: kResetting state";
    return;
  } else if (decoder_state_ == kError) {
    DVLOGF(2) << "early out: kError state";
    return;
  } else if (decoder_state_ == kChangingResolution) {
    DVLOGF(2) << "early out: kChangingResolution state";
    return;
  }

  bool resolution_change_pending = false;
  if (event_pending)
    resolution_change_pending = DequeueResolutionChangeEvent();
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
      FROM_HERE, base::Bind(&V4L2VideoDecodeAccelerator::DevicePollTask,
                            base::Unretained(this), poll_device));

  DVLOG(1) << "ServiceDeviceTask(): buffer counts: DEC["
           << decoder_input_queue_.size() << "->"
           << input_ready_queue_.size() << "] => DEVICE["
           << free_input_buffers_.size() << "+"
           << input_buffer_queued_count_ << "/"
           << input_buffer_map_.size() << "->"
           << free_output_buffers_.size() << "+"
           << output_buffer_queued_count_ << "/"
           << output_buffer_map_.size() << "] => PROCESSOR["
           << image_processor_bitstream_buffer_ids_.size() << "] => CLIENT["
           << decoder_frames_at_client_ << "]";

  ScheduleDecodeBufferTaskIfNeeded();
  if (resolution_change_pending)
    StartResolutionChange();
}

void V4L2VideoDecodeAccelerator::Enqueue() {
  DVLOGF(3);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
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
      PLOGF(ERROR) << "SetDevicePollInterrupt failed";
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
      PLOGF(ERROR) << "SetDevicePollInterrupt(): failed";
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

bool V4L2VideoDecodeAccelerator::DequeueResolutionChangeEvent() {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_NE(decoder_state_, kUninitialized);
  DVLOGF(3);

  struct v4l2_event ev;
  memset(&ev, 0, sizeof(ev));

  while (device_->Ioctl(VIDIOC_DQEVENT, &ev) == 0) {
    if (ev.type == V4L2_EVENT_SOURCE_CHANGE) {
      if (ev.u.src_change.changes & V4L2_EVENT_SRC_CH_RESOLUTION) {
        DVLOGF(3) << "got resolution change event.";
        return true;
      }
    } else {
      LOGF(ERROR) << "got an event (" << ev.type
                  << ") we haven't subscribed to.";
    }
  }
  return false;
}

void V4L2VideoDecodeAccelerator::Dequeue() {
  DVLOGF(3);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
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
    dqbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    dqbuf.memory = V4L2_MEMORY_MMAP;
    dqbuf.m.planes = planes;
    dqbuf.length = 1;
    if (device_->Ioctl(VIDIOC_DQBUF, &dqbuf) != 0) {
      if (errno == EAGAIN) {
        // EAGAIN if we're just out of buffers to dequeue.
        break;
      }
      PLOGF(ERROR) << "ioctl() failed: VIDIOC_DQBUF";
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
    std::unique_ptr<struct v4l2_plane[]> planes(
        new v4l2_plane[output_planes_count_]);
    memset(&dqbuf, 0, sizeof(dqbuf));
    memset(planes.get(), 0, sizeof(struct v4l2_plane) * output_planes_count_);
    dqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    dqbuf.memory = V4L2_MEMORY_MMAP;
    dqbuf.m.planes = planes.get();
    dqbuf.length = output_planes_count_;
    if (device_->Ioctl(VIDIOC_DQBUF, &dqbuf) != 0) {
      if (errno == EAGAIN) {
        // EAGAIN if we're just out of buffers to dequeue.
        break;
      }
      PLOGF(ERROR) << "ioctl() failed: VIDIOC_DQBUF";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    }
    OutputRecord& output_record = output_buffer_map_[dqbuf.index];
    DCHECK_EQ(output_record.state, kAtDevice);
    DCHECK_NE(output_record.egl_image, EGL_NO_IMAGE_KHR);
    DCHECK_NE(output_record.picture_id, -1);
    output_buffer_queued_count_--;
    if (dqbuf.m.planes[0].bytesused == 0) {
      // This is an empty output buffer returned as part of a flush.
      output_record.state = kFree;
      free_output_buffers_.push(dqbuf.index);
    } else {
      int32_t bitstream_buffer_id = dqbuf.timestamp.tv_sec;
      DCHECK_GE(bitstream_buffer_id, 0);
      DVLOGF(3) << "Dequeue output buffer: dqbuf index=" << dqbuf.index
                << " bitstream input_id=" << bitstream_buffer_id;
      if (image_processor_device_) {
        output_record.state = kAtProcessor;
        image_processor_bitstream_buffer_ids_.push(bitstream_buffer_id);
        std::vector<int> fds;
        for (auto& fd : output_record.fds) {
          fds.push_back(fd.get());
        }
        scoped_refptr<VideoFrame> frame = VideoFrame::WrapExternalDmabufs(
            V4L2Device::V4L2PixFmtToVideoPixelFormat(output_format_fourcc_),
            coded_size_, gfx::Rect(visible_size_), visible_size_, fds,
            base::TimeDelta());
        // Unretained is safe because |this| owns image processor and there will
        // be no callbacks after processor destroys. Also, this class ensures it
        // is safe to post a task from child thread to decoder thread using
        // Unretained.
        image_processor_->Process(
            frame, dqbuf.index,
            BindToCurrentLoop(
                base::Bind(&V4L2VideoDecodeAccelerator::FrameProcessed,
                           base::Unretained(this), bitstream_buffer_id)));
      } else {
        output_record.state = kAtClient;
        decoder_frames_at_client_++;
        const Picture picture(output_record.picture_id, bitstream_buffer_id,
                              gfx::Rect(visible_size_), false);
        pending_picture_ready_.push(
            PictureRecord(output_record.cleared, picture));
        SendPictureReady();
        output_record.cleared = true;
      }
    }
  }

  NotifyFlushDoneIfNeeded();
}

bool V4L2VideoDecodeAccelerator::EnqueueInputRecord() {
  DVLOGF(3);
  DCHECK(!input_ready_queue_.empty());

  // Enqueue an input (VIDEO_OUTPUT) buffer.
  const int buffer = input_ready_queue_.front();
  InputRecord& input_record = input_buffer_map_[buffer];
  DCHECK(!input_record.at_device);
  struct v4l2_buffer qbuf;
  struct v4l2_plane qbuf_plane;
  memset(&qbuf, 0, sizeof(qbuf));
  memset(&qbuf_plane, 0, sizeof(qbuf_plane));
  qbuf.index = buffer;
  qbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  qbuf.timestamp.tv_sec = input_record.input_id;
  qbuf.memory = V4L2_MEMORY_MMAP;
  qbuf.m.planes = &qbuf_plane;
  qbuf.m.planes[0].bytesused = input_record.bytes_used;
  qbuf.length = 1;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QBUF, &qbuf);
  input_ready_queue_.pop();
  input_record.at_device = true;
  input_buffer_queued_count_++;
  DVLOGF(3) << "enqueued input_id=" << input_record.input_id
            << " size=" << input_record.bytes_used;
  return true;
}

bool V4L2VideoDecodeAccelerator::EnqueueOutputRecord() {
  DVLOGF(3);
  DCHECK(!free_output_buffers_.empty());

  // Enqueue an output (VIDEO_CAPTURE) buffer.
  const int buffer = free_output_buffers_.front();
  OutputRecord& output_record = output_buffer_map_[buffer];
  DCHECK_EQ(output_record.state, kFree);
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
      DLOGF(WARNING) << "eglClientWaitSyncKHR failed!";
    }
    if (eglDestroySyncKHR(egl_display_, output_record.egl_sync) != EGL_TRUE) {
      LOGF(ERROR) << "eglDestroySyncKHR failed!";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return false;
    }
    output_record.egl_sync = EGL_NO_SYNC_KHR;
  }
  struct v4l2_buffer qbuf;
  std::unique_ptr<struct v4l2_plane[]> qbuf_planes(
      new v4l2_plane[output_planes_count_]);
  memset(&qbuf, 0, sizeof(qbuf));
  memset(qbuf_planes.get(), 0,
         sizeof(struct v4l2_plane) * output_planes_count_);
  qbuf.index = buffer;
  qbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  qbuf.memory = V4L2_MEMORY_MMAP;
  qbuf.m.planes = qbuf_planes.get();
  qbuf.length = output_planes_count_;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QBUF, &qbuf);
  free_output_buffers_.pop();
  output_record.state = kAtDevice;
  output_buffer_queued_count_++;
  return true;
}

void V4L2VideoDecodeAccelerator::ReusePictureBufferTask(
    int32_t picture_buffer_id,
    std::unique_ptr<EGLSyncKHRRef> egl_sync_ref) {
  DVLOGF(3) << "picture_buffer_id=" << picture_buffer_id;
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  TRACE_EVENT0("Video Decoder", "V4L2VDA::ReusePictureBufferTask");

  // We run ReusePictureBufferTask even if we're in kResetting.
  if (decoder_state_ == kError) {
    DVLOGF(2) << "early out: kError state";
    return;
  }

  if (decoder_state_ == kChangingResolution) {
    DVLOGF(2) << "early out: kChangingResolution";
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
    DVLOGF(4) << "got picture id= " << picture_buffer_id
              << " not in use (anymore?).";
    return;
  }

  OutputRecord& output_record = output_buffer_map_[index];
  if (output_record.state != kAtClient) {
    LOGF(ERROR) << "picture_buffer_id not reusable";
    NOTIFY_ERROR(INVALID_ARGUMENT);
    return;
  }

  DCHECK_EQ(output_record.egl_sync, EGL_NO_SYNC_KHR);
  output_record.egl_sync = egl_sync_ref->egl_sync;
  output_record.state = kFree;
  free_output_buffers_.push(index);
  decoder_frames_at_client_--;
  // Take ownership of the EGLSync.
  egl_sync_ref->egl_sync = EGL_NO_SYNC_KHR;
  // We got a buffer back, so enqueue it back.
  Enqueue();
}

void V4L2VideoDecodeAccelerator::FlushTask() {
  DVLOGF(3);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  TRACE_EVENT0("Video Decoder", "V4L2VDA::FlushTask");

  // Flush outstanding buffers.
  if (decoder_state_ == kInitialized || decoder_state_ == kAfterReset) {
    // There's nothing in the pipe, so return done immediately.
    DVLOGF(3) << "returning flush";
    child_task_runner_->PostTask(FROM_HERE,
                                 base::Bind(&Client::NotifyFlushDone, client_));
    return;
  } else if (decoder_state_ == kError) {
    DVLOGF(2) << "early out: kError state";
    return;
  }

  // We don't support stacked flushing.
  DCHECK(!decoder_flushing_);

  // Queue up an empty buffer -- this triggers the flush.
  decoder_input_queue_.push(
      linked_ptr<BitstreamBufferRef>(new BitstreamBufferRef(
          decode_client_, decode_task_runner_, nullptr, kFlushBufferId)));
  decoder_flushing_ = true;
  SendPictureReady();  // Send all pending PictureReady.

  ScheduleDecodeBufferTaskIfNeeded();
}

void V4L2VideoDecodeAccelerator::NotifyFlushDoneIfNeeded() {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  if (!decoder_flushing_)
    return;

  // Pipeline is empty when:
  // * Decoder input queue is empty of non-delayed buffers.
  // * There is no currently filling input buffer.
  // * Input holding queue is empty.
  // * All input (VIDEO_OUTPUT) buffers are returned.
  // * All image processor buffers are returned.
  if (!decoder_input_queue_.empty()) {
    if (decoder_input_queue_.front()->input_id !=
        decoder_delay_bitstream_buffer_id_)
      return;
  }
  if (decoder_current_input_buffer_ != -1)
    return;
  if ((input_ready_queue_.size() + input_buffer_queued_count_) != 0)
    return;
  if (image_processor_bitstream_buffer_ids_.size() != 0)
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
  if (!(StopDevicePoll() && StopOutputStream() && StopInputStream()))
    return;

  if (!StartDevicePoll())
    return;

  decoder_delay_bitstream_buffer_id_ = -1;
  decoder_flushing_ = false;
  DVLOGF(3) << "returning flush";
  child_task_runner_->PostTask(FROM_HERE,
                               base::Bind(&Client::NotifyFlushDone, client_));

  // While we were flushing, we early-outed DecodeBufferTask()s.
  ScheduleDecodeBufferTaskIfNeeded();
}

void V4L2VideoDecodeAccelerator::ResetTask() {
  DVLOGF(3);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  TRACE_EVENT0("Video Decoder", "V4L2VDA::ResetTask");

  if (decoder_state_ == kError) {
    DVLOGF(2) << "early out: kError state";
    return;
  }

  // If we are in the middle of switching resolutions, postpone reset until
  // it's done. We don't have to worry about timing of this wrt to decoding,
  // because output pipe is already stopped if we are changing resolution.
  // We will come back here after we are done with the resolution change.
  DCHECK(!resolution_change_reset_pending_);
  if (decoder_state_ == kChangingResolution) {
    resolution_change_reset_pending_ = true;
    return;
  }

  // After the output stream is stopped, the codec should not post any
  // resolution change events. So we dequeue the resolution change event
  // afterwards. The event could be posted before or while stopping the output
  // stream. The codec will expect the buffer of new size after the seek, so
  // we need to handle the resolution change event first.
  if (!(StopDevicePoll() && StopOutputStream()))
    return;

  if (DequeueResolutionChangeEvent()) {
    resolution_change_reset_pending_ = true;
    StartResolutionChange();
    return;
  }

  if (!StopInputStream())
    return;

  decoder_current_bitstream_buffer_.reset();
  while (!decoder_input_queue_.empty())
    decoder_input_queue_.pop();

  decoder_current_input_buffer_ = -1;

  // Drop all buffers in image processor.
  while (!image_processor_bitstream_buffer_ids_.empty())
    image_processor_bitstream_buffer_ids_.pop();

  // If we were flushing, we'll never return any more BitstreamBuffers or
  // PictureBuffers; they have all been dropped and returned by now.
  NotifyFlushDoneIfNeeded();

  // Mark that we're resetting, then enqueue a ResetDoneTask().  All intervening
  // jobs will early-out in the kResetting state.
  decoder_state_ = kResetting;
  SendPictureReady();  // Send all pending PictureReady.
  decoder_thread_.message_loop()->PostTask(
      FROM_HERE, base::Bind(&V4L2VideoDecodeAccelerator::ResetDoneTask,
                            base::Unretained(this)));
}

void V4L2VideoDecodeAccelerator::ResetDoneTask() {
  DVLOGF(3);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  TRACE_EVENT0("Video Decoder", "V4L2VDA::ResetDoneTask");

  if (decoder_state_ == kError) {
    DVLOGF(2) << "early out: kError state";
    return;
  }

  if (!StartDevicePoll())
    return;

  // Reset format-specific bits.
  if (video_profile_ >= H264PROFILE_MIN && video_profile_ <= H264PROFILE_MAX) {
    decoder_h264_parser_.reset(new H264Parser());
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
  child_task_runner_->PostTask(FROM_HERE,
                               base::Bind(&Client::NotifyResetDone, client_));

  // While we were resetting, we early-outed DecodeBufferTask()s.
  ScheduleDecodeBufferTaskIfNeeded();
}

void V4L2VideoDecodeAccelerator::DestroyTask() {
  DVLOGF(3);
  TRACE_EVENT0("Video Decoder", "V4L2VDA::DestroyTask");

  // DestroyTask() should run regardless of decoder_state_.

  StopDevicePoll();
  StopOutputStream();
  StopInputStream();

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
  DVLOGF(3);
  DCHECK(!device_poll_thread_.IsRunning());
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  // Start up the device poll thread and schedule its first DevicePollTask().
  if (!device_poll_thread_.Start()) {
    LOGF(ERROR) << "Device thread failed to start";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }
  device_poll_thread_.message_loop()->PostTask(
      FROM_HERE, base::Bind(&V4L2VideoDecodeAccelerator::DevicePollTask,
                            base::Unretained(this), 0));

  return true;
}

bool V4L2VideoDecodeAccelerator::StopDevicePoll() {
  DVLOGF(3);

  if (!device_poll_thread_.IsRunning())
    return true;

  if (decoder_thread_.IsRunning())
    DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  // Signal the DevicePollTask() to stop, and stop the device poll thread.
  if (!device_->SetDevicePollInterrupt()) {
    PLOGF(ERROR) << "SetDevicePollInterrupt(): failed";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }
  device_poll_thread_.Stop();
  // Clear the interrupt now, to be sure.
  if (!device_->ClearDevicePollInterrupt()) {
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }
  DVLOGF(3) << "device poll stopped";
  return true;
}

bool V4L2VideoDecodeAccelerator::StopOutputStream() {
  DVLOGF(3);
  if (!output_streamon_)
    return true;

  __u32 type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_STREAMOFF, &type);
  output_streamon_ = false;

  for (size_t i = 0; i < output_buffer_map_.size(); ++i) {
    // After streamoff, the device drops ownership of all buffers, even if we
    // don't dequeue them explicitly. Some of them may still be owned by the
    // client however. Reuse only those that aren't.
    OutputRecord& output_record = output_buffer_map_[i];
    if (output_record.state == kAtDevice) {
      output_record.state = kFree;
      free_output_buffers_.push(i);
      DCHECK_EQ(output_record.egl_sync, EGL_NO_SYNC_KHR);
    }
  }
  output_buffer_queued_count_ = 0;
  return true;
}

bool V4L2VideoDecodeAccelerator::StopInputStream() {
  DVLOGF(3);
  if (!input_streamon_)
    return true;

  __u32 type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_STREAMOFF, &type);
  input_streamon_ = false;

  // Reset accounting info for input.
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

  return true;
}

void V4L2VideoDecodeAccelerator::StartResolutionChange() {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_NE(decoder_state_, kUninitialized);
  DCHECK_NE(decoder_state_, kResetting);

  DVLOGF(3) << "Initiate resolution change";

  if (!(StopDevicePoll() && StopOutputStream()))
    return;

  decoder_state_ = kChangingResolution;

  if (!image_processor_bitstream_buffer_ids_.empty()) {
    DVLOGF(3) << "Wait image processor to finish before destroying buffers.";
    return;
  }

  // Post a task to clean up buffers on child thread. This will also ensure
  // that we won't accept ReusePictureBuffer() anymore after that.
  child_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&V4L2VideoDecodeAccelerator::ResolutionChangeDestroyBuffers,
                 weak_this_));
}

void V4L2VideoDecodeAccelerator::FinishResolutionChange() {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_EQ(decoder_state_, kChangingResolution);
  DVLOGF(3);

  if (decoder_state_ == kError) {
    DVLOGF(2) << "early out: kError state";
    return;
  }

  struct v4l2_format format;
  bool again;
  gfx::Size visible_size;
  bool ret = GetFormatInfo(&format, &visible_size, &again);
  if (!ret || again) {
    LOGF(ERROR) << "Couldn't get format information after resolution change";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  if (!CreateBuffersForFormat(format, visible_size)) {
    LOGF(ERROR) << "Couldn't reallocate buffers after resolution change";
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
  DVLOGF(3);
  DCHECK(device_poll_thread_.task_runner()->BelongsToCurrentThread());
  TRACE_EVENT0("Video Decoder", "V4L2VDA::DevicePollTask");

  bool event_pending = false;

  if (!device_->Poll(poll_device, &event_pending)) {
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  // All processing should happen on ServiceDeviceTask(), since we shouldn't
  // touch decoder state from this thread.
  decoder_thread_.message_loop()->PostTask(
      FROM_HERE, base::Bind(&V4L2VideoDecodeAccelerator::ServiceDeviceTask,
                            base::Unretained(this), event_pending));
}

void V4L2VideoDecodeAccelerator::NotifyError(Error error) {
  DVLOGF(2);

  if (!child_task_runner_->BelongsToCurrentThread()) {
    child_task_runner_->PostTask(
        FROM_HERE, base::Bind(&V4L2VideoDecodeAccelerator::NotifyError,
                              weak_this_, error));
    return;
  }

  if (client_) {
    client_->NotifyError(error);
    client_ptr_factory_.reset();
  }
}

void V4L2VideoDecodeAccelerator::SetErrorState(Error error) {
  // We can touch decoder_state_ only if this is the decoder thread or the
  // decoder thread isn't running.
  if (decoder_thread_.task_runner() &&
      !decoder_thread_.task_runner()->BelongsToCurrentThread()) {
    decoder_thread_.task_runner()->PostTask(
        FROM_HERE, base::Bind(&V4L2VideoDecodeAccelerator::SetErrorState,
                              base::Unretained(this), error));
    return;
  }

  // Post NotifyError only if we are already initialized, as the API does
  // not allow doing so before that.
  if (decoder_state_ != kError && decoder_state_ != kUninitialized)
    NotifyError(error);

  decoder_state_ = kError;
}

bool V4L2VideoDecodeAccelerator::GetFormatInfo(struct v4l2_format* format,
                                               gfx::Size* visible_size,
                                               bool* again) {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  *again = false;
  memset(format, 0, sizeof(*format));
  format->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  if (device_->Ioctl(VIDIOC_G_FMT, format) != 0) {
    if (errno == EINVAL) {
      // EINVAL means we haven't seen sufficient stream to decode the format.
      *again = true;
      return true;
    } else {
      PLOGF(ERROR) << "ioctl() failed: VIDIOC_G_FMT";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return false;
    }
  }

  // Make sure we are still getting the format we set on initialization.
  if (format->fmt.pix_mp.pixelformat != output_format_fourcc_) {
    LOGF(ERROR) << "Unexpected format from G_FMT on output";
    return false;
  }

  gfx::Size coded_size(format->fmt.pix_mp.width, format->fmt.pix_mp.height);
  if (visible_size != nullptr)
    *visible_size = GetVisibleSize(coded_size);

  return true;
}

bool V4L2VideoDecodeAccelerator::CreateBuffersForFormat(
    const struct v4l2_format& format,
    const gfx::Size& visible_size) {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  output_planes_count_ = format.fmt.pix_mp.num_planes;
  coded_size_.SetSize(format.fmt.pix_mp.width, format.fmt.pix_mp.height);
  visible_size_ = visible_size;
  if (image_processor_device_) {
    V4L2ImageProcessor processor(image_processor_device_);
    egl_image_size_ = visible_size_;
    egl_image_planes_count_ = 0;
    if (!processor.TryOutputFormat(egl_image_format_fourcc_, &egl_image_size_,
                                   &egl_image_planes_count_)) {
      LOGF(ERROR) << "Fail to get output size and plane count of processor";
      return false;
    }
  } else {
    egl_image_size_ = coded_size_;
    egl_image_planes_count_ = output_planes_count_;
  }
  DVLOGF(3) << "new resolution: " << coded_size_.ToString()
            << ", visible size: " << visible_size_.ToString()
            << ", EGLImage size: " << egl_image_size_.ToString();

  return CreateOutputBuffers();
}

gfx::Size V4L2VideoDecodeAccelerator::GetVisibleSize(
    const gfx::Size& coded_size) {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  struct v4l2_crop crop_arg;
  memset(&crop_arg, 0, sizeof(crop_arg));
  crop_arg.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

  if (device_->Ioctl(VIDIOC_G_CROP, &crop_arg) != 0) {
    PLOGF(ERROR) << "ioctl() VIDIOC_G_CROP failed";
    return coded_size;
  }

  gfx::Rect rect(crop_arg.c.left, crop_arg.c.top, crop_arg.c.width,
                 crop_arg.c.height);
  DVLOGF(3) << "visible rectangle is " << rect.ToString();
  if (!gfx::Rect(coded_size).Contains(rect)) {
    DLOGF(ERROR) << "visible rectangle " << rect.ToString()
                 << " is not inside coded size " << coded_size.ToString();
    return coded_size;
  }
  if (rect.IsEmpty()) {
    DLOGF(ERROR) << "visible size is empty";
    return coded_size;
  }

  // Chrome assume picture frame is coded at (0, 0).
  if (!rect.origin().IsOrigin()) {
    DLOGF(ERROR) << "Unexpected visible rectangle " << rect.ToString()
                 << ", top-left is not origin";
    return coded_size;
  }

  return rect.size();
}

bool V4L2VideoDecodeAccelerator::CreateInputBuffers() {
  DVLOGF(3);
  // We always run this as we prepare to initialize.
  DCHECK_EQ(decoder_state_, kUninitialized);
  DCHECK(!input_streamon_);
  DCHECK(input_buffer_map_.empty());

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = kInputBufferCount;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
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
    buffer.index = i;
    buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.m.planes = planes;
    buffer.length = 1;
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QUERYBUF, &buffer);
    void* address = device_->Mmap(NULL,
                                  buffer.m.planes[0].length,
                                  PROT_READ | PROT_WRITE,
                                  MAP_SHARED,
                                  buffer.m.planes[0].m.mem_offset);
    if (address == MAP_FAILED) {
      PLOGF(ERROR) << "mmap() failed";
      return false;
    }
    input_buffer_map_[i].address = address;
    input_buffer_map_[i].length = buffer.m.planes[0].length;
  }

  return true;
}

bool V4L2VideoDecodeAccelerator::SetupFormats() {
  // We always run this as we prepare to initialize.
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(decoder_state_, kUninitialized);
  DCHECK(!input_streamon_);
  DCHECK(!output_streamon_);

  __u32 input_format_fourcc =
      V4L2Device::VideoCodecProfileToV4L2PixFmt(video_profile_, false);
  if (!input_format_fourcc) {
    NOTREACHED();
    return false;
  }

  size_t input_size;
  gfx::Size max_resolution, min_resolution;
  device_->GetSupportedResolution(input_format_fourcc, &min_resolution,
                                  &max_resolution);
  if (max_resolution.width() > 1920 && max_resolution.height() > 1088)
    input_size = kInputBufferMaxSizeFor4k;
  else
    input_size = kInputBufferMaxSizeFor1080p;

  struct v4l2_fmtdesc fmtdesc;
  memset(&fmtdesc, 0, sizeof(fmtdesc));
  fmtdesc.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  bool is_format_supported = false;
  while (device_->Ioctl(VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
    if (fmtdesc.pixelformat == input_format_fourcc) {
      is_format_supported = true;
      break;
    }
    ++fmtdesc.index;
  }

  if (!is_format_supported) {
    DVLOGF(1) << "Input fourcc " << input_format_fourcc
              << " not supported by device.";
    return false;
  }

  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  format.fmt.pix_mp.pixelformat = input_format_fourcc;
  format.fmt.pix_mp.plane_fmt[0].sizeimage = input_size;
  format.fmt.pix_mp.num_planes = 1;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_FMT, &format);

  // We have to set up the format for output, because the driver may not allow
  // changing it once we start streaming; whether it can support our chosen
  // output format or not may depend on the input format.
  memset(&fmtdesc, 0, sizeof(fmtdesc));
  fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  while (device_->Ioctl(VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
    if (device_->CanCreateEGLImageFrom(fmtdesc.pixelformat)) {
      output_format_fourcc_ = fmtdesc.pixelformat;
      break;
    }
    ++fmtdesc.index;
  }

  if (output_format_fourcc_ == 0) {
    DVLOGF(1) << "Could not find a usable output format. Try image processor";
    image_processor_device_ = V4L2Device::Create(V4L2Device::kImageProcessor);
    if (!image_processor_device_) {
      DVLOGF(1) << "No image processor device.";
      return false;
    }
    output_format_fourcc_ = FindImageProcessorInputFormat();
    if (output_format_fourcc_ == 0) {
      LOGF(ERROR) << "Can't find a usable input format from image processor";
      return false;
    }
    egl_image_format_fourcc_ = FindImageProcessorOutputFormat();
    if (egl_image_format_fourcc_ == 0) {
      LOGF(ERROR) << "Can't find a usable output format from image processor";
      return false;
    }
    egl_image_device_ = image_processor_device_;
  } else {
    egl_image_format_fourcc_ = output_format_fourcc_;
    egl_image_device_ = device_;
  }
  DVLOGF(2) << "Output format=" << output_format_fourcc_;

  // Just set the fourcc for output; resolution, etc., will come from the
  // driver once it extracts it from the stream.
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  format.fmt.pix_mp.pixelformat = output_format_fourcc_;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_FMT, &format);

  return true;
}

uint32_t V4L2VideoDecodeAccelerator::FindImageProcessorInputFormat() {
  V4L2ImageProcessor image_processor(image_processor_device_);
  std::vector<uint32_t> processor_input_formats =
      image_processor.GetSupportedInputFormats();
  struct v4l2_fmtdesc fmtdesc;
  memset(&fmtdesc, 0, sizeof(fmtdesc));
  fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  while (device_->Ioctl(VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
    if (std::find(processor_input_formats.begin(),
                  processor_input_formats.end(),
                  fmtdesc.pixelformat) != processor_input_formats.end()) {
      DVLOGF(1) << "Image processor input format=" << fmtdesc.pixelformat;
      return fmtdesc.pixelformat;
    }
    ++fmtdesc.index;
  }
  return 0;
}

uint32_t V4L2VideoDecodeAccelerator::FindImageProcessorOutputFormat() {
  V4L2ImageProcessor image_processor(image_processor_device_);
  std::vector<uint32_t> processor_output_formats =
      image_processor.GetSupportedOutputFormats();
  for (uint32_t processor_output_format : processor_output_formats) {
    if (device_->CanCreateEGLImageFrom(processor_output_format)) {
      DVLOGF(1) << "Image processor output format=" << processor_output_format;
      return processor_output_format;
    }
  }

  return 0;
}

bool V4L2VideoDecodeAccelerator::CreateOutputBuffers() {
  DVLOGF(3);
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

  const uint32_t buffer_count = output_dpb_size_ + kDpbOutputBufferExtraCount;
  DVLOGF(3) << "buffer_count=" << buffer_count
            << ", coded_size=" << egl_image_size_.ToString();

  child_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Client::ProvidePictureBuffers, client_,
                            buffer_count, PIXEL_FORMAT_UNKNOWN, 1,
                            egl_image_size_, device_->GetTextureTarget()));

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
  DVLOGF(3);
  DCHECK(child_task_runner_->BelongsToCurrentThread());
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
  DVLOGF(3);
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  DCHECK(!output_streamon_);
  bool success = true;

  for (size_t i = 0; i < output_buffer_map_.size(); ++i) {
    OutputRecord& output_record = output_buffer_map_[i];

    if (output_record.egl_image != EGL_NO_IMAGE_KHR) {
      if (egl_image_device_->DestroyEGLImage(
              egl_display_, output_record.egl_image) != EGL_TRUE) {
        DVLOGF(1) << "DestroyEGLImage failed.";
        success = false;
      }
    }

    if (output_record.egl_sync != EGL_NO_SYNC_KHR) {
      if (eglDestroySyncKHR(egl_display_, output_record.egl_sync) != EGL_TRUE) {
        DVLOGF(1) << "eglDestroySyncKHR failed.";
        success = false;
      }
    }

    DVLOGF(1) << "dismissing PictureBuffer id=" << output_record.picture_id;
    child_task_runner_->PostTask(
        FROM_HERE, base::Bind(&Client::DismissPictureBuffer, client_,
                              output_record.picture_id));
  }

  if (image_processor_)
    image_processor_.release()->Destroy();

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  if (device_->Ioctl(VIDIOC_REQBUFS, &reqbufs) != 0) {
    PLOGF(ERROR) << "ioctl() failed: VIDIOC_REQBUFS";
    success = false;
  }

  output_buffer_map_.clear();
  while (!free_output_buffers_.empty())
    free_output_buffers_.pop();
  output_buffer_queued_count_ = 0;
  // The client may still hold some buffers. The texture holds a reference to
  // the buffer. It is OK to free the buffer and destroy EGLImage here.
  decoder_frames_at_client_ = 0;

  return success;
}

void V4L2VideoDecodeAccelerator::ResolutionChangeDestroyBuffers() {
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  DVLOGF(3);

  if (!DestroyOutputBuffers()) {
    LOGF(ERROR) << "Failed destroying output buffers.";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  // Finish resolution change on decoder thread.
  decoder_thread_.message_loop()->PostTask(
      FROM_HERE, base::Bind(&V4L2VideoDecodeAccelerator::FinishResolutionChange,
                            base::Unretained(this)));
}

void V4L2VideoDecodeAccelerator::SendPictureReady() {
  DVLOGF(3);
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  bool resetting_or_flushing =
      (decoder_state_ == kResetting || decoder_flushing_);
  while (pending_picture_ready_.size() > 0) {
    bool cleared = pending_picture_ready_.front().cleared;
    const Picture& picture = pending_picture_ready_.front().picture;
    if (cleared && picture_clearing_count_ == 0) {
      // This picture is cleared. It can be posted to a thread different than
      // the main GPU thread to reduce latency. This should be the case after
      // all pictures are cleared at the beginning.
      decode_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&Client::PictureReady, decode_client_, picture));
      pending_picture_ready_.pop();
    } else if (!cleared || resetting_or_flushing) {
      DVLOGF(3) << "cleared=" << pending_picture_ready_.front().cleared
                << ", decoder_state_=" << decoder_state_
                << ", decoder_flushing_=" << decoder_flushing_
                << ", picture_clearing_count_=" << picture_clearing_count_;
      // If the picture is not cleared, post it to the child thread because it
      // has to be cleared in the child thread. A picture only needs to be
      // cleared once. If the decoder is resetting or flushing, send all
      // pictures to ensure PictureReady arrive before reset or flush done.
      child_task_runner_->PostTaskAndReply(
          FROM_HERE, base::Bind(&Client::PictureReady, client_, picture),
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
  DVLOGF(3) << "clearing count=" << picture_clearing_count_;
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_GT(picture_clearing_count_, 0);
  picture_clearing_count_--;
  SendPictureReady();
}

void V4L2VideoDecodeAccelerator::FrameProcessed(int32_t bitstream_buffer_id,
                                                int output_buffer_index) {
  DVLOGF(3) << "output_buffer_index=" << output_buffer_index
            << ", bitstream_buffer_id=" << bitstream_buffer_id;
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_GE(output_buffer_index, 0);
  DCHECK_LT(output_buffer_index, static_cast<int>(output_buffer_map_.size()));

  OutputRecord& output_record = output_buffer_map_[output_buffer_index];
  DCHECK_EQ(output_record.state, kAtProcessor);
  if (!image_processor_bitstream_buffer_ids_.empty() &&
      image_processor_bitstream_buffer_ids_.front() == bitstream_buffer_id) {
    DVLOGF(3) << "picture_id=" << output_record.picture_id;
    DCHECK_NE(output_record.egl_image, EGL_NO_IMAGE_KHR);
    DCHECK_NE(output_record.picture_id, -1);
    // Send the processed frame to render.
    output_record.state = kAtClient;
    decoder_frames_at_client_++;
    image_processor_bitstream_buffer_ids_.pop();
    const Picture picture(output_record.picture_id, bitstream_buffer_id,
                          gfx::Rect(visible_size_), false);
    pending_picture_ready_.push(PictureRecord(output_record.cleared, picture));
    SendPictureReady();
    output_record.cleared = true;
    // Flush or resolution change may be waiting image processor to finish.
    if (image_processor_bitstream_buffer_ids_.empty()) {
      NotifyFlushDoneIfNeeded();
      if (decoder_state_ == kChangingResolution)
        StartResolutionChange();
    }
  } else {
    DVLOGF(2) << "Bitstream buffer id " << bitstream_buffer_id << " not found "
              << "because of Reset. Drop the buffer";
    output_record.state = kFree;
    free_output_buffers_.push(output_buffer_index);
    // Do not queue the buffer if a resolution change is in progress. The queue
    // is about to be destroyed anyway. Otherwise, the queue will be started in
    // Enqueue and REQBUFS(0) will fail.
    if (decoder_state_ != kChangingResolution)
      Enqueue();
  }
}

void V4L2VideoDecodeAccelerator::ImageProcessorError() {
  LOGF(ERROR) << "Image processor error";
  NOTIFY_ERROR(PLATFORM_FAILURE);
}

}  // namespace media
