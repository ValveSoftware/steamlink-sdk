// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_REMOTING_RENDERER_FACTORY_H_
#define MEDIA_REMOTING_REMOTING_RENDERER_FACTORY_H_

#include "media/base/renderer_factory.h"
#include "media/remoting/remoting_renderer_controller.h"

namespace media {

// Create renderer for local playback or remoting according to info from
// |remoting_renderer_controller|.
class RemotingRendererFactory : public RendererFactory {
 public:
  RemotingRendererFactory(
      std::unique_ptr<RendererFactory> default_renderer_factory,
      std::unique_ptr<RemotingRendererController> remoting_renderer_controller);
  ~RemotingRendererFactory() override;

  std::unique_ptr<Renderer> CreateRenderer(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      AudioRendererSink* audio_renderer_sink,
      VideoRendererSink* video_renderer_sink,
      const RequestSurfaceCB& request_surface_cb) override;

 private:
  const std::unique_ptr<RendererFactory> default_renderer_factory_;
  const std::unique_ptr<RemotingRendererController>
      remoting_renderer_controller_;

  DISALLOW_COPY_AND_ASSIGN(RemotingRendererFactory);
};

}  // namespace media

#endif  // MEDIA_REMOTING_REMOTING_RENDERER_FACTORY_H_
