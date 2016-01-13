// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/numerics/safe_conversions.h"
#include "content/common/gpu/media/v4l2_video_encode_accelerator.h"
#include "content/public/common/content_switches.h"
#include "media/base/bitstream_buffer.h"

#define NOTIFY_ERROR(x)                            \
  do {                                             \
    SetEncoderState(kError);                       \
    DLOG(ERROR) << "calling NotifyError(): " << x; \
    NotifyError(x);                                \
  } while (0)

#define IOCTL_OR_ERROR_RETURN_VALUE(type, arg, value)              \
  do {                                                             \
    if (device_->Ioctl(type, arg) != 0) {                          \
      DPLOG(ERROR) << __func__ << "(): ioctl() failed: " << #type; \
      NOTIFY_ERROR(kPlatformFailureError);                         \
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

struct V4L2VideoEncodeAccelerator::BitstreamBufferRef {
  BitstreamBufferRef(int32 id, scoped_ptr<base::SharedMemory> shm, size_t size)
      : id(id), shm(shm.Pass()), size(size) {}
  const int32 id;
  const scoped_ptr<base::SharedMemory> shm;
  const size_t size;
};

V4L2VideoEncodeAccelerator::InputRecord::InputRecord() : at_device(false) {
}

V4L2VideoEncodeAccelerator::OutputRecord::OutputRecord()
    : at_device(false), address(NULL), length(0) {
}

V4L2VideoEncodeAccelerator::V4L2VideoEncodeAccelerator(
    scoped_ptr<V4L2Device> device)
    : child_message_loop_proxy_(base::MessageLoopProxy::current()),
      weak_this_ptr_factory_(this),
      weak_this_(weak_this_ptr_factory_.GetWeakPtr()),
      output_buffer_byte_size_(0),
      device_input_format_(media::VideoFrame::UNKNOWN),
      input_planes_count_(0),
      output_format_fourcc_(0),
      encoder_thread_("V4L2EncoderThread"),
      encoder_state_(kUninitialized),
      stream_header_size_(0),
      device_(device.Pass()),
      input_streamon_(false),
      input_buffer_queued_count_(0),
      input_memory_type_(V4L2_MEMORY_USERPTR),
      output_streamon_(false),
      output_buffer_queued_count_(0),
      device_poll_thread_("V4L2EncoderDevicePollThread") {
}

V4L2VideoEncodeAccelerator::~V4L2VideoEncodeAccelerator() {
  DCHECK(!encoder_thread_.IsRunning());
  DCHECK(!device_poll_thread_.IsRunning());
  DVLOG(4) << __func__;

  DestroyInputBuffers();
  DestroyOutputBuffers();
}

bool V4L2VideoEncodeAccelerator::Initialize(
    media::VideoFrame::Format input_format,
    const gfx::Size& input_visible_size,
    media::VideoCodecProfile output_profile,
    uint32 initial_bitrate,
    Client* client) {
  DVLOG(3) << __func__ << ": input_format="
           << media::VideoFrame::FormatToString(input_format)
           << ", input_visible_size=" << input_visible_size.ToString()
           << ", output_profile=" << output_profile
           << ", initial_bitrate=" << initial_bitrate;

  visible_size_ = input_visible_size;

  client_ptr_factory_.reset(new base::WeakPtrFactory<Client>(client));
  client_ = client_ptr_factory_->GetWeakPtr();

  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK_EQ(encoder_state_, kUninitialized);

  struct v4l2_capability caps;
  memset(&caps, 0, sizeof(caps));
  const __u32 kCapsRequired = V4L2_CAP_VIDEO_CAPTURE_MPLANE |
                              V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QUERYCAP, &caps);
  if ((caps.capabilities & kCapsRequired) != kCapsRequired) {
    DLOG(ERROR) << "Initialize(): ioctl() failed: VIDIOC_QUERYCAP: "
                   "caps check failed: 0x" << std::hex << caps.capabilities;
    return false;
  }

  if (!SetFormats(input_format, output_profile)) {
    DLOG(ERROR) << "Failed setting up formats";
    return false;
  }

  if (input_format != device_input_format_) {
    DVLOG(1) << "Input format not supported by the HW, will convert to "
             << media::VideoFrame::FormatToString(device_input_format_);

    scoped_ptr<V4L2Device> device =
        V4L2Device::Create(V4L2Device::kImageProcessor);
    image_processor_.reset(new V4L2ImageProcessor(device.Pass()));

    // Convert from input_format to device_input_format_, keeping the size
    // at visible_size_ and requiring the output buffers to be of at least
    // input_allocated_size_.
    if (!image_processor_->Initialize(
            input_format,
            device_input_format_,
            visible_size_,
            visible_size_,
            input_allocated_size_,
            base::Bind(&V4L2VideoEncodeAccelerator::ImageProcessorError,
                       weak_this_))) {
      DLOG(ERROR) << "Failed initializing image processor";
      return false;
    }
  }

  if (!InitControls())
    return false;

  if (!CreateOutputBuffers())
    return false;

  if (!encoder_thread_.Start()) {
    DLOG(ERROR) << "Initialize(): encoder thread failed to start";
    return false;
  }

  RequestEncodingParametersChange(initial_bitrate, kInitialFramerate);

  SetEncoderState(kInitialized);

  child_message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&Client::RequireBitstreamBuffers,
                 client_,
                 kInputBufferCount,
                 image_processor_.get() ?
                     image_processor_->input_allocated_size() :
                     input_allocated_size_,
                 output_buffer_byte_size_));
  return true;
}

void V4L2VideoEncodeAccelerator::ImageProcessorError() {
  DVLOG(1) << "Image processor error";
  NOTIFY_ERROR(kPlatformFailureError);
}

void V4L2VideoEncodeAccelerator::Encode(
    const scoped_refptr<media::VideoFrame>& frame,
    bool force_keyframe) {
  DVLOG(3) << "Encode(): force_keyframe=" << force_keyframe;
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());

  if (image_processor_) {
    image_processor_->Process(
        frame,
        base::Bind(&V4L2VideoEncodeAccelerator::FrameProcessed,
                   weak_this_,
                   force_keyframe));
  } else {
    encoder_thread_.message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&V4L2VideoEncodeAccelerator::EncodeTask,
                   base::Unretained(this),
                   frame,
                   force_keyframe));
  }
}

void V4L2VideoEncodeAccelerator::UseOutputBitstreamBuffer(
    const media::BitstreamBuffer& buffer) {
  DVLOG(3) << "UseOutputBitstreamBuffer(): id=" << buffer.id();
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());

  if (buffer.size() < output_buffer_byte_size_) {
    NOTIFY_ERROR(kInvalidArgumentError);
    return;
  }

  scoped_ptr<base::SharedMemory> shm(
      new base::SharedMemory(buffer.handle(), false));
  if (!shm->Map(buffer.size())) {
    NOTIFY_ERROR(kPlatformFailureError);
    return;
  }

  scoped_ptr<BitstreamBufferRef> buffer_ref(
      new BitstreamBufferRef(buffer.id(), shm.Pass(), buffer.size()));
  encoder_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&V4L2VideoEncodeAccelerator::UseOutputBitstreamBufferTask,
                 base::Unretained(this),
                 base::Passed(&buffer_ref)));
}

void V4L2VideoEncodeAccelerator::RequestEncodingParametersChange(
    uint32 bitrate,
    uint32 framerate) {
  DVLOG(3) << "RequestEncodingParametersChange(): bitrate=" << bitrate
           << ", framerate=" << framerate;
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());

  encoder_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(
          &V4L2VideoEncodeAccelerator::RequestEncodingParametersChangeTask,
          base::Unretained(this),
          bitrate,
          framerate));
}

void V4L2VideoEncodeAccelerator::Destroy() {
  DVLOG(3) << "Destroy()";
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());

  // We're destroying; cancel all callbacks.
  client_ptr_factory_.reset();

  if (image_processor_.get())
    image_processor_.release()->Destroy();

  // If the encoder thread is running, destroy using posted task.
  if (encoder_thread_.IsRunning()) {
    encoder_thread_.message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&V4L2VideoEncodeAccelerator::DestroyTask,
                   base::Unretained(this)));
    // DestroyTask() will put the encoder into kError state and cause all tasks
    // to no-op.
    encoder_thread_.Stop();
  } else {
    // Otherwise, call the destroy task directly.
    DestroyTask();
  }

  // Set to kError state just in case.
  SetEncoderState(kError);

  delete this;
}

// static
std::vector<media::VideoEncodeAccelerator::SupportedProfile>
V4L2VideoEncodeAccelerator::GetSupportedProfiles() {
  std::vector<SupportedProfile> profiles;
  SupportedProfile profile;

  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kEnableWebRtcHWVp8Encoding)) {
    profile.profile = media::VP8PROFILE_MAIN;
    profile.max_resolution.SetSize(1920, 1088);
    profile.max_framerate.numerator = 30;
    profile.max_framerate.denominator = 1;
    profiles.push_back(profile);
  } else {
    profile.profile = media::H264PROFILE_MAIN;
    profile.max_resolution.SetSize(1920, 1088);
    profile.max_framerate.numerator = 30;
    profile.max_framerate.denominator = 1;
    profiles.push_back(profile);
  }

  return profiles;
}

void V4L2VideoEncodeAccelerator::FrameProcessed(
    bool force_keyframe,
    const scoped_refptr<media::VideoFrame>& frame) {
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DVLOG(3) << "FrameProcessed(): force_keyframe=" << force_keyframe;

  encoder_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&V4L2VideoEncodeAccelerator::EncodeTask,
                 base::Unretained(this),
                 frame,
                 force_keyframe));
}

void V4L2VideoEncodeAccelerator::EncodeTask(
    const scoped_refptr<media::VideoFrame>& frame,
    bool force_keyframe) {
  DVLOG(3) << "EncodeTask(): force_keyframe=" << force_keyframe;
  DCHECK_EQ(encoder_thread_.message_loop(), base::MessageLoop::current());
  DCHECK_NE(encoder_state_, kUninitialized);

  if (encoder_state_ == kError) {
    DVLOG(2) << "EncodeTask(): early out: kError state";
    return;
  }

  encoder_input_queue_.push_back(frame);
  Enqueue();

  if (force_keyframe) {
    // TODO(posciak): this presently makes for slightly imprecise encoding
    // parameters updates.  To precisely align the parameter updates with the
    // incoming input frame, we should queue the parameters together with the
    // frame onto encoder_input_queue_ and apply them when the input is about
    // to be queued to the codec.
    struct v4l2_ext_control ctrls[1];
    struct v4l2_ext_controls control;
    memset(&ctrls, 0, sizeof(ctrls));
    memset(&control, 0, sizeof(control));
    ctrls[0].id = V4L2_CID_MPEG_MFC51_VIDEO_FORCE_FRAME_TYPE;
    ctrls[0].value = V4L2_MPEG_MFC51_VIDEO_FORCE_FRAME_TYPE_I_FRAME;
    control.ctrl_class = V4L2_CTRL_CLASS_MPEG;
    control.count = 1;
    control.controls = ctrls;
    IOCTL_OR_ERROR_RETURN(VIDIOC_S_EXT_CTRLS, &control);
  }
}

void V4L2VideoEncodeAccelerator::UseOutputBitstreamBufferTask(
    scoped_ptr<BitstreamBufferRef> buffer_ref) {
  DVLOG(3) << "UseOutputBitstreamBufferTask(): id=" << buffer_ref->id;
  DCHECK_EQ(encoder_thread_.message_loop(), base::MessageLoop::current());

  encoder_output_queue_.push_back(
      linked_ptr<BitstreamBufferRef>(buffer_ref.release()));
  Enqueue();

  if (encoder_state_ == kInitialized) {
    // Finish setting up our OUTPUT queue.  See: Initialize().
    // VIDIOC_REQBUFS on OUTPUT queue.
    if (!CreateInputBuffers())
      return;
    if (!StartDevicePoll())
      return;
    encoder_state_ = kEncoding;
  }
}

void V4L2VideoEncodeAccelerator::DestroyTask() {
  DVLOG(3) << "DestroyTask()";

  // DestroyTask() should run regardless of encoder_state_.

  // Stop streaming and the device_poll_thread_.
  StopDevicePoll();

  // Set our state to kError, and early-out all tasks.
  encoder_state_ = kError;
}

void V4L2VideoEncodeAccelerator::ServiceDeviceTask() {
  DVLOG(3) << "ServiceDeviceTask()";
  DCHECK_EQ(encoder_thread_.message_loop(), base::MessageLoop::current());
  DCHECK_NE(encoder_state_, kUninitialized);
  DCHECK_NE(encoder_state_, kInitialized);

  if (encoder_state_ == kError) {
    DVLOG(2) << "ServiceDeviceTask(): early out: kError state";
    return;
  }

  Dequeue();
  Enqueue();

  // Clear the interrupt fd.
  if (!device_->ClearDevicePollInterrupt())
    return;

  // Device can be polled as soon as either input or output buffers are queued.
  bool poll_device =
      (input_buffer_queued_count_ + output_buffer_queued_count_ > 0);

  // ServiceDeviceTask() should only ever be scheduled from DevicePollTask(),
  // so either:
  // * device_poll_thread_ is running normally
  // * device_poll_thread_ scheduled us, but then a DestroyTask() shut it down,
  //   in which case we're in kError state, and we should have early-outed
  //   already.
  DCHECK(device_poll_thread_.message_loop());
  // Queue the DevicePollTask() now.
  device_poll_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&V4L2VideoEncodeAccelerator::DevicePollTask,
                 base::Unretained(this),
                 poll_device));

  DVLOG(2) << __func__ << ": buffer counts: ENC["
           << encoder_input_queue_.size() << "] => DEVICE["
           << free_input_buffers_.size() << "+"
           << input_buffer_queued_count_ << "/"
           << input_buffer_map_.size() << "->"
           << free_output_buffers_.size() << "+"
           << output_buffer_queued_count_ << "/"
           << output_buffer_map_.size() << "] => OUT["
           << encoder_output_queue_.size() << "]";
}

void V4L2VideoEncodeAccelerator::Enqueue() {
  DCHECK_EQ(encoder_thread_.message_loop(), base::MessageLoop::current());

  DVLOG(3) << "Enqueue() "
           << "free_input_buffers: " << free_input_buffers_.size()
           << "input_queue: " << encoder_input_queue_.size();

  // Enqueue all the inputs we can.
  const int old_inputs_queued = input_buffer_queued_count_;
  // while (!ready_input_buffers_.empty()) {
  while (!encoder_input_queue_.empty() && !free_input_buffers_.empty()) {
    if (!EnqueueInputRecord())
      return;
  }
  if (old_inputs_queued == 0 && input_buffer_queued_count_ != 0) {
    // We just started up a previously empty queue.
    // Queue state changed; signal interrupt.
    if (!device_->SetDevicePollInterrupt())
      return;
    // Start VIDIOC_STREAMON if we haven't yet.
    if (!input_streamon_) {
      __u32 type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
      IOCTL_OR_ERROR_RETURN(VIDIOC_STREAMON, &type);
      input_streamon_ = true;
    }
  }

  // Enqueue all the outputs we can.
  const int old_outputs_queued = output_buffer_queued_count_;
  while (!free_output_buffers_.empty() && !encoder_output_queue_.empty()) {
    if (!EnqueueOutputRecord())
      return;
  }
  if (old_outputs_queued == 0 && output_buffer_queued_count_ != 0) {
    // We just started up a previously empty queue.
    // Queue state changed; signal interrupt.
    if (!device_->SetDevicePollInterrupt())
      return;
    // Start VIDIOC_STREAMON if we haven't yet.
    if (!output_streamon_) {
      __u32 type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
      IOCTL_OR_ERROR_RETURN(VIDIOC_STREAMON, &type);
      output_streamon_ = true;
    }
  }
}

void V4L2VideoEncodeAccelerator::Dequeue() {
  DVLOG(3) << "Dequeue()";
  DCHECK_EQ(encoder_thread_.message_loop(), base::MessageLoop::current());

  // Dequeue completed input (VIDEO_OUTPUT) buffers, and recycle to the free
  // list.
  struct v4l2_buffer dqbuf;
  struct v4l2_plane planes[VIDEO_MAX_PLANES];
  while (input_buffer_queued_count_ > 0) {
    DVLOG(4) << "inputs queued: " << input_buffer_queued_count_;
    DCHECK(input_streamon_);
    memset(&dqbuf, 0, sizeof(dqbuf));
    memset(&planes, 0, sizeof(planes));
    dqbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    dqbuf.memory = V4L2_MEMORY_MMAP;
    dqbuf.m.planes = planes;
    dqbuf.length = input_planes_count_;
    if (device_->Ioctl(VIDIOC_DQBUF, &dqbuf) != 0) {
      if (errno == EAGAIN) {
        // EAGAIN if we're just out of buffers to dequeue.
        break;
      }
      DPLOG(ERROR) << "Dequeue(): ioctl() failed: VIDIOC_DQBUF";
      NOTIFY_ERROR(kPlatformFailureError);
      return;
    }
    InputRecord& input_record = input_buffer_map_[dqbuf.index];
    DCHECK(input_record.at_device);
    input_record.at_device = false;

    input_record.frame = NULL;
    free_input_buffers_.push_back(dqbuf.index);
    input_buffer_queued_count_--;
  }

  // Dequeue completed output (VIDEO_CAPTURE) buffers, and recycle to the
  // free list.  Notify the client that an output buffer is complete.
  while (output_buffer_queued_count_ > 0) {
    DCHECK(output_streamon_);
    memset(&dqbuf, 0, sizeof(dqbuf));
    memset(planes, 0, sizeof(planes));
    dqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    dqbuf.memory = V4L2_MEMORY_MMAP;
    dqbuf.m.planes = planes;
    dqbuf.length = 1;
    if (device_->Ioctl(VIDIOC_DQBUF, &dqbuf) != 0) {
      if (errno == EAGAIN) {
        // EAGAIN if we're just out of buffers to dequeue.
        break;
      }
      DPLOG(ERROR) << "Dequeue(): ioctl() failed: VIDIOC_DQBUF";
      NOTIFY_ERROR(kPlatformFailureError);
      return;
    }
    const bool key_frame = ((dqbuf.flags & V4L2_BUF_FLAG_KEYFRAME) != 0);
    OutputRecord& output_record = output_buffer_map_[dqbuf.index];
    DCHECK(output_record.at_device);
    DCHECK(output_record.buffer_ref.get());

    void* output_data = output_record.address;
    size_t output_size = dqbuf.m.planes[0].bytesused;
    // This shouldn't happen, but just in case. We should be able to recover
    // after next keyframe after showing some corruption.
    DCHECK_LE(output_size, output_buffer_byte_size_);
    if (output_size > output_buffer_byte_size_)
      output_size = output_buffer_byte_size_;
    uint8* target_data =
        reinterpret_cast<uint8*>(output_record.buffer_ref->shm->memory());
    if (output_format_fourcc_ == V4L2_PIX_FMT_H264) {
      if (stream_header_size_ == 0) {
        // Assume that the first buffer dequeued is the stream header.
        stream_header_size_ = output_size;
        stream_header_.reset(new uint8[stream_header_size_]);
        memcpy(stream_header_.get(), output_data, stream_header_size_);
      }
      if (key_frame &&
          output_buffer_byte_size_ - stream_header_size_ >= output_size) {
        // Insert stream header before every keyframe.
        memcpy(target_data, stream_header_.get(), stream_header_size_);
        memcpy(target_data + stream_header_size_, output_data, output_size);
        output_size += stream_header_size_;
      } else {
        memcpy(target_data, output_data, output_size);
      }
    } else {
      memcpy(target_data, output_data, output_size);
    }

    DVLOG(3) << "Dequeue(): returning "
                "bitstream_buffer_id=" << output_record.buffer_ref->id
             << ", size=" << output_size << ", key_frame=" << key_frame;
    child_message_loop_proxy_->PostTask(
        FROM_HERE,
        base::Bind(&Client::BitstreamBufferReady,
                   client_,
                   output_record.buffer_ref->id,
                   output_size,
                   key_frame));
    output_record.at_device = false;
    output_record.buffer_ref.reset();
    free_output_buffers_.push_back(dqbuf.index);
    output_buffer_queued_count_--;
  }
}

bool V4L2VideoEncodeAccelerator::EnqueueInputRecord() {
  DVLOG(3) << "EnqueueInputRecord()";
  DCHECK(!free_input_buffers_.empty());
  DCHECK(!encoder_input_queue_.empty());

  // Enqueue an input (VIDEO_OUTPUT) buffer.
  scoped_refptr<media::VideoFrame> frame = encoder_input_queue_.front();
  const int index = free_input_buffers_.back();
  InputRecord& input_record = input_buffer_map_[index];
  DCHECK(!input_record.at_device);
  struct v4l2_buffer qbuf;
  struct v4l2_plane qbuf_planes[VIDEO_MAX_PLANES];
  memset(&qbuf, 0, sizeof(qbuf));
  memset(qbuf_planes, 0, sizeof(qbuf_planes));
  qbuf.index = index;
  qbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  qbuf.m.planes = qbuf_planes;

  DCHECK_EQ(device_input_format_, frame->format());
  for (size_t i = 0; i < input_planes_count_; ++i) {
    qbuf.m.planes[i].bytesused =
        base::checked_cast<__u32>(media::VideoFrame::PlaneAllocationSize(
            frame->format(), i, input_allocated_size_));

    switch (input_memory_type_) {
      case V4L2_MEMORY_USERPTR:
        qbuf.m.planes[i].m.userptr =
            reinterpret_cast<unsigned long>(frame->data(i));
        DCHECK(qbuf.m.planes[i].m.userptr);
        break;

      case V4L2_MEMORY_DMABUF:
        qbuf.m.planes[i].m.fd = frame->dmabuf_fd(i);
        DCHECK_NE(qbuf.m.planes[i].m.fd, -1);
        break;

      default:
        NOTREACHED();
        return false;
    }
  }

  qbuf.memory = input_memory_type_;
  qbuf.length = input_planes_count_;

  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QBUF, &qbuf);
  input_record.at_device = true;
  input_record.frame = frame;
  encoder_input_queue_.pop_front();
  free_input_buffers_.pop_back();
  input_buffer_queued_count_++;
  return true;
}

bool V4L2VideoEncodeAccelerator::EnqueueOutputRecord() {
  DVLOG(3) << "EnqueueOutputRecord()";
  DCHECK(!free_output_buffers_.empty());
  DCHECK(!encoder_output_queue_.empty());

  // Enqueue an output (VIDEO_CAPTURE) buffer.
  linked_ptr<BitstreamBufferRef> output_buffer = encoder_output_queue_.back();
  const int index = free_output_buffers_.back();
  OutputRecord& output_record = output_buffer_map_[index];
  DCHECK(!output_record.at_device);
  DCHECK(!output_record.buffer_ref.get());
  struct v4l2_buffer qbuf;
  struct v4l2_plane qbuf_planes[1];
  memset(&qbuf, 0, sizeof(qbuf));
  memset(qbuf_planes, 0, sizeof(qbuf_planes));
  qbuf.index = index;
  qbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  qbuf.memory = V4L2_MEMORY_MMAP;
  qbuf.m.planes = qbuf_planes;
  qbuf.length = 1;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QBUF, &qbuf);
  output_record.at_device = true;
  output_record.buffer_ref = output_buffer;
  encoder_output_queue_.pop_back();
  free_output_buffers_.pop_back();
  output_buffer_queued_count_++;
  return true;
}

bool V4L2VideoEncodeAccelerator::StartDevicePoll() {
  DVLOG(3) << "StartDevicePoll()";
  DCHECK_EQ(encoder_thread_.message_loop(), base::MessageLoop::current());
  DCHECK(!device_poll_thread_.IsRunning());

  // Start up the device poll thread and schedule its first DevicePollTask().
  if (!device_poll_thread_.Start()) {
    DLOG(ERROR) << "StartDevicePoll(): Device thread failed to start";
    NOTIFY_ERROR(kPlatformFailureError);
    return false;
  }
  // Enqueue a poll task with no devices to poll on -- it will wait only on the
  // interrupt fd.
  device_poll_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&V4L2VideoEncodeAccelerator::DevicePollTask,
                 base::Unretained(this),
                 false));

  return true;
}

bool V4L2VideoEncodeAccelerator::StopDevicePoll() {
  DVLOG(3) << "StopDevicePoll()";

  // Signal the DevicePollTask() to stop, and stop the device poll thread.
  if (!device_->SetDevicePollInterrupt())
    return false;
  device_poll_thread_.Stop();
  // Clear the interrupt now, to be sure.
  if (!device_->ClearDevicePollInterrupt())
    return false;

  if (input_streamon_) {
    __u32 type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_STREAMOFF, &type);
  }
  input_streamon_ = false;

  if (output_streamon_) {
    __u32 type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_STREAMOFF, &type);
  }
  output_streamon_ = false;

  // Reset all our accounting info.
  encoder_input_queue_.clear();
  free_input_buffers_.clear();
  for (size_t i = 0; i < input_buffer_map_.size(); ++i) {
    InputRecord& input_record = input_buffer_map_[i];
    input_record.at_device = false;
    input_record.frame = NULL;
    free_input_buffers_.push_back(i);
  }
  input_buffer_queued_count_ = 0;

  free_output_buffers_.clear();
  for (size_t i = 0; i < output_buffer_map_.size(); ++i) {
    OutputRecord& output_record = output_buffer_map_[i];
    output_record.at_device = false;
    output_record.buffer_ref.reset();
    free_output_buffers_.push_back(i);
  }
  output_buffer_queued_count_ = 0;

  encoder_output_queue_.clear();

  DVLOG(3) << "StopDevicePoll(): device poll stopped";
  return true;
}

void V4L2VideoEncodeAccelerator::DevicePollTask(bool poll_device) {
  DVLOG(3) << "DevicePollTask()";
  DCHECK_EQ(device_poll_thread_.message_loop(), base::MessageLoop::current());

  bool event_pending;
  if (!device_->Poll(poll_device, &event_pending)) {
    NOTIFY_ERROR(kPlatformFailureError);
    return;
  }

  // All processing should happen on ServiceDeviceTask(), since we shouldn't
  // touch encoder state from this thread.
  encoder_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&V4L2VideoEncodeAccelerator::ServiceDeviceTask,
                 base::Unretained(this)));
}

void V4L2VideoEncodeAccelerator::NotifyError(Error error) {
  DVLOG(1) << "NotifyError(): error=" << error;

  if (!child_message_loop_proxy_->BelongsToCurrentThread()) {
    child_message_loop_proxy_->PostTask(
        FROM_HERE,
        base::Bind(
            &V4L2VideoEncodeAccelerator::NotifyError, weak_this_, error));
    return;
  }

  if (client_) {
    client_->NotifyError(error);
    client_ptr_factory_.reset();
  }
}

void V4L2VideoEncodeAccelerator::SetEncoderState(State state) {
  DVLOG(3) << "SetEncoderState(): state=" << state;

  // We can touch encoder_state_ only if this is the encoder thread or the
  // encoder thread isn't running.
  if (encoder_thread_.message_loop() != NULL &&
      encoder_thread_.message_loop() != base::MessageLoop::current()) {
    encoder_thread_.message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&V4L2VideoEncodeAccelerator::SetEncoderState,
                   base::Unretained(this),
                   state));
  } else {
    encoder_state_ = state;
  }
}

void V4L2VideoEncodeAccelerator::RequestEncodingParametersChangeTask(
    uint32 bitrate,
    uint32 framerate) {
  DVLOG(3) << "RequestEncodingParametersChangeTask(): bitrate=" << bitrate
           << ", framerate=" << framerate;
  DCHECK_EQ(encoder_thread_.message_loop(), base::MessageLoop::current());

  if (bitrate < 1)
    bitrate = 1;
  if (framerate < 1)
    framerate = 1;

  struct v4l2_ext_control ctrls[1];
  struct v4l2_ext_controls control;
  memset(&ctrls, 0, sizeof(ctrls));
  memset(&control, 0, sizeof(control));
  ctrls[0].id = V4L2_CID_MPEG_VIDEO_BITRATE;
  ctrls[0].value = bitrate;
  control.ctrl_class = V4L2_CTRL_CLASS_MPEG;
  control.count = arraysize(ctrls);
  control.controls = ctrls;
  IOCTL_OR_ERROR_RETURN(VIDIOC_S_EXT_CTRLS, &control);

  struct v4l2_streamparm parms;
  memset(&parms, 0, sizeof(parms));
  parms.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  // Note that we are provided "frames per second" but V4L2 expects "time per
  // frame"; hence we provide the reciprocal of the framerate here.
  parms.parm.output.timeperframe.numerator = 1;
  parms.parm.output.timeperframe.denominator = framerate;
  IOCTL_OR_ERROR_RETURN(VIDIOC_S_PARM, &parms);
}

bool V4L2VideoEncodeAccelerator::SetOutputFormat(
    media::VideoCodecProfile output_profile) {
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(!input_streamon_);
  DCHECK(!output_streamon_);

  output_format_fourcc_ =
      V4L2Device::VideoCodecProfileToV4L2PixFmt(output_profile);
  if (!output_format_fourcc_) {
    DLOG(ERROR) << "Initialize(): invalid output_profile=" << output_profile;
    return false;
  }

  output_buffer_byte_size_ = kOutputBufferSize;

  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  format.fmt.pix_mp.width = visible_size_.width();
  format.fmt.pix_mp.height = visible_size_.height();
  format.fmt.pix_mp.pixelformat = output_format_fourcc_;
  format.fmt.pix_mp.plane_fmt[0].sizeimage =
      base::checked_cast<__u32>(output_buffer_byte_size_);
  format.fmt.pix_mp.num_planes = 1;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_FMT, &format);

  // Device might have adjusted the required output size.
  size_t adjusted_output_buffer_size =
      base::checked_cast<size_t>(format.fmt.pix_mp.plane_fmt[0].sizeimage);
  DCHECK_GE(adjusted_output_buffer_size, output_buffer_byte_size_);
  output_buffer_byte_size_ = adjusted_output_buffer_size;

  return true;
}

bool V4L2VideoEncodeAccelerator::NegotiateInputFormat(
    media::VideoFrame::Format input_format) {
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(!input_streamon_);
  DCHECK(!output_streamon_);

  device_input_format_ = media::VideoFrame::UNKNOWN;
  input_planes_count_ = 0;

  uint32 input_format_fourcc =
      V4L2Device::VideoFrameFormatToV4L2PixFmt(input_format);
  if (!input_format_fourcc) {
    DVLOG(1) << "Unsupported input format";
    return false;
  }

  size_t input_planes_count = media::VideoFrame::NumPlanes(input_format);
  DCHECK_LE(input_planes_count, static_cast<size_t>(VIDEO_MAX_PLANES));

  // First see if we the device can use the provided input_format directly.
  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  format.fmt.pix_mp.width = visible_size_.width();
  format.fmt.pix_mp.height = visible_size_.height();
  format.fmt.pix_mp.pixelformat = input_format_fourcc;
  format.fmt.pix_mp.num_planes = input_planes_count;
  if (device_->Ioctl(VIDIOC_S_FMT, &format) != 0) {
    // Error or format unsupported by device, try to negotiate a fallback.
    input_format_fourcc = device_->PreferredInputFormat();
    input_format =
        V4L2Device::V4L2PixFmtToVideoFrameFormat(input_format_fourcc);
    if (input_format == media::VideoFrame::UNKNOWN)
      return false;

    input_planes_count = media::VideoFrame::NumPlanes(input_format);
    DCHECK_LE(input_planes_count, static_cast<size_t>(VIDEO_MAX_PLANES));

    // Device might have adjusted parameters, reset them along with the format.
    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    format.fmt.pix_mp.width = visible_size_.width();
    format.fmt.pix_mp.height = visible_size_.height();
    format.fmt.pix_mp.pixelformat = input_format_fourcc;
    format.fmt.pix_mp.num_planes = input_planes_count;
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_FMT, &format);
    DCHECK_EQ(format.fmt.pix_mp.num_planes, input_planes_count);
  }

  // Take device-adjusted sizes for allocated size.
  input_allocated_size_ = V4L2Device::CodedSizeFromV4L2Format(format);
  DCHECK(gfx::Rect(input_allocated_size_).Contains(gfx::Rect(visible_size_)));

  device_input_format_ = input_format;
  input_planes_count_ = input_planes_count;
  return true;
}

bool V4L2VideoEncodeAccelerator::SetFormats(
    media::VideoFrame::Format input_format,
    media::VideoCodecProfile output_profile) {
  DVLOG(3) << "SetFormats()";
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(!input_streamon_);
  DCHECK(!output_streamon_);

  if (!SetOutputFormat(output_profile))
    return false;

  if (!NegotiateInputFormat(input_format))
    return false;

  struct v4l2_crop crop;
  memset(&crop, 0, sizeof(crop));
  crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  crop.c.left = 0;
  crop.c.top = 0;
  crop.c.width = visible_size_.width();
  crop.c.height = visible_size_.height();
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_CROP, &crop);

  return true;
}

bool V4L2VideoEncodeAccelerator::InitControls() {
  struct v4l2_ext_control ctrls[9];
  struct v4l2_ext_controls control;
  memset(&ctrls, 0, sizeof(ctrls));
  memset(&control, 0, sizeof(control));
  // No B-frames, for lowest decoding latency.
  ctrls[0].id = V4L2_CID_MPEG_VIDEO_B_FRAMES;
  ctrls[0].value = 0;
  // Enable frame-level bitrate control.
  ctrls[1].id = V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE;
  ctrls[1].value = 1;
  // Enable "tight" bitrate mode. For this to work properly, frame- and mb-level
  // bitrate controls have to be enabled as well.
  ctrls[2].id = V4L2_CID_MPEG_MFC51_VIDEO_RC_REACTION_COEFF;
  ctrls[2].value = 1;
  // Force bitrate control to average over a GOP (for tight bitrate
  // tolerance).
  ctrls[3].id = V4L2_CID_MPEG_MFC51_VIDEO_RC_FIXED_TARGET_BIT;
  ctrls[3].value = 1;
  // Quantization parameter maximum value (for variable bitrate control).
  ctrls[4].id = V4L2_CID_MPEG_VIDEO_H264_MAX_QP;
  ctrls[4].value = 51;
  // Separate stream header so we can cache it and insert into the stream.
  ctrls[5].id = V4L2_CID_MPEG_VIDEO_HEADER_MODE;
  ctrls[5].value = V4L2_MPEG_VIDEO_HEADER_MODE_SEPARATE;
  // Enable macroblock-level bitrate control.
  ctrls[6].id = V4L2_CID_MPEG_VIDEO_MB_RC_ENABLE;
  ctrls[6].value = 1;
  // Use H.264 level 4.0 to match the supported max resolution.
  ctrls[7].id = V4L2_CID_MPEG_VIDEO_H264_LEVEL;
  ctrls[7].value = V4L2_MPEG_VIDEO_H264_LEVEL_4_0;
  // Disable periodic key frames.
  ctrls[8].id = V4L2_CID_MPEG_VIDEO_GOP_SIZE;
  ctrls[8].value = 0;
  control.ctrl_class = V4L2_CTRL_CLASS_MPEG;
  control.count = arraysize(ctrls);
  control.controls = ctrls;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_EXT_CTRLS, &control);

  return true;
}

bool V4L2VideoEncodeAccelerator::CreateInputBuffers() {
  DVLOG(3) << "CreateInputBuffers()";
  // This function runs on encoder_thread_ after output buffers have been
  // provided by the client.
  DCHECK_EQ(encoder_thread_.message_loop(), base::MessageLoop::current());
  DCHECK(!input_streamon_);

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  // Driver will modify to the appropriate number of buffers.
  reqbufs.count = 1;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  // TODO(posciak): Once we start doing zero-copy, we should decide based on
  // the current pipeline setup which memory type to use. This should probably
  // be decided based on an argument to Initialize().
  if (image_processor_.get())
    input_memory_type_ = V4L2_MEMORY_DMABUF;
  else
    input_memory_type_ = V4L2_MEMORY_USERPTR;

  reqbufs.memory = input_memory_type_;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_REQBUFS, &reqbufs);

  DCHECK(input_buffer_map_.empty());
  input_buffer_map_.resize(reqbufs.count);
  for (size_t i = 0; i < input_buffer_map_.size(); ++i)
    free_input_buffers_.push_back(i);

  return true;
}

bool V4L2VideoEncodeAccelerator::CreateOutputBuffers() {
  DVLOG(3) << "CreateOutputBuffers()";
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(!output_streamon_);

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = kOutputBufferCount;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_REQBUFS, &reqbufs);

  DCHECK(output_buffer_map_.empty());
  output_buffer_map_.resize(reqbufs.count);
  for (size_t i = 0; i < output_buffer_map_.size(); ++i) {
    struct v4l2_plane planes[1];
    struct v4l2_buffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    memset(planes, 0, sizeof(planes));
    buffer.index = i;
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.m.planes = planes;
    buffer.length = arraysize(planes);
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QUERYBUF, &buffer);
    void* address = device_->Mmap(NULL,
                                  buffer.m.planes[0].length,
                                  PROT_READ | PROT_WRITE,
                                  MAP_SHARED,
                                  buffer.m.planes[0].m.mem_offset);
    if (address == MAP_FAILED) {
      DPLOG(ERROR) << "CreateOutputBuffers(): mmap() failed";
      return false;
    }
    output_buffer_map_[i].address = address;
    output_buffer_map_[i].length = buffer.m.planes[0].length;
    free_output_buffers_.push_back(i);
  }

  return true;
}

void V4L2VideoEncodeAccelerator::DestroyInputBuffers() {
  DVLOG(3) << "DestroyInputBuffers()";
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(!input_streamon_);

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  reqbufs.memory = input_memory_type_;
  IOCTL_OR_LOG_ERROR(VIDIOC_REQBUFS, &reqbufs);

  input_buffer_map_.clear();
  free_input_buffers_.clear();
}

void V4L2VideoEncodeAccelerator::DestroyOutputBuffers() {
  DVLOG(3) << "DestroyOutputBuffers()";
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(!output_streamon_);

  for (size_t i = 0; i < output_buffer_map_.size(); ++i) {
    if (output_buffer_map_[i].address != NULL)
      device_->Munmap(output_buffer_map_[i].address,
                      output_buffer_map_[i].length);
  }

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  IOCTL_OR_LOG_ERROR(VIDIOC_REQBUFS, &reqbufs);

  output_buffer_map_.clear();
  free_output_buffers_.clear();
}

}  // namespace content
