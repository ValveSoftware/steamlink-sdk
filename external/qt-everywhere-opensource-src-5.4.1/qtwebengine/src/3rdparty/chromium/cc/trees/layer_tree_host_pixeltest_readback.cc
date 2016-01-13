// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "cc/layers/content_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/texture_layer.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/test/layer_tree_pixel_test.h"
#include "cc/test/paths.h"
#include "cc/test/solid_color_content_layer_client.h"
#include "cc/trees/layer_tree_impl.h"

#if !defined(OS_ANDROID)

namespace cc {
namespace {

class LayerTreeHostReadbackPixelTest : public LayerTreePixelTest {
 protected:
  LayerTreeHostReadbackPixelTest()
      : insert_copy_request_after_frame_count_(0) {}

  virtual scoped_ptr<CopyOutputRequest> CreateCopyOutputRequest() OVERRIDE {
    scoped_ptr<CopyOutputRequest> request;

    switch (test_type_) {
      case GL_WITH_BITMAP:
      case SOFTWARE_WITH_BITMAP:
        request = CopyOutputRequest::CreateBitmapRequest(
            base::Bind(&LayerTreeHostReadbackPixelTest::ReadbackResultAsBitmap,
                       base::Unretained(this)));
        break;
      case SOFTWARE_WITH_DEFAULT:
        request = CopyOutputRequest::CreateRequest(
            base::Bind(&LayerTreeHostReadbackPixelTest::ReadbackResultAsBitmap,
                       base::Unretained(this)));
        break;
      case GL_WITH_DEFAULT:
        request = CopyOutputRequest::CreateRequest(
            base::Bind(&LayerTreeHostReadbackPixelTest::ReadbackResultAsTexture,
                       base::Unretained(this)));
        break;
    }

    if (!copy_subrect_.IsEmpty())
      request->set_area(copy_subrect_);
    return request.Pass();
  }

  virtual void BeginTest() OVERRIDE {
    if (insert_copy_request_after_frame_count_ == 0) {
      Layer* const target =
          readback_target_ ? readback_target_ : layer_tree_host()->root_layer();
      target->RequestCopyOfOutput(CreateCopyOutputRequest().Pass());
    }
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    if (insert_copy_request_after_frame_count_ ==
        layer_tree_host()->source_frame_number()) {
      Layer* const target =
          readback_target_ ? readback_target_ : layer_tree_host()->root_layer();
      target->RequestCopyOfOutput(CreateCopyOutputRequest().Pass());
    }
  }

  void ReadbackResultAsBitmap(scoped_ptr<CopyOutputResult> result) {
    EXPECT_TRUE(proxy()->IsMainThread());
    EXPECT_TRUE(result->HasBitmap());
    result_bitmap_ = result->TakeBitmap().Pass();
    EndTest();
  }

  void ReadbackResultAsTexture(scoped_ptr<CopyOutputResult> result) {
    EXPECT_TRUE(proxy()->IsMainThread());
    EXPECT_TRUE(result->HasTexture());

    TextureMailbox texture_mailbox;
    scoped_ptr<SingleReleaseCallback> release_callback;
    result->TakeTexture(&texture_mailbox, &release_callback);
    EXPECT_TRUE(texture_mailbox.IsValid());
    EXPECT_TRUE(texture_mailbox.IsTexture());

    scoped_ptr<SkBitmap> bitmap =
        CopyTextureMailboxToBitmap(result->size(), texture_mailbox);
    release_callback->Run(0, false);

    ReadbackResultAsBitmap(CopyOutputResult::CreateBitmapResult(bitmap.Pass()));
  }

  gfx::Rect copy_subrect_;
  int insert_copy_request_after_frame_count_;
};

void IgnoreReadbackResult(scoped_ptr<CopyOutputResult> result) {}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackRootLayer_Software) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTest(SOFTWARE_WITH_DEFAULT,
               background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackRootLayer_Software_Bitmap) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTest(SOFTWARE_WITH_BITMAP,
               background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackRootLayer_GL_Bitmap) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTest(GL_WITH_BITMAP,
               background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackRootLayer_GL) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTest(GL_WITH_DEFAULT,
               background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest,
       ReadbackRootLayerWithChild_Software) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(150, 150, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  RunPixelTest(SOFTWARE_WITH_DEFAULT,
               background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackRootLayerWithChild_GL_Bitmap) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(150, 150, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  RunPixelTest(GL_WITH_BITMAP,
               background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackRootLayerWithChild_GL) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(150, 150, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  RunPixelTest(GL_WITH_DEFAULT,
               background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackNonRootLayer_Software) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTestWithReadbackTarget(SOFTWARE_WITH_DEFAULT,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackNonRootLayer_GL_Bitmap) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTestWithReadbackTarget(GL_WITH_BITMAP,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackNonRootLayer_GL) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTestWithReadbackTarget(GL_WITH_DEFAULT,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest,
       ReadbackSmallNonRootLayer_Software) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(100, 100, 100, 100), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTestWithReadbackTarget(SOFTWARE_WITH_DEFAULT,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackSmallNonRootLayer_GL_Bitmap) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(100, 100, 100, 100), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTestWithReadbackTarget(GL_WITH_BITMAP,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackSmallNonRootLayer_GL) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(100, 100, 100, 100), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTestWithReadbackTarget(GL_WITH_DEFAULT,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest,
       ReadbackSmallNonRootLayerWithChild_Software) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(100, 100, 100, 100), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(50, 50, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  RunPixelTestWithReadbackTarget(SOFTWARE_WITH_DEFAULT,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest,
       ReadbackSmallNonRootLayerWithChild_GL_Bitmap) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(100, 100, 100, 100), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(50, 50, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  RunPixelTestWithReadbackTarget(GL_WITH_BITMAP,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackSmallNonRootLayerWithChild_GL) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(100, 100, 100, 100), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(50, 50, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  RunPixelTestWithReadbackTarget(GL_WITH_DEFAULT,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest,
       ReadbackSubtreeSurroundsTargetLayer_Software) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(0, 0, 200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> target = CreateSolidColorLayer(
      gfx::Rect(100, 100, 100, 100), SK_ColorRED);
  background->AddChild(target);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(-100, -100, 300, 300), SK_ColorGREEN);
  target->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(50, 50, 50, 50), SK_ColorBLUE);
  target->AddChild(blue);

  copy_subrect_ = gfx::Rect(0, 0, 100, 100);
  RunPixelTestWithReadbackTarget(SOFTWARE_WITH_DEFAULT,
                                 background,
                                 target.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest,
       ReadbackSubtreeSurroundsLayer_GL_Bitmap) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(0, 0, 200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> target = CreateSolidColorLayer(
      gfx::Rect(100, 100, 100, 100), SK_ColorRED);
  background->AddChild(target);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(-100, -100, 300, 300), SK_ColorGREEN);
  target->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(50, 50, 50, 50), SK_ColorBLUE);
  target->AddChild(blue);

  copy_subrect_ = gfx::Rect(0, 0, 100, 100);
  RunPixelTestWithReadbackTarget(GL_WITH_BITMAP,
                                 background,
                                 target.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest,
       ReadbackSubtreeSurroundsTargetLayer_GL) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(0, 0, 200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> target = CreateSolidColorLayer(
      gfx::Rect(100, 100, 100, 100), SK_ColorRED);
  background->AddChild(target);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(-100, -100, 300, 300), SK_ColorGREEN);
  target->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(50, 50, 50, 50), SK_ColorBLUE);
  target->AddChild(blue);

  copy_subrect_ = gfx::Rect(0, 0, 100, 100);
  RunPixelTestWithReadbackTarget(GL_WITH_DEFAULT,
                                 background,
                                 target.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest,
       ReadbackSubtreeExtendsBeyondTargetLayer_Software) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(0, 0, 200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> target = CreateSolidColorLayer(
      gfx::Rect(50, 50, 150, 150), SK_ColorRED);
  background->AddChild(target);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(50, 50, 200, 200), SK_ColorGREEN);
  target->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(100, 100, 50, 50), SK_ColorBLUE);
  target->AddChild(blue);

  copy_subrect_ = gfx::Rect(50, 50, 100, 100);
  RunPixelTestWithReadbackTarget(SOFTWARE_WITH_DEFAULT,
                                 background,
                                 target.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest,
       ReadbackSubtreeExtendsBeyondTargetLayer_GL_Bitmap) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(0, 0, 200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> target = CreateSolidColorLayer(
      gfx::Rect(50, 50, 150, 150), SK_ColorRED);
  background->AddChild(target);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(50, 50, 200, 200), SK_ColorGREEN);
  target->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(100, 100, 50, 50), SK_ColorBLUE);
  target->AddChild(blue);

  copy_subrect_ = gfx::Rect(50, 50, 100, 100);
  RunPixelTestWithReadbackTarget(GL_WITH_BITMAP,
                                 background,
                                 target.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest,
       ReadbackSubtreeExtendsBeyondTargetLayer_GL) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(0, 0, 200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> target = CreateSolidColorLayer(
      gfx::Rect(50, 50, 150, 150), SK_ColorRED);
  background->AddChild(target);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(50, 50, 200, 200), SK_ColorGREEN);
  target->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(100, 100, 50, 50), SK_ColorBLUE);
  target->AddChild(blue);

  copy_subrect_ = gfx::Rect(50, 50, 100, 100);
  RunPixelTestWithReadbackTarget(GL_WITH_DEFAULT,
                                 background,
                                 target.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackHiddenSubtree_Software) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorBLACK);

  scoped_refptr<SolidColorLayer> hidden_target =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorGREEN);
  hidden_target->SetHideLayerAndSubtree(true);
  background->AddChild(hidden_target);

  scoped_refptr<SolidColorLayer> blue =
      CreateSolidColorLayer(gfx::Rect(150, 150, 50, 50), SK_ColorBLUE);
  hidden_target->AddChild(blue);

  RunPixelTestWithReadbackTarget(
      SOFTWARE_WITH_DEFAULT,
      background,
      hidden_target.get(),
      base::FilePath(FILE_PATH_LITERAL("green_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackHiddenSubtree_GL_Bitmap) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorBLACK);

  scoped_refptr<SolidColorLayer> hidden_target =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorGREEN);
  hidden_target->SetHideLayerAndSubtree(true);
  background->AddChild(hidden_target);

  scoped_refptr<SolidColorLayer> blue =
      CreateSolidColorLayer(gfx::Rect(150, 150, 50, 50), SK_ColorBLUE);
  hidden_target->AddChild(blue);

  RunPixelTestWithReadbackTarget(
      GL_WITH_BITMAP,
      background,
      hidden_target.get(),
      base::FilePath(FILE_PATH_LITERAL("green_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackHiddenSubtree_GL) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorBLACK);

  scoped_refptr<SolidColorLayer> hidden_target =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorGREEN);
  hidden_target->SetHideLayerAndSubtree(true);
  background->AddChild(hidden_target);

  scoped_refptr<SolidColorLayer> blue =
      CreateSolidColorLayer(gfx::Rect(150, 150, 50, 50), SK_ColorBLUE);
  hidden_target->AddChild(blue);

  RunPixelTestWithReadbackTarget(
      GL_WITH_DEFAULT,
      background,
      hidden_target.get(),
      base::FilePath(FILE_PATH_LITERAL("green_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest,
       HiddenSubtreeNotVisibleWhenDrawnForReadback_Software) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorBLACK);

  scoped_refptr<SolidColorLayer> hidden_target =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorGREEN);
  hidden_target->SetHideLayerAndSubtree(true);
  background->AddChild(hidden_target);

  scoped_refptr<SolidColorLayer> blue =
      CreateSolidColorLayer(gfx::Rect(150, 150, 50, 50), SK_ColorBLUE);
  hidden_target->AddChild(blue);

  hidden_target->RequestCopyOfOutput(CopyOutputRequest::CreateBitmapRequest(
      base::Bind(&IgnoreReadbackResult)));
  RunPixelTest(SOFTWARE_WITH_DEFAULT,
               background,
               base::FilePath(FILE_PATH_LITERAL("black.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest,
       HiddenSubtreeNotVisibleWhenDrawnForReadback_GL_Bitmap) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorBLACK);

  scoped_refptr<SolidColorLayer> hidden_target =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorGREEN);
  hidden_target->SetHideLayerAndSubtree(true);
  background->AddChild(hidden_target);

  scoped_refptr<SolidColorLayer> blue =
      CreateSolidColorLayer(gfx::Rect(150, 150, 50, 50), SK_ColorBLUE);
  hidden_target->AddChild(blue);

  hidden_target->RequestCopyOfOutput(CopyOutputRequest::CreateBitmapRequest(
      base::Bind(&IgnoreReadbackResult)));
  RunPixelTest(GL_WITH_BITMAP,
               background,
               base::FilePath(FILE_PATH_LITERAL("black.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest,
       HiddenSubtreeNotVisibleWhenDrawnForReadback_GL) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorBLACK);

  scoped_refptr<SolidColorLayer> hidden_target =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorGREEN);
  hidden_target->SetHideLayerAndSubtree(true);
  background->AddChild(hidden_target);

  scoped_refptr<SolidColorLayer> blue =
      CreateSolidColorLayer(gfx::Rect(150, 150, 50, 50), SK_ColorBLUE);
  hidden_target->AddChild(blue);

  hidden_target->RequestCopyOfOutput(CopyOutputRequest::CreateBitmapRequest(
      base::Bind(&IgnoreReadbackResult)));
  RunPixelTest(GL_WITH_DEFAULT,
               background,
               base::FilePath(FILE_PATH_LITERAL("black.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackSubrect_Software) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(100, 100, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  // Grab the middle of the root layer.
  copy_subrect_ = gfx::Rect(50, 50, 100, 100);

  RunPixelTest(SOFTWARE_WITH_DEFAULT,
               background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackSubrect_GL_Bitmap) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(100, 100, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  // Grab the middle of the root layer.
  copy_subrect_ = gfx::Rect(50, 50, 100, 100);

  RunPixelTest(GL_WITH_BITMAP,
               background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackSubrect_GL) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(100, 100, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  // Grab the middle of the root layer.
  copy_subrect_ = gfx::Rect(50, 50, 100, 100);

  RunPixelTest(GL_WITH_DEFAULT,
               background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackNonRootLayerSubrect_Software) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(25, 25, 150, 150), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(75, 75, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  // Grab the middle of the green layer.
  copy_subrect_ = gfx::Rect(25, 25, 100, 100);

  RunPixelTestWithReadbackTarget(SOFTWARE_WITH_DEFAULT,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackNonRootLayerSubrect_GL_Bitmap) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(25, 25, 150, 150), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(75, 75, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  // Grab the middle of the green layer.
  copy_subrect_ = gfx::Rect(25, 25, 100, 100);

  RunPixelTestWithReadbackTarget(GL_WITH_BITMAP,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackNonRootLayerSubrect_GL) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(25, 25, 150, 150), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(75, 75, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  // Grab the middle of the green layer.
  copy_subrect_ = gfx::Rect(25, 25, 100, 100);

  RunPixelTestWithReadbackTarget(GL_WITH_DEFAULT,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackWhenNoDamage_Software) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(0, 0, 200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> parent =
      CreateSolidColorLayer(gfx::Rect(0, 0, 150, 150), SK_ColorRED);
  background->AddChild(parent);

  scoped_refptr<SolidColorLayer> target =
      CreateSolidColorLayer(gfx::Rect(0, 0, 100, 100), SK_ColorGREEN);
  parent->AddChild(target);

  scoped_refptr<SolidColorLayer> blue =
      CreateSolidColorLayer(gfx::Rect(50, 50, 50, 50), SK_ColorBLUE);
  target->AddChild(blue);

  insert_copy_request_after_frame_count_ = 1;
  RunPixelTestWithReadbackTarget(
      SOFTWARE_WITH_DEFAULT,
      background,
      target.get(),
      base::FilePath(FILE_PATH_LITERAL("green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackWhenNoDamage_GL_Bitmap) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(0, 0, 200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> parent =
      CreateSolidColorLayer(gfx::Rect(0, 0, 150, 150), SK_ColorRED);
  background->AddChild(parent);

  scoped_refptr<SolidColorLayer> target =
      CreateSolidColorLayer(gfx::Rect(0, 0, 100, 100), SK_ColorGREEN);
  parent->AddChild(target);

  scoped_refptr<SolidColorLayer> blue =
      CreateSolidColorLayer(gfx::Rect(50, 50, 50, 50), SK_ColorBLUE);
  target->AddChild(blue);

  insert_copy_request_after_frame_count_ = 1;
  RunPixelTestWithReadbackTarget(
      GL_WITH_BITMAP,
      background,
      target.get(),
      base::FilePath(FILE_PATH_LITERAL("green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackWhenNoDamage_GL) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(0, 0, 200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> parent =
      CreateSolidColorLayer(gfx::Rect(0, 0, 150, 150), SK_ColorRED);
  background->AddChild(parent);

  scoped_refptr<SolidColorLayer> target =
      CreateSolidColorLayer(gfx::Rect(0, 0, 100, 100), SK_ColorGREEN);
  parent->AddChild(target);

  scoped_refptr<SolidColorLayer> blue =
      CreateSolidColorLayer(gfx::Rect(50, 50, 50, 50), SK_ColorBLUE);
  target->AddChild(blue);

  insert_copy_request_after_frame_count_ = 1;
  RunPixelTestWithReadbackTarget(
      GL_WITH_DEFAULT,
      background,
      target.get(),
      base::FilePath(FILE_PATH_LITERAL("green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest,
       ReadbackOutsideViewportWhenNoDamage_Software) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(0, 0, 200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> parent =
      CreateSolidColorLayer(gfx::Rect(0, 0, 200, 200), SK_ColorRED);
  EXPECT_FALSE(parent->masks_to_bounds());
  background->AddChild(parent);

  scoped_refptr<SolidColorLayer> target =
      CreateSolidColorLayer(gfx::Rect(250, 250, 100, 100), SK_ColorGREEN);
  parent->AddChild(target);

  scoped_refptr<SolidColorLayer> blue =
      CreateSolidColorLayer(gfx::Rect(50, 50, 50, 50), SK_ColorBLUE);
  target->AddChild(blue);

  insert_copy_request_after_frame_count_ = 1;
  RunPixelTestWithReadbackTarget(
      SOFTWARE_WITH_DEFAULT,
      background,
      target.get(),
      base::FilePath(FILE_PATH_LITERAL("green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest,
       ReadbackOutsideViewportWhenNoDamage_GL_Bitmap) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(0, 0, 200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> parent =
      CreateSolidColorLayer(gfx::Rect(0, 0, 200, 200), SK_ColorRED);
  EXPECT_FALSE(parent->masks_to_bounds());
  background->AddChild(parent);

  scoped_refptr<SolidColorLayer> target =
      CreateSolidColorLayer(gfx::Rect(250, 250, 100, 100), SK_ColorGREEN);
  parent->AddChild(target);

  scoped_refptr<SolidColorLayer> blue =
      CreateSolidColorLayer(gfx::Rect(50, 50, 50, 50), SK_ColorBLUE);
  target->AddChild(blue);

  insert_copy_request_after_frame_count_ = 1;
  RunPixelTestWithReadbackTarget(
      GL_WITH_BITMAP,
      background,
      target.get(),
      base::FilePath(FILE_PATH_LITERAL("green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackOutsideViewportWhenNoDamage_GL) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(0, 0, 200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> parent =
      CreateSolidColorLayer(gfx::Rect(0, 0, 200, 200), SK_ColorRED);
  EXPECT_FALSE(parent->masks_to_bounds());
  background->AddChild(parent);

  scoped_refptr<SolidColorLayer> target =
      CreateSolidColorLayer(gfx::Rect(250, 250, 100, 100), SK_ColorGREEN);
  parent->AddChild(target);

  scoped_refptr<SolidColorLayer> blue =
      CreateSolidColorLayer(gfx::Rect(50, 50, 50, 50), SK_ColorBLUE);
  target->AddChild(blue);

  insert_copy_request_after_frame_count_ = 1;
  RunPixelTestWithReadbackTarget(
      GL_WITH_DEFAULT,
      background,
      target.get(),
      base::FilePath(FILE_PATH_LITERAL("green_small_with_blue_corner.png")));
}

class LayerTreeHostReadbackDeviceScalePixelTest
    : public LayerTreeHostReadbackPixelTest {
 protected:
  LayerTreeHostReadbackDeviceScalePixelTest()
      : device_scale_factor_(1.f),
        white_client_(SK_ColorWHITE),
        green_client_(SK_ColorGREEN),
        blue_client_(SK_ColorBLUE) {}

  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    // Cause the device scale factor to be inherited by contents scales.
    settings->layer_transforms_should_scale_layer_contents = true;
  }

  virtual void SetupTree() OVERRIDE {
    layer_tree_host()->SetDeviceScaleFactor(device_scale_factor_);
    LayerTreePixelTest::SetupTree();
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    LayerImpl* root_impl = host_impl->active_tree()->root_layer();

    LayerImpl* background_impl = root_impl->children()[0];
    EXPECT_EQ(device_scale_factor_, background_impl->contents_scale_x());
    EXPECT_EQ(device_scale_factor_, background_impl->contents_scale_y());

    LayerImpl* green_impl = background_impl->children()[0];
    EXPECT_EQ(device_scale_factor_, green_impl->contents_scale_x());
    EXPECT_EQ(device_scale_factor_, green_impl->contents_scale_y());

    LayerImpl* blue_impl = green_impl->children()[0];
    EXPECT_EQ(device_scale_factor_, blue_impl->contents_scale_x());
    EXPECT_EQ(device_scale_factor_, blue_impl->contents_scale_y());
  }

  float device_scale_factor_;
  SolidColorContentLayerClient white_client_;
  SolidColorContentLayerClient green_client_;
  SolidColorContentLayerClient blue_client_;
};

TEST_F(LayerTreeHostReadbackDeviceScalePixelTest,
       ReadbackSubrect_Software) {
  scoped_refptr<ContentLayer> background = ContentLayer::Create(&white_client_);
  background->SetBounds(gfx::Size(100, 100));
  background->SetIsDrawable(true);

  scoped_refptr<ContentLayer> green = ContentLayer::Create(&green_client_);
  green->SetBounds(gfx::Size(100, 100));
  green->SetIsDrawable(true);
  background->AddChild(green);

  scoped_refptr<ContentLayer> blue = ContentLayer::Create(&blue_client_);
  blue->SetPosition(gfx::Point(50, 50));
  blue->SetBounds(gfx::Size(25, 25));
  blue->SetIsDrawable(true);
  green->AddChild(blue);

  // Grab the middle of the root layer.
  copy_subrect_ = gfx::Rect(25, 25, 50, 50);
  device_scale_factor_ = 2.f;

  this->impl_side_painting_ = false;
  RunPixelTest(SOFTWARE_WITH_DEFAULT,
               background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackDeviceScalePixelTest,
       ReadbackSubrect_GL) {
  scoped_refptr<ContentLayer> background = ContentLayer::Create(&white_client_);
  background->SetBounds(gfx::Size(100, 100));
  background->SetIsDrawable(true);

  scoped_refptr<ContentLayer> green = ContentLayer::Create(&green_client_);
  green->SetBounds(gfx::Size(100, 100));
  green->SetIsDrawable(true);
  background->AddChild(green);

  scoped_refptr<ContentLayer> blue = ContentLayer::Create(&blue_client_);
  blue->SetPosition(gfx::Point(50, 50));
  blue->SetBounds(gfx::Size(25, 25));
  blue->SetIsDrawable(true);
  green->AddChild(blue);

  // Grab the middle of the root layer.
  copy_subrect_ = gfx::Rect(25, 25, 50, 50);
  device_scale_factor_ = 2.f;

  this->impl_side_painting_ = false;
  RunPixelTest(GL_WITH_DEFAULT,
               background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackDeviceScalePixelTest,
       ReadbackNonRootLayerSubrect_Software) {
  scoped_refptr<ContentLayer> background = ContentLayer::Create(&white_client_);
  background->SetBounds(gfx::Size(100, 100));
  background->SetIsDrawable(true);

  scoped_refptr<ContentLayer> green = ContentLayer::Create(&green_client_);
  green->SetPosition(gfx::Point(10, 20));
  green->SetBounds(gfx::Size(90, 80));
  green->SetIsDrawable(true);
  background->AddChild(green);

  scoped_refptr<ContentLayer> blue = ContentLayer::Create(&blue_client_);
  blue->SetPosition(gfx::Point(50, 50));
  blue->SetBounds(gfx::Size(25, 25));
  blue->SetIsDrawable(true);
  green->AddChild(blue);

  // Grab the green layer's content with blue in the bottom right.
  copy_subrect_ = gfx::Rect(25, 25, 50, 50);
  device_scale_factor_ = 2.f;

  this->impl_side_painting_ = false;
  RunPixelTestWithReadbackTarget(SOFTWARE_WITH_DEFAULT,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackDeviceScalePixelTest,
       ReadbackNonRootLayerSubrect_GL) {
  scoped_refptr<ContentLayer> background = ContentLayer::Create(&white_client_);
  background->SetBounds(gfx::Size(100, 100));
  background->SetIsDrawable(true);

  scoped_refptr<ContentLayer> green = ContentLayer::Create(&green_client_);
  green->SetPosition(gfx::Point(10, 20));
  green->SetBounds(gfx::Size(90, 80));
  green->SetIsDrawable(true);
  background->AddChild(green);

  scoped_refptr<ContentLayer> blue = ContentLayer::Create(&blue_client_);
  blue->SetPosition(gfx::Point(50, 50));
  blue->SetBounds(gfx::Size(25, 25));
  blue->SetIsDrawable(true);
  green->AddChild(blue);

  // Grab the green layer's content with blue in the bottom right.
  copy_subrect_ = gfx::Rect(25, 25, 50, 50);
  device_scale_factor_ = 2.f;

  this->impl_side_painting_ = false;
  RunPixelTestWithReadbackTarget(GL_WITH_DEFAULT,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackNonRootLayerOutsideViewport) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  // Only the top left quarter of the layer is inside the viewport, so the
  // blue layer is entirely outside.
  green->SetPosition(gfx::Point(100, 100));
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(150, 150, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  RunPixelTestWithReadbackTarget(GL_WITH_DEFAULT,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_with_blue_corner.png")));
}

}  // namespace
}  // namespace cc

#endif  // OS_ANDROID
