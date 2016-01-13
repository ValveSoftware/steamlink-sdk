// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/numerics/safe_conversions.h"
#include "content/common/gpu/media/v4l2_image_processor.h"
#include "media/base/bind_to_current_loop.h"

#define NOTIFY_ERROR()                      \
  do {                                      \
    DLOG(ERROR) << "calling NotifyError()"; \
    NotifyError();                          \
  } while (0)

#define IOCTL_OR_ERROR_RETURN_VALUE(type, arg, value)              \
  do {                                                             \
    if (device_->Ioctl(type, arg) != 0) {                          \
      DPLOG(ERROR) << __func__ << "(): ioctl() failed: " << #type; \
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

V4L2ImageProcessor::InputRecord::InputRecord() : at_device(false) {
}

V4L2ImageProcessor::OutputRecord::OutputRecord()
    : at_device(false), at_client(false) {
}

V4L2ImageProcessor::JobRecord::JobRecord() {
}

V4L2ImageProcessor::V4L2ImageProcessor(scoped_ptr<V4L2Device> device)
    : input_format_(media::VideoFrame::UNKNOWN),
      output_format_(media::VideoFrame::UNKNOWN),
      input_format_fourcc_(0),
      output_format_fourcc_(0),
      input_planes_count_(0),
      output_planes_count_(0),
      child_message_loop_proxy_(base::MessageLoopProxy::current()),
      device_(device.Pass()),
      device_thread_("V4L2ImageProcessorThread"),
      device_poll_thread_("V4L2ImageProcessorDevicePollThread"),
      input_streamon_(false),
      input_buffer_queued_count_(0),
      output_streamon_(false),
      output_buffer_queued_count_(0),
      device_weak_factory_(this) {
}

V4L2ImageProcessor::~V4L2ImageProcessor() {
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(!device_thread_.IsRunning());
  DCHECK(!device_poll_thread_.IsRunning());

  DestroyInputBuffers();
  DestroyOutputBuffers();
}

void V4L2ImageProcessor::NotifyError() {
  if (!child_message_loop_proxy_->BelongsToCurrentThread())
    child_message_loop_proxy_->PostTask(FROM_HERE, error_cb_);
  else
    error_cb_.Run();
}

bool V4L2ImageProcessor::Initialize(media::VideoFrame::Format input_format,
                                    media::VideoFrame::Format output_format,
                                    gfx::Size input_visible_size,
                                    gfx::Size output_visible_size,
                                    gfx::Size output_allocated_size,
                                    const base::Closure& error_cb) {
  DCHECK(!error_cb.is_null());
  error_cb_ = error_cb;

  // TODO(posciak): Replace Exynos-specific format/parameter hardcoding in this
  // class with proper capability enumeration.
  DCHECK_EQ(input_format, media::VideoFrame::I420);
  DCHECK_EQ(output_format, media::VideoFrame::NV12);

  input_format_ = input_format;
  output_format_ = output_format;
  input_format_fourcc_ = V4L2Device::VideoFrameFormatToV4L2PixFmt(input_format);
  output_format_fourcc_ =
      V4L2Device::VideoFrameFormatToV4L2PixFmt(output_format);

  if (!input_format_fourcc_ || !output_format_fourcc_) {
    DLOG(ERROR) << "Unrecognized format(s)";
    return false;
  }

  input_visible_size_ = input_visible_size;
  output_visible_size_ = output_visible_size;
  output_allocated_size_ = output_allocated_size;

  input_planes_count_ = media::VideoFrame::NumPlanes(input_format);
  DCHECK_LE(input_planes_count_, static_cast<size_t>(VIDEO_MAX_PLANES));
  output_planes_count_ = media::VideoFrame::NumPlanes(output_format);
  DCHECK_LE(output_planes_count_, static_cast<size_t>(VIDEO_MAX_PLANES));

  // Capabilities check.
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

  if (!CreateInputBuffers() || !CreateOutputBuffers())
    return false;

  if (!device_thread_.Start()) {
    DLOG(ERROR) << "Initialize(): encoder thread failed to start";
    return false;
  }

  // StartDevicePoll will NOTIFY_ERROR on failure, so IgnoreResult is fine here.
  device_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&V4L2ImageProcessor::StartDevicePoll),
                 base::Unretained(this)));

  DVLOG(1) << "V4L2ImageProcessor initialized for "
           << " input_format:"
           << media::VideoFrame::FormatToString(input_format)
           << ", output_format:"
           << media::VideoFrame::FormatToString(output_format)
           << ", input_visible_size: " << input_visible_size.ToString()
           << ", input_allocated_size: " << input_allocated_size_.ToString()
           << ", output_visible_size: " << output_visible_size.ToString()
           << ", output_allocated_size: " << output_allocated_size.ToString();

  return true;
}

void V4L2ImageProcessor::Process(const scoped_refptr<media::VideoFrame>& frame,
                                 const FrameReadyCB& cb) {
  DVLOG(3) << __func__ << ": ts=" << frame->timestamp().InMilliseconds();

  scoped_ptr<JobRecord> job_record(new JobRecord());
  job_record->frame = frame;
  job_record->ready_cb = cb;

  device_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&V4L2ImageProcessor::ProcessTask,
                 base::Unretained(this),
                 base::Passed(&job_record)));
}

void V4L2ImageProcessor::ProcessTask(scoped_ptr<JobRecord> job_record) {
  DCHECK_EQ(device_thread_.message_loop(), base::MessageLoop::current());

  input_queue_.push(make_linked_ptr(job_record.release()));
  Enqueue();
}

void V4L2ImageProcessor::Destroy() {
  DVLOG(3) << __func__;
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());

  // If the device thread is running, destroy using posted task.
  if (device_thread_.IsRunning()) {
    device_thread_.message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&V4L2ImageProcessor::DestroyTask, base::Unretained(this)));
    // Wait for tasks to finish/early-exit.
    device_thread_.Stop();
  } else {
    // Otherwise DestroyTask() is not needed.
    DCHECK(!device_poll_thread_.IsRunning());
    DCHECK(!device_weak_factory_.HasWeakPtrs());
  }

  delete this;
}

void V4L2ImageProcessor::DestroyTask() {
  DCHECK_EQ(device_thread_.message_loop(), base::MessageLoop::current());

  device_weak_factory_.InvalidateWeakPtrs();

  // Stop streaming and the device_poll_thread_.
  StopDevicePoll();
}

bool V4L2ImageProcessor::CreateInputBuffers() {
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(!input_streamon_);

  struct v4l2_control control;
  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_ROTATE;
  control.value = 0;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_CTRL, &control);

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_HFLIP;
  control.value = 0;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_CTRL, &control);

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_VFLIP;
  control.value = 0;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_CTRL, &control);

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_ALPHA_COMPONENT;
  control.value = 255;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_CTRL, &control);

  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  format.fmt.pix_mp.width = input_visible_size_.width();
  format.fmt.pix_mp.height = input_visible_size_.height();
  format.fmt.pix_mp.pixelformat = input_format_fourcc_;
  format.fmt.pix_mp.num_planes = input_planes_count_;
  for (size_t i = 0; i < input_planes_count_; ++i) {
    format.fmt.pix_mp.plane_fmt[i].sizeimage =
        media::VideoFrame::PlaneAllocationSize(
            input_format_, i, input_allocated_size_);
    format.fmt.pix_mp.plane_fmt[i].bytesperline =
        base::checked_cast<__u32>(input_allocated_size_.width());
  }
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_FMT, &format);

  input_allocated_size_ = V4L2Device::CodedSizeFromV4L2Format(format);
  DCHECK(gfx::Rect(input_allocated_size_).Contains(
      gfx::Rect(input_visible_size_)));

  struct v4l2_crop crop;
  memset(&crop, 0, sizeof(crop));
  crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  crop.c.left = 0;
  crop.c.top = 0;
  crop.c.width = base::checked_cast<__u32>(input_visible_size_.width());
  crop.c.height = base::checked_cast<__u32>(input_visible_size_.height());
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_CROP, &crop);

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = kInputBufferCount;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  reqbufs.memory = V4L2_MEMORY_USERPTR;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_REQBUFS, &reqbufs);

  DCHECK(input_buffer_map_.empty());
  input_buffer_map_.resize(reqbufs.count);

  for (size_t i = 0; i < input_buffer_map_.size(); ++i)
    free_input_buffers_.push_back(i);

  return true;
}

bool V4L2ImageProcessor::CreateOutputBuffers() {
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(!output_streamon_);

  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  format.fmt.pix_mp.width = output_allocated_size_.width();
  format.fmt.pix_mp.height = output_allocated_size_.height();
  format.fmt.pix_mp.pixelformat = output_format_fourcc_;
  format.fmt.pix_mp.num_planes = output_planes_count_;
  for (size_t i = 0; i < output_planes_count_; ++i) {
    format.fmt.pix_mp.plane_fmt[i].sizeimage =
        media::VideoFrame::PlaneAllocationSize(
            output_format_, i, output_allocated_size_);
    format.fmt.pix_mp.plane_fmt[i].bytesperline =
        base::checked_cast<__u32>(output_allocated_size_.width());
  }
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_FMT, &format);

  gfx::Size adjusted_allocated_size =
      V4L2Device::CodedSizeFromV4L2Format(format);
  DCHECK(gfx::Rect(adjusted_allocated_size).Contains(
      gfx::Rect(output_allocated_size_)));
  output_allocated_size_ = adjusted_allocated_size;

  struct v4l2_crop crop;
  memset(&crop, 0, sizeof(crop));
  crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  crop.c.left = 0;
  crop.c.top = 0;
  crop.c.width = base::checked_cast<__u32>(output_visible_size_.width());
  crop.c.height = base::checked_cast<__u32>(output_visible_size_.height());
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_CROP, &crop);

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = kOutputBufferCount;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_REQBUFS, &reqbufs);

  DCHECK(output_buffer_map_.empty());
  output_buffer_map_.resize(reqbufs.count);
  for (size_t i = 0; i < output_buffer_map_.size(); ++i) {
    OutputRecord& output_record = output_buffer_map_[i];
    output_record.fds.resize(output_planes_count_);
    for (size_t j = 0; j < output_planes_count_; ++j) {
      struct v4l2_exportbuffer expbuf;
      memset(&expbuf, 0, sizeof(expbuf));
      expbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
      expbuf.index = i;
      expbuf.plane = j;
      expbuf.flags = O_CLOEXEC;
      IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_EXPBUF, &expbuf);
      output_record.fds[j] = expbuf.fd;
    }
    free_output_buffers_.push_back(i);
  }

  return true;
}

void V4L2ImageProcessor::DestroyInputBuffers() {
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(!input_streamon_);

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  reqbufs.memory = V4L2_MEMORY_USERPTR;
  IOCTL_OR_LOG_ERROR(VIDIOC_REQBUFS, &reqbufs);

  input_buffer_map_.clear();
  free_input_buffers_.clear();
}

void V4L2ImageProcessor::DestroyOutputBuffers() {
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(!output_streamon_);

  for (size_t buf = 0; buf < output_buffer_map_.size(); ++buf) {
    OutputRecord& output_record = output_buffer_map_[buf];
    for (size_t plane = 0; plane < output_record.fds.size(); ++plane)
      close(output_record.fds[plane]);
    output_record.fds.clear();
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

void V4L2ImageProcessor::DevicePollTask(bool poll_device) {
  DCHECK_EQ(device_poll_thread_.message_loop(), base::MessageLoop::current());

  bool event_pending;
  if (!device_->Poll(poll_device, &event_pending)) {
    NOTIFY_ERROR();
    return;
  }

  // All processing should happen on ServiceDeviceTask(), since we shouldn't
  // touch encoder state from this thread.
  device_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&V4L2ImageProcessor::ServiceDeviceTask,
                 base::Unretained(this)));
}

void V4L2ImageProcessor::ServiceDeviceTask() {
  DCHECK_EQ(device_thread_.message_loop(), base::MessageLoop::current());
  // ServiceDeviceTask() should only ever be scheduled from DevicePollTask(),
  // so either:
  // * device_poll_thread_ is running normally
  // * device_poll_thread_ scheduled us, but then a DestroyTask() shut it down,
  //   in which case we should early-out.
  if (!device_poll_thread_.message_loop())
    return;

  Dequeue();
  Enqueue();

  if (!device_->ClearDevicePollInterrupt())
    return;

  bool poll_device =
      (input_buffer_queued_count_ > 0 && output_buffer_queued_count_ > 0);

  device_poll_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&V4L2ImageProcessor::DevicePollTask,
                 base::Unretained(this),
                 poll_device));

  DVLOG(2) << __func__ << ": buffer counts: INPUT["
           << input_queue_.size() << "] => DEVICE["
           << free_input_buffers_.size() << "+"
           << input_buffer_queued_count_ << "/"
           << input_buffer_map_.size() << "->"
           << free_output_buffers_.size() << "+"
           << output_buffer_queued_count_ << "/"
           << output_buffer_map_.size() << "] => CLIENT["
           << output_buffer_map_.size() - output_buffer_queued_count_ -
              free_output_buffers_.size() << "]";
}

void V4L2ImageProcessor::Enqueue() {
  DCHECK_EQ(device_thread_.message_loop(), base::MessageLoop::current());

  const int old_inputs_queued = input_buffer_queued_count_;
  while (!input_queue_.empty() && !free_input_buffers_.empty()) {
    if (!EnqueueInputRecord())
      return;
  }
  if (old_inputs_queued == 0 && input_buffer_queued_count_ != 0) {
    // We started up a previously empty queue.
    // Queue state changed; signal interrupt.
    if (!device_->SetDevicePollInterrupt())
      return;
    // VIDIOC_STREAMON if we haven't yet.
    if (!input_streamon_) {
      __u32 type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
      IOCTL_OR_ERROR_RETURN(VIDIOC_STREAMON, &type);
      input_streamon_ = true;
    }
  }

  // TODO(posciak): Fix this to be non-Exynos specific.
  // Exynos GSC is liable to race conditions if more than one output buffer is
  // simultaneously enqueued, so enqueue just one.
  if (output_buffer_queued_count_ == 0 && !free_output_buffers_.empty()) {
    const int old_outputs_queued = output_buffer_queued_count_;
    if (!EnqueueOutputRecord())
      return;
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
  DCHECK_LE(output_buffer_queued_count_, 1);
}

void V4L2ImageProcessor::Dequeue() {
  DCHECK_EQ(device_thread_.message_loop(), base::MessageLoop::current());

  // Dequeue completed input (VIDEO_OUTPUT) buffers,
  // and recycle to the free list.
  struct v4l2_buffer dqbuf;
  struct v4l2_plane planes[VIDEO_MAX_PLANES];
  while (input_buffer_queued_count_ > 0) {
    DCHECK(input_streamon_);
    memset(&dqbuf, 0, sizeof(dqbuf));
    memset(&planes, 0, sizeof(planes));
    dqbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    dqbuf.memory = V4L2_MEMORY_USERPTR;
    dqbuf.m.planes = planes;
    dqbuf.length = input_planes_count_;
    if (device_->Ioctl(VIDIOC_DQBUF, &dqbuf) != 0) {
      if (errno == EAGAIN) {
        // EAGAIN if we're just out of buffers to dequeue.
        break;
      }
      DPLOG(ERROR) << "ioctl() failed: VIDIOC_DQBUF";
      NOTIFY_ERROR();
      return;
    }
    InputRecord& input_record = input_buffer_map_[dqbuf.index];
    DCHECK(input_record.at_device);
    input_record.at_device = false;
    input_record.frame = NULL;
    free_input_buffers_.push_back(dqbuf.index);
    input_buffer_queued_count_--;
  }

  // Dequeue completed output (VIDEO_CAPTURE) buffers, recycle to the free list.
  // Return the finished buffer to the client via the job ready callback.
  while (output_buffer_queued_count_ > 0) {
    DCHECK(output_streamon_);
    memset(&dqbuf, 0, sizeof(dqbuf));
    memset(&planes, 0, sizeof(planes));
    dqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    dqbuf.memory = V4L2_MEMORY_DMABUF;
    dqbuf.m.planes = planes;
    dqbuf.length = output_planes_count_;
    if (device_->Ioctl(VIDIOC_DQBUF, &dqbuf) != 0) {
      if (errno == EAGAIN) {
        // EAGAIN if we're just out of buffers to dequeue.
        break;
      }
      DPLOG(ERROR) << "ioctl() failed: VIDIOC_DQBUF";
      NOTIFY_ERROR();
      return;
    }
    OutputRecord& output_record = output_buffer_map_[dqbuf.index];
    DCHECK(output_record.at_device);
    output_record.at_device = false;
    output_record.at_client = true;
    output_buffer_queued_count_--;

    // Jobs are always processed in FIFO order.
    DCHECK(!running_jobs_.empty());
    linked_ptr<JobRecord> job_record = running_jobs_.front();
    running_jobs_.pop();

    scoped_refptr<media::VideoFrame> output_frame =
        media::VideoFrame::WrapExternalDmabufs(
            output_format_,
            output_allocated_size_,
            gfx::Rect(output_visible_size_),
            output_visible_size_,
            output_record.fds,
            job_record->frame->timestamp(),
            media::BindToCurrentLoop(
                base::Bind(&V4L2ImageProcessor::ReuseOutputBuffer,
                           device_weak_factory_.GetWeakPtr(),
                           dqbuf.index)));

    DVLOG(3) << "Processing finished, returning frame, ts="
             << output_frame->timestamp().InMilliseconds();

    child_message_loop_proxy_->PostTask(
        FROM_HERE, base::Bind(job_record->ready_cb, output_frame));
  }
}

void V4L2ImageProcessor::ReuseOutputBuffer(int index) {
  DVLOG(3) << "Reusing output buffer, index=" << index;
  DCHECK_EQ(device_thread_.message_loop(), base::MessageLoop::current());

  OutputRecord& output_record = output_buffer_map_[index];
  DCHECK(output_record.at_client);
  DCHECK(!output_record.at_device);
  output_record.at_client = false;
  free_output_buffers_.push_back(index);

  Enqueue();
}

bool V4L2ImageProcessor::EnqueueInputRecord() {
  DCHECK(!input_queue_.empty());
  DCHECK(!free_input_buffers_.empty());

  // Enqueue an input (VIDEO_OUTPUT) buffer for an input video frame.
  linked_ptr<JobRecord> job_record = input_queue_.front();
  input_queue_.pop();
  const int index = free_input_buffers_.back();
  InputRecord& input_record = input_buffer_map_[index];
  DCHECK(!input_record.at_device);
  input_record.frame = job_record->frame;
  struct v4l2_buffer qbuf;
  struct v4l2_plane qbuf_planes[VIDEO_MAX_PLANES];
  memset(&qbuf, 0, sizeof(qbuf));
  memset(qbuf_planes, 0, sizeof(qbuf_planes));
  qbuf.index = index;
  qbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  qbuf.memory = V4L2_MEMORY_USERPTR;
  qbuf.m.planes = qbuf_planes;
  qbuf.length = input_planes_count_;
  for (size_t i = 0; i < input_planes_count_; ++i) {
    qbuf.m.planes[i].bytesused = media::VideoFrame::PlaneAllocationSize(
        input_record.frame->format(), i, input_allocated_size_);
    qbuf.m.planes[i].length = qbuf.m.planes[i].bytesused;
    qbuf.m.planes[i].m.userptr =
        reinterpret_cast<unsigned long>(input_record.frame->data(i));
  }
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QBUF, &qbuf);
  input_record.at_device = true;
  running_jobs_.push(job_record);
  free_input_buffers_.pop_back();
  input_buffer_queued_count_++;

  DVLOG(3) << __func__ << ": enqueued frame ts="
           << job_record->frame->timestamp().InMilliseconds() << " to device.";

  return true;
}

bool V4L2ImageProcessor::EnqueueOutputRecord() {
  DCHECK(!free_output_buffers_.empty());

  // Enqueue an output (VIDEO_CAPTURE) buffer.
  const int index = free_output_buffers_.back();
  OutputRecord& output_record = output_buffer_map_[index];
  DCHECK(!output_record.at_device);
  struct v4l2_buffer qbuf;
  struct v4l2_plane qbuf_planes[VIDEO_MAX_PLANES];
  memset(&qbuf, 0, sizeof(qbuf));
  memset(qbuf_planes, 0, sizeof(qbuf_planes));
  qbuf.index = index;
  qbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  qbuf.memory = V4L2_MEMORY_MMAP;
  qbuf.m.planes = qbuf_planes;
  qbuf.length = output_planes_count_;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QBUF, &qbuf);
  output_record.at_device = true;
  free_output_buffers_.pop_back();
  output_buffer_queued_count_++;
  return true;
}

bool V4L2ImageProcessor::StartDevicePoll() {
  DVLOG(3) << __func__ << ": starting device poll";
  DCHECK_EQ(device_thread_.message_loop(), base::MessageLoop::current());
  DCHECK(!device_poll_thread_.IsRunning());

  // Start up the device poll thread and schedule its first DevicePollTask().
  if (!device_poll_thread_.Start()) {
    DLOG(ERROR) << "StartDevicePoll(): Device thread failed to start";
    NOTIFY_ERROR();
    return false;
  }
  // Enqueue a poll task with no devices to poll on - will wait only for the
  // poll interrupt
  device_poll_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(
          &V4L2ImageProcessor::DevicePollTask, base::Unretained(this), false));

  return true;
}

bool V4L2ImageProcessor::StopDevicePoll() {
  DVLOG(3) << __func__ << ": stopping device poll";
  if (device_thread_.IsRunning())
    DCHECK_EQ(device_thread_.message_loop(), base::MessageLoop::current());

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
  while (!input_queue_.empty())
    input_queue_.pop();

  while (!running_jobs_.empty())
    running_jobs_.pop();

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
    if (!output_record.at_client)
      free_output_buffers_.push_back(i);
  }
  output_buffer_queued_count_ = 0;

  return true;
}

}  // namespace content
