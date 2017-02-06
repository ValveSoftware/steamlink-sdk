// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <AVFoundation/AVFoundation.h>
#include <memory>

#include "base/mac/sdk_forward_declarations.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accelerated_widget_mac/ca_renderer_layer_tree.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/mac/io_surface.h"

namespace gpu {

namespace {

struct CALayerProperties {
  bool is_clipped = true;
  gfx::Rect clip_rect;
  int sorting_context_id = 0;
  gfx::Transform transform;
  gfx::RectF contents_rect = gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f);
  gfx::Rect rect = gfx::Rect(0, 0, 256, 256);
  unsigned background_color = SkColorSetARGB(0xFF, 0xFF, 0xFF, 0xFF);
  unsigned edge_aa_mask = 0;
  float opacity = 1.0f;
  float scale_factor = 1.0f;
  unsigned filter = GL_LINEAR;
  base::ScopedCFTypeRef<CVPixelBufferRef> cv_pixel_buffer;
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface;
};

bool ScheduleCALayer(ui::CARendererLayerTree* tree,
                     CALayerProperties* properties) {
  return tree->ScheduleCALayer(
      properties->is_clipped, properties->clip_rect,
      properties->sorting_context_id, properties->transform,
      properties->io_surface, properties->cv_pixel_buffer,
      properties->contents_rect, properties->rect, properties->background_color,
      properties->edge_aa_mask, properties->opacity, properties->filter);
}

void UpdateCALayerTree(std::unique_ptr<ui::CARendererLayerTree>& ca_layer_tree,
                       CALayerProperties* properties,
                       CALayer* superlayer) {
  std::unique_ptr<ui::CARendererLayerTree> new_ca_layer_tree(
      new ui::CARendererLayerTree);
  bool result = ScheduleCALayer(new_ca_layer_tree.get(), properties);
  EXPECT_TRUE(result);
  new_ca_layer_tree->CommitScheduledCALayers(
      superlayer, std::move(ca_layer_tree), properties->scale_factor);
  std::swap(new_ca_layer_tree, ca_layer_tree);
}

}  // namespace

class CALayerTreeTest : public testing::Test {
 protected:
  void SetUp() override {
    superlayer_.reset([[CALayer alloc] init]);
    fullscreen_low_power_layer_.reset(
        [[AVSampleBufferDisplayLayer alloc] init]);
  }

  base::scoped_nsobject<CALayer> superlayer_;
  base::scoped_nsobject<AVSampleBufferDisplayLayer> fullscreen_low_power_layer_;
};

// Test updating each layer's properties.
TEST_F(CALayerTreeTest, PropertyUpdates) {
  CALayerProperties properties;
  properties.clip_rect = gfx::Rect(2, 4, 8, 16);
  properties.transform.Translate(10, 20);
  properties.contents_rect = gfx::RectF(0.0f, 0.25f, 0.5f, 0.75f);
  properties.rect = gfx::Rect(16, 32, 64, 128);
  properties.background_color = SkColorSetARGB(0xFF, 0xFF, 0, 0);
  properties.edge_aa_mask = GL_CA_LAYER_EDGE_LEFT_CHROMIUM;
  properties.opacity = 0.5f;
  properties.io_surface.reset(
      gfx::CreateIOSurface(gfx::Size(256, 256), gfx::BufferFormat::BGRA_8888));

  std::unique_ptr<ui::CARendererLayerTree> ca_layer_tree;
  CALayer* root_layer = nil;
  CALayer* clip_and_sorting_layer = nil;
  CALayer* transform_layer = nil;
  CALayer* content_layer = nil;

  // Validate the initial values.
  {
    std::unique_ptr<ui::CARendererLayerTree> new_ca_layer_tree(
        new ui::CARendererLayerTree);

    UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    root_layer = [[superlayer_ sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    clip_and_sorting_layer = [[root_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    transform_layer = [[clip_and_sorting_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[transform_layer sublayers] count]);
    content_layer = [[transform_layer sublayers] objectAtIndex:0];

    // Validate the clip and sorting context layer.
    EXPECT_TRUE([clip_and_sorting_layer masksToBounds]);
    EXPECT_EQ(gfx::Rect(properties.clip_rect.size()),
              gfx::Rect([clip_and_sorting_layer bounds]));
    EXPECT_EQ(properties.clip_rect.origin(),
              gfx::Point([clip_and_sorting_layer position]));
    EXPECT_EQ(-properties.clip_rect.origin().x(),
              [clip_and_sorting_layer sublayerTransform].m41);
    EXPECT_EQ(-properties.clip_rect.origin().y(),
              [clip_and_sorting_layer sublayerTransform].m42);

    // Validate the transform layer.
    EXPECT_EQ(properties.transform.matrix().get(3, 0),
              [transform_layer sublayerTransform].m41);
    EXPECT_EQ(properties.transform.matrix().get(3, 1),
              [transform_layer sublayerTransform].m42);

    // Validate the content layer.
    EXPECT_EQ(static_cast<id>(properties.io_surface.get()),
              [content_layer contents]);
    EXPECT_EQ(properties.contents_rect,
              gfx::RectF([content_layer contentsRect]));
    EXPECT_EQ(properties.rect.origin(), gfx::Point([content_layer position]));
    EXPECT_EQ(gfx::Rect(properties.rect.size()),
              gfx::Rect([content_layer bounds]));
    EXPECT_EQ(kCALayerLeftEdge, [content_layer edgeAntialiasingMask]);
    EXPECT_EQ(properties.opacity, [content_layer opacity]);
    EXPECT_NSEQ(kCAFilterLinear, [content_layer minificationFilter]);
    EXPECT_NSEQ(kCAFilterLinear, [content_layer magnificationFilter]);
    if ([content_layer respondsToSelector:(@selector(contentsScale))])
      EXPECT_EQ(properties.scale_factor, [content_layer contentsScale]);
  }

  // Update just the clip rect and re-commit.
  {
    properties.clip_rect = gfx::Rect(4, 8, 16, 32);
    UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

    // Validate the tree structure
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    EXPECT_EQ(root_layer, [[superlayer_ sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    EXPECT_EQ(clip_and_sorting_layer, [[root_layer sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    EXPECT_EQ(transform_layer,
              [[clip_and_sorting_layer sublayers] objectAtIndex:0]);

    // Validate the clip and sorting context layer.
    EXPECT_TRUE([clip_and_sorting_layer masksToBounds]);
    EXPECT_EQ(gfx::Rect(properties.clip_rect.size()),
              gfx::Rect([clip_and_sorting_layer bounds]));
    EXPECT_EQ(properties.clip_rect.origin(),
              gfx::Point([clip_and_sorting_layer position]));
    EXPECT_EQ(-properties.clip_rect.origin().x(),
              [clip_and_sorting_layer sublayerTransform].m41);
    EXPECT_EQ(-properties.clip_rect.origin().y(),
              [clip_and_sorting_layer sublayerTransform].m42);
  }

  // Disable clipping and re-commit.
  {
    properties.is_clipped = false;
    UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

    // Validate the tree structure
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    EXPECT_EQ(root_layer, [[superlayer_ sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    EXPECT_EQ(clip_and_sorting_layer, [[root_layer sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    EXPECT_EQ(transform_layer,
              [[clip_and_sorting_layer sublayers] objectAtIndex:0]);

    // Validate the clip and sorting context layer.
    EXPECT_FALSE([clip_and_sorting_layer masksToBounds]);
    EXPECT_EQ(gfx::Rect(), gfx::Rect([clip_and_sorting_layer bounds]));
    EXPECT_EQ(gfx::Point(), gfx::Point([clip_and_sorting_layer position]));
    EXPECT_EQ(0.0, [clip_and_sorting_layer sublayerTransform].m41);
    EXPECT_EQ(0.0, [clip_and_sorting_layer sublayerTransform].m42);
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
  }

  // Change the transform and re-commit.
  {
    properties.transform.Translate(5, 5);
    UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    EXPECT_EQ(root_layer, [[superlayer_ sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    EXPECT_EQ(clip_and_sorting_layer, [[root_layer sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    EXPECT_EQ(transform_layer,
              [[clip_and_sorting_layer sublayers] objectAtIndex:0]);

    // Validate the transform layer.
    EXPECT_EQ(properties.transform.matrix().get(3, 0),
              [transform_layer sublayerTransform].m41);
    EXPECT_EQ(properties.transform.matrix().get(3, 1),
              [transform_layer sublayerTransform].m42);
  }

  // Change the edge antialiasing mask and commit.
  {
    properties.edge_aa_mask = GL_CA_LAYER_EDGE_TOP_CHROMIUM;
    UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    EXPECT_EQ(root_layer, [[superlayer_ sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    EXPECT_EQ(clip_and_sorting_layer, [[root_layer sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    EXPECT_EQ(transform_layer,
              [[clip_and_sorting_layer sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[transform_layer sublayers] count]);
    EXPECT_EQ(content_layer, [[transform_layer sublayers] objectAtIndex:0]);

    // Validate the content layer. Note that top and bottom edges flip.
    EXPECT_EQ(kCALayerBottomEdge, [content_layer edgeAntialiasingMask]);
  }

  // Change the contents and commit.
  {
    properties.io_surface.reset();
    UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    EXPECT_EQ(root_layer, [[superlayer_ sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    EXPECT_EQ(clip_and_sorting_layer, [[root_layer sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    EXPECT_EQ(transform_layer,
              [[clip_and_sorting_layer sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[transform_layer sublayers] count]);
    EXPECT_EQ(content_layer, [[transform_layer sublayers] objectAtIndex:0]);

    // Validate the content layer. Note that edge anti-aliasing no longer flips.
    EXPECT_EQ(nil, [content_layer contents]);
    EXPECT_EQ(kCALayerTopEdge, [content_layer edgeAntialiasingMask]);
  }

  // Change the rect size.
  {
    properties.rect = gfx::Rect(properties.rect.origin(), gfx::Size(32, 16));
    UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    EXPECT_EQ(root_layer, [[superlayer_ sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    EXPECT_EQ(clip_and_sorting_layer, [[root_layer sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    EXPECT_EQ(transform_layer,
              [[clip_and_sorting_layer sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[transform_layer sublayers] count]);
    EXPECT_EQ(content_layer, [[transform_layer sublayers] objectAtIndex:0]);

    // Validate the content layer.
    EXPECT_EQ(properties.rect.origin(), gfx::Point([content_layer position]));
    EXPECT_EQ(gfx::Rect(properties.rect.size()),
              gfx::Rect([content_layer bounds]));
  }

  // Change the rect position.
  {
    properties.rect = gfx::Rect(gfx::Point(16, 4), properties.rect.size());
    UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    EXPECT_EQ(root_layer, [[superlayer_ sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    EXPECT_EQ(clip_and_sorting_layer, [[root_layer sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    EXPECT_EQ(transform_layer,
              [[clip_and_sorting_layer sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[transform_layer sublayers] count]);
    EXPECT_EQ(content_layer, [[transform_layer sublayers] objectAtIndex:0]);

    // Validate the content layer.
    EXPECT_EQ(properties.rect.origin(), gfx::Point([content_layer position]));
    EXPECT_EQ(gfx::Rect(properties.rect.size()),
              gfx::Rect([content_layer bounds]));
  }

  // Change the opacity.
  {
    properties.opacity = 1.0f;
    UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    EXPECT_EQ(root_layer, [[superlayer_ sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    EXPECT_EQ(clip_and_sorting_layer, [[root_layer sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    EXPECT_EQ(transform_layer,
              [[clip_and_sorting_layer sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[transform_layer sublayers] count]);
    EXPECT_EQ(content_layer, [[transform_layer sublayers] objectAtIndex:0]);

    // Validate the content layer.
    EXPECT_EQ(properties.opacity, [content_layer opacity]);
  }

  // Change the filter.
  {
    properties.filter = GL_NEAREST;
    UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    EXPECT_EQ(root_layer, [[superlayer_ sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    EXPECT_EQ(clip_and_sorting_layer, [[root_layer sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    EXPECT_EQ(transform_layer,
              [[clip_and_sorting_layer sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[transform_layer sublayers] count]);
    EXPECT_EQ(content_layer, [[transform_layer sublayers] objectAtIndex:0]);

    // Validate the content layer.
    EXPECT_NSEQ(kCAFilterNearest, [content_layer minificationFilter]);
    EXPECT_NSEQ(kCAFilterNearest, [content_layer magnificationFilter]);
  }

  // Add the clipping and IOSurface contents back.
  {
    properties.is_clipped = true;
    properties.io_surface.reset(
        gfx::CreateIOSurface(gfx::Size(256, 256),
                             gfx::BufferFormat::BGRA_8888));
    UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    EXPECT_EQ(root_layer, [[superlayer_ sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    EXPECT_EQ(clip_and_sorting_layer, [[root_layer sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    EXPECT_EQ(transform_layer,
              [[clip_and_sorting_layer sublayers] objectAtIndex:0]);
    EXPECT_EQ(1u, [[transform_layer sublayers] count]);
    EXPECT_EQ(content_layer, [[transform_layer sublayers] objectAtIndex:0]);

    // Validate the content layer.
    EXPECT_EQ(static_cast<id>(properties.io_surface.get()),
              [content_layer contents]);
    EXPECT_EQ(kCALayerBottomEdge, [content_layer edgeAntialiasingMask]);
  }

  // Change the scale factor. This should result in a new tree being created.
  {
    properties.scale_factor = 2.0f;
    UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    EXPECT_NE(root_layer, [[superlayer_ sublayers] objectAtIndex:0]);
    root_layer = [[superlayer_ sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    EXPECT_NE(clip_and_sorting_layer, [[root_layer sublayers] objectAtIndex:0]);
    clip_and_sorting_layer = [[root_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    EXPECT_NE(transform_layer,
              [[clip_and_sorting_layer sublayers] objectAtIndex:0]);
    transform_layer = [[clip_and_sorting_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[transform_layer sublayers] count]);
    EXPECT_NE(content_layer, [[transform_layer sublayers] objectAtIndex:0]);
    content_layer = [[transform_layer sublayers] objectAtIndex:0];

    // Validate the clip and sorting context layer.
    EXPECT_TRUE([clip_and_sorting_layer masksToBounds]);
    EXPECT_EQ(gfx::ConvertRectToDIP(properties.scale_factor,
                                    gfx::Rect(properties.clip_rect.size())),
              gfx::Rect([clip_and_sorting_layer bounds]));
    EXPECT_EQ(gfx::ConvertPointToDIP(properties.scale_factor,
                                     properties.clip_rect.origin()),
              gfx::Point([clip_and_sorting_layer position]));
    EXPECT_EQ(-properties.clip_rect.origin().x() / properties.scale_factor,
              [clip_and_sorting_layer sublayerTransform].m41);
    EXPECT_EQ(-properties.clip_rect.origin().y() / properties.scale_factor,
              [clip_and_sorting_layer sublayerTransform].m42);

    // Validate the transform layer.
    EXPECT_EQ(properties.transform.matrix().get(3, 0) / properties.scale_factor,
              [transform_layer sublayerTransform].m41);
    EXPECT_EQ(properties.transform.matrix().get(3, 1) / properties.scale_factor,
              [transform_layer sublayerTransform].m42);

    // Validate the content layer.
    EXPECT_EQ(static_cast<id>(properties.io_surface.get()),
              [content_layer contents]);
    EXPECT_EQ(properties.contents_rect,
              gfx::RectF([content_layer contentsRect]));
    EXPECT_EQ(gfx::ConvertPointToDIP(properties.scale_factor,
                                     properties.rect.origin()),
              gfx::Point([content_layer position]));
    EXPECT_EQ(gfx::ConvertRectToDIP(properties.scale_factor,
                                    gfx::Rect(properties.rect.size())),
              gfx::Rect([content_layer bounds]));
    EXPECT_EQ(kCALayerBottomEdge, [content_layer edgeAntialiasingMask]);
    EXPECT_EQ(properties.opacity, [content_layer opacity]);
    if ([content_layer respondsToSelector:(@selector(contentsScale))])
      EXPECT_EQ(properties.scale_factor, [content_layer contentsScale]);
  }
}

// Verify that sorting context zero is split at non-flat transforms.
TEST_F(CALayerTreeTest, SplitSortingContextZero) {
  CALayerProperties properties;
  properties.is_clipped = false;
  properties.clip_rect = gfx::Rect();
  properties.rect = gfx::Rect(0, 0, 256, 256);

  // We'll use the IOSurface contents to identify the content layers.
  base::ScopedCFTypeRef<IOSurfaceRef> io_surfaces[5];
  for (size_t i = 0; i < 5; ++i) {
    io_surfaces[i].reset(gfx::CreateIOSurface(
        gfx::Size(256, 256), gfx::BufferFormat::BGRA_8888));
  }

  // Have 5 transforms:
  // * 2 flat but different (1 sorting context layer, 2 transform layers)
  // * 1 non-flat (new sorting context layer)
  // * 2 flat and the same (new sorting context layer, 1 transform layer)
  gfx::Transform transforms[5];
  transforms[0].Translate(10, 10);
  transforms[1].RotateAboutZAxis(45.0f);
  transforms[2].RotateAboutYAxis(45.0f);
  transforms[3].Translate(10, 10);
  transforms[4].Translate(10, 10);

  // Schedule and commit the layers.
  std::unique_ptr<ui::CARendererLayerTree> ca_layer_tree(
      new ui::CARendererLayerTree);
  for (size_t i = 0; i < 5; ++i) {
    properties.io_surface = io_surfaces[i];
    properties.transform = transforms[i];
    bool result = ScheduleCALayer(ca_layer_tree.get(), &properties);
    EXPECT_TRUE(result);
  }
  ca_layer_tree->CommitScheduledCALayers(superlayer_, nullptr,
                                         properties.scale_factor);

  // Validate the root layer.
  EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
  CALayer* root_layer = [[superlayer_ sublayers] objectAtIndex:0];

  // Validate that we have 3 sorting context layers.
  EXPECT_EQ(3u, [[root_layer sublayers] count]);
  CALayer* clip_and_sorting_layer_0 = [[root_layer sublayers] objectAtIndex:0];
  CALayer* clip_and_sorting_layer_1 = [[root_layer sublayers] objectAtIndex:1];
  CALayer* clip_and_sorting_layer_2 = [[root_layer sublayers] objectAtIndex:2];

  // Validate that the first sorting context has 2 transform layers each with
  // one content layer.
  EXPECT_EQ(2u, [[clip_and_sorting_layer_0 sublayers] count]);
  CALayer* transform_layer_0_0 =
      [[clip_and_sorting_layer_0 sublayers] objectAtIndex:0];
  CALayer* transform_layer_0_1 =
      [[clip_and_sorting_layer_0 sublayers] objectAtIndex:1];
  EXPECT_EQ(1u, [[transform_layer_0_0 sublayers] count]);
  CALayer* content_layer_0 = [[transform_layer_0_0 sublayers] objectAtIndex:0];
  EXPECT_EQ(1u, [[transform_layer_0_1 sublayers] count]);
  CALayer* content_layer_1 = [[transform_layer_0_1 sublayers] objectAtIndex:0];

  // Validate that the second sorting context has 1 transform layer with one
  // content layer.
  EXPECT_EQ(1u, [[clip_and_sorting_layer_1 sublayers] count]);
  CALayer* transform_layer_1_0 =
      [[clip_and_sorting_layer_1 sublayers] objectAtIndex:0];
  EXPECT_EQ(1u, [[transform_layer_1_0 sublayers] count]);
  CALayer* content_layer_2 = [[transform_layer_1_0 sublayers] objectAtIndex:0];

  // Validate that the third sorting context has 1 transform layer with two
  // content layers.
  EXPECT_EQ(1u, [[clip_and_sorting_layer_2 sublayers] count]);
  CALayer* transform_layer_2_0 =
      [[clip_and_sorting_layer_2 sublayers] objectAtIndex:0];
  EXPECT_EQ(2u, [[transform_layer_2_0 sublayers] count]);
  CALayer* content_layer_3 = [[transform_layer_2_0 sublayers] objectAtIndex:0];
  CALayer* content_layer_4 = [[transform_layer_2_0 sublayers] objectAtIndex:1];

  // Validate that the layers come out in order.
  EXPECT_EQ(static_cast<id>(io_surfaces[0].get()), [content_layer_0 contents]);
  EXPECT_EQ(static_cast<id>(io_surfaces[1].get()), [content_layer_1 contents]);
  EXPECT_EQ(static_cast<id>(io_surfaces[2].get()), [content_layer_2 contents]);
  EXPECT_EQ(static_cast<id>(io_surfaces[3].get()), [content_layer_3 contents]);
  EXPECT_EQ(static_cast<id>(io_surfaces[4].get()), [content_layer_4 contents]);
}

// Verify that sorting contexts are allocated appropriately.
TEST_F(CALayerTreeTest, SortingContexts) {
  CALayerProperties properties;
  properties.is_clipped = false;
  properties.clip_rect = gfx::Rect();
  properties.rect = gfx::Rect(0, 0, 256, 256);

  // We'll use the IOSurface contents to identify the content layers.
  base::ScopedCFTypeRef<IOSurfaceRef> io_surfaces[3];
  for (size_t i = 0; i < 3; ++i) {
    io_surfaces[i].reset(gfx::CreateIOSurface(
        gfx::Size(256, 256), gfx::BufferFormat::BGRA_8888));
  }

  int sorting_context_ids[3] = {3, -1, 0};

  // Schedule and commit the layers.
  std::unique_ptr<ui::CARendererLayerTree> ca_layer_tree(
      new ui::CARendererLayerTree);
  for (size_t i = 0; i < 3; ++i) {
    properties.sorting_context_id = sorting_context_ids[i];
    properties.io_surface = io_surfaces[i];
    bool result = ScheduleCALayer(ca_layer_tree.get(), &properties);
    EXPECT_TRUE(result);
  }
  ca_layer_tree->CommitScheduledCALayers(superlayer_, nullptr,
                                         properties.scale_factor);

  // Validate the root layer.
  EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
  CALayer* root_layer = [[superlayer_ sublayers] objectAtIndex:0];

  // Validate that we have 3 sorting context layers.
  EXPECT_EQ(3u, [[root_layer sublayers] count]);
  CALayer* clip_and_sorting_layer_0 = [[root_layer sublayers] objectAtIndex:0];
  CALayer* clip_and_sorting_layer_1 = [[root_layer sublayers] objectAtIndex:1];
  CALayer* clip_and_sorting_layer_2 = [[root_layer sublayers] objectAtIndex:2];

  // Validate that each sorting context has 1 transform layer.
  EXPECT_EQ(1u, [[clip_and_sorting_layer_0 sublayers] count]);
  CALayer* transform_layer_0 =
      [[clip_and_sorting_layer_0 sublayers] objectAtIndex:0];
  EXPECT_EQ(1u, [[clip_and_sorting_layer_1 sublayers] count]);
  CALayer* transform_layer_1 =
      [[clip_and_sorting_layer_1 sublayers] objectAtIndex:0];
  EXPECT_EQ(1u, [[clip_and_sorting_layer_2 sublayers] count]);
  CALayer* transform_layer_2 =
      [[clip_and_sorting_layer_2 sublayers] objectAtIndex:0];

  // Validate that each transform has 1 content layer.
  EXPECT_EQ(1u, [[transform_layer_0 sublayers] count]);
  CALayer* content_layer_0 = [[transform_layer_0 sublayers] objectAtIndex:0];
  EXPECT_EQ(1u, [[transform_layer_1 sublayers] count]);
  CALayer* content_layer_1 = [[transform_layer_1 sublayers] objectAtIndex:0];
  EXPECT_EQ(1u, [[transform_layer_2 sublayers] count]);
  CALayer* content_layer_2 = [[transform_layer_2 sublayers] objectAtIndex:0];

  // Validate that the layers come out in order.
  EXPECT_EQ(static_cast<id>(io_surfaces[0].get()), [content_layer_0 contents]);
  EXPECT_EQ(static_cast<id>(io_surfaces[1].get()), [content_layer_1 contents]);
  EXPECT_EQ(static_cast<id>(io_surfaces[2].get()), [content_layer_2 contents]);
}

// Verify that sorting contexts must all have the same clipping properties.
TEST_F(CALayerTreeTest, SortingContextMustHaveConsistentClip) {
  CALayerProperties properties;

  // Vary the clipping parameters within sorting contexts.
  bool is_clippeds[3] = { true, true, false};
  gfx::Rect clip_rects[3] = {
      gfx::Rect(0, 0, 16, 16),
      gfx::Rect(4, 8, 16, 32),
      gfx::Rect(0, 0, 16, 16)
  };

  std::unique_ptr<ui::CARendererLayerTree> ca_layer_tree(
      new ui::CARendererLayerTree);
  // First send the various clip parameters to sorting context zero. This is
  // legitimate.
  for (size_t i = 0; i < 3; ++i) {
    properties.is_clipped = is_clippeds[i];
    properties.clip_rect = clip_rects[i];

    bool result = ScheduleCALayer(ca_layer_tree.get(), &properties);
    EXPECT_TRUE(result);
  }

  // Next send the various clip parameters to a non-zero sorting context. This
  // will fail when we try to change the clip within the sorting context.
  for (size_t i = 0; i < 3; ++i) {
    properties.sorting_context_id = 3;
    properties.is_clipped = is_clippeds[i];
    properties.clip_rect = clip_rects[i];

    bool result = ScheduleCALayer(ca_layer_tree.get(), &properties);
    if (i == 0)
      EXPECT_TRUE(result);
    else
      EXPECT_FALSE(result);
  }
  // Try once more with the original clip and verify it works.
  {
    properties.is_clipped = is_clippeds[0];
    properties.clip_rect = clip_rects[0];

    bool result = ScheduleCALayer(ca_layer_tree.get(), &properties);
    EXPECT_TRUE(result);
  }
}

// Test updating each layer's properties.
TEST_F(CALayerTreeTest, AVLayer) {
  CALayerProperties properties;
  properties.io_surface.reset(gfx::CreateIOSurface(
      gfx::Size(256, 256), gfx::BufferFormat::YUV_420_BIPLANAR));

  std::unique_ptr<ui::CARendererLayerTree> ca_layer_tree;
  CALayer* root_layer = nil;
  CALayer* clip_and_sorting_layer = nil;
  CALayer* transform_layer = nil;
  CALayer* content_layer1 = nil;
  CALayer* content_layer2 = nil;
  CALayer* content_layer3 = nil;

  // Validate the initial values.
  {
    UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    root_layer = [[superlayer_ sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    clip_and_sorting_layer = [[root_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    transform_layer = [[clip_and_sorting_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[transform_layer sublayers] count]);
    content_layer1 = [[transform_layer sublayers] objectAtIndex:0];

    // Validate the content layer.
    EXPECT_FALSE([content_layer1
        isKindOfClass:NSClassFromString(@"AVSampleBufferDisplayLayer")]);
  }

  properties.io_surface.reset(gfx::CreateIOSurface(
      gfx::Size(256, 256), gfx::BufferFormat::YUV_420_BIPLANAR));

  // Pass another frame.
  {
    UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    root_layer = [[superlayer_ sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    clip_and_sorting_layer = [[root_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    transform_layer = [[clip_and_sorting_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[transform_layer sublayers] count]);
    content_layer2 = [[transform_layer sublayers] objectAtIndex:0];

    // Validate the content layer.
    EXPECT_FALSE([content_layer2
        isKindOfClass:NSClassFromString(@"AVSampleBufferDisplayLayer")]);
    EXPECT_EQ(content_layer2, content_layer1);
  }

  properties.io_surface.reset(gfx::CreateIOSurface(
      gfx::Size(256, 256), gfx::BufferFormat::YUV_420_BIPLANAR));
  CVPixelBufferCreateWithIOSurface(nullptr, properties.io_surface, nullptr,
                                   properties.cv_pixel_buffer.InitializeInto());

  // Pass a frame with a CVPixelBuffer
  {
    UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    root_layer = [[superlayer_ sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    clip_and_sorting_layer = [[root_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    transform_layer = [[clip_and_sorting_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[transform_layer sublayers] count]);
    content_layer2 = [[transform_layer sublayers] objectAtIndex:0];

    // Validate the content layer.
    EXPECT_TRUE([content_layer2
        isKindOfClass:NSClassFromString(@"AVSampleBufferDisplayLayer")]);
    EXPECT_NE(content_layer2, content_layer1);
  }

  properties.io_surface.reset(gfx::CreateIOSurface(
      gfx::Size(256, 256), gfx::BufferFormat::YUV_420_BIPLANAR));
  properties.cv_pixel_buffer.reset();

  // Pass a frame that is clipped.
  properties.contents_rect = gfx::RectF(0, 0, 1, 0.9);
  {
    UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    root_layer = [[superlayer_ sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    clip_and_sorting_layer = [[root_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    transform_layer = [[clip_and_sorting_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[transform_layer sublayers] count]);
    content_layer3 = [[transform_layer sublayers] objectAtIndex:0];

    // Validate the content layer.
    EXPECT_FALSE([content_layer3
        isKindOfClass:NSClassFromString(@"AVSampleBufferDisplayLayer")]);
    EXPECT_NE(content_layer3, content_layer2);
  }
}

// Test fullscreen low power detection.
TEST_F(CALayerTreeTest, FullscreenLowPower) {
  CALayerProperties properties;
  properties.io_surface.reset(gfx::CreateIOSurface(
      gfx::Size(256, 256), gfx::BufferFormat::YUV_420_BIPLANAR));
  properties.is_clipped = false;
  CVPixelBufferCreateWithIOSurface(nullptr, properties.io_surface, nullptr,
                                   properties.cv_pixel_buffer.InitializeInto());

  CALayerProperties properties_black;
  properties_black.is_clipped = false;
  properties_black.background_color = SK_ColorBLACK;
  CALayerProperties properties_white;
  properties_white.is_clipped = false;
  properties_white.background_color = SK_ColorWHITE;

  std::unique_ptr<ui::CARendererLayerTree> ca_layer_tree;

  // Test a configuration with no background.
  {
    std::unique_ptr<ui::CARendererLayerTree> new_ca_layer_tree(
        new ui::CARendererLayerTree);
    bool result = ScheduleCALayer(new_ca_layer_tree.get(), &properties);
    EXPECT_TRUE(result);
    new_ca_layer_tree->CommitScheduledCALayers(
        superlayer_, std::move(ca_layer_tree), properties.scale_factor);
    bool fullscreen_low_power_valid =
        new_ca_layer_tree->CommitFullscreenLowPowerLayer(
            fullscreen_low_power_layer_);
    std::swap(new_ca_layer_tree, ca_layer_tree);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    CALayer* root_layer = [[superlayer_ sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    CALayer* clip_and_sorting_layer = [[root_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    CALayer* transform_layer =
        [[clip_and_sorting_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[transform_layer sublayers] count]);
    CALayer* content_layer = [[transform_layer sublayers] objectAtIndex:0];

    // Validate the content layer and fullscreen low power mode.
    EXPECT_TRUE([content_layer
        isKindOfClass:NSClassFromString(@"AVSampleBufferDisplayLayer")]);
    EXPECT_TRUE(fullscreen_low_power_valid);
  }

  // Test a configuration with a black background.
  {
    std::unique_ptr<ui::CARendererLayerTree> new_ca_layer_tree(
        new ui::CARendererLayerTree);
    bool result = ScheduleCALayer(new_ca_layer_tree.get(), &properties_black);
    EXPECT_TRUE(result);
    result = ScheduleCALayer(new_ca_layer_tree.get(), &properties);
    EXPECT_TRUE(result);
    new_ca_layer_tree->CommitScheduledCALayers(
        superlayer_, std::move(ca_layer_tree), properties.scale_factor);
    bool fullscreen_low_power_valid =
        new_ca_layer_tree->CommitFullscreenLowPowerLayer(
            fullscreen_low_power_layer_);
    std::swap(new_ca_layer_tree, ca_layer_tree);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    CALayer* root_layer = [[superlayer_ sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    CALayer* clip_and_sorting_layer = [[root_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    CALayer* transform_layer =
        [[clip_and_sorting_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(2u, [[transform_layer sublayers] count]);
    CALayer* content_layer = [[transform_layer sublayers] objectAtIndex:1];

    // Validate the content layer and fullscreen low power mode.
    EXPECT_TRUE([content_layer
        isKindOfClass:NSClassFromString(@"AVSampleBufferDisplayLayer")]);
    EXPECT_TRUE(fullscreen_low_power_valid);
  }

  // Test a configuration with a white background. It will fail.
  {
    std::unique_ptr<ui::CARendererLayerTree> new_ca_layer_tree(
        new ui::CARendererLayerTree);
    bool result = ScheduleCALayer(new_ca_layer_tree.get(), &properties_white);
    EXPECT_TRUE(result);
    result = ScheduleCALayer(new_ca_layer_tree.get(), &properties);
    EXPECT_TRUE(result);
    new_ca_layer_tree->CommitScheduledCALayers(
        superlayer_, std::move(ca_layer_tree), properties.scale_factor);
    bool fullscreen_low_power_valid =
        new_ca_layer_tree->CommitFullscreenLowPowerLayer(
            fullscreen_low_power_layer_);
    std::swap(new_ca_layer_tree, ca_layer_tree);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    CALayer* root_layer = [[superlayer_ sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    CALayer* clip_and_sorting_layer = [[root_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    CALayer* transform_layer =
        [[clip_and_sorting_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(2u, [[transform_layer sublayers] count]);
    CALayer* content_layer = [[transform_layer sublayers] objectAtIndex:1];

    // Validate the content layer and fullscreen low power mode.
    EXPECT_TRUE([content_layer
        isKindOfClass:NSClassFromString(@"AVSampleBufferDisplayLayer")]);
    EXPECT_FALSE(fullscreen_low_power_valid);
  }

  // Test a configuration with a black foreground. It too will fail.
  {
    std::unique_ptr<ui::CARendererLayerTree> new_ca_layer_tree(
        new ui::CARendererLayerTree);
    bool result = ScheduleCALayer(new_ca_layer_tree.get(), &properties);
    EXPECT_TRUE(result);
    result = ScheduleCALayer(new_ca_layer_tree.get(), &properties_black);
    EXPECT_TRUE(result);
    new_ca_layer_tree->CommitScheduledCALayers(
        superlayer_, std::move(ca_layer_tree), properties.scale_factor);
    bool fullscreen_low_power_valid =
        new_ca_layer_tree->CommitFullscreenLowPowerLayer(
            fullscreen_low_power_layer_);
    std::swap(new_ca_layer_tree, ca_layer_tree);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    CALayer* root_layer = [[superlayer_ sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    CALayer* clip_and_sorting_layer = [[root_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    CALayer* transform_layer =
        [[clip_and_sorting_layer sublayers] objectAtIndex:0];
    EXPECT_EQ(2u, [[transform_layer sublayers] count]);
    CALayer* content_layer = [[transform_layer sublayers] objectAtIndex:0];

    // Validate the content layer and fullscreen low power mode.
    EXPECT_TRUE([content_layer
        isKindOfClass:NSClassFromString(@"AVSampleBufferDisplayLayer")]);
    EXPECT_FALSE(fullscreen_low_power_valid);
  }
}

}  // namespace gpu
