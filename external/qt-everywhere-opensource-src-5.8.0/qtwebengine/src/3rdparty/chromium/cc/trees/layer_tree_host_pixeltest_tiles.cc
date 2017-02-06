// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "cc/layers/content_layer_client.h"
#include "cc/layers/picture_layer.h"
#include "cc/output/copy_output_request.h"
#include "cc/playback/display_item_list.h"
#include "cc/playback/display_item_list_settings.h"
#include "cc/playback/drawing_display_item.h"
#include "cc/test/layer_tree_pixel_test.h"
#include "cc/test/test_gpu_memory_buffer_manager.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"

#if !defined(OS_ANDROID)

namespace cc {
namespace {

enum RasterMode {
  PARTIAL_ONE_COPY,
  FULL_ONE_COPY,
  PARTIAL_GPU,
  FULL_GPU,
  PARTIAL_BITMAP,
  FULL_BITMAP,
};

class LayerTreeHostTilesPixelTest : public LayerTreePixelTest {
 protected:
  void InitializeSettings(LayerTreeSettings* settings) override {
    LayerTreePixelTest::InitializeSettings(settings);
    switch (raster_mode_) {
      case PARTIAL_ONE_COPY:
        settings->use_zero_copy = false;
        settings->use_partial_raster = true;
        break;
      case FULL_ONE_COPY:
        settings->use_zero_copy = false;
        settings->use_partial_raster = false;
        break;
      case PARTIAL_BITMAP:
        settings->use_partial_raster = true;
        break;
      case FULL_BITMAP:
        settings->use_partial_raster = false;
        break;
      case PARTIAL_GPU:
        settings->gpu_rasterization_enabled = true;
        settings->gpu_rasterization_forced = true;
        settings->use_partial_raster = true;
        break;
      case FULL_GPU:
        settings->gpu_rasterization_enabled = true;
        settings->gpu_rasterization_forced = true;
        settings->use_partial_raster = false;
        break;
    }
  }

  void BeginTest() override {
    // Don't set up a readback target at the start of the test.
    PostSetNeedsCommitToMainThread();
  }

  void DoReadback() {
    Layer* target =
        readback_target_ ? readback_target_ : layer_tree_host()->root_layer();
    target->RequestCopyOfOutput(CreateCopyOutputRequest());
  }

  void RunRasterPixelTest(bool threaded,
                          RasterMode mode,
                          scoped_refptr<Layer> content_root,
                          base::FilePath file_name) {
    raster_mode_ = mode;

    PixelTestType test_type = PIXEL_TEST_SOFTWARE;
    switch (mode) {
      case PARTIAL_ONE_COPY:
      case FULL_ONE_COPY:
      case PARTIAL_GPU:
      case FULL_GPU:
        test_type = PIXEL_TEST_GL;
        break;
      case PARTIAL_BITMAP:
      case FULL_BITMAP:
        test_type = PIXEL_TEST_SOFTWARE;
    }

    if (threaded)
      RunPixelTest(test_type, content_root, file_name);
    else
      RunSingleThreadedPixelTest(test_type, content_root, file_name);
  }

  base::FilePath ref_file_;
  std::unique_ptr<SkBitmap> result_bitmap_;
  RasterMode raster_mode_;
};

class BlueYellowClient : public ContentLayerClient {
 public:
  explicit BlueYellowClient(const gfx::Size& size)
      : size_(size), blue_top_(true) {}

  gfx::Rect PaintableRegion() override { return gfx::Rect(size_); }
  scoped_refptr<DisplayItemList> PaintContentsToDisplayList(
      PaintingControlSetting painting_status) override {
    DisplayItemListSettings settings;
    settings.use_cached_picture = false;
    scoped_refptr<DisplayItemList> display_list =
        DisplayItemList::Create(PaintableRegion(), settings);

    SkPictureRecorder recorder;
    sk_sp<SkCanvas> canvas =
        sk_ref_sp(recorder.beginRecording(gfx::RectToSkRect(gfx::Rect(size_))));
    gfx::Rect top(0, 0, size_.width(), size_.height() / 2);
    gfx::Rect bottom(0, size_.height() / 2, size_.width(), size_.height() / 2);

    gfx::Rect blue_rect = blue_top_ ? top : bottom;
    gfx::Rect yellow_rect = blue_top_ ? bottom : top;

    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);

    paint.setColor(SK_ColorBLUE);
    canvas->drawRect(gfx::RectToSkRect(blue_rect), paint);
    paint.setColor(SK_ColorYELLOW);
    canvas->drawRect(gfx::RectToSkRect(yellow_rect), paint);

    display_list->CreateAndAppendItem<DrawingDisplayItem>(
        PaintableRegion(), recorder.finishRecordingAsPicture());
    display_list->Finalize();
    return display_list;
  }

  bool FillsBoundsCompletely() const override { return true; }
  size_t GetApproximateUnsharedMemoryUsage() const override { return 0; }

  void set_blue_top(bool b) { blue_top_ = b; }

 private:
  gfx::Size size_;
  bool blue_top_;
};

class LayerTreeHostTilesTestPartialInvalidation
    : public LayerTreeHostTilesPixelTest {
 public:
  LayerTreeHostTilesTestPartialInvalidation()
      : client_(gfx::Size(200, 200)),
        picture_layer_(PictureLayer::Create(&client_)) {
    picture_layer_->SetBounds(gfx::Size(200, 200));
    picture_layer_->SetIsDrawable(true);
  }

  void DidCommitAndDrawFrame() override {
    switch (layer_tree_host()->source_frame_number()) {
      case 1:
        // We have done one frame, so the layer's content has been rastered.
        // Now we change the picture behind it to record something completely
        // different, but we give a smaller invalidation rect. The layer should
        // only re-raster the stuff in the rect. If it doesn't do partial raster
        // it would re-raster the whole thing instead.
        client_.set_blue_top(false);
        Finish();
        picture_layer_->SetNeedsDisplayRect(gfx::Rect(50, 50, 100, 100));

        // Add a copy request to see what happened!
        DoReadback();
        break;
    }
  }

 protected:
  BlueYellowClient client_;
  scoped_refptr<PictureLayer> picture_layer_;
};

TEST_F(LayerTreeHostTilesTestPartialInvalidation,
       PartialRaster_SingleThread_OneCopy) {
  RunRasterPixelTest(
      false, PARTIAL_ONE_COPY, picture_layer_,
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_partial_flipped.png")));
}

TEST_F(LayerTreeHostTilesTestPartialInvalidation,
       FullRaster_SingleThread_OneCopy) {
  RunRasterPixelTest(
      false, FULL_ONE_COPY, picture_layer_,
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_flipped.png")));
}

TEST_F(LayerTreeHostTilesTestPartialInvalidation,
       PartialRaster_MultiThread_OneCopy) {
  RunRasterPixelTest(
      true, PARTIAL_ONE_COPY, picture_layer_,
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_partial_flipped.png")));
}

TEST_F(LayerTreeHostTilesTestPartialInvalidation,
       FullRaster_MultiThread_OneCopy) {
  RunRasterPixelTest(
      true, FULL_ONE_COPY, picture_layer_,
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_flipped.png")));
}

TEST_F(LayerTreeHostTilesTestPartialInvalidation,
       PartialRaster_SingleThread_Software) {
  RunRasterPixelTest(
      false, PARTIAL_BITMAP, picture_layer_,
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_partial_flipped.png")));
}

TEST_F(LayerTreeHostTilesTestPartialInvalidation,
       FulllRaster_SingleThread_Software) {
  RunRasterPixelTest(
      false, FULL_BITMAP, picture_layer_,
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_flipped.png")));
}

TEST_F(LayerTreeHostTilesTestPartialInvalidation,
       PartialRaster_SingleThread_GpuRaster) {
  RunRasterPixelTest(
      false, PARTIAL_GPU, picture_layer_,
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_partial_flipped.png")));
}

TEST_F(LayerTreeHostTilesTestPartialInvalidation,
       FullRaster_SingleThread_GpuRaster) {
  RunRasterPixelTest(
      false, FULL_GPU, picture_layer_,
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_flipped.png")));
}

}  // namespace
}  // namespace cc

#endif  // !defined(OS_ANDROID)
