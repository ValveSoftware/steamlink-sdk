// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/large_icon_service.h"

#include <deque>
#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon/core/favicon_client.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "components/favicon_base/favicon_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace favicon {
namespace {

const char kDummyUrl[] = "http://www.example.com";
const char kDummyIconUrl[] = "http://www.example.com/touch_icon.png";

const SkColor kTestColor = SK_ColorRED;

favicon_base::FaviconRawBitmapResult CreateTestBitmap(
    int w, int h, SkColor color) {
  favicon_base::FaviconRawBitmapResult result;
  result.expired = false;

  // Create bitmap and fill with |color|.
  scoped_refptr<base::RefCountedBytes> data(new base::RefCountedBytes());
  SkBitmap bitmap;
  bitmap.allocN32Pixels(w, h);
  bitmap.eraseColor(color);
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &data->data());
  result.bitmap_data = data;

  result.pixel_size = gfx::Size(w, h);
  result.icon_url = GURL(kDummyIconUrl);
  result.icon_type = favicon_base::TOUCH_ICON;
  CHECK(result.is_valid());
  return result;
}

// A mock FaviconService that emits pre-programmed response.
class MockFaviconService : public FaviconService {
 public:
  MockFaviconService() : FaviconService(nullptr, nullptr) {
  }

  ~MockFaviconService() override {
  }

  base::CancelableTaskTracker::TaskId GetLargestRawFaviconForPageURL(
      const GURL& page_url,
      const std::vector<int>& icon_types,
      int minimum_size_in_pixels,
      const favicon_base::FaviconRawBitmapCallback& callback,
      base::CancelableTaskTracker* tracker) override {
    favicon_base::FaviconRawBitmapResult mock_result =
        mock_result_queue_.front();
    mock_result_queue_.pop_front();
    return tracker->PostTask(
        base::MessageLoop::current()->task_runner().get(), FROM_HERE,
        base::Bind(callback, mock_result));
  }

  void InjectResult(const favicon_base::FaviconRawBitmapResult& mock_result) {
    mock_result_queue_.push_back(mock_result);
  }

  bool HasUnusedResults() {
    return !mock_result_queue_.empty();
  }

 private:
  std::deque<favicon_base::FaviconRawBitmapResult> mock_result_queue_;

  DISALLOW_COPY_AND_ASSIGN(MockFaviconService);
};

// This class provides access to LargeIconService internals, using the current
// thread's task runner for testing.
class TestLargeIconService : public LargeIconService {
 public:
  explicit TestLargeIconService(MockFaviconService* mock_favicon_service)
      : LargeIconService(mock_favicon_service,
                         base::MessageLoop::current()->task_runner()) {}
  ~TestLargeIconService() override {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestLargeIconService);
};

class LargeIconServiceTest : public testing::Test {
 public:
  LargeIconServiceTest() : is_callback_invoked_(false) {
  }

  ~LargeIconServiceTest() override {
  }

  void SetUp() override {
    testing::Test::SetUp();
    mock_favicon_service_.reset(new MockFaviconService());
    large_icon_service_.reset(
        new TestLargeIconService(mock_favicon_service_.get()));
  }

  void ResultCallback(const favicon_base::LargeIconResult& result) {
    is_callback_invoked_ = true;

    // Checking presence and absence of results.
    EXPECT_EQ(expected_bitmap_.is_valid(), result.bitmap.is_valid());
    EXPECT_EQ(expected_fallback_icon_style_ != nullptr,
              result.fallback_icon_style != nullptr);

    if (expected_bitmap_.is_valid()) {
      EXPECT_EQ(expected_bitmap_.pixel_size, result.bitmap.pixel_size);
      // Not actually checking bitmap content.
    }
    if (expected_fallback_icon_style_.get()) {
      EXPECT_EQ(*expected_fallback_icon_style_,
                *result.fallback_icon_style);
    }
    // Ensure all mock results have been consumed.
    EXPECT_FALSE(mock_favicon_service_->HasUnusedResults());
  }

 protected:
  base::MessageLoopForIO loop_;

  std::unique_ptr<MockFaviconService> mock_favicon_service_;
  std::unique_ptr<TestLargeIconService> large_icon_service_;
  base::CancelableTaskTracker cancelable_task_tracker_;

  favicon_base::FaviconRawBitmapResult expected_bitmap_;
  std::unique_ptr<favicon_base::FallbackIconStyle>
      expected_fallback_icon_style_;

  bool is_callback_invoked_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LargeIconServiceTest);
};

TEST_F(LargeIconServiceTest, SameSize) {
  mock_favicon_service_->InjectResult(CreateTestBitmap(24, 24, kTestColor));
  expected_bitmap_ = CreateTestBitmap(24, 24, kTestColor);
  large_icon_service_->GetLargeIconOrFallbackStyle(
      GURL(kDummyUrl),
      24,  // |min_source_size_in_pixel|
      24,  // |desired_size_in_pixel|
      base::Bind(&LargeIconServiceTest::ResultCallback, base::Unretained(this)),
      &cancelable_task_tracker_);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(is_callback_invoked_);
}

TEST_F(LargeIconServiceTest, ScaleDown) {
  mock_favicon_service_->InjectResult(CreateTestBitmap(32, 32, kTestColor));
  expected_bitmap_ = CreateTestBitmap(24, 24, kTestColor);
  large_icon_service_->GetLargeIconOrFallbackStyle(
      GURL(kDummyUrl),
      24,
      24,
      base::Bind(&LargeIconServiceTest::ResultCallback, base::Unretained(this)),
      &cancelable_task_tracker_);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(is_callback_invoked_);
}

TEST_F(LargeIconServiceTest, ScaleUp) {
  mock_favicon_service_->InjectResult(CreateTestBitmap(16, 16, kTestColor));
  expected_bitmap_ = CreateTestBitmap(24, 24, kTestColor);
  large_icon_service_->GetLargeIconOrFallbackStyle(
      GURL(kDummyUrl),
      14,  // Lowered requirement so stored bitmap is admitted.
      24,
      base::Bind(&LargeIconServiceTest::ResultCallback, base::Unretained(this)),
      &cancelable_task_tracker_);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(is_callback_invoked_);
}

// |desired_size_in_pixel| == 0 means retrieve original image without scaling.
TEST_F(LargeIconServiceTest, NoScale) {
  mock_favicon_service_->InjectResult(CreateTestBitmap(24, 24, kTestColor));
  expected_bitmap_ = CreateTestBitmap(24, 24, kTestColor);
  large_icon_service_->GetLargeIconOrFallbackStyle(
      GURL(kDummyUrl),
      16,
      0,
      base::Bind(&LargeIconServiceTest::ResultCallback, base::Unretained(this)),
      &cancelable_task_tracker_);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(is_callback_invoked_);
}

TEST_F(LargeIconServiceTest, FallbackSinceIconTooSmall) {
  mock_favicon_service_->InjectResult(CreateTestBitmap(16, 16, kTestColor));
  expected_fallback_icon_style_.reset(new favicon_base::FallbackIconStyle);
  expected_fallback_icon_style_->background_color = kTestColor;
  large_icon_service_->GetLargeIconOrFallbackStyle(
      GURL(kDummyUrl),
      24,
      24,
      base::Bind(&LargeIconServiceTest::ResultCallback, base::Unretained(this)),
      &cancelable_task_tracker_);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(is_callback_invoked_);
}

TEST_F(LargeIconServiceTest, FallbackSinceIconNotSquare) {
  mock_favicon_service_->InjectResult(CreateTestBitmap(24, 32, kTestColor));
  expected_fallback_icon_style_.reset(new favicon_base::FallbackIconStyle);
  expected_fallback_icon_style_->background_color = kTestColor;
  large_icon_service_->GetLargeIconOrFallbackStyle(
      GURL(kDummyUrl),
      24,
      24,
      base::Bind(&LargeIconServiceTest::ResultCallback, base::Unretained(this)),
      &cancelable_task_tracker_);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(is_callback_invoked_);
}

TEST_F(LargeIconServiceTest, FallbackSinceIconMissing) {
  mock_favicon_service_->InjectResult(favicon_base::FaviconRawBitmapResult());
  // Expect default fallback style, including background.
  expected_fallback_icon_style_.reset(new favicon_base::FallbackIconStyle);
  large_icon_service_->GetLargeIconOrFallbackStyle(
      GURL(kDummyUrl),
      24,
      24,
      base::Bind(&LargeIconServiceTest::ResultCallback, base::Unretained(this)),
      &cancelable_task_tracker_);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(is_callback_invoked_);
}

TEST_F(LargeIconServiceTest, FallbackSinceIconMissingNoScale) {
  mock_favicon_service_->InjectResult(favicon_base::FaviconRawBitmapResult());
  // Expect default fallback style, including background.
  expected_fallback_icon_style_.reset(new favicon_base::FallbackIconStyle);
  large_icon_service_->GetLargeIconOrFallbackStyle(
      GURL(kDummyUrl),
      24,
      0,
      base::Bind(&LargeIconServiceTest::ResultCallback, base::Unretained(this)),
      &cancelable_task_tracker_);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(is_callback_invoked_);
}

// Oddball case where we demand a high resolution icon to scale down. Generates
// fallback even though an icon with the final size is available.
TEST_F(LargeIconServiceTest, FallbackSinceTooPicky) {
  mock_favicon_service_->InjectResult(CreateTestBitmap(24, 24, kTestColor));
  expected_fallback_icon_style_.reset(new favicon_base::FallbackIconStyle);
  expected_fallback_icon_style_->background_color = kTestColor;
  large_icon_service_->GetLargeIconOrFallbackStyle(
      GURL(kDummyUrl),
      32,
      24,
      base::Bind(&LargeIconServiceTest::ResultCallback, base::Unretained(this)),
      &cancelable_task_tracker_);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(is_callback_invoked_);
}

}  // namespace
}  // namespace favicon
