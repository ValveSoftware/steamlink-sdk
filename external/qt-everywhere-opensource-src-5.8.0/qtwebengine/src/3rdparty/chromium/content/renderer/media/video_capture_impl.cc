// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Notes about usage of this object by VideoCaptureImplManager.
//
// VideoCaptureImplManager access this object by using a Unretained()
// binding and tasks on the IO thread. It is then important that
// VideoCaptureImpl never post task to itself. All operations must be
// synchronous.

#include "content/renderer/media/video_capture_impl.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/child/child_process.h"
#include "content/common/media/video_capture_messages.h"
#include "gpu/ipc/client/gpu_memory_buffer_impl.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"

namespace content {

// A holder of a memory-backed buffer and accessors to it.
class VideoCaptureImpl::ClientBuffer
    : public base::RefCountedThreadSafe<ClientBuffer> {
 public:
  ClientBuffer(std::unique_ptr<base::SharedMemory> buffer, size_t buffer_size)
      : buffer_(std::move(buffer)), buffer_size_(buffer_size) {}

  base::SharedMemory* buffer() const { return buffer_.get(); }
  size_t buffer_size() const { return buffer_size_; }

 private:
  friend class base::RefCountedThreadSafe<ClientBuffer>;

  virtual ~ClientBuffer() {}

  const std::unique_ptr<base::SharedMemory> buffer_;
  const size_t buffer_size_;

  DISALLOW_COPY_AND_ASSIGN(ClientBuffer);
};

// A holder of a GpuMemoryBuffer-backed buffer, Map()ed on ctor and Unmap()ed on
// dtor. Creates and owns GpuMemoryBuffer instances.
class VideoCaptureImpl::ClientBuffer2
    : public base::RefCountedThreadSafe<ClientBuffer2> {
 public:
  ClientBuffer2(
      const std::vector<gfx::GpuMemoryBufferHandle>& client_handles,
      const gfx::Size& size)
      : handles_(client_handles),
        size_(size) {
    const media::VideoPixelFormat format = media::PIXEL_FORMAT_I420;
    DCHECK_EQ(handles_.size(), media::VideoFrame::NumPlanes(format));
    for (size_t i = 0; i < handles_.size(); ++i) {
      const size_t width = media::VideoFrame::Columns(i, format, size_.width());
      const size_t height = media::VideoFrame::Rows(i, format, size_.height());
      buffers_.push_back(gpu::GpuMemoryBufferImpl::CreateFromHandle(
          handles_[i], gfx::Size(width, height), gfx::BufferFormat::R_8,
          gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
          base::Bind(&ClientBuffer2::DestroyGpuMemoryBuffer,
                     base::Unretained(this))));
      bool rv = buffers_[i]->Map();
      DCHECK(rv);
      data_[i] = reinterpret_cast<uint8_t*>(buffers_[i]->memory(0u));
      strides_[i] = width;
    }
  }

  uint8_t* data(int plane) const { return data_[plane]; }
  int32_t stride(int plane) const { return strides_[plane]; }
  std::vector<gfx::GpuMemoryBufferHandle> gpu_memory_buffer_handles() {
    return handles_;
  }

 private:
  friend class base::RefCountedThreadSafe<ClientBuffer2>;

  virtual ~ClientBuffer2() {
    for (auto& buffer : buffers_)
      buffer->Unmap();
  }

  void DestroyGpuMemoryBuffer(const gpu::SyncToken& sync_token) {}

  const std::vector<gfx::GpuMemoryBufferHandle> handles_;
  const gfx::Size size_;
  ScopedVector<gfx::GpuMemoryBuffer> buffers_;
  uint8_t* data_[media::VideoFrame::kMaxPlanes];
  int32_t strides_[media::VideoFrame::kMaxPlanes];

  DISALLOW_COPY_AND_ASSIGN(ClientBuffer2);
};

VideoCaptureImpl::ClientInfo::ClientInfo() {}
VideoCaptureImpl::ClientInfo::ClientInfo(const ClientInfo& other) = default;
VideoCaptureImpl::ClientInfo::~ClientInfo() {}

VideoCaptureImpl::VideoCaptureImpl(
    const media::VideoCaptureSessionId session_id,
    VideoCaptureMessageFilter* filter)
    : message_filter_(filter),
      device_id_(0),
      session_id_(session_id),
      suspended_(false),
      state_(VIDEO_CAPTURE_STATE_STOPPED),
      weak_factory_(this) {
  DCHECK(filter);
}

VideoCaptureImpl::~VideoCaptureImpl() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
}

void VideoCaptureImpl::Init() {
  // For creating callbacks in unittest, this class may be constructed from a
  // different thread than the IO thread, e.g. wherever unittest runs on.
  // Therefore, this function should define the thread ownership.
#if DCHECK_IS_ON()
  io_task_runner_ = base::ThreadTaskRunnerHandle::Get();
#endif
  message_filter_->AddDelegate(this);
}

void VideoCaptureImpl::DeInit() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (state_ == VIDEO_CAPTURE_STATE_STARTED)
    Send(new VideoCaptureHostMsg_Stop(device_id_));
  message_filter_->RemoveDelegate(this);
}

void VideoCaptureImpl::SuspendCapture(bool suspend) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  Send(suspend ? static_cast<IPC::Message*>(
                     new VideoCaptureHostMsg_Pause(device_id_))
               : static_cast<IPC::Message*>(new VideoCaptureHostMsg_Resume(
                     device_id_, session_id_, params_)));
}

void VideoCaptureImpl::StartCapture(
    int client_id,
    const media::VideoCaptureParams& params,
    const VideoCaptureStateUpdateCB& state_update_cb,
    const VideoCaptureDeliverFrameCB& deliver_frame_cb) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  ClientInfo client_info;
  client_info.params = params;
  client_info.state_update_cb = state_update_cb;
  client_info.deliver_frame_cb = deliver_frame_cb;

  if (state_ == VIDEO_CAPTURE_STATE_ERROR) {
    state_update_cb.Run(VIDEO_CAPTURE_STATE_ERROR);
  } else if (clients_pending_on_filter_.count(client_id) ||
             clients_pending_on_restart_.count(client_id) ||
             clients_.count(client_id)) {
    LOG(FATAL) << "This client has already started.";
  } else if (!device_id_) {
    clients_pending_on_filter_[client_id] = client_info;
  } else {
    // Note: |state_| might not be started at this point. But we tell
    // client that we have started.
    state_update_cb.Run(VIDEO_CAPTURE_STATE_STARTED);
    if (state_ == VIDEO_CAPTURE_STATE_STARTED) {
      clients_[client_id] = client_info;
      // TODO(sheu): Allowing resolution change will require that all
      // outstanding clients of a capture session support resolution change.
      DCHECK_EQ(params_.resolution_change_policy,
                params.resolution_change_policy);
    } else if (state_ == VIDEO_CAPTURE_STATE_STOPPING) {
      clients_pending_on_restart_[client_id] = client_info;
      DVLOG(1) << "StartCapture: Got new resolution "
               << params.requested_format.frame_size.ToString()
               << " during stopping.";
    } else {
      clients_[client_id] = client_info;
      if (state_ == VIDEO_CAPTURE_STATE_STARTED)
        return;
      params_ = params;
      if (params_.requested_format.frame_rate >
          media::limits::kMaxFramesPerSecond) {
        params_.requested_format.frame_rate =
            media::limits::kMaxFramesPerSecond;
      }
      DVLOG(1) << "StartCapture: starting with first resolution "
               << params_.requested_format.frame_size.ToString();
      StartCaptureInternal();
    }
  }
}

void VideoCaptureImpl::StopCapture(int client_id) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  // A client ID can be in only one client list.
  // If this ID is in any client list, we can just remove it from
  // that client list and don't have to run the other following RemoveClient().
  if (!RemoveClient(client_id, &clients_pending_on_filter_)) {
    if (!RemoveClient(client_id, &clients_pending_on_restart_)) {
      RemoveClient(client_id, &clients_);
    }
  }

  if (clients_.empty()) {
    DVLOG(1) << "StopCapture: No more client, stopping ...";
    StopDevice();
    client_buffers_.clear();
    client_buffer2s_.clear();
    weak_factory_.InvalidateWeakPtrs();
  }
}

void VideoCaptureImpl::RequestRefreshFrame() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  Send(new VideoCaptureHostMsg_RequestRefreshFrame(device_id_));
}

void VideoCaptureImpl::GetDeviceSupportedFormats(
    const VideoCaptureDeviceFormatsCB& callback) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  device_formats_cb_queue_.push_back(callback);
  if (device_formats_cb_queue_.size() == 1)
    Send(new VideoCaptureHostMsg_GetDeviceSupportedFormats(device_id_,
                                                           session_id_));
}

void VideoCaptureImpl::GetDeviceFormatsInUse(
    const VideoCaptureDeviceFormatsCB& callback) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  device_formats_in_use_cb_queue_.push_back(callback);
  if (device_formats_in_use_cb_queue_.size() == 1)
    Send(
        new VideoCaptureHostMsg_GetDeviceFormatsInUse(device_id_, session_id_));
}

void VideoCaptureImpl::OnBufferCreated(base::SharedMemoryHandle handle,
                                       int length,
                                       int buffer_id) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  // In case client calls StopCapture before the arrival of created buffer,
  // just close this buffer and return.
  if (state_ != VIDEO_CAPTURE_STATE_STARTED) {
    base::SharedMemory::CloseHandle(handle);
    return;
  }

  std::unique_ptr<base::SharedMemory> shm(
      new base::SharedMemory(handle, false));
  if (!shm->Map(length)) {
    DLOG(ERROR) << "OnBufferCreated: Map failed.";
    return;
  }
  const bool inserted =
      client_buffers_.insert(std::make_pair(
                                 buffer_id,
                                 new ClientBuffer(std::move(shm), length)))
          .second;
  DCHECK(inserted);
}

void VideoCaptureImpl::OnBufferCreated2(
    const std::vector<gfx::GpuMemoryBufferHandle>& handles,
    const gfx::Size& size,
    int buffer_id) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  // In case client calls StopCapture before the arrival of created buffer,
  // just close this buffer and return.
  if (state_ != VIDEO_CAPTURE_STATE_STARTED)
    return;

  const bool inserted =
      client_buffer2s_.insert(std::make_pair(buffer_id,
                                             new ClientBuffer2(handles, size)))
          .second;
  DCHECK(inserted);
}

void VideoCaptureImpl::OnBufferDestroyed(int buffer_id) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  const auto& cb_iter = client_buffers_.find(buffer_id);
  if (cb_iter != client_buffers_.end()) {
    DCHECK(!cb_iter->second.get() || cb_iter->second->HasOneRef())
        << "Instructed to delete buffer we are still using.";
    client_buffers_.erase(cb_iter);
  } else {
    const auto& cb2_iter = client_buffer2s_.find(buffer_id);
    if (cb2_iter != client_buffer2s_.end()) {
      DCHECK(!cb2_iter->second.get() || cb2_iter->second->HasOneRef())
          << "Instructed to delete buffer we are still using.";
      client_buffer2s_.erase(cb2_iter);
    }
  }
}

void VideoCaptureImpl::OnBufferReceived(
    int buffer_id,
    base::TimeDelta timestamp,
    const base::DictionaryValue& metadata,
    media::VideoPixelFormat pixel_format,
    media::VideoFrame::StorageType storage_type,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (state_ != VIDEO_CAPTURE_STATE_STARTED || suspended_ ||
      pixel_format != media::PIXEL_FORMAT_I420 ||
      (storage_type != media::VideoFrame::STORAGE_SHMEM &&
       storage_type != media::VideoFrame::STORAGE_GPU_MEMORY_BUFFERS)) {
    // Crash in debug builds since the host should not have provided a buffer
    // with an unsupported pixel format or storage type.
    DCHECK_EQ(media::PIXEL_FORMAT_I420, pixel_format);
    DCHECK(storage_type == media::VideoFrame::STORAGE_SHMEM ||
           storage_type == media::VideoFrame::STORAGE_GPU_MEMORY_BUFFERS);
    Send(new VideoCaptureHostMsg_BufferReady(device_id_, buffer_id,
                                             gpu::SyncToken(), -1.0));
    return;
  }

  base::TimeTicks reference_time;
  media::VideoFrameMetadata frame_metadata;
  frame_metadata.MergeInternalValuesFrom(metadata);
  const bool success = frame_metadata.GetTimeTicks(
      media::VideoFrameMetadata::REFERENCE_TIME, &reference_time);
  DCHECK(success);

  if (first_frame_ref_time_.is_null())
    first_frame_ref_time_ = reference_time;

  // If the timestamp is not prepared, we use reference time to make a rough
  // estimate. e.g. ThreadSafeCaptureOracle::DidCaptureFrame().
  // TODO(miu): Fix upstream capturers to always set timestamp and reference
  // time. See http://crbug/618407/ for tracking.
  if (timestamp.is_zero())
    timestamp = reference_time - first_frame_ref_time_;

  // TODO(qiangchen): Change the metric name to "reference_time" and
  // "timestamp", so that we have consistent naming everywhere.
  // Used by chrome/browser/extension/api/cast_streaming/performance_test.cc
  TRACE_EVENT_INSTANT2("cast_perf_test", "OnBufferReceived",
                       TRACE_EVENT_SCOPE_THREAD, "timestamp",
                       (reference_time - base::TimeTicks()).InMicroseconds(),
                       "time_delta", timestamp.InMicroseconds());

  scoped_refptr<media::VideoFrame> frame;
  BufferFinishedCallback buffer_finished_callback;
  std::unique_ptr<gpu::SyncToken> release_sync_token(new gpu::SyncToken);
  switch (storage_type) {
    case media::VideoFrame::STORAGE_GPU_MEMORY_BUFFERS: {
      const auto& iter = client_buffer2s_.find(buffer_id);
      DCHECK(iter != client_buffer2s_.end());
      scoped_refptr<ClientBuffer2> buffer = iter->second;
      const auto& handles = buffer->gpu_memory_buffer_handles();
      frame = media::VideoFrame::WrapExternalYuvGpuMemoryBuffers(
          media::PIXEL_FORMAT_I420, coded_size, gfx::Rect(coded_size),
          coded_size, buffer->stride(media::VideoFrame::kYPlane),
          buffer->stride(media::VideoFrame::kUPlane),
          buffer->stride(media::VideoFrame::kVPlane),
          buffer->data(media::VideoFrame::kYPlane),
          buffer->data(media::VideoFrame::kUPlane),
          buffer->data(media::VideoFrame::kVPlane),
          handles[media::VideoFrame::kYPlane],
          handles[media::VideoFrame::kUPlane],
          handles[media::VideoFrame::kVPlane], timestamp);
      buffer_finished_callback = media::BindToCurrentLoop(
          base::Bind(&VideoCaptureImpl::OnClientBufferFinished2,
                     weak_factory_.GetWeakPtr(), buffer_id, buffer));
      break;
    }
    case media::VideoFrame::STORAGE_SHMEM: {
      const auto& iter = client_buffers_.find(buffer_id);
      DCHECK(iter != client_buffers_.end());
      const scoped_refptr<ClientBuffer> buffer = iter->second;
      frame = media::VideoFrame::WrapExternalSharedMemory(
          pixel_format, coded_size, visible_rect,
          gfx::Size(visible_rect.width(), visible_rect.height()),
          reinterpret_cast<uint8_t*>(buffer->buffer()->memory()),
          buffer->buffer_size(), buffer->buffer()->handle(),
          0 /* shared_memory_offset */, timestamp);
      buffer_finished_callback = media::BindToCurrentLoop(
          base::Bind(&VideoCaptureImpl::OnClientBufferFinished,
                     weak_factory_.GetWeakPtr(), buffer_id, buffer));
      break;
    }
    default:
      NOTREACHED();
      break;
  }
  if (!frame) {
    Send(new VideoCaptureHostMsg_BufferReady(device_id_, buffer_id,
                                             gpu::SyncToken(), -1.0));
    return;
  }

  frame->AddDestructionObserver(
      base::Bind(&VideoCaptureImpl::DidFinishConsumingFrame, frame->metadata(),
                 base::Passed(&release_sync_token), buffer_finished_callback));

  frame->metadata()->MergeInternalValuesFrom(metadata);

  // TODO(qiangchen): Dive into the full code path to let frame metadata hold
  // reference time rather than using an extra parameter.
  for (const auto& client : clients_)
    client.second.deliver_frame_cb.Run(frame, reference_time);
}

void VideoCaptureImpl::OnClientBufferFinished(
    int buffer_id,
    const scoped_refptr<ClientBuffer>& /* ignored_buffer */,
    const gpu::SyncToken& release_sync_token,
    double consumer_resource_utilization) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  Send(new VideoCaptureHostMsg_BufferReady(device_id_, buffer_id,
                                           release_sync_token,
                                           consumer_resource_utilization));
}
void VideoCaptureImpl::OnClientBufferFinished2(
    int buffer_id,
    const scoped_refptr<ClientBuffer2>& gpu_memory_buffer /* ignored_buffer */,
    const gpu::SyncToken& release_sync_token,
    double consumer_resource_utilization) {
  OnClientBufferFinished(buffer_id, scoped_refptr<ClientBuffer>(),
                         release_sync_token, consumer_resource_utilization);
}

void VideoCaptureImpl::OnStateChanged(VideoCaptureState state) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  switch (state) {
    case VIDEO_CAPTURE_STATE_STARTED:
      // Camera has started in the browser process. Since we have already
      // told all clients that we have started there's nothing to do.
      break;
    case VIDEO_CAPTURE_STATE_STOPPED:
      state_ = VIDEO_CAPTURE_STATE_STOPPED;
      DVLOG(1) << "OnStateChanged: stopped!, device_id = " << device_id_;
      client_buffers_.clear();
      client_buffer2s_.clear();
      weak_factory_.InvalidateWeakPtrs();
      if (!clients_.empty() || !clients_pending_on_restart_.empty())
        RestartCapture();
      break;
    case VIDEO_CAPTURE_STATE_PAUSED:
      for (const auto& client : clients_)
        client.second.state_update_cb.Run(VIDEO_CAPTURE_STATE_PAUSED);
      break;
    case VIDEO_CAPTURE_STATE_ERROR:
      DVLOG(1) << "OnStateChanged: error!, device_id = " << device_id_;
      for (const auto& client : clients_)
        client.second.state_update_cb.Run(VIDEO_CAPTURE_STATE_ERROR);
      clients_.clear();
      state_ = VIDEO_CAPTURE_STATE_ERROR;
      break;
    case VIDEO_CAPTURE_STATE_ENDED:
      DVLOG(1) << "OnStateChanged: ended!, device_id = " << device_id_;
      for (const auto& client : clients_) {
        // We'll only notify the client that the stream has stopped.
        client.second.state_update_cb.Run(VIDEO_CAPTURE_STATE_STOPPED);
      }
      clients_.clear();
      state_ = VIDEO_CAPTURE_STATE_ENDED;
      break;
    default:
      break;
  }
}

void VideoCaptureImpl::OnDeviceSupportedFormatsEnumerated(
    const media::VideoCaptureFormats& supported_formats) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  for (size_t i = 0; i < device_formats_cb_queue_.size(); ++i)
    device_formats_cb_queue_[i].Run(supported_formats);
  device_formats_cb_queue_.clear();
}

void VideoCaptureImpl::OnDeviceFormatsInUseReceived(
    const media::VideoCaptureFormats& formats_in_use) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  for (size_t i = 0; i < device_formats_in_use_cb_queue_.size(); ++i)
    device_formats_in_use_cb_queue_[i].Run(formats_in_use);
  device_formats_in_use_cb_queue_.clear();
}

void VideoCaptureImpl::OnDelegateAdded(int32_t device_id) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << "OnDelegateAdded: device_id " << device_id;

  device_id_ = device_id;
  ClientInfoMap::iterator it = clients_pending_on_filter_.begin();
  while (it != clients_pending_on_filter_.end()) {
    const int client_id = it->first;
    const ClientInfo client_info = it->second;
    clients_pending_on_filter_.erase(it++);
    StartCapture(client_id, client_info.params, client_info.state_update_cb,
                 client_info.deliver_frame_cb);
  }
}

void VideoCaptureImpl::StopDevice() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (state_ == VIDEO_CAPTURE_STATE_STARTED) {
    state_ = VIDEO_CAPTURE_STATE_STOPPING;
    Send(new VideoCaptureHostMsg_Stop(device_id_));
    params_.requested_format.frame_size.SetSize(0, 0);
  }
}

void VideoCaptureImpl::RestartCapture() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, VIDEO_CAPTURE_STATE_STOPPED);

  int width = 0;
  int height = 0;
  clients_.insert(clients_pending_on_restart_.begin(),
                  clients_pending_on_restart_.end());
  clients_pending_on_restart_.clear();
  for (const auto& client : clients_) {
    width = std::max(width,
                     client.second.params.requested_format.frame_size.width());
    height = std::max(
        height, client.second.params.requested_format.frame_size.height());
  }
  params_.requested_format.frame_size.SetSize(width, height);
  DVLOG(1) << "RestartCapture, "
           << params_.requested_format.frame_size.ToString();
  StartCaptureInternal();
}

void VideoCaptureImpl::StartCaptureInternal() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DCHECK(device_id_);

  Send(new VideoCaptureHostMsg_Start(device_id_, session_id_, params_));
  state_ = VIDEO_CAPTURE_STATE_STARTED;
}

void VideoCaptureImpl::Send(IPC::Message* message) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  message_filter_->Send(message);
}

bool VideoCaptureImpl::RemoveClient(int client_id, ClientInfoMap* clients) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  bool found = false;

  const ClientInfoMap::iterator it = clients->find(client_id);
  if (it != clients->end()) {
    it->second.state_update_cb.Run(VIDEO_CAPTURE_STATE_STOPPED);
    clients->erase(it);
    found = true;
  }
  return found;
}

// static
void VideoCaptureImpl::DidFinishConsumingFrame(
    const media::VideoFrameMetadata* metadata,
    std::unique_ptr<gpu::SyncToken> release_sync_token,
    const BufferFinishedCallback& callback_to_io_thread) {
  // Note: This function may be called on any thread by the VideoFrame
  // destructor.  |metadata| is still valid for read-access at this point.
  double consumer_resource_utilization = -1.0;
  if (!metadata->GetDouble(media::VideoFrameMetadata::RESOURCE_UTILIZATION,
                           &consumer_resource_utilization)) {
    consumer_resource_utilization = -1.0;
  }

  callback_to_io_thread.Run(*release_sync_token, consumer_resource_utilization);
}

}  // namespace content
