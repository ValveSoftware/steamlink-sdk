// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_VIDEO_RESOURCE_UPDATER_H_
#define CC_RESOURCES_VIDEO_RESOURCE_UPDATER_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/base/cc_export.h"
#include "cc/resources/release_callback_impl.h"
#include "cc/resources/resource_format.h"
#include "cc/resources/texture_mailbox.h"
#include "ui/gfx/geometry/size.h"

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
    RGBA_PREMULTIPLIED_RESOURCE,
    RGBA_RESOURCE,
    STREAM_TEXTURE_RESOURCE,

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
  std::vector<ReleaseCallbackImpl> release_callbacks;
  bool read_lock_fences_enabled;

  // TODO(danakj): Remove these too.
  std::vector<unsigned> software_resources;
  ReleaseCallbackImpl software_release_callback;

  // Used by hardware textures which do not return values in the 0-1 range.
  // After a lookup, subtract offset and multiply by multiplier.
  float offset;
  float multiplier;

  VideoFrameExternalResources();
  VideoFrameExternalResources(const VideoFrameExternalResources& other);
  ~VideoFrameExternalResources();
};

// VideoResourceUpdater is used by the video system to produce frame content as
// resources consumable by the compositor.
class CC_EXPORT VideoResourceUpdater
    : public base::SupportsWeakPtr<VideoResourceUpdater> {
 public:
  VideoResourceUpdater(ContextProvider* context_provider,
                       ResourceProvider* resource_provider);
  ~VideoResourceUpdater();

  VideoFrameExternalResources CreateExternalResourcesFromVideoFrame(
      scoped_refptr<media::VideoFrame> video_frame);

 private:
  class PlaneResource {
   public:
    PlaneResource(unsigned resource_id,
                  const gfx::Size& resource_size,
                  ResourceFormat resource_format,
                  gpu::Mailbox mailbox);
    PlaneResource(const PlaneResource& other);

    // Returns true if this resource matches the unique identifiers of another
    // VideoFrame resource.
    bool Matches(int unique_frame_id, size_t plane_index);

    // Sets the unique identifiers for this resource, may only be called when
    // there is a single reference to the resource (i.e. |ref_count_| == 1).
    void SetUniqueId(int unique_frame_id, size_t plane_index);

    // Accessors for resource identifiers provided at construction time.
    unsigned resource_id() const { return resource_id_; }
    const gfx::Size& resource_size() const { return resource_size_; }
    ResourceFormat resource_format() const { return resource_format_; }
    const gpu::Mailbox& mailbox() const { return mailbox_; }

    // Various methods for managing references. See |ref_count_| for details.
    void add_ref() { ++ref_count_; }
    void remove_ref() { --ref_count_; }
    void clear_refs() { ref_count_ = 0; }
    bool has_refs() const { return ref_count_ != 0; }

   private:
    // The balance between the number of times this resource has been returned
    // from CreateForSoftwarePlanes vs released in RecycleResource.
    int ref_count_ = 0;

    // These two members are used for identifying the data stored in this
    // resource; they uniquely identify a media::VideoFrame plane.
    int unique_frame_id_ = 0;
    size_t plane_index_ = 0u;
    // Indicates if the above two members have been set or not.
    bool has_unique_frame_id_and_plane_index_ = false;

    const unsigned resource_id_;
    const gfx::Size resource_size_;
    const ResourceFormat resource_format_;
    const gpu::Mailbox mailbox_;
  };

  // This needs to be a container where iterators can be erased without
  // invalidating other iterators.
  typedef std::list<PlaneResource> ResourceList;
  ResourceList::iterator AllocateResource(const gfx::Size& plane_size,
                                          ResourceFormat format,
                                          bool has_mailbox,
                                          bool immutable_hint);
  void DeleteResource(ResourceList::iterator resource_it);
  void CopyPlaneTexture(media::VideoFrame* video_frame,
                        const gpu::MailboxHolder& mailbox_holder,
                        VideoFrameExternalResources* external_resources);
  VideoFrameExternalResources CreateForHardwarePlanes(
      scoped_refptr<media::VideoFrame> video_frame);
  VideoFrameExternalResources CreateForSoftwarePlanes(
      scoped_refptr<media::VideoFrame> video_frame);

  static void RecycleResource(base::WeakPtr<VideoResourceUpdater> updater,
                              unsigned resource_id,
                              const gpu::SyncToken& sync_token,
                              bool lost_resource,
                              BlockingTaskRunner* main_thread_task_runner);
  static void ReturnTexture(base::WeakPtr<VideoResourceUpdater> updater,
                            const scoped_refptr<media::VideoFrame>& video_frame,
                            const gpu::SyncToken& sync_token,
                            bool lost_resource,
                            BlockingTaskRunner* main_thread_task_runner);

  ContextProvider* context_provider_;
  ResourceProvider* resource_provider_;
  std::unique_ptr<media::SkCanvasVideoRenderer> video_renderer_;
  std::vector<uint8_t> upload_pixels_;

  // Recycle resources so that we can reduce the number of allocations and
  // data transfers.
  ResourceList all_resources_;

  DISALLOW_COPY_AND_ASSIGN(VideoResourceUpdater);
};

}  // namespace cc

#endif  // CC_RESOURCES_VIDEO_RESOURCE_UPDATER_H_
