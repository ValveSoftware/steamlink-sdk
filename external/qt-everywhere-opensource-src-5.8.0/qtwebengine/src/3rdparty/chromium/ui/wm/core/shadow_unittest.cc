// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/shadow.h"

#include <memory>

#include "base/macros.h"
#include "base/path_service.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/resources/grit/ui_resources.h"

namespace wm {

namespace {

const int kSmallBitmapSize = 129;
const int kLargeBitmapSize = 269;

// Mock for the ResourceBundle::Delegate class.
class MockResourceBundleDelegate : public ui::ResourceBundle::Delegate {
 public:
  MockResourceBundleDelegate() : last_resource_id_(0) {
    SkBitmap bitmap_small, bitmap_large;
    bitmap_small.allocPixels(
        SkImageInfo::MakeN32Premul(kSmallBitmapSize, kSmallBitmapSize));
    bitmap_large.allocPixels(
        SkImageInfo::MakeN32Premul(kLargeBitmapSize, kLargeBitmapSize));
    image_small_ = gfx::Image::CreateFrom1xBitmap(bitmap_small);
    image_large_ = gfx::Image::CreateFrom1xBitmap(bitmap_large);
  }
  ~MockResourceBundleDelegate() override {}

  // ResourceBundle::Delegate:
  base::FilePath GetPathForResourcePack(const base::FilePath& pack_path,
                                        ui::ScaleFactor scale_factor) override {
    return base::FilePath();
  }
  base::FilePath GetPathForLocalePack(const base::FilePath& pack_path,
                                      const std::string& locale) override {
    return base::FilePath();
  }
  gfx::Image GetImageNamed(int resource_id) override {
    last_resource_id_ = resource_id;
    switch (resource_id) {
      case IDR_WINDOW_BUBBLE_SHADOW_SMALL:
        return image_small_;
      case IDR_AURA_SHADOW_ACTIVE:
      case IDR_AURA_SHADOW_INACTIVE:
        return image_large_;
      default:
        NOTREACHED();
        return gfx::Image();
    }
  }
  gfx::Image GetNativeImageNamed(int resource_id) override {
    return gfx::Image();
  }
  base::RefCountedStaticMemory* LoadDataResourceBytes(
      int resource_id,
      ui::ScaleFactor scale_factor) override {
    return NULL;
  }
  bool GetRawDataResource(int resource_id,
                          ui::ScaleFactor scale_factor,
                          base::StringPiece* value) override {
    return false;
  }
  bool GetLocalizedString(int message_id, base::string16* value) override {
    return false;
  }

  int last_resource_id() const { return last_resource_id_; }

 private:
  gfx::Image image_small_;
  gfx::Image image_large_;
  int last_resource_id_;

  DISALLOW_COPY_AND_ASSIGN(MockResourceBundleDelegate);
};

}  // namespace

class ShadowTest: public aura::test::AuraTestBase {
 public:
  ShadowTest() {}
  ~ShadowTest() override {}

  MockResourceBundleDelegate* delegate() { return delegate_.get(); }

  // aura::testAuraBase:
  void SetUp() override {
    aura::test::AuraTestBase::SetUp();
    delegate_.reset(new MockResourceBundleDelegate());
    if (ResourceBundle::HasSharedInstance())
      ui::ResourceBundle::CleanupSharedInstance();
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        "en-US", delegate(), ui::ResourceBundle::LOAD_COMMON_RESOURCES);
  }
  void TearDown() override {
    ui::ResourceBundle::CleanupSharedInstance();
    base::FilePath ui_test_pak_path;
    ASSERT_TRUE(PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path));
    ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);
    aura::test::AuraTestBase::TearDown();
  }
 private:
  std::unique_ptr<MockResourceBundleDelegate> delegate_;
  DISALLOW_COPY_AND_ASSIGN(ShadowTest);
};

// Test if the proper image is set for the specified style.
TEST_F(ShadowTest, UpdateImagesForStyle) {
  Shadow shadow;

  shadow.Init(Shadow::STYLE_SMALL);
  EXPECT_EQ(delegate()->last_resource_id(), IDR_WINDOW_BUBBLE_SHADOW_SMALL);
  shadow.SetStyle(Shadow::STYLE_ACTIVE);
  EXPECT_EQ(delegate()->last_resource_id(), IDR_AURA_SHADOW_ACTIVE);
  shadow.SetStyle(Shadow::STYLE_INACTIVE);
  EXPECT_EQ(delegate()->last_resource_id(), IDR_AURA_SHADOW_INACTIVE);
}

// Test if the proper content bounds is calculated based on the current style.
TEST_F(ShadowTest, SetContentBounds) {
  Shadow shadow;

  // Verify that layer bounds are inset from content bounds.
  shadow.Init(Shadow::STYLE_ACTIVE);
  gfx::Rect content_bounds(100, 100, 300, 300);
  shadow.SetContentBounds(content_bounds);
  EXPECT_EQ(shadow.content_bounds(), content_bounds);
  EXPECT_EQ(shadow.layer()->bounds(), gfx::Rect(36, 36, 428, 428));

  shadow.SetStyle(Shadow::STYLE_SMALL);
  EXPECT_EQ(shadow.content_bounds(), content_bounds);
  EXPECT_EQ(shadow.layer()->bounds(), gfx::Rect(96, 96, 308, 308));
}
}  // namespace wm
