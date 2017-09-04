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
#include "base/trace_event/trace_event.h"
#include "content/child/child_process.h"
#include "content/public/child/child_thread.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/service_manager/public/cpp/interface_provider.h"

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

// Information about a video capture client of ours.
struct VideoCaptureImpl::ClientInfo {
  ClientInfo() = default;

  ClientInfo(const ClientInfo& other) = default;

  ~ClientInfo() = default;

  media::VideoCaptureParams params;

  VideoCaptureStateUpdateCB state_update_cb;

  VideoCaptureDeliverFrameCB deliver_frame_cb;
};

VideoCaptureImpl::VideoCaptureImpl(media::VideoCaptureSessionId session_id)
    : device_id_(session_id),
      session_id_(session_id),
      video_capture_host_for_testing_(nullptr),
      observer_binding_(this),
      state_(VIDEO_CAPTURE_STATE_STOPPED),
      weak_factory_(this) {
  io_thread_checker_.DetachFromThread();

  if (ChildThread::Get()) {  // This will be null in unit tests.
    mojom::VideoCaptureHostPtr temp_video_capture_host;
    ChildThread::Get()->GetRemoteInterfaces()->GetInterface(
        mojo::GetProxy(&temp_video_capture_host));
    video_capture_host_info_ = temp_video_capture_host.PassInterface();
  }
}

VideoCaptureImpl::~VideoCaptureImpl() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  if (state_ == VIDEO_CAPTURE_STATE_STARTED && GetVideoCaptureHost())
    GetVideoCaptureHost()->Stop(device_id_);
}

void VideoCaptureImpl::SuspendCapture(bool suspend) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  if (suspend)
    GetVideoCaptureHost()->Pause(device_id_);
  else
    GetVideoCaptureHost()->Resume(device_id_, session_id_, params_);
}

void VideoCaptureImpl::StartCapture(
    int client_id,
    const media::VideoCaptureParams& params,
    const VideoCaptureStateUpdateCB& state_update_cb,
    const VideoCaptureDeliverFrameCB& deliver_frame_cb) {
  DVLOG(1) << __func__ << " |device_id_| = " << device_id_;
  DCHECK(io_thread_checker_.CalledOnValidThread());
  ClientInfo client_info;
  client_info.params = params;
  client_info.state_update_cb = state_update_cb;
  client_info.deliver_frame_cb = deliver_frame_cb;

  if (state_ == VIDEO_CAPTURE_STATE_ERROR) {
    state_update_cb.Run(VIDEO_CAPTURE_STATE_ERROR);
  } else if (clients_pending_on_restart_.count(client_id) ||
             clients_.count(client_id)) {
    DLOG(FATAL) << __func__ << " This client has already started.";
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
      DVLOG(1) << __func__ << " Got new resolution while stopping: "
               << params.requested_format.frame_size.ToString();
    } else {
      clients_[client_id] = client_info;
      if (state_ == VIDEO_CAPTURE_STATE_STARTED)
        return;
      params_ = params;
      params_.requested_format.frame_rate =
          std::min(params_.requested_format.frame_rate,
                   static_cast<float>(media::limits::kMaxFramesPerSecond));

      DVLOG(1) << "StartCapture: starting with first resolution "
               << params_.requested_format.frame_size.ToString();
      StartCaptureInternal();
    }
  }
}

void VideoCaptureImpl::StopCapture(int client_id) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  // A client ID can be in only one client list.
  // If this ID is in any client list, we can just remove it from
  // that client list and don't have to run the other following RemoveClient().
  if (!RemoveClient(client_id, &clients_pending_on_restart_)) {
    RemoveClient(client_id, &clients_);
  }

  if (!clients_.empty())
    return;
  DVLOG(1) << "StopCapture: No more client, stopping ...";
  StopDevice();
  client_buffers_.clear();
  weak_factory_.InvalidateWeakPtrs();
}

void VideoCaptureImpl::RequestRefreshFrame() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  GetVideoCaptureHost()->RequestRefreshFrame(device_id_);
}

void VideoCaptureImpl::GetDeviceSupportedFormats(
    const VideoCaptureDeviceFormatsCB& callback) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  GetVideoCaptureHost()->GetDeviceSupportedFormats(
      device_id_, session_id_,
      base::Bind(&VideoCaptureImpl::OnDeviceSupportedFormats,
                 weak_factory_.GetWeakPtr(), callback));
}

void VideoCaptureImpl::GetDeviceFormatsInUse(
    const VideoCaptureDeviceFormatsCB& callback) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  GetVideoCaptureHost()->GetDeviceFormatsInUse(
      device_id_, session_id_,
      base::Bind(&VideoCaptureImpl::OnDeviceFormatsInUse,
                 weak_factory_.GetWeakPtr(), callback));
}

void VideoCaptureImpl::OnStateChanged(mojom::VideoCaptureState state) {
  DVLOG(1) << __func__ << " state: " << state;
  DCHECK(io_thread_checker_.CalledOnValidThread());

  switch (state) {
    case mojom::VideoCaptureState::STARTED:
      // Capture has started in the browser process. Since we have already
      // told all clients that we have started there's nothing to do.
      break;
    case mojom::VideoCaptureState::STOPPED:
      state_ = VIDEO_CAPTURE_STATE_STOPPED;
      client_buffers_.clear();
      weak_factory_.InvalidateWeakPtrs();
      if (!clients_.empty() || !clients_pending_on_restart_.empty())
        RestartCapture();
      break;
    case mojom::VideoCaptureState::PAUSED:
      for (const auto& client : clients_)
        client.second.state_update_cb.Run(VIDEO_CAPTURE_STATE_PAUSED);
      break;
    case mojom::VideoCaptureState::RESUMED:
      for (const auto& client : clients_)
        client.second.state_update_cb.Run(VIDEO_CAPTURE_STATE_RESUMED);
      break;
    case mojom::VideoCaptureState::FAILED:
      for (const auto& client : clients_)
        client.second.state_update_cb.Run(VIDEO_CAPTURE_STATE_ERROR);
      clients_.clear();
      state_ = VIDEO_CAPTURE_STATE_ERROR;
      break;
    case mojom::VideoCaptureState::ENDED:
      // We'll only notify the client that the stream has stopped.
      for (const auto& client : clients_)
        client.second.state_update_cb.Run(VIDEO_CAPTURE_STATE_STOPPED);
      clients_.clear();
      state_ = VIDEO_CAPTURE_STATE_ENDED;
      break;
  }
}

void VideoCaptureImpl::OnBufferCreated(int32_t buffer_id,
                                       mojo::ScopedSharedBufferHandle handle) {
  DVLOG(1) << __func__ << " buffer_id: " << buffer_id;
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(handle.is_valid());

  if (state_ != VIDEO_CAPTURE_STATE_STARTED)
    return;

  base::SharedMemoryHandle memory_handle;
  size_t memory_size = 0;
  bool read_only_flag = false;

  const MojoResult result = mojo::UnwrapSharedMemoryHandle(
      std::move(handle), &memory_handle, &memory_size, &read_only_flag);
  DCHECK_EQ(MOJO_RESULT_OK, result);
  DCHECK_GT(memory_size, 0u);

  std::unique_ptr<base::SharedMemory> shm(
      new base::SharedMemory(memory_handle, true /* read_only */));
  if (!shm->Map(memory_size)) {
    DLOG(ERROR) << "OnBufferCreated: Map failed.";
    return;
  }
  const bool inserted =
      client_buffers_
          .insert(std::make_pair(buffer_id,
                                 new ClientBuffer(std::move(shm), memory_size)))
          .second;
  DCHECK(inserted);
}

void VideoCaptureImpl::OnBufferReady(int32_t buffer_id,
                                     mojom::VideoFrameInfoPtr info) {
  DVLOG(1) << __func__ << " buffer_id: " << buffer_id;
  DCHECK(io_thread_checker_.CalledOnValidThread());

  bool consume_buffer = state_ == VIDEO_CAPTURE_STATE_STARTED;
  if ((info->pixel_format != media::PIXEL_FORMAT_I420 &&
       info->pixel_format != media::PIXEL_FORMAT_Y16) ||
      info->storage_type != media::PIXEL_STORAGE_CPU) {
    consume_buffer = false;
    LOG(DFATAL) << "Wrong pixel format or storage, got pixel format:"
                << VideoPixelFormatToString(info->pixel_format)
                << ", storage:" << info->storage_type;
  }
  if (!consume_buffer) {
    GetVideoCaptureHost()->ReleaseBuffer(device_id_, buffer_id,
                                         gpu::SyncToken(), -1.0);
    return;
  }

  base::TimeTicks reference_time;
  media::VideoFrameMetadata frame_metadata;
  frame_metadata.MergeInternalValuesFrom(info->metadata);
  const bool success = frame_metadata.GetTimeTicks(
      media::VideoFrameMetadata::REFERENCE_TIME, &reference_time);
  DCHECK(success);

  if (first_frame_ref_time_.is_null())
    first_frame_ref_time_ = reference_time;

  // If the timestamp is not prepared, we use reference time to make a rough
  // estimate. e.g. ThreadSafeCaptureOracle::DidCaptureFrame().
  // TODO(miu): Fix upstream capturers to always set timestamp and reference
  // time. See http://crbug/618407/ for tracking.
  if (info->timestamp.is_zero())
    info->timestamp = reference_time - first_frame_ref_time_;

  // TODO(qiangchen): Change the metric name to "reference_time" and
  // "timestamp", so that we have consistent naming everywhere.
  // Used by chrome/browser/extension/api/cast_streaming/performance_test.cc
  TRACE_EVENT_INSTANT2("cast_perf_test", "OnBufferReceived",
                       TRACE_EVENT_SCOPE_THREAD, "timestamp",
                       (reference_time - base::TimeTicks()).InMicroseconds(),
                       "time_delta", info->timestamp.InMicroseconds());

  const auto& iter = client_buffers_.find(buffer_id);
  DCHECK(iter != client_buffers_.end());
  const scoped_refptr<ClientBuffer> buffer = iter->second;
  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::WrapExternalSharedMemory(
          static_cast<media::VideoPixelFormat>(info->pixel_format),
          info->coded_size, info->visible_rect, info->visible_rect.size(),
          reinterpret_cast<uint8_t*>(buffer->buffer()->memory()),
          buffer->buffer_size(), buffer->buffer()->handle(),
          0 /* shared_memory_offset */, info->timestamp);
  if (!frame) {
    GetVideoCaptureHost()->ReleaseBuffer(device_id_, buffer_id,
                                         gpu::SyncToken(), -1.0);
    return;
  }

  BufferFinishedCallback buffer_finished_callback = media::BindToCurrentLoop(
      base::Bind(&VideoCaptureImpl::OnClientBufferFinished,
                 weak_factory_.GetWeakPtr(), buffer_id, buffer));
  std::unique_ptr<gpu::SyncToken> release_sync_token(new gpu::SyncToken);
  frame->AddDestructionObserver(
      base::Bind(&VideoCaptureImpl::DidFinishConsumingFrame, frame->metadata(),
                 base::Passed(&release_sync_token), buffer_finished_callback));

  frame->metadata()->MergeInternalValuesFrom(info->metadata);

  // TODO(qiangchen): Dive into the full code path to let frame metadata hold
  // reference time rather than using an extra parameter.
  for (const auto& client : clients_)
    client.second.deliver_frame_cb.Run(frame, reference_time);
}

void VideoCaptureImpl::OnBufferDestroyed(int32_t buffer_id) {
  DCHECK(io_thread_checker_.CalledOnValidThread());

  const auto& cb_iter = client_buffers_.find(buffer_id);
  if (cb_iter != client_buffers_.end()) {
    DCHECK(!cb_iter->second.get() || cb_iter->second->HasOneRef())
        << "Instructed to delete buffer we are still using.";
    client_buffers_.erase(cb_iter);
  }
}

void VideoCaptureImpl::OnClientBufferFinished(
    int buffer_id,
    const scoped_refptr<ClientBuffer>& /* ignored_buffer */,
    const gpu::SyncToken& release_sync_token,
    double consumer_resource_utilization) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  GetVideoCaptureHost()->ReleaseBuffer(
      device_id_, buffer_id, release_sync_token, consumer_resource_utilization);
}

void VideoCaptureImpl::StopDevice() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  if (state_ != VIDEO_CAPTURE_STATE_STARTED)
    return;
  state_ = VIDEO_CAPTURE_STATE_STOPPING;
  GetVideoCaptureHost()->Stop(device_id_);
  params_.requested_format.frame_size.SetSize(0, 0);
}

void VideoCaptureImpl::RestartCapture() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
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
  DVLOG(1) << __func__ << " " << params_.requested_format.frame_size.ToString();
  StartCaptureInternal();
}

void VideoCaptureImpl::StartCaptureInternal() {
  DCHECK(io_thread_checker_.CalledOnValidThread());

  GetVideoCaptureHost()->Start(device_id_, session_id_, params_,
                               observer_binding_.CreateInterfacePtrAndBind());
  state_ = VIDEO_CAPTURE_STATE_STARTED;
}

void VideoCaptureImpl::OnDeviceSupportedFormats(
    const VideoCaptureDeviceFormatsCB& callback,
    const media::VideoCaptureFormats& supported_formats) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  callback.Run(supported_formats);
}

void VideoCaptureImpl::OnDeviceFormatsInUse(
    const VideoCaptureDeviceFormatsCB& callback,
    const media::VideoCaptureFormats& formats_in_use) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  callback.Run(formats_in_use);
}

bool VideoCaptureImpl::RemoveClient(int client_id, ClientInfoMap* clients) {
  DCHECK(io_thread_checker_.CalledOnValidThread());

  const ClientInfoMap::iterator it = clients->find(client_id);
  if (it == clients->end())
    return false;

  it->second.state_update_cb.Run(VIDEO_CAPTURE_STATE_STOPPED);
  clients->erase(it);
  return true;
}

mojom::VideoCaptureHost* VideoCaptureImpl::GetVideoCaptureHost() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  if (video_capture_host_for_testing_)
    return video_capture_host_for_testing_;

  if (!video_capture_host_.get())
    video_capture_host_.Bind(std::move(video_capture_host_info_));
  return video_capture_host_.get();
};

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
