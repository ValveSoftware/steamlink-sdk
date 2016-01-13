// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

#ifndef CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_H_
#define CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_H_

#include <list>
#include <map>

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "content/common/media/video_capture.h"
#include "content/public/renderer/media_stream_video_sink.h"
#include "content/renderer/media/video_capture_message_filter.h"
#include "media/video/capture/video_capture_types.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace gpu {
struct MailboxHolder;
}  // namespace gpu

namespace media {
class VideoFrame;
}  // namespace media

namespace content {

class CONTENT_EXPORT VideoCaptureImpl
    : public VideoCaptureMessageFilter::Delegate {
 public:
  virtual ~VideoCaptureImpl();

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
  void StartCapture(
      int client_id,
      const media::VideoCaptureParams& params,
      const VideoCaptureStateUpdateCB& state_update_cb,
      const VideoCaptureDeliverFrameCB& deliver_frame_cb);

  // Stop capturing. |client_id| is the identifier used to call StartCapture.
  void StopCapture(int client_id);

  // Get capturing formats supported by this device.
  // |callback| will be invoked with the results.
  void GetDeviceSupportedFormats(
      const VideoCaptureDeviceFormatsCB& callback);

  // Get capturing formats currently in use by this device.
  // |callback| will be invoked with the results.
  void GetDeviceFormatsInUse(
      const VideoCaptureDeviceFormatsCB& callback);

  media::VideoCaptureSessionId session_id() const { return session_id_; }

 private:
  friend class VideoCaptureImplTest;
  friend class MockVideoCaptureImpl;

  // Carries a shared memory for transferring video frames from browser to
  // renderer.
  class ClientBuffer;

  // Contains information for a video capture client. Including parameters
  // for capturing and callbacks to the client.
  struct ClientInfo {
    ClientInfo();
    ~ClientInfo();
    media::VideoCaptureParams params;
    VideoCaptureStateUpdateCB state_update_cb;
    VideoCaptureDeliverFrameCB deliver_frame_cb;
  };
  typedef std::map<int, ClientInfo> ClientInfoMap;

  // VideoCaptureMessageFilter::Delegate interface.
  virtual void OnBufferCreated(base::SharedMemoryHandle handle,
                               int length,
                               int buffer_id) OVERRIDE;
  virtual void OnBufferDestroyed(int buffer_id) OVERRIDE;
  virtual void OnBufferReceived(int buffer_id,
                                const media::VideoCaptureFormat& format,
                                base::TimeTicks) OVERRIDE;
  virtual void OnMailboxBufferReceived(int buffer_id,
                                       const gpu::MailboxHolder& mailbox_holder,
                                       const media::VideoCaptureFormat& format,
                                       base::TimeTicks timestamp) OVERRIDE;
  virtual void OnStateChanged(VideoCaptureState state) OVERRIDE;
  virtual void OnDeviceSupportedFormatsEnumerated(
      const media::VideoCaptureFormats& supported_formats) OVERRIDE;
  virtual void OnDeviceFormatsInUseReceived(
      const media::VideoCaptureFormats& formats_in_use) OVERRIDE;
  virtual void OnDelegateAdded(int32 device_id) OVERRIDE;

  // Sends an IPC message to browser process when all clients are done with the
  // buffer.
  void OnClientBufferFinished(int buffer_id,
                              const scoped_refptr<ClientBuffer>& buffer,
                              const std::vector<uint32>& release_sync_points);

  void StopDevice();
  void RestartCapture();
  void StartCaptureInternal();

  virtual void Send(IPC::Message* message);

  // Helpers.
  bool RemoveClient(int client_id, ClientInfoMap* clients);

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
  typedef std::map<int32, scoped_refptr<ClientBuffer> > ClientBufferMap;
  ClientBufferMap client_buffers_;

  ClientInfoMap clients_;
  ClientInfoMap clients_pending_on_filter_;
  ClientInfoMap clients_pending_on_restart_;

  // Member params_ represents the video format requested by the
  // client to this class via StartCapture().
  media::VideoCaptureParams params_;

  // The device's video capture format sent from browser process side.
  media::VideoCaptureFormat last_frame_format_;

  // The device's first captured frame timestamp sent from browser process side.
  base::TimeTicks first_frame_timestamp_;

  bool suspended_;
  VideoCaptureState state_;

  // |weak_factory_| and |thread_checker_| are bound to the IO thread.
  base::ThreadChecker thread_checker_;

  // WeakPtrFactory pointing back to |this| object, for use with
  // media::VideoFrames constructed in OnBufferReceived() from buffers cached
  // in |client_buffers_|.
  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<VideoCaptureImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_H_
