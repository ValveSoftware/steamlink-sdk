// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_MEDIA_CLIENT_H_
#define MEDIA_MOJO_SERVICES_MOJO_MEDIA_CLIENT_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "media/mojo/services/media_mojo_export.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace service_manager {
namespace mojom {
class InterfaceProvider;
}
}

namespace media {

class AudioDecoder;
class AudioRendererSink;
class CdmFactory;
class MediaLog;
class RendererFactory;
class VideoDecoder;
class VideoRendererSink;

class MEDIA_MOJO_EXPORT MojoMediaClient {
 public:
  // Called before the host application is scheduled to quit.
  // The application message loop is still valid at this point, so all clean
  // up tasks requiring the message loop must be completed before returning.
  virtual ~MojoMediaClient();

  // Called exactly once before any other method.
  virtual void Initialize();

  virtual std::unique_ptr<AudioDecoder> CreateAudioDecoder(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  virtual std::unique_ptr<VideoDecoder> CreateVideoDecoder(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Returns the output sink used for rendering audio on |audio_device_id|.
  // May be null if the RendererFactory doesn't need an audio sink.
  virtual scoped_refptr<AudioRendererSink> CreateAudioRendererSink(
      const std::string& audio_device_id);

  // Returns the output sink used for rendering video.
  // May be null if the RendererFactory doesn't need a video sink.
  virtual std::unique_ptr<VideoRendererSink> CreateVideoRendererSink(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  // Returns the RendererFactory to be used by MojoRendererService.
  virtual std::unique_ptr<RendererFactory> CreateRendererFactory(
      const scoped_refptr<MediaLog>& media_log);

  // Returns the CdmFactory to be used by MojoCdmService.
  virtual std::unique_ptr<CdmFactory> CreateCdmFactory(
      service_manager::mojom::InterfaceProvider* interface_provider);

 protected:
  MojoMediaClient();
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_MEDIA_CLIENT_H_
