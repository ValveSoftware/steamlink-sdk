// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/remoting_renderer_factory.h"

#include "base/logging.h"
#include "media/remoting/remote_renderer_impl.h"

namespace media {

RemotingRendererFactory::RemotingRendererFactory(
    std::unique_ptr<RendererFactory> default_renderer_factory,
    std::unique_ptr<RemotingRendererController> remoting_renderer_controller)
    : default_renderer_factory_(std::move(default_renderer_factory)),
      remoting_renderer_controller_(std::move(remoting_renderer_controller)) {}

RemotingRendererFactory::~RemotingRendererFactory() {}

std::unique_ptr<Renderer> RemotingRendererFactory::CreateRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    AudioRendererSink* audio_renderer_sink,
    VideoRendererSink* video_renderer_sink,
    const RequestSurfaceCB& request_surface_cb) {
  if (remoting_renderer_controller_ &&
      remoting_renderer_controller_->remote_rendering_started()) {
    VLOG(1) << "Create Remoting renderer.";
    return base::WrapUnique(new RemoteRendererImpl(
        media_task_runner, remoting_renderer_controller_->GetWeakPtr(),
        video_renderer_sink));
  } else {
    VLOG(1) << "Create Local playback renderer.";
    return default_renderer_factory_->CreateRenderer(
        media_task_runner, worker_task_runner, audio_renderer_sink,
        video_renderer_sink, request_surface_cb);
  }
}

}  // namespace media
