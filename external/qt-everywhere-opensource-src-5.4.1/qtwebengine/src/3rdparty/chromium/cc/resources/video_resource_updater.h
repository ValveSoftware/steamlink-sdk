// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_VIDEO_RESOURCE_UPDATER_H_
#define CC_RESOURCES_VIDEO_RESOURCE_UPDATER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/resources/release_callback.h"
#include "cc/resources/resource_format.h"
#include "cc/resources/texture_mailbox.h"
#include "ui/gfx/size.h"

namespace media {
class SkCanvasVideoRenderer;
class VideoFrame;
}

namespace cc {
class ContextProvider;
class ResourceProvider;

class CC_EXPORT VideoFrameExternalResources {
 public:
  // Specifies what type of data is contained in the mailboxes, as well as how
  // many mailboxes will be present.
  enum ResourceType {
    NONE,
    YUV_RESOURCE,
    RGB_RESOURCE,
    STREAM_TEXTURE_RESOURCE,
    IO_SURFACE,

#if defined(VIDEO_HOLE)
    // TODO(danakj): Implement this with a solid color layer instead of a video
    // frame and video layer.
    HOLE,
#endif  // defined(VIDEO_HOLE)

    // TODO(danakj): Remove this and abstract TextureMailbox into
    // "ExternalResource" that can hold a hardware or software backing.
    SOFTWARE_RESOURCE
  };

  ResourceType type;
  std::vector<TextureMailbox> mailboxes;
  std::vector<ReleaseCallback> release_callbacks;

  // TODO(danakj): Remove these too.
  std::vector<unsigned> software_resources;
  ReleaseCallback software_release_callback;

  VideoFrameExternalResources();
  ~VideoFrameExternalResources();
};

// VideoResourceUpdater is by the video system to produce frame content as
// resources consumable by the compositor.
class CC_EXPORT VideoResourceUpdater
    : public base::SupportsWeakPtr<VideoResourceUpdater> {
 public:
  explicit VideoResourceUpdater(ContextProvider* context_provider,
                                ResourceProvider* resource_provider);
  ~VideoResourceUpdater();

  VideoFrameExternalResources CreateExternalResourcesFromVideoFrame(
      const scoped_refptr<media::VideoFrame>& video_frame);

 private:
  struct PlaneResource {
    unsigned resource_id;
    gfx::Size resource_size;
    ResourceFormat resource_format;
    gpu::Mailbox mailbox;

    PlaneResource(unsigned resource_id,
                  const gfx::Size& resource_size,
                  ResourceFormat resource_format,
                  gpu::Mailbox mailbox)
        : resource_id(resource_id),
          resource_size(resource_size),
          resource_format(resource_format),
          mailbox(mailbox) {}
  };

  void DeleteResource(unsigned resource_id);
  bool VerifyFrame(const scoped_refptr<media::VideoFrame>& video_frame);
  VideoFrameExternalResources CreateForHardwarePlanes(
      const scoped_refptr<media::VideoFrame>& video_frame);
  VideoFrameExternalResources CreateForSoftwarePlanes(
      const scoped_refptr<media::VideoFrame>& video_frame);

  struct RecycleResourceData {
    unsigned resource_id;
    gfx::Size resource_size;
    ResourceFormat resource_format;
    gpu::Mailbox mailbox;
  };
  static void RecycleResource(base::WeakPtr<VideoResourceUpdater> updater,
                              RecycleResourceData data,
                              uint32 sync_point,
                              bool lost_resource);

  ContextProvider* context_provider_;
  ResourceProvider* resource_provider_;
  scoped_ptr<media::SkCanvasVideoRenderer> video_renderer_;

  std::vector<unsigned> all_resources_;
  std::vector<PlaneResource> recycled_resources_;

  DISALLOW_COPY_AND_ASSIGN(VideoResourceUpdater);
};

}  // namespace cc

#endif  // CC_RESOURCES_VIDEO_RESOURCE_UPDATER_H_
