// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accelerated_widget_mac/ca_renderer_layer_tree.h"

#include <AVFoundation/AVFoundation.h>
#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CoreVideo.h>
#include <GLES2/gl2extchromium.h>

#include "base/command_line.h"
#include "base/mac/mac_util.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/geometry/dip_util.h"

#if !defined(MAC_OS_X_VERSION_10_8) || \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_8
extern NSString* const AVLayerVideoGravityResize;
extern "C" void NSAccessibilityPostNotificationWithUserInfo(
    id object,
    NSString* notification,
    NSDictionary* user_info);
extern "C" OSStatus CMSampleBufferCreateForImageBuffer(
    CFAllocatorRef,
    CVImageBufferRef,
    Boolean dataReady,
    CMSampleBufferMakeDataReadyCallback,
    void*,
    CMVideoFormatDescriptionRef,
    const CMSampleTimingInfo*,
    CMSampleBufferRef*);
extern "C" CFArrayRef CMSampleBufferGetSampleAttachmentsArray(CMSampleBufferRef,
                                                              Boolean);
extern "C" OSStatus CMVideoFormatDescriptionCreateForImageBuffer(
    CFAllocatorRef,
    CVImageBufferRef,
    CMVideoFormatDescriptionRef*);
extern "C" CMTime CMTimeMake(int64_t, int32_t);
extern CFStringRef const kCMSampleAttachmentKey_DisplayImmediately;
extern const CMTime kCMTimeInvalid;
#endif  // MAC_OS_X_VERSION_10_8

namespace ui {

namespace {

// This will enqueue |io_surface| to be drawn by |av_layer|. This will
// retain |cv_pixel_buffer| until it is no longer being displayed.
bool AVSampleBufferDisplayLayerEnqueueCVPixelBuffer(
    AVSampleBufferDisplayLayer* av_layer,
    CVPixelBufferRef cv_pixel_buffer) {
  OSStatus os_status = noErr;

  base::ScopedCFTypeRef<CMVideoFormatDescriptionRef> video_info;
  os_status = CMVideoFormatDescriptionCreateForImageBuffer(
      nullptr, cv_pixel_buffer, video_info.InitializeInto());
  if (os_status != noErr) {
    LOG(ERROR) << "CMVideoFormatDescriptionCreateForImageBuffer failed with "
               << os_status;
    return false;
  }

  // The frame time doesn't matter because we will specify to display
  // immediately.
  CMTime frame_time = CMTimeMake(0, 1);
  CMSampleTimingInfo timing_info = {frame_time, frame_time, kCMTimeInvalid};

  base::ScopedCFTypeRef<CMSampleBufferRef> sample_buffer;
  os_status = CMSampleBufferCreateForImageBuffer(
      nullptr, cv_pixel_buffer, YES, nullptr, nullptr, video_info, &timing_info,
      sample_buffer.InitializeInto());
  if (os_status != noErr) {
    LOG(ERROR) << "CMSampleBufferCreateForImageBuffer failed with "
               << os_status;
    return false;
  }

  // Specify to display immediately via the sample buffer attachments.
  CFArrayRef attachments =
      CMSampleBufferGetSampleAttachmentsArray(sample_buffer, YES);
  if (!attachments) {
    LOG(ERROR) << "CMSampleBufferGetSampleAttachmentsArray failed";
    return false;
  }
  if (CFArrayGetCount(attachments) < 1) {
    LOG(ERROR) << "CMSampleBufferGetSampleAttachmentsArray result was empty";
    return false;
  }
  CFMutableDictionaryRef attachments_dictionary =
      reinterpret_cast<CFMutableDictionaryRef>(
          const_cast<void*>(CFArrayGetValueAtIndex(attachments, 0)));
  if (!attachments_dictionary) {
    LOG(ERROR) << "Failed to get attachments dictionary";
    return false;
  }
  CFDictionarySetValue(attachments_dictionary,
                       kCMSampleAttachmentKey_DisplayImmediately,
                       kCFBooleanTrue);

  [av_layer enqueueSampleBuffer:sample_buffer];
  return true;
}

// This will enqueue |io_surface| to be drawn by |av_layer| by wrapping
// |io_surface| in a CVPixelBuffer. This will increase the in-use count
// of and retain |io_surface| until it is no longer being displayed.
bool AVSampleBufferDisplayLayerEnqueueIOSurface(
    AVSampleBufferDisplayLayer* av_layer,
    IOSurfaceRef io_surface) {
  CVReturn cv_return = kCVReturnSuccess;

  base::ScopedCFTypeRef<CVPixelBufferRef> cv_pixel_buffer;
  cv_return = CVPixelBufferCreateWithIOSurface(
      nullptr, io_surface, nullptr, cv_pixel_buffer.InitializeInto());
  if (cv_return != kCVReturnSuccess) {
    LOG(ERROR) << "CVPixelBufferCreateWithIOSurface failed with " << cv_return;
    return false;
  }

  return AVSampleBufferDisplayLayerEnqueueCVPixelBuffer(av_layer,
                                                        cv_pixel_buffer);
}

}  // namespace

CARendererLayerTree::CARendererLayerTree() {}
CARendererLayerTree::~CARendererLayerTree() {}

bool CARendererLayerTree::ScheduleCALayer(
    bool is_clipped,
    const gfx::Rect& clip_rect,
    unsigned sorting_context_id,
    const gfx::Transform& transform,
    base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
    base::ScopedCFTypeRef<CVPixelBufferRef> cv_pixel_buffer,
    const gfx::RectF& contents_rect,
    const gfx::Rect& rect,
    unsigned background_color,
    unsigned edge_aa_mask,
    float opacity,
    unsigned filter) {
  // Excessive logging to debug white screens (crbug.com/583805).
  // TODO(ccameron): change this back to a DLOG.
  if (has_committed_) {
    LOG(ERROR) << "ScheduleCALayer called after CommitScheduledCALayers.";
    return false;
  }
  return root_layer_.AddContentLayer(is_clipped, clip_rect, sorting_context_id,
                                     transform, io_surface, cv_pixel_buffer,
                                     contents_rect, rect, background_color,
                                     edge_aa_mask, opacity, filter);
}

void CARendererLayerTree::CommitScheduledCALayers(
    CALayer* superlayer,
    std::unique_ptr<CARendererLayerTree> old_tree,
    float scale_factor) {
  TRACE_EVENT0("gpu", "CARendererLayerTree::CommitScheduledCALayers");
  RootLayer* old_root_layer = nullptr;
  if (old_tree) {
    DCHECK(old_tree->has_committed_);
    if (old_tree->scale_factor_ == scale_factor)
      old_root_layer = &old_tree->root_layer_;
  }

  root_layer_.CommitToCA(superlayer, old_root_layer, scale_factor);
  // If there are any extra CALayers in |old_tree| that were not stolen by this
  // tree, they will be removed from the CALayer tree in this deallocation.
  old_tree.reset();
  has_committed_ = true;
  scale_factor_ = scale_factor;
}

bool CARendererLayerTree::CommitFullscreenLowPowerLayer(
    AVSampleBufferDisplayLayer* fullscreen_low_power_layer) {
  DCHECK(has_committed_);
  const ContentLayer* video_layer = nullptr;
  gfx::RectF video_layer_frame_dip;
  for (const auto& clip_layer : root_layer_.clip_and_sorting_layers) {
    for (const auto& transform_layer : clip_layer.transform_layers) {
      for (const auto& content_layer : transform_layer.content_layers) {
        // Detached mode requires that no layers be on top of the video layer.
        if (video_layer)
          return false;

        // See if this is the video layer.
        if (content_layer.use_av_layer) {
          video_layer = &content_layer;
          video_layer_frame_dip = gfx::RectF(video_layer->rect);
          if (!transform_layer.transform.IsPositiveScaleOrTranslation())
            return false;
          if (content_layer.opacity != 1)
            return false;
          transform_layer.transform.TransformRect(&video_layer_frame_dip);
          video_layer_frame_dip.Scale(1 / scale_factor_);
          continue;
        }

        // If we haven't found the video layer yet, make sure everything is
        // solid black or transparent
        if (content_layer.io_surface)
          return false;
        if (content_layer.background_color != SK_ColorBLACK &&
            content_layer.background_color != SK_ColorTRANSPARENT) {
          return false;
        }
      }
    }
  }
  if (!video_layer)
    return false;

  if (video_layer->cv_pixel_buffer) {
    AVSampleBufferDisplayLayerEnqueueCVPixelBuffer(
        fullscreen_low_power_layer, video_layer->cv_pixel_buffer);
  } else {
    AVSampleBufferDisplayLayerEnqueueIOSurface(fullscreen_low_power_layer,
                                               video_layer->io_surface);
  }
  [fullscreen_low_power_layer setVideoGravity:AVLayerVideoGravityResize];
  [fullscreen_low_power_layer setFrame:video_layer_frame_dip.ToCGRect()];
  return true;
}


CARendererLayerTree::RootLayer::RootLayer() {}

// Note that for all destructors, the the CALayer will have been reset to nil if
// another layer has taken it.
CARendererLayerTree::RootLayer::~RootLayer() {
  [ca_layer removeFromSuperlayer];
}

CARendererLayerTree::ClipAndSortingLayer::ClipAndSortingLayer(
    bool is_clipped,
    gfx::Rect clip_rect,
    unsigned sorting_context_id,
    bool is_singleton_sorting_context)
    : is_clipped(is_clipped),
      clip_rect(clip_rect),
      sorting_context_id(sorting_context_id),
      is_singleton_sorting_context(is_singleton_sorting_context) {}

CARendererLayerTree::ClipAndSortingLayer::ClipAndSortingLayer(
    ClipAndSortingLayer&& layer)
    : transform_layers(std::move(layer.transform_layers)),
      is_clipped(layer.is_clipped),
      clip_rect(layer.clip_rect),
      sorting_context_id(layer.sorting_context_id),
      is_singleton_sorting_context(layer.is_singleton_sorting_context),
      ca_layer(layer.ca_layer) {
  // Ensure that the ca_layer be reset, so that when the destructor is called,
  // the layer hierarchy is unaffected.
  // TODO(ccameron): Add a move constructor for scoped_nsobject to do this
  // automatically.
  layer.ca_layer.reset();
}

CARendererLayerTree::ClipAndSortingLayer::~ClipAndSortingLayer() {
  [ca_layer removeFromSuperlayer];
}

CARendererLayerTree::TransformLayer::TransformLayer(
    const gfx::Transform& transform)
    : transform(transform) {}

CARendererLayerTree::TransformLayer::TransformLayer(TransformLayer&& layer)
    : transform(layer.transform),
      content_layers(std::move(layer.content_layers)),
      ca_layer(layer.ca_layer) {
  layer.ca_layer.reset();
}

CARendererLayerTree::TransformLayer::~TransformLayer() {
  [ca_layer removeFromSuperlayer];
}

CARendererLayerTree::ContentLayer::ContentLayer(
    base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
    base::ScopedCFTypeRef<CVPixelBufferRef> cv_pixel_buffer,
    const gfx::RectF& contents_rect,
    const gfx::Rect& rect,
    unsigned background_color,
    unsigned edge_aa_mask,
    float opacity,
    unsigned filter)
    : io_surface(io_surface),
      cv_pixel_buffer(cv_pixel_buffer),
      contents_rect(contents_rect),
      rect(rect),
      background_color(background_color),
      ca_edge_aa_mask(0),
      opacity(opacity),
      ca_filter(filter == GL_LINEAR ? kCAFilterLinear : kCAFilterNearest) {
  DCHECK(filter == GL_LINEAR || filter == GL_NEAREST);

  // Because the root layer has setGeometryFlipped:YES, there is some ambiguity
  // about what exactly top and bottom mean. This ambiguity is resolved in
  // different ways for solid color CALayers and for CALayers that have content
  // (surprise!). For CALayers with IOSurface content, the top edge in the AA
  // mask refers to what appears as the bottom edge on-screen. For CALayers
  // without content (solid color layers), the top edge in the AA mask is the
  // top edge on-screen.
  // https://crbug.com/567946
  if (edge_aa_mask & GL_CA_LAYER_EDGE_LEFT_CHROMIUM)
    ca_edge_aa_mask |= kCALayerLeftEdge;
  if (edge_aa_mask & GL_CA_LAYER_EDGE_RIGHT_CHROMIUM)
    ca_edge_aa_mask |= kCALayerRightEdge;
  if (io_surface) {
    if (edge_aa_mask & GL_CA_LAYER_EDGE_TOP_CHROMIUM)
      ca_edge_aa_mask |= kCALayerBottomEdge;
    if (edge_aa_mask & GL_CA_LAYER_EDGE_BOTTOM_CHROMIUM)
      ca_edge_aa_mask |= kCALayerTopEdge;
  } else {
    if (edge_aa_mask & GL_CA_LAYER_EDGE_TOP_CHROMIUM)
      ca_edge_aa_mask |= kCALayerTopEdge;
    if (edge_aa_mask & GL_CA_LAYER_EDGE_BOTTOM_CHROMIUM)
      ca_edge_aa_mask |= kCALayerBottomEdge;
  }

  // Only allow 4:2:0 frames which fill the layer's contents to be promoted to
  // AV layers.
  if (IOSurfaceGetPixelFormat(io_surface) ==
          kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange &&
      contents_rect == gfx::RectF(0, 0, 1, 1)) {
    // Disable AVSampleBufferDisplayLayer on <10.11 due to reports of memory
    // leaks on 10.9.
    // https://crbug.com/631485
    if (base::mac::IsOSElCapitanOrLater())
      use_av_layer = true;
  }
}

CARendererLayerTree::ContentLayer::ContentLayer(ContentLayer&& layer)
    : io_surface(layer.io_surface),
      cv_pixel_buffer(layer.cv_pixel_buffer),
      contents_rect(layer.contents_rect),
      rect(layer.rect),
      background_color(layer.background_color),
      ca_edge_aa_mask(layer.ca_edge_aa_mask),
      opacity(layer.opacity),
      ca_filter(layer.ca_filter),
      ca_layer(std::move(layer.ca_layer)),
      av_layer(std::move(layer.av_layer)),
      use_av_layer(layer.use_av_layer) {
  DCHECK(!layer.ca_layer);
  DCHECK(!layer.av_layer);
}

CARendererLayerTree::ContentLayer::~ContentLayer() {
  [ca_layer removeFromSuperlayer];
}

bool CARendererLayerTree::RootLayer::AddContentLayer(
    bool is_clipped,
    const gfx::Rect& clip_rect,
    unsigned sorting_context_id,
    const gfx::Transform& transform,
    base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
    base::ScopedCFTypeRef<CVPixelBufferRef> cv_pixel_buffer,
    const gfx::RectF& contents_rect,
    const gfx::Rect& rect,
    unsigned background_color,
    unsigned edge_aa_mask,
    float opacity,
    unsigned filter) {
  bool needs_new_clip_and_sorting_layer = true;

  // In sorting_context_id 0, all quads are listed in back-to-front order.
  // This is accomplished by having the CALayers be siblings of each other.
  // If a quad has a 3D transform, it is necessary to put it in its own sorting
  // context, so that it will not intersect with quads before and after it.
  bool is_singleton_sorting_context =
      !sorting_context_id && !transform.IsFlat();

  if (!clip_and_sorting_layers.empty()) {
    ClipAndSortingLayer& current_layer = clip_and_sorting_layers.back();
    // It is in error to change the clipping settings within a non-zero sorting
    // context. The result will be incorrect layering and intersection.
    if (sorting_context_id &&
        current_layer.sorting_context_id == sorting_context_id &&
        (current_layer.is_clipped != is_clipped ||
         current_layer.clip_rect != clip_rect)) {
      // Excessive logging to debug white screens (crbug.com/583805).
      // TODO(ccameron): change this back to a DLOG.
      LOG(ERROR) << "CALayer changed clip inside non-zero sorting context.";
      return false;
    }
    if (!is_singleton_sorting_context &&
        !current_layer.is_singleton_sorting_context &&
        current_layer.is_clipped == is_clipped &&
        current_layer.clip_rect == clip_rect &&
        current_layer.sorting_context_id == sorting_context_id) {
      needs_new_clip_and_sorting_layer = false;
    }
  }
  if (needs_new_clip_and_sorting_layer) {
    clip_and_sorting_layers.push_back(
        ClipAndSortingLayer(is_clipped, clip_rect, sorting_context_id,
                            is_singleton_sorting_context));
  }
  clip_and_sorting_layers.back().AddContentLayer(
      transform, io_surface, cv_pixel_buffer, contents_rect, rect,
      background_color, edge_aa_mask, opacity, filter);
  return true;
}

void CARendererLayerTree::ClipAndSortingLayer::AddContentLayer(
    const gfx::Transform& transform,
    base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
    base::ScopedCFTypeRef<CVPixelBufferRef> cv_pixel_buffer,
    const gfx::RectF& contents_rect,
    const gfx::Rect& rect,
    unsigned background_color,
    unsigned edge_aa_mask,
    float opacity,
    unsigned filter) {
  bool needs_new_transform_layer = true;
  if (!transform_layers.empty()) {
    const TransformLayer& current_layer = transform_layers.back();
    if (current_layer.transform == transform)
      needs_new_transform_layer = false;
  }
  if (needs_new_transform_layer)
    transform_layers.push_back(TransformLayer(transform));
  transform_layers.back().AddContentLayer(io_surface, cv_pixel_buffer,
                                          contents_rect, rect, background_color,
                                          edge_aa_mask, opacity, filter);
}

void CARendererLayerTree::TransformLayer::AddContentLayer(
    base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
    base::ScopedCFTypeRef<CVPixelBufferRef> cv_pixel_buffer,
    const gfx::RectF& contents_rect,
    const gfx::Rect& rect,
    unsigned background_color,
    unsigned edge_aa_mask,
    float opacity,
    unsigned filter) {
  content_layers.push_back(ContentLayer(io_surface, cv_pixel_buffer,
                                        contents_rect, rect, background_color,
                                        edge_aa_mask, opacity, filter));
}

void CARendererLayerTree::RootLayer::CommitToCA(CALayer* superlayer,
                                                RootLayer* old_layer,
                                                float scale_factor) {
  if (old_layer) {
    DCHECK(old_layer->ca_layer);
    std::swap(ca_layer, old_layer->ca_layer);
  } else {
    ca_layer.reset([[CALayer alloc] init]);
    [ca_layer setAnchorPoint:CGPointZero];
    [superlayer setSublayers:nil];
    [superlayer addSublayer:ca_layer];
    [superlayer setBorderWidth:0];
  }
  // Excessive logging to debug white screens (crbug.com/583805).
  // TODO(ccameron): change this back to a DCHECK.
  if ([ca_layer superlayer] != superlayer) {
    LOG(ERROR) << "CARendererLayerTree root layer not attached to tree.";
  }

  for (size_t i = 0; i < clip_and_sorting_layers.size(); ++i) {
    ClipAndSortingLayer* old_clip_and_sorting_layer = nullptr;
    if (old_layer && i < old_layer->clip_and_sorting_layers.size()) {
      old_clip_and_sorting_layer = &old_layer->clip_and_sorting_layers[i];
    }
    clip_and_sorting_layers[i].CommitToCA(
        ca_layer.get(), old_clip_and_sorting_layer, scale_factor);
  }
}

void CARendererLayerTree::ClipAndSortingLayer::CommitToCA(
    CALayer* superlayer,
    ClipAndSortingLayer* old_layer,
    float scale_factor) {
  bool update_is_clipped = true;
  bool update_clip_rect = true;
  if (old_layer) {
    DCHECK(old_layer->ca_layer);
    std::swap(ca_layer, old_layer->ca_layer);
    update_is_clipped = old_layer->is_clipped != is_clipped;
    update_clip_rect = update_is_clipped || old_layer->clip_rect != clip_rect;
  } else {
    ca_layer.reset([[CALayer alloc] init]);
    [ca_layer setAnchorPoint:CGPointZero];
    [superlayer addSublayer:ca_layer];
  }
  // Excessive logging to debug white screens (crbug.com/583805).
  // TODO(ccameron): change this back to a DCHECK.
  if ([ca_layer superlayer] != superlayer) {
    LOG(ERROR) << "CARendererLayerTree root layer not attached to tree.";
  }

  if (update_is_clipped)
    [ca_layer setMasksToBounds:is_clipped];

  if (update_clip_rect) {
    if (is_clipped) {
      gfx::RectF dip_clip_rect = gfx::RectF(clip_rect);
      dip_clip_rect.Scale(1 / scale_factor);
      [ca_layer setPosition:CGPointMake(dip_clip_rect.x(), dip_clip_rect.y())];
      [ca_layer setBounds:CGRectMake(0, 0, dip_clip_rect.width(),
                                     dip_clip_rect.height())];
      [ca_layer
          setSublayerTransform:CATransform3DMakeTranslation(
                                   -dip_clip_rect.x(), -dip_clip_rect.y(), 0)];
    } else {
      [ca_layer setPosition:CGPointZero];
      [ca_layer setBounds:CGRectZero];
      [ca_layer setSublayerTransform:CATransform3DIdentity];
    }
  }

  for (size_t i = 0; i < transform_layers.size(); ++i) {
    TransformLayer* old_transform_layer = nullptr;
    if (old_layer && i < old_layer->transform_layers.size())
      old_transform_layer = &old_layer->transform_layers[i];
    transform_layers[i].CommitToCA(ca_layer.get(), old_transform_layer,
                                   scale_factor);
  }
}

void CARendererLayerTree::TransformLayer::CommitToCA(CALayer* superlayer,
                                                     TransformLayer* old_layer,
                                                     float scale_factor) {
  bool update_transform = true;
  if (old_layer) {
    DCHECK(old_layer->ca_layer);
    std::swap(ca_layer, old_layer->ca_layer);
    update_transform = old_layer->transform != transform;
  } else {
    ca_layer.reset([[CATransformLayer alloc] init]);
    [superlayer addSublayer:ca_layer];
  }
  DCHECK_EQ([ca_layer superlayer], superlayer);

  if (update_transform) {
    gfx::Transform pre_scale;
    gfx::Transform post_scale;
    pre_scale.Scale(1 / scale_factor, 1 / scale_factor);
    post_scale.Scale(scale_factor, scale_factor);
    gfx::Transform conjugated_transform = pre_scale * transform * post_scale;

    CATransform3D ca_transform;
    conjugated_transform.matrix().asColMajord(&ca_transform.m11);
    [ca_layer setTransform:ca_transform];
  }

  for (size_t i = 0; i < content_layers.size(); ++i) {
    ContentLayer* old_content_layer = nullptr;
    if (old_layer && i < old_layer->content_layers.size())
      old_content_layer = &old_layer->content_layers[i];
    content_layers[i].CommitToCA(ca_layer.get(), old_content_layer,
                                 scale_factor);
  }
}

void CARendererLayerTree::ContentLayer::CommitToCA(CALayer* superlayer,
                                                   ContentLayer* old_layer,
                                                   float scale_factor) {
  bool update_contents = true;
  bool update_contents_rect = true;
  bool update_rect = true;
  bool update_background_color = true;
  bool update_ca_edge_aa_mask = true;
  bool update_opacity = true;
  bool update_ca_filter = true;
  if (old_layer && old_layer->use_av_layer == use_av_layer) {
    DCHECK(old_layer->ca_layer);
    std::swap(ca_layer, old_layer->ca_layer);
    std::swap(av_layer, old_layer->av_layer);
    update_contents = old_layer->io_surface != io_surface ||
                      old_layer->cv_pixel_buffer != cv_pixel_buffer;
    update_contents_rect = old_layer->contents_rect != contents_rect;
    update_rect = old_layer->rect != rect;
    update_background_color = old_layer->background_color != background_color;
    update_ca_edge_aa_mask = old_layer->ca_edge_aa_mask != ca_edge_aa_mask;
    update_opacity = old_layer->opacity != opacity;
    update_ca_filter = old_layer->ca_filter != ca_filter;
  } else {
    if (use_av_layer) {
      av_layer.reset([[AVSampleBufferDisplayLayer alloc] init]);
      ca_layer.reset([av_layer retain]);
      [av_layer setVideoGravity:AVLayerVideoGravityResize];
    } else {
      ca_layer.reset([[CALayer alloc] init]);
    }
    [ca_layer setAnchorPoint:CGPointZero];
    if (old_layer && old_layer->ca_layer)
      [superlayer replaceSublayer:old_layer->ca_layer with:ca_layer];
    else
      [superlayer addSublayer:ca_layer];
  }
  DCHECK_EQ([ca_layer superlayer], superlayer);
  bool update_anything = update_contents || update_contents_rect ||
                         update_rect || update_background_color ||
                         update_ca_edge_aa_mask || update_opacity ||
                         update_ca_filter;
  if (use_av_layer) {
    if (update_contents) {
      if (cv_pixel_buffer) {
        AVSampleBufferDisplayLayerEnqueueCVPixelBuffer(av_layer,
                                                       cv_pixel_buffer);
      } else {
        AVSampleBufferDisplayLayerEnqueueIOSurface(av_layer, io_surface);
      }
    }
  } else {
    if (update_contents) {
      [ca_layer setContents:(__bridge id)(io_surface.get())];
      if ([ca_layer respondsToSelector:(@selector(setContentsScale:))])
        [ca_layer setContentsScale:scale_factor];
    }
    if (update_contents_rect)
      [ca_layer setContentsRect:contents_rect.ToCGRect()];
  }
  if (update_rect) {
    gfx::RectF dip_rect = gfx::RectF(rect);
    dip_rect.Scale(1 / scale_factor);
    [ca_layer setPosition:CGPointMake(dip_rect.x(), dip_rect.y())];
    [ca_layer setBounds:CGRectMake(0, 0, dip_rect.width(), dip_rect.height())];
  }
  if (update_background_color) {
    CGFloat rgba_color_components[4] = {
        SkColorGetR(background_color) / 255.,
        SkColorGetG(background_color) / 255.,
        SkColorGetB(background_color) / 255.,
        SkColorGetA(background_color) / 255.,
    };
    base::ScopedCFTypeRef<CGColorRef> srgb_background_color(CGColorCreate(
        CGColorSpaceCreateWithName(kCGColorSpaceSRGB), rgba_color_components));
    [ca_layer setBackgroundColor:srgb_background_color];
  }
  if (update_ca_edge_aa_mask)
    [ca_layer setEdgeAntialiasingMask:ca_edge_aa_mask];
  if (update_opacity)
    [ca_layer setOpacity:opacity];
  if (update_ca_filter) {
    [ca_layer setMagnificationFilter:ca_filter];
    [ca_layer setMinificationFilter:ca_filter];
  }

  static bool show_borders = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kShowMacOverlayBorders);
  if (show_borders) {
    base::ScopedCFTypeRef<CGColorRef> color;
    if (update_anything) {
      if (use_av_layer) {
        // Yellow represents an AV layer that changed this frame.
        color.reset(CGColorCreateGenericRGB(1, 1, 0, 1));
      } else {
        // Pink represents a CALayer that changed this frame.
        color.reset(CGColorCreateGenericRGB(1, 0, 1, 1));
      }
    } else {
      // Grey represents a CALayer that has not changed.
      color.reset(CGColorCreateGenericRGB(0.5, 0.5, 0.5, 1));
    }
    [ca_layer setBorderWidth:1];
    [ca_layer setBorderColor:color];
  }
}

}  // namespace ui
