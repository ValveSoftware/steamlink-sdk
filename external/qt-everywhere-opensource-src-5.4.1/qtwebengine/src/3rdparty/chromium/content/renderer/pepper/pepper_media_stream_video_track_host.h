// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_MEDIA_STREAM_VIDEO_TRACK_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_MEDIA_STREAM_VIDEO_TRACK_HOST_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "content/public/renderer/media_stream_video_sink.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "content/renderer/pepper/pepper_media_stream_track_host_base.h"
#include "media/base/video_frame.h"
#include "ppapi/c/ppb_video_frame.h"
#include "ppapi/shared_impl/media_stream_video_track_shared.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "ui/gfx/size.h"

namespace content {

class PepperMediaStreamVideoTrackHost : public PepperMediaStreamTrackHostBase,
                                        public MediaStreamVideoSink,
                                        public MediaStreamVideoSource {
 public:
  // Input mode constructor.
  // In input mode, this class passes video frames from |track| to the
  // associated pepper plugin.
  PepperMediaStreamVideoTrackHost(RendererPpapiHost* host,
                                  PP_Instance instance,
                                  PP_Resource resource,
                                  const blink::WebMediaStreamTrack& track);

  // Output mode constructor.
  // In output mode, this class passes video frames from the associated
  // pepper plugin to a newly created blink::WebMediaStreamTrack.
  PepperMediaStreamVideoTrackHost(RendererPpapiHost* host,
                                  PP_Instance instance,
                                  PP_Resource resource);

  virtual bool IsMediaStreamVideoTrackHost() OVERRIDE;

  blink::WebMediaStreamTrack track() { return track_; }

 private:

  virtual ~PepperMediaStreamVideoTrackHost();

  void InitBuffers();

  // PepperMediaStreamTrackHostBase overrides:
  virtual void OnClose() OVERRIDE;
  virtual int32_t OnHostMsgEnqueueBuffer(
      ppapi::host::HostMessageContext* context, int32_t index) OVERRIDE;

  // Sends frame with |index| to |track_|.
  int32_t SendFrameToTrack(int32_t index);

  void OnVideoFrame(const scoped_refptr<media::VideoFrame>& frame,
                    const media::VideoCaptureFormat& format,
                    const base::TimeTicks& estimated_capture_time);

  // MediaStreamVideoSource overrides:
  virtual void GetCurrentSupportedFormats(
      int max_requested_width,
      int max_requested_height,
      const VideoCaptureDeviceFormatsCB& callback) OVERRIDE;

  virtual void StartSourceImpl(
      const media::VideoCaptureParams& params,
      const VideoCaptureDeliverFrameCB& frame_callback) OVERRIDE;

  virtual void StopSourceImpl() OVERRIDE;

  // ResourceHost overrides:
  virtual void DidConnectPendingHostToResource() OVERRIDE;

  // ResourceMessageHandler overrides:
  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

  // Message handlers:
  int32_t OnHostMsgConfigure(
      ppapi::host::HostMessageContext* context,
      const ppapi::MediaStreamVideoTrackShared::Attributes& attributes);

  void InitBlinkTrack();
  void OnTrackStarted(MediaStreamSource* source, bool success);

  blink::WebMediaStreamTrack track_;

  // True if it has been added to |blink::WebMediaStreamTrack| as a sink.
  bool connected_;

  // Number of buffers.
  int32_t number_of_buffers_;

  // Size of frames which are received from MediaStreamVideoSink.
  gfx::Size source_frame_size_;

  // Plugin specified frame size.
  gfx::Size plugin_frame_size_;

  // Format of frames which are received from MediaStreamVideoSink.
  PP_VideoFrame_Format source_frame_format_;

  // Plugin specified frame format.
  PP_VideoFrame_Format plugin_frame_format_;

  // The size of frame pixels in bytes.
  uint32_t frame_data_size_;

  // TODO(ronghuawu): Remove |type_| and split PepperMediaStreamVideoTrackHost
  // into 2 classes for read and write.
  TrackType type_;
  bool output_started_;

  // Internal class used for delivering video frames on the IO-thread to
  // the MediaStreamVideoSource implementation.
  class FrameDeliverer;
  scoped_refptr<FrameDeliverer> frame_deliverer_;

  base::WeakPtrFactory<PepperMediaStreamVideoTrackHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperMediaStreamVideoTrackHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_MEDIA_STREAM_VIDEO_TRACK_HOST_H_
