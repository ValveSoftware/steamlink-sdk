// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RTC_VIDEO_DECODER_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_RTC_VIDEO_DECODER_FACTORY_H_

#include "base/message_loop/message_loop_proxy.h"
#include "base/threading/thread.h"
#include "content/common/content_export.h"
#include "third_party/libjingle/source/talk/media/webrtc/webrtcvideodecoderfactory.h"
#include "third_party/webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"

namespace webrtc {
class VideoDecoder;
}  // namespace webrtc

namespace media {
class GpuVideoAcceleratorFactories;
}  // namespace media

namespace content {

// TODO(wuchengli): add unittest.
class CONTENT_EXPORT RTCVideoDecoderFactory
    : NON_EXPORTED_BASE(public cricket::WebRtcVideoDecoderFactory) {
 public:
  explicit RTCVideoDecoderFactory(
      const scoped_refptr<media::GpuVideoAcceleratorFactories>& gpu_factories);
  virtual ~RTCVideoDecoderFactory();

  // Runs on Chrome_libJingle_WorkerThread. The child thread is blocked while
  // this runs.
  virtual webrtc::VideoDecoder* CreateVideoDecoder(webrtc::VideoCodecType type)
      OVERRIDE;

  // Runs on Chrome_libJingle_WorkerThread. The child thread is blocked while
  // this runs.
  virtual void DestroyVideoDecoder(webrtc::VideoDecoder* decoder) OVERRIDE;

 private:
  scoped_refptr<media::GpuVideoAcceleratorFactories> gpu_factories_;

  DISALLOW_COPY_AND_ASSIGN(RTCVideoDecoderFactory);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RTC_VIDEO_DECODER_FACTORY_H_
