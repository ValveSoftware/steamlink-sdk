// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_V4L2_VIDEO_PROCESSOR_H_
#define CONTENT_COMMON_GPU_MEDIA_V4L2_VIDEO_PROCESSOR_H_

#include <queue>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "content/common/content_export.h"
#include "content/common/gpu/media/v4l2_video_device.h"
#include "media/base/video_frame.h"

namespace content {

// Handles image processing accelerators that expose a V4L2 memory-to-memory
// interface. The threading model of this class is the same as for other V4L2
// hardware accelerators (see V4L2VideoDecodeAccelerator) for more details.
class CONTENT_EXPORT V4L2ImageProcessor {
 public:
  explicit V4L2ImageProcessor(scoped_ptr<V4L2Device> device);
  virtual ~V4L2ImageProcessor();

  // Initializes the processor to convert from |input_format| to |output_format|
  // and/or scale from |input_visible_size| to |output_visible_size|.
  // Request the output buffers to be of at least |output_allocated_size|.
  // Provided |error_cb| will be called if an error occurs.
  // Return true if the requested configuration is supported.
  bool Initialize(media::VideoFrame::Format input_format,
                  media::VideoFrame::Format output_format,
                  gfx::Size input_visible_size,
                  gfx::Size output_visible_size,
                  gfx::Size output_allocated_size,
                  const base::Closure& error_cb);

  // Returns allocated size required by the processor to be fed with.
  gfx::Size input_allocated_size() { return input_allocated_size_; }

  // Callback to be used to return a processed image to the client. The client
  // should drop references to |frame| once it's done with it.
  typedef base::Callback<void(const scoped_refptr<media::VideoFrame>& frame)>
      FrameReadyCB;

  // Called by client to process |frame|. The resulting processed frame will
  // be returned via |cb|. The processor will drop all its references to |frame|
  // after it finishes accessing it.
  void Process(const scoped_refptr<media::VideoFrame>& frame,
               const FrameReadyCB& cb);

  // Stop all processing and clean up.
  void Destroy();

 private:
  // Record for input buffers.
  struct InputRecord {
    InputRecord();
    scoped_refptr<media::VideoFrame> frame;
    bool at_device;
  };

  // Record for output buffers.
  struct OutputRecord {
    OutputRecord();
    bool at_device;
    bool at_client;
    std::vector<int> fds;
  };

  // Job record. Jobs are processed in a FIFO order. This is separate from
  // InputRecord, because an InputRecord may be returned before we dequeue
  // the corresponding output buffer. It can't always be associated with
  // an OutputRecord immediately either, because at the time of submission we
  // may not have one available (and don't need one to submit input to the
  // device).
  struct JobRecord {
    JobRecord();
    scoped_refptr<media::VideoFrame> frame;
    FrameReadyCB ready_cb;
  };

  enum {
    // Arbitrarily tuned.
    kInputBufferCount = 2,
    kOutputBufferCount = 2,
  };

  void ReuseOutputBuffer(int index);

  void Enqueue();
  void Dequeue();
  bool EnqueueInputRecord();
  bool EnqueueOutputRecord();
  bool CreateInputBuffers();
  bool CreateOutputBuffers();
  void DestroyInputBuffers();
  void DestroyOutputBuffers();

  void NotifyError();
  void DestroyTask();

  void ProcessTask(scoped_ptr<JobRecord> job_record);
  void ServiceDeviceTask();

  // Attempt to start/stop device_poll_thread_.
  bool StartDevicePoll();
  bool StopDevicePoll();

  // Ran on device_poll_thread_ to wait for device events.
  void DevicePollTask(bool poll_device);

  // Size and format-related members remain constant after initialization.
  // The visible/allocated sizes of the input frame.
  gfx::Size input_visible_size_;
  gfx::Size input_allocated_size_;

  // The visible/allocated sizes of the destination frame.
  gfx::Size output_visible_size_;
  gfx::Size output_allocated_size_;

  media::VideoFrame::Format input_format_;
  media::VideoFrame::Format output_format_;
  uint32 input_format_fourcc_;
  uint32 output_format_fourcc_;

  size_t input_planes_count_;
  size_t output_planes_count_;

  // Our original calling message loop for the child thread.
  const scoped_refptr<base::MessageLoopProxy> child_message_loop_proxy_;

  // V4L2 device in use.
  scoped_ptr<V4L2Device> device_;

  // Thread to communicate with the device on.
  base::Thread device_thread_;
  // Thread used to poll the V4L2 for events only.
  base::Thread device_poll_thread_;

  // All the below members are to be accessed from device_thread_ only
  // (if it's running).
  std::queue<linked_ptr<JobRecord> > input_queue_;
  std::queue<linked_ptr<JobRecord> > running_jobs_;

  // Input queue state.
  bool input_streamon_;
  // Number of input buffers enqueued to the device.
  int input_buffer_queued_count_;
  // Input buffers ready to use; LIFO since we don't care about ordering.
  std::vector<int> free_input_buffers_;
  // Mapping of int index to an input buffer record.
  std::vector<InputRecord> input_buffer_map_;

  // Output queue state.
  bool output_streamon_;
  // Number of output buffers enqueued to the device.
  int output_buffer_queued_count_;
  // Output buffers ready to use; LIFO since we don't care about ordering.
  std::vector<int> free_output_buffers_;
  // Mapping of int index to an output buffer record.
  std::vector<OutputRecord> output_buffer_map_;

  // Error callback to the client.
  base::Closure error_cb_;

  // Weak factory for producing weak pointers on the device_thread_
  base::WeakPtrFactory<V4L2ImageProcessor> device_weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(V4L2ImageProcessor);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_V4L2_VIDEO_PROCESSOR_H_
