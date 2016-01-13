// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/ozone/platform/dri/dri_buffer.h"
#include "ui/ozone/platform/dri/dri_surface.h"
#include "ui/ozone/platform/dri/dri_surface_factory.h"
#include "ui/ozone/platform/dri/hardware_display_controller.h"
#include "ui/ozone/platform/dri/screen_manager.h"
#include "ui/ozone/platform/dri/test/mock_dri_surface.h"
#include "ui/ozone/platform/dri/test/mock_dri_wrapper.h"
#include "ui/ozone/platform/dri/test/mock_surface_generator.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

namespace {

const drmModeModeInfo kDefaultMode =
    {0, 6, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, {'\0'}};

// SSFO would normally allocate DRM resources. We can't rely on having a DRM
// backend to allocate and display our buffers. Thus, we replace these
// resources with stubs. For DRM calls, we simply use stubs that do nothing and
// for buffers we use the default SkBitmap allocator.
class MockDriSurfaceFactory : public ui::DriSurfaceFactory {
 public:
  MockDriSurfaceFactory(ui::DriWrapper* dri, ui::ScreenManager* screen_manager)
      : DriSurfaceFactory(dri, screen_manager), dri_(dri) {}
  virtual ~MockDriSurfaceFactory() {};

  const std::vector<ui::MockDriSurface*>& get_surfaces() const {
    return surfaces_;
  }

 private:
  virtual ui::DriSurface* CreateSurface(const gfx::Size& size) OVERRIDE {
    ui::MockDriSurface* surface = new ui::MockDriSurface(dri_, size);
    surfaces_.push_back(surface);
    return surface;
  }

  ui::DriWrapper* dri_;
  std::vector<ui::MockDriSurface*> surfaces_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(MockDriSurfaceFactory);
};

class MockScreenManager : public ui::ScreenManager {
 public:
  MockScreenManager(ui::DriWrapper* dri,
                    ui::ScanoutSurfaceGenerator* surface_generator)
      : ScreenManager(dri, surface_generator),
        dri_(dri) {}
  virtual ~MockScreenManager() {}

  // Normally we'd use DRM to figure out the controller configuration. But we
  // can't use DRM in unit tests, so we just create a fake configuration.
  virtual void ForceInitializationOfPrimaryDisplay() OVERRIDE {
    ConfigureDisplayController(1, 2, kDefaultMode);
  }

 private:
  ui::DriWrapper* dri_;  // Not owned.
  std::vector<ui::MockDriSurface*> surfaces_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(MockScreenManager);
};

}  // namespace

class DriSurfaceFactoryTest : public testing::Test {
 public:
  DriSurfaceFactoryTest() {}

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;
 protected:
  scoped_ptr<base::MessageLoop> message_loop_;
  scoped_ptr<ui::MockDriWrapper> dri_;
  scoped_ptr<ui::MockSurfaceGenerator> surface_generator_;
  scoped_ptr<MockScreenManager> screen_manager_;
  scoped_ptr<MockDriSurfaceFactory> factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DriSurfaceFactoryTest);
};

void DriSurfaceFactoryTest::SetUp() {
  message_loop_.reset(new base::MessageLoopForUI);
  dri_.reset(new ui::MockDriWrapper(3));
  surface_generator_.reset(new ui::MockSurfaceGenerator(dri_.get()));
  screen_manager_.reset(new MockScreenManager(dri_.get(),
                                              surface_generator_.get()));
  factory_.reset(new MockDriSurfaceFactory(dri_.get(), screen_manager_.get()));
}

void DriSurfaceFactoryTest::TearDown() {
  factory_.reset();
  message_loop_.reset();
}

TEST_F(DriSurfaceFactoryTest, FailInitialization) {
  dri_->fail_init();
  EXPECT_EQ(ui::SurfaceFactoryOzone::FAILED, factory_->InitializeHardware());
}

TEST_F(DriSurfaceFactoryTest, SuccessfulInitialization) {
  EXPECT_EQ(ui::SurfaceFactoryOzone::INITIALIZED,
            factory_->InitializeHardware());
}

TEST_F(DriSurfaceFactoryTest, SuccessfulWidgetRealization) {
  EXPECT_EQ(ui::SurfaceFactoryOzone::INITIALIZED,
            factory_->InitializeHardware());

  gfx::AcceleratedWidget w = factory_->GetAcceleratedWidget();
  EXPECT_EQ(ui::DriSurfaceFactory::kDefaultWidgetHandle, w);

  EXPECT_TRUE(factory_->CreateCanvasForWidget(w));
}

TEST_F(DriSurfaceFactoryTest, CheckNativeSurfaceContents) {
  EXPECT_EQ(ui::SurfaceFactoryOzone::INITIALIZED,
            factory_->InitializeHardware());

  gfx::AcceleratedWidget w = factory_->GetAcceleratedWidget();
  EXPECT_EQ(ui::DriSurfaceFactory::kDefaultWidgetHandle, w);

  scoped_ptr<ui::SurfaceOzoneCanvas> surface =
      factory_->CreateCanvasForWidget(w);

  surface->ResizeCanvas(
      gfx::Size(kDefaultMode.hdisplay, kDefaultMode.vdisplay));
  surface->GetCanvas()->drawColor(SK_ColorWHITE);
  surface->PresentCanvas(
      gfx::Rect(0, 0, kDefaultMode.hdisplay / 2, kDefaultMode.vdisplay / 2));

  const std::vector<ui::DriBuffer*>& bitmaps =
      surface_generator_->surfaces()[0]->bitmaps();

  SkBitmap image;
  bitmaps[1]->canvas()->readPixels(&image, 0, 0);

  // Make sure the updates are correctly propagated to the native surface.
  for (int i = 0; i < image.height(); ++i) {
    for (int j = 0; j < image.width(); ++j) {
      if (j < kDefaultMode.hdisplay / 2 && i < kDefaultMode.vdisplay / 2)
        EXPECT_EQ(SK_ColorWHITE, image.getColor(j, i));
      else
        EXPECT_EQ(SK_ColorBLACK, image.getColor(j, i));
    }
  }
}

TEST_F(DriSurfaceFactoryTest, SetCursorImage) {
  EXPECT_EQ(ui::SurfaceFactoryOzone::INITIALIZED,
            factory_->InitializeHardware());

  gfx::AcceleratedWidget w = factory_->GetAcceleratedWidget();
  EXPECT_EQ(ui::DriSurfaceFactory::kDefaultWidgetHandle, w);

  scoped_ptr<ui::SurfaceOzoneCanvas> surf = factory_->CreateCanvasForWidget(w);
  EXPECT_TRUE(surf);

  SkBitmap image;
  SkImageInfo info = SkImageInfo::Make(
      6, 4, kN32_SkColorType, kPremul_SkAlphaType);
  image.allocPixels(info);
  image.eraseColor(SK_ColorWHITE);

  factory_->SetHardwareCursor(w, image, gfx::Point(4, 2));
  const std::vector<ui::MockDriSurface*>& surfaces = factory_->get_surfaces();

  // The first surface is the cursor surface since it is allocated early in the
  // initialization process.
  const std::vector<ui::DriBuffer*>& bitmaps = surfaces[0]->bitmaps();

  // The surface should have been initialized to a double-buffered surface.
  EXPECT_EQ(2u, bitmaps.size());

  SkBitmap cursor;
  bitmaps[1]->canvas()->readPixels(&cursor, 0, 0);

  // Check that the frontbuffer is displaying the right image as set above.
  for (int i = 0; i < cursor.height(); ++i) {
    for (int j = 0; j < cursor.width(); ++j) {
      if (j < info.width() && i < info.height())
        EXPECT_EQ(SK_ColorWHITE, cursor.getColor(j, i));
      else
        EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT),
                  cursor.getColor(j, i));
    }
  }
}
