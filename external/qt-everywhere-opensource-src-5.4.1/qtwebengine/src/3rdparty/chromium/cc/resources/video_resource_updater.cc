// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/video_resource_updater.h"

#include "base/bind.h"
#include "cc/output/gl_renderer.h"
#include "cc/resources/resource_provider.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "media/base/video_frame.h"
#include "media/filters/skcanvas_video_renderer.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gfx/size_conversions.h"

namespace cc {

const ResourceFormat kYUVResourceFormat = LUMINANCE_8;
const ResourceFormat kRGBResourceFormat = RGBA_8888;

VideoFrameExternalResources::VideoFrameExternalResources() : type(NONE) {}

VideoFrameExternalResources::~VideoFrameExternalResources() {}

VideoResourceUpdater::VideoResourceUpdater(ContextProvider* context_provider,
                                           ResourceProvider* resource_provider)
    : context_provider_(context_provider),
      resource_provider_(resource_provider) {
}

VideoResourceUpdater::~VideoResourceUpdater() {
  while (!all_resources_.empty()) {
    resource_provider_->DeleteResource(all_resources_.back());
    all_resources_.pop_back();
  }
}

void VideoResourceUpdater::DeleteResource(unsigned resource_id) {
  resource_provider_->DeleteResource(resource_id);
  all_resources_.erase(std::remove(all_resources_.begin(),
                                   all_resources_.end(),
                                   resource_id));
}

VideoFrameExternalResources VideoResourceUpdater::
    CreateExternalResourcesFromVideoFrame(
        const scoped_refptr<media::VideoFrame>& video_frame) {
  if (!VerifyFrame(video_frame))
    return VideoFrameExternalResources();

  if (video_frame->format() == media::VideoFrame::NATIVE_TEXTURE)
    return CreateForHardwarePlanes(video_frame);
  else
    return CreateForSoftwarePlanes(video_frame);
}

bool VideoResourceUpdater::VerifyFrame(
    const scoped_refptr<media::VideoFrame>& video_frame) {
  switch (video_frame->format()) {
    // Acceptable inputs.
    case media::VideoFrame::YV12:
    case media::VideoFrame::I420:
    case media::VideoFrame::YV12A:
    case media::VideoFrame::YV16:
    case media::VideoFrame::YV12J:
    case media::VideoFrame::YV24:
    case media::VideoFrame::NATIVE_TEXTURE:
#if defined(VIDEO_HOLE)
    case media::VideoFrame::HOLE:
#endif  // defined(VIDEO_HOLE)
      return true;

    // Unacceptable inputs. ¯\(°_o)/¯
    case media::VideoFrame::UNKNOWN:
    case media::VideoFrame::NV12:
      break;
  }
  return false;
}

// For frames that we receive in software format, determine the dimensions of
// each plane in the frame.
static gfx::Size SoftwarePlaneDimension(
    const scoped_refptr<media::VideoFrame>& input_frame,
    ResourceFormat output_resource_format,
    size_t plane_index) {
  if (output_resource_format == kYUVResourceFormat) {
    return media::VideoFrame::PlaneSize(
        input_frame->format(), plane_index, input_frame->coded_size());
  }

  DCHECK_EQ(output_resource_format, kRGBResourceFormat);
  return input_frame->coded_size();
}

VideoFrameExternalResources VideoResourceUpdater::CreateForSoftwarePlanes(
    const scoped_refptr<media::VideoFrame>& video_frame) {
  media::VideoFrame::Format input_frame_format = video_frame->format();

#if defined(VIDEO_HOLE)
  if (input_frame_format == media::VideoFrame::HOLE) {
    VideoFrameExternalResources external_resources;
    external_resources.type = VideoFrameExternalResources::HOLE;
    return external_resources;
  }
#endif  // defined(VIDEO_HOLE)

  // Only YUV software video frames are supported.
  DCHECK(input_frame_format == media::VideoFrame::YV12 ||
         input_frame_format == media::VideoFrame::I420 ||
         input_frame_format == media::VideoFrame::YV12A ||
         input_frame_format == media::VideoFrame::YV12J ||
         input_frame_format == media::VideoFrame::YV16 ||
         input_frame_format == media::VideoFrame::YV24);
  if (input_frame_format != media::VideoFrame::YV12 &&
      input_frame_format != media::VideoFrame::I420 &&
      input_frame_format != media::VideoFrame::YV12A &&
      input_frame_format != media::VideoFrame::YV12J &&
      input_frame_format != media::VideoFrame::YV16 &&
      input_frame_format != media::VideoFrame::YV24)
    return VideoFrameExternalResources();

  bool software_compositor = context_provider_ == NULL;

  ResourceFormat output_resource_format = kYUVResourceFormat;
  size_t output_plane_count = media::VideoFrame::NumPlanes(input_frame_format);

  // TODO(skaslev): If we're in software compositing mode, we do the YUV -> RGB
  // conversion here. That involves an extra copy of each frame to a bitmap.
  // Obviously, this is suboptimal and should be addressed once ubercompositor
  // starts shaping up.
  if (software_compositor) {
    output_resource_format = kRGBResourceFormat;
    output_plane_count = 1;
  }

  int max_resource_size = resource_provider_->max_texture_size();
  std::vector<PlaneResource> plane_resources;
  bool allocation_success = true;

  for (size_t i = 0; i < output_plane_count; ++i) {
    gfx::Size output_plane_resource_size =
        SoftwarePlaneDimension(video_frame, output_resource_format, i);
    if (output_plane_resource_size.IsEmpty() ||
        output_plane_resource_size.width() > max_resource_size ||
        output_plane_resource_size.height() > max_resource_size) {
      allocation_success = false;
      break;
    }

    ResourceProvider::ResourceId resource_id = 0;
    gpu::Mailbox mailbox;

    // Try recycle a previously-allocated resource.
    for (size_t i = 0; i < recycled_resources_.size(); ++i) {
      bool resource_matches =
          recycled_resources_[i].resource_format == output_resource_format &&
          recycled_resources_[i].resource_size == output_plane_resource_size;
      bool not_in_use =
          !software_compositor || !resource_provider_->InUseByConsumer(
                                       recycled_resources_[i].resource_id);
      if (resource_matches && not_in_use) {
        resource_id = recycled_resources_[i].resource_id;
        mailbox = recycled_resources_[i].mailbox;
        recycled_resources_.erase(recycled_resources_.begin() + i);
        break;
      }
    }

    if (resource_id == 0) {
      // TODO(danakj): Abstract out hw/sw resource create/delete from
      // ResourceProvider and stop using ResourceProvider in this class.
      resource_id =
          resource_provider_->CreateResource(output_plane_resource_size,
                                             GL_CLAMP_TO_EDGE,
                                             ResourceProvider::TextureUsageAny,
                                             output_resource_format);

      DCHECK(mailbox.IsZero());

      if (!software_compositor) {
        DCHECK(context_provider_);

        gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();

        GLC(gl, gl->GenMailboxCHROMIUM(mailbox.name));
        ResourceProvider::ScopedWriteLockGL lock(resource_provider_,
                                                 resource_id);
        GLC(gl, gl->BindTexture(GL_TEXTURE_2D, lock.texture_id()));
        GLC(gl, gl->ProduceTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name));
        GLC(gl, gl->BindTexture(GL_TEXTURE_2D, 0));
      }

      if (resource_id)
        all_resources_.push_back(resource_id);
    }

    if (resource_id == 0) {
      allocation_success = false;
      break;
    }

    DCHECK(software_compositor || !mailbox.IsZero());
    plane_resources.push_back(PlaneResource(resource_id,
                                            output_plane_resource_size,
                                            output_resource_format,
                                            mailbox));
  }

  if (!allocation_success) {
    for (size_t i = 0; i < plane_resources.size(); ++i)
      DeleteResource(plane_resources[i].resource_id);
    return VideoFrameExternalResources();
  }

  VideoFrameExternalResources external_resources;

  if (software_compositor) {
    DCHECK_EQ(plane_resources.size(), 1u);
    DCHECK_EQ(plane_resources[0].resource_format, kRGBResourceFormat);
    DCHECK(plane_resources[0].mailbox.IsZero());

    if (!video_renderer_)
      video_renderer_.reset(new media::SkCanvasVideoRenderer);

    {
      ResourceProvider::ScopedWriteLockSoftware lock(
          resource_provider_, plane_resources[0].resource_id);
      video_renderer_->Paint(video_frame.get(),
                             lock.sk_canvas(),
                             video_frame->visible_rect(),
                             0xff);
    }

    RecycleResourceData recycle_data = {
      plane_resources[0].resource_id,
      plane_resources[0].resource_size,
      plane_resources[0].resource_format,
      gpu::Mailbox()
    };
    external_resources.software_resources.push_back(
        plane_resources[0].resource_id);
    external_resources.software_release_callback =
        base::Bind(&RecycleResource, AsWeakPtr(), recycle_data);
    external_resources.type = VideoFrameExternalResources::SOFTWARE_RESOURCE;

    return external_resources;
  }

  for (size_t i = 0; i < plane_resources.size(); ++i) {
    // Update each plane's resource id with its content.
    DCHECK_EQ(plane_resources[i].resource_format, kYUVResourceFormat);

    const uint8_t* input_plane_pixels = video_frame->data(i);

    gfx::Rect image_rect(0,
                         0,
                         video_frame->stride(i),
                         plane_resources[i].resource_size.height());
    gfx::Rect source_rect(plane_resources[i].resource_size);
    resource_provider_->SetPixels(plane_resources[i].resource_id,
                                  input_plane_pixels,
                                  image_rect,
                                  source_rect,
                                  gfx::Vector2d());

    RecycleResourceData recycle_data = {
      plane_resources[i].resource_id,
      plane_resources[i].resource_size,
      plane_resources[i].resource_format,
      plane_resources[i].mailbox
    };

    external_resources.mailboxes.push_back(
        TextureMailbox(plane_resources[i].mailbox, GL_TEXTURE_2D, 0));
    external_resources.release_callbacks.push_back(
        base::Bind(&RecycleResource, AsWeakPtr(), recycle_data));
  }

  external_resources.type = VideoFrameExternalResources::YUV_RESOURCE;
  return external_resources;
}

static void ReturnTexture(const scoped_refptr<media::VideoFrame>& frame,
                          uint32 sync_point,
                          bool lost_resource) {
  frame->AppendReleaseSyncPoint(sync_point);
}

VideoFrameExternalResources VideoResourceUpdater::CreateForHardwarePlanes(
    const scoped_refptr<media::VideoFrame>& video_frame) {
  media::VideoFrame::Format frame_format = video_frame->format();

  DCHECK_EQ(frame_format, media::VideoFrame::NATIVE_TEXTURE);
  if (frame_format != media::VideoFrame::NATIVE_TEXTURE)
      return VideoFrameExternalResources();

  if (!context_provider_)
    return VideoFrameExternalResources();

  const gpu::MailboxHolder* mailbox_holder = video_frame->mailbox_holder();
  VideoFrameExternalResources external_resources;
  switch (mailbox_holder->texture_target) {
    case GL_TEXTURE_2D:
      external_resources.type = VideoFrameExternalResources::RGB_RESOURCE;
      break;
    case GL_TEXTURE_EXTERNAL_OES:
      external_resources.type =
          VideoFrameExternalResources::STREAM_TEXTURE_RESOURCE;
      break;
    case GL_TEXTURE_RECTANGLE_ARB:
      external_resources.type = VideoFrameExternalResources::IO_SURFACE;
      break;
    default:
      NOTREACHED();
      return VideoFrameExternalResources();
  }

  external_resources.mailboxes.push_back(
      TextureMailbox(mailbox_holder->mailbox,
                     mailbox_holder->texture_target,
                     mailbox_holder->sync_point));
  external_resources.release_callbacks.push_back(
      base::Bind(&ReturnTexture, video_frame));
  return external_resources;
}

// static
void VideoResourceUpdater::RecycleResource(
    base::WeakPtr<VideoResourceUpdater> updater,
    RecycleResourceData data,
    uint32 sync_point,
    bool lost_resource) {
  if (!updater.get()) {
    // Resource was already deleted.
    return;
  }

  ContextProvider* context_provider = updater->context_provider_;
  if (context_provider && sync_point) {
    GLC(context_provider->ContextGL(),
        context_provider->ContextGL()->WaitSyncPointCHROMIUM(sync_point));
  }

  if (lost_resource) {
    updater->DeleteResource(data.resource_id);
    return;
  }

  // Drop recycled resources that are the wrong format.
  while (!updater->recycled_resources_.empty() &&
         updater->recycled_resources_.back().resource_format !=
         data.resource_format) {
    updater->DeleteResource(updater->recycled_resources_.back().resource_id);
    updater->recycled_resources_.pop_back();
  }

  PlaneResource recycled_resource(data.resource_id,
                                  data.resource_size,
                                  data.resource_format,
                                  data.mailbox);
  updater->recycled_resources_.push_back(recycled_resource);
}

}  // namespace cc
