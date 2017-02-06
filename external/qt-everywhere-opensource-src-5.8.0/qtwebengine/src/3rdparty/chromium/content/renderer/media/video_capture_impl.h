// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_H_
#define CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_H_

#include <stdint.h>

#include <list>
#include <map>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "content/common/media/video_capture.h"
#include "content/public/renderer/media_stream_video_sink.h"
#include "content/renderer/media/video_capture_message_filter.h"
#include "media/base/video_capture_types.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace media {
class VideoFrame;
}  // namespace media

namespace content {

// VideoCaptureImpl represents a capture device in renderer process. It provides
// interfaces for clients to Start/Stop capture. It also communicates to clients
// when buffer is ready, state of capture device is changed.

// VideoCaptureImpl is also a delegate of VideoCaptureMessageFilter which relays
// operation of a capture device to the browser process and receives responses
// from browser process.
//
// VideoCaptureImpl is an IO thread only object. See the comments in
// video_capture_impl_manager.cc for the lifetime of this object.
// All methods must be called on the IO thread.
//
// This is an internal class used by VideoCaptureImplManager only. Do not access
// this directly.
class CONTENT_EXPORT VideoCaptureImpl
    : public VideoCaptureMessageFilter::Delegate {
 public:
  ~VideoCaptureImpl() override;

  VideoCaptureImpl(media::VideoCaptureSessionId session_id,
                   VideoCaptureMessageFilter* filter);

  // Start listening to IPC messages.
  void Init();

  // Stop listening to IPC messages.
  void DeInit();

  // Stop/resume delivering video frames to clients, based on flag |suspend|.
  void SuspendCapture(bool suspend);

  // Start capturing using the provided parameters.
  // |client_id| must be unique to this object in the render process. It is
  // used later to stop receiving video frames.
  // |state_update_cb| will be called when state changes.
  // |deliver_frame_cb| will be called when a frame is ready.
  void StartCapture(int client_id,
                    const media::VideoCaptureParams& params,
                    const VideoCaptureStateUpdateCB& state_update_cb,
                    const VideoCaptureDeliverFrameCB& deliver_frame_cb);

  // Stop capturing. |client_id| is the identifier used to call StartCapture.
  void StopCapture(int client_id);

  // Requests that the video capturer send a frame "soon" (e.g., to resolve
  // picture loss or quality issues).
  void RequestRefreshFrame();

  // Get capturing formats supported by this device.
  // |callback| will be invoked with the results.
  void GetDeviceSupportedFormats(const VideoCaptureDeviceFormatsCB& callback);

  // Get capturing formats currently in use by this device.
  // |callback| will be invoked with the results.
  void GetDeviceFormatsInUse(const VideoCaptureDeviceFormatsCB& callback);

  media::VideoCaptureSessionId session_id() const { return session_id_; }

 private:
  friend class VideoCaptureImplTest;
  friend class MockVideoCaptureImpl;

  // Carries a shared memory for transferring video frames from browser to
  // renderer.
  class ClientBuffer;
  class ClientBuffer2;

  // Contains information for a video capture client. Including parameters
  // for capturing and callbacks to the client.
  struct ClientInfo {
    ClientInfo();
    ClientInfo(const ClientInfo& other);
    ~ClientInfo();
    media::VideoCaptureParams params;
    VideoCaptureStateUpdateCB state_update_cb;
    VideoCaptureDeliverFrameCB deliver_frame_cb;
  };
  typedef std::map<int, ClientInfo> ClientInfoMap;

  typedef base::Callback<void(const gpu::SyncToken& sync_token,
                              double consumer_resource_utilization)>
      BufferFinishedCallback;

  // VideoCaptureMessageFilter::Delegate interface.
  void OnBufferCreated(base::SharedMemoryHandle handle,
                       int length,
                       int buffer_id) override;
  void OnBufferCreated2(const std::vector<gfx::GpuMemoryBufferHandle>& handles,
                        const gfx::Size& size,
                        int buffer_id) override;
  void OnBufferDestroyed(int buffer_id) override;
  void OnBufferReceived(int buffer_id,
                        base::TimeDelta timestamp,
                        const base::DictionaryValue& metadata,
                        media::VideoPixelFormat pixel_format,
                        media::VideoFrame::StorageType storage_type,
                        const gfx::Size& coded_size,
                        const gfx::Rect& visible_rect) override;
  void OnStateChanged(VideoCaptureState state) override;
  void OnDeviceSupportedFormatsEnumerated(
      const media::VideoCaptureFormats& supported_formats) override;
  void OnDeviceFormatsInUseReceived(
      const media::VideoCaptureFormats& formats_in_use) override;
  void OnDelegateAdded(int32_t device_id) override;

  // Sends an IPC message to browser process when all clients are done with the
  // buffer.
  void OnClientBufferFinished(int buffer_id,
                              const scoped_refptr<ClientBuffer>& buffer,
                              const gpu::SyncToken& release_sync_token,
                              double consumer_resource_utilization);
  void OnClientBufferFinished2(int buffer_id,
                               const scoped_refptr<ClientBuffer2>& buffer,
                               const gpu::SyncToken& release_sync_token,
                               double consumer_resource_utilization);

  void StopDevice();
  void RestartCapture();
  void StartCaptureInternal();

  virtual void Send(IPC::Message* message);

  // Helpers.
  bool RemoveClient(int client_id, ClientInfoMap* clients);

  // Called (by an unknown thread) when all consumers are done with a VideoFrame
  // and its ref-count has gone to zero.  This helper function grabs the
  // RESOURCE_UTILIZATION value from the |metadata| and then runs the given
  // callback, to trampoline back to the IO thread with the values.
  static void DidFinishConsumingFrame(
      const media::VideoFrameMetadata* metadata,
      std::unique_ptr<gpu::SyncToken> release_sync_token,
      const BufferFinishedCallback& callback_to_io_thread);

  const scoped_refptr<VideoCaptureMessageFilter> message_filter_;
  int device_id_;
  const int session_id_;

  // Vector of callbacks to be notified of device format enumerations, used only
  // on IO Thread.
  std::vector<VideoCaptureDeviceFormatsCB> device_formats_cb_queue_;
  // Vector of callbacks to be notified of a device's in use capture format(s),
  // used only on IO Thread.
  std::vector<VideoCaptureDeviceFormatsCB> device_formats_in_use_cb_queue_;

  // Buffers available for sending to the client.
  typedef std::map<int32_t, scoped_refptr<ClientBuffer>> ClientBufferMap;
  ClientBufferMap client_buffers_;
  typedef std::map<int32_t, scoped_refptr<ClientBuffer2>> ClientBuffer2Map;
  ClientBuffer2Map client_buffer2s_;

  ClientInfoMap clients_;
  ClientInfoMap clients_pending_on_filter_;
  ClientInfoMap clients_pending_on_restart_;

  // Member params_ represents the video format requested by the
  // client to this class via StartCapture().
  media::VideoCaptureParams params_;

  // The device's first captured frame referecne time sent from browser process
  // side.
  base::TimeTicks first_frame_ref_time_;

  bool suspended_;
  VideoCaptureState state_;

  // IO message loop reference for checking correct class operation.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // WeakPtrFactory pointing back to |this| object, for use with
  // media::VideoFrames constructed in OnBufferReceived() from buffers cached
  // in |client_buffers_|.
  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<VideoCaptureImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_H_
