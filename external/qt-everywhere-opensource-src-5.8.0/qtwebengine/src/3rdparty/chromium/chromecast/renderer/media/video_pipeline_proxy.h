// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_MEDIA_VIDEO_PIPELINE_PROXY_H_
#define CHROMECAST_RENDERER_MEDIA_VIDEO_PIPELINE_PROXY_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/base/pipeline_status.h"

namespace base {
class SingleThreadTaskRunner;
class SharedMemory;
}

namespace media {
class VideoDecoderConfig;
}

namespace chromecast {
namespace media {
class AvStreamerProxy;
class CodedFrameProvider;
class MediaChannelProxy;
struct VideoPipelineClient;
class VideoPipelineProxyInternal;

class VideoPipelineProxy {
 public:
  VideoPipelineProxy(scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
                     scoped_refptr<MediaChannelProxy> media_channel_proxy);
  ~VideoPipelineProxy();

  void Initialize(const std::vector<::media::VideoDecoderConfig>& configs,
                  std::unique_ptr<CodedFrameProvider> frame_provider,
                  const ::media::PipelineStatusCB& status_cb);
  void StartFeeding();
  void Flush(const base::Closure& done_cb);
  void Stop();

  void SetClient(const VideoPipelineClient& video_client);

 private:
  base::ThreadChecker thread_checker_;

  void OnAvPipeCreated(const std::vector<::media::VideoDecoderConfig>& configs,
                       const ::media::PipelineStatusCB& status_cb,
                       std::unique_ptr<base::SharedMemory> shared_memory);
  void OnPipeWrite();
  void OnPipeRead();

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // |proxy_| main goal is to convert function calls to IPC messages.
  std::unique_ptr<VideoPipelineProxyInternal> proxy_;

  std::unique_ptr<AvStreamerProxy> video_streamer_;

  base::WeakPtr<VideoPipelineProxy> weak_this_;
  base::WeakPtrFactory<VideoPipelineProxy> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoPipelineProxy);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_MEDIA_VIDEO_PIPELINE_PROXY_H_
