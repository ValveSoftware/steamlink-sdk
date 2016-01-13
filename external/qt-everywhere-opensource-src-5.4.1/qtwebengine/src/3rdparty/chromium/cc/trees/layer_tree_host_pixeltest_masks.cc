// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "cc/layers/content_layer.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/image_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/test/layer_tree_pixel_test.h"
#include "cc/test/pixel_comparator.h"

#if !defined(OS_ANDROID)

namespace cc {
namespace {

class LayerTreeHostMasksPixelTest : public LayerTreePixelTest {};

class MaskContentLayerClient : public ContentLayerClient {
 public:
  MaskContentLayerClient() {}
  virtual ~MaskContentLayerClient() {}

  virtual void DidChangeLayerCanUseLCDText() OVERRIDE {}

  virtual bool FillsBoundsCompletely() const OVERRIDE { return false; }

  virtual void PaintContents(
      SkCanvas* canvas,
      const gfx::Rect& rect,
      gfx::RectF* opaque_rect,
      ContentLayerClient::GraphicsContextStatus gc_status) OVERRIDE {
    SkPaint paint;
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(SkIntToScalar(2));
    paint.setColor(SK_ColorWHITE);

    canvas->clear(SK_ColorTRANSPARENT);
    gfx::Rect inset_rect(rect);
    while (!inset_rect.IsEmpty()) {
      inset_rect.Inset(3, 3, 2, 2);
      canvas->drawRect(
          SkRect::MakeXYWH(inset_rect.x(), inset_rect.y(),
                           inset_rect.width(), inset_rect.height()),
          paint);
      inset_rect.Inset(3, 3, 2, 2);
    }
  }
};

TEST_F(LayerTreeHostMasksPixelTest, MaskOfLayer) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayerWithBorder(
      gfx::Rect(50, 50, 100, 100), kCSSGreen, 1, SK_ColorBLACK);
  background->AddChild(green);

  MaskContentLayerClient client;
  scoped_refptr<ContentLayer> mask = ContentLayer::Create(&client);
  mask->SetBounds(gfx::Size(100, 100));
  mask->SetIsDrawable(true);
  mask->SetIsMask(true);
  green->SetMaskLayer(mask.get());

  this->impl_side_painting_ = false;
  RunPixelTest(GL_WITH_BITMAP,
               background,
               base::FilePath(FILE_PATH_LITERAL("mask_of_layer.png")));
}

TEST_F(LayerTreeHostMasksPixelTest, ImageMaskOfLayer) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<ImageLayer> mask = ImageLayer::Create();
  mask->SetIsDrawable(true);
  mask->SetIsMask(true);
  mask->SetBounds(gfx::Size(100, 100));

  SkBitmap bitmap;
  bitmap.allocN32Pixels(400, 400);
  SkCanvas canvas(bitmap);
  canvas.scale(SkIntToScalar(4), SkIntToScalar(4));
  MaskContentLayerClient client;
  client.PaintContents(&canvas,
                       gfx::Rect(100, 100),
                       NULL,
                       ContentLayerClient::GRAPHICS_CONTEXT_ENABLED);
  mask->SetBitmap(bitmap);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayerWithBorder(
      gfx::Rect(50, 50, 100, 100), kCSSGreen, 1, SK_ColorBLACK);
  green->SetMaskLayer(mask.get());
  background->AddChild(green);

  this->impl_side_painting_ = false;
  RunPixelTest(GL_WITH_BITMAP,
               background,
               base::FilePath(FILE_PATH_LITERAL("image_mask_of_layer.png")));
}

TEST_F(LayerTreeHostMasksPixelTest, MaskOfClippedLayer) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  // Clip to the top half of the green layer.
  scoped_refptr<Layer> clip = Layer::Create();
  clip->SetPosition(gfx::Point(0, 0));
  clip->SetBounds(gfx::Size(200, 100));
  clip->SetMasksToBounds(true);
  background->AddChild(clip);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayerWithBorder(
      gfx::Rect(50, 50, 100, 100), kCSSGreen, 1, SK_ColorBLACK);
  clip->AddChild(green);

  MaskContentLayerClient client;
  scoped_refptr<ContentLayer> mask = ContentLayer::Create(&client);
  mask->SetBounds(gfx::Size(100, 100));
  mask->SetIsDrawable(true);
  mask->SetIsMask(true);
  green->SetMaskLayer(mask.get());

  this->impl_side_painting_ = false;
  RunPixelTest(GL_WITH_BITMAP,
               background,
               base::FilePath(FILE_PATH_LITERAL("mask_of_clipped_layer.png")));
}

TEST_F(LayerTreeHostMasksPixelTest, MaskWithReplica) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  MaskContentLayerClient client;
  scoped_refptr<ContentLayer> mask = ContentLayer::Create(&client);
  mask->SetBounds(gfx::Size(100, 100));
  mask->SetIsDrawable(true);
  mask->SetIsMask(true);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayerWithBorder(
      gfx::Rect(0, 0, 100, 100), kCSSGreen, 1, SK_ColorBLACK);
  background->AddChild(green);
  green->SetMaskLayer(mask.get());

  gfx::Transform replica_transform;
  replica_transform.Rotate(-90.0);

  scoped_refptr<Layer> replica = Layer::Create();
  replica->SetTransformOrigin(gfx::Point3F(50.f, 50.f, 0.f));
  replica->SetPosition(gfx::Point(100, 100));
  replica->SetTransform(replica_transform);
  green->SetReplicaLayer(replica.get());

  this->impl_side_painting_ = false;
  RunPixelTest(GL_WITH_BITMAP,
               background,
               base::FilePath(FILE_PATH_LITERAL("mask_with_replica.png")));
}

TEST_F(LayerTreeHostMasksPixelTest, MaskWithReplicaOfClippedLayer) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  MaskContentLayerClient client;
  scoped_refptr<ContentLayer> mask = ContentLayer::Create(&client);
  mask->SetBounds(gfx::Size(100, 100));
  mask->SetIsDrawable(true);
  mask->SetIsMask(true);

  // Clip to the bottom half of the green layer, and the left half of the
  // replica.
  scoped_refptr<Layer> clip = Layer::Create();
  clip->SetPosition(gfx::Point(0, 50));
  clip->SetBounds(gfx::Size(150, 150));
  clip->SetMasksToBounds(true);
  background->AddChild(clip);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayerWithBorder(
      gfx::Rect(0, -50, 100, 100), kCSSGreen, 1, SK_ColorBLACK);
  clip->AddChild(green);
  green->SetMaskLayer(mask.get());

  gfx::Transform replica_transform;
  replica_transform.Rotate(-90.0);

  scoped_refptr<Layer> replica = Layer::Create();
  replica->SetTransformOrigin(gfx::Point3F(50.f, 50.f, 0.f));
  replica->SetPosition(gfx::Point(100, 100));
  replica->SetTransform(replica_transform);
  green->SetReplicaLayer(replica.get());

  this->impl_side_painting_ = false;
  RunPixelTest(GL_WITH_BITMAP,
               background,
               base::FilePath(FILE_PATH_LITERAL(
                   "mask_with_replica_of_clipped_layer.png")));
}

TEST_F(LayerTreeHostMasksPixelTest, MaskOfReplica) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  MaskContentLayerClient client;
  scoped_refptr<ContentLayer> mask = ContentLayer::Create(&client);
  mask->SetBounds(gfx::Size(100, 100));
  mask->SetIsDrawable(true);
  mask->SetIsMask(true);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayerWithBorder(
      gfx::Rect(50, 0, 100, 100), kCSSGreen, 1, SK_ColorBLACK);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> orange = CreateSolidColorLayer(
      gfx::Rect(-50, 50, 50, 50), kCSSOrange);
  green->AddChild(orange);

  gfx::Transform replica_transform;
  replica_transform.Rotate(180.0);
  replica_transform.Translate(100.0, 0.0);

  scoped_refptr<Layer> replica = Layer::Create();
  replica->SetTransformOrigin(gfx::Point3F(100.f, 100.f, 0.f));
  replica->SetPosition(gfx::Point());
  replica->SetTransform(replica_transform);
  replica->SetMaskLayer(mask.get());
  green->SetReplicaLayer(replica.get());

  this->impl_side_painting_ = false;
  RunPixelTest(GL_WITH_BITMAP,
               background,
               base::FilePath(FILE_PATH_LITERAL("mask_of_replica.png")));
}

TEST_F(LayerTreeHostMasksPixelTest, MaskOfReplicaOfClippedLayer) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  MaskContentLayerClient client;
  scoped_refptr<ContentLayer> mask = ContentLayer::Create(&client);
  mask->SetBounds(gfx::Size(100, 100));
  mask->SetIsDrawable(true);
  mask->SetIsMask(true);

  // Clip to the bottom 3/4 of the green layer, and the top 3/4 of the replica.
  scoped_refptr<Layer> clip = Layer::Create();
  clip->SetPosition(gfx::Point(0, 25));
  clip->SetBounds(gfx::Size(200, 150));
  clip->SetMasksToBounds(true);
  background->AddChild(clip);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayerWithBorder(
      gfx::Rect(50, -25, 100, 100), kCSSGreen, 1, SK_ColorBLACK);
  clip->AddChild(green);

  scoped_refptr<SolidColorLayer> orange = CreateSolidColorLayer(
      gfx::Rect(-50, 50, 50, 50), kCSSOrange);
  green->AddChild(orange);

  gfx::Transform replica_transform;
  replica_transform.Rotate(180.0);
  replica_transform.Translate(100.0, 0.0);

  scoped_refptr<Layer> replica = Layer::Create();
  replica->SetTransformOrigin(gfx::Point3F(100.f, 100.f, 0.f));
  replica->SetPosition(gfx::Point());
  replica->SetTransform(replica_transform);
  replica->SetMaskLayer(mask.get());
  green->SetReplicaLayer(replica.get());

  this->impl_side_painting_ = false;
  RunPixelTest(GL_WITH_BITMAP,
               background,
               base::FilePath(FILE_PATH_LITERAL(
                   "mask_of_replica_of_clipped_layer.png")));
}

}  // namespace
}  // namespace cc

#endif  // OS_ANDROID
