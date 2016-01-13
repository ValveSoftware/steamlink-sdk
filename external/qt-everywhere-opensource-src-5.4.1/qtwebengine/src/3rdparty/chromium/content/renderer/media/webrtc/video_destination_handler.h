// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_VIDEO_DESTINATION_HANDLER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_VIDEO_DESTINATION_HANDLER_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "media/base/video_frame_pool.h"

namespace content {

class PeerConnectionDependencyFactory;
class MediaStreamRegistryInterface;
class PPB_ImageData_Impl;

// Interface used by the effects pepper plugin to output the processed frame
// to the video track.
class CONTENT_EXPORT FrameWriterInterface {
 public:
  // The ownership of the |image_data| deosn't transfer. So the implementation
  // of this interface should make a copy of the |image_data| before return.
  virtual void PutFrame(PPB_ImageData_Impl* image_data,
                        int64 time_stamp_ns) = 0;
  virtual ~FrameWriterInterface() {}
};

// PpFrameWriter implements MediaStreamVideoSource and can therefore provide
// video frames to MediaStreamVideoTracks. It also implements
// FrameWriterInterface, which will be used by the effects pepper plugin to
// inject the processed frame.
class CONTENT_EXPORT PpFrameWriter
    : NON_EXPORTED_BASE(public MediaStreamVideoSource),
      public FrameWriterInterface,
      NON_EXPORTED_BASE(public base::SupportsWeakPtr<PpFrameWriter>) {
 public:
  PpFrameWriter();
  virtual ~PpFrameWriter();

  // FrameWriterInterface implementation.
  // This method will be called by the Pepper host from render thread.
  virtual void PutFrame(PPB_ImageData_Impl* image_data,
                        int64 time_stamp_ns) OVERRIDE;
 protected:
  // MediaStreamVideoSource implementation.
  virtual void GetCurrentSupportedFormats(
      int max_requested_width,
      int max_requested_height,
      const VideoCaptureDeviceFormatsCB& callback) OVERRIDE;
  virtual void StartSourceImpl(
      const media::VideoCaptureParams& params,
      const VideoCaptureDeliverFrameCB& frame_callback) OVERRIDE;
  virtual void StopSourceImpl() OVERRIDE;

 private:
  media::VideoFramePool frame_pool_;

  class FrameWriterDelegate;
  scoped_refptr<FrameWriterDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(PpFrameWriter);
};

// VideoDestinationHandler is a glue class between the content MediaStream and
// the effects pepper plugin host.
class CONTENT_EXPORT VideoDestinationHandler {
 public:
  // Instantiates and adds a new video track to the MediaStream specified by
  // |url|. Returns a handler for delivering frames to the new video track as
  // |frame_writer|.
  // If |registry| is NULL the global blink::WebMediaStreamRegistry will be
  // used to look up the media stream.
  // The caller of the function takes the ownership of |frame_writer|.
  // Returns true on success and false on failure.
  static bool Open(MediaStreamRegistryInterface* registry,
                   const std::string& url,
                   FrameWriterInterface** frame_writer);

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoDestinationHandler);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_VIDEO_DESTINATION_HANDLER_H_

