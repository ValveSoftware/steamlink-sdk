// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/favicon_handler.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <vector>

#include "base/macros.h"
#include "components/favicon/core/favicon_driver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/layout.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image.h"

namespace favicon {
namespace {

// Fill the given bmp with valid png data.
void FillDataToBitmap(int w, int h, SkBitmap* bmp) {
  bmp->allocN32Pixels(w, h);

  unsigned char* src_data =
      reinterpret_cast<unsigned char*>(bmp->getAddr32(0, 0));
  for (int i = 0; i < w * h; i++) {
    src_data[i * 4 + 0] = static_cast<unsigned char>(i % 255);
    src_data[i * 4 + 1] = static_cast<unsigned char>(i % 255);
    src_data[i * 4 + 2] = static_cast<unsigned char>(i % 255);
    src_data[i * 4 + 3] = static_cast<unsigned char>(i % 255);
  }
}

// Fill the given data buffer with valid png data.
void FillBitmap(int w, int h, std::vector<unsigned char>* output) {
  SkBitmap bitmap;
  FillDataToBitmap(w, h, &bitmap);
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, output);
}

void SetFaviconRawBitmapResult(
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    bool expired,
    std::vector<favicon_base::FaviconRawBitmapResult>* favicon_bitmap_results) {
  scoped_refptr<base::RefCountedBytes> data(new base::RefCountedBytes());
  FillBitmap(gfx::kFaviconSize, gfx::kFaviconSize, &data->data());
  favicon_base::FaviconRawBitmapResult bitmap_result;
  bitmap_result.expired = expired;
  bitmap_result.bitmap_data = data;
  // Use a pixel size other than (0,0) as (0,0) has a special meaning.
  bitmap_result.pixel_size = gfx::Size(gfx::kFaviconSize, gfx::kFaviconSize);
  bitmap_result.icon_type = icon_type;
  bitmap_result.icon_url = icon_url;

  favicon_bitmap_results->push_back(bitmap_result);
}

void SetFaviconRawBitmapResult(
    const GURL& icon_url,
    std::vector<favicon_base::FaviconRawBitmapResult>* favicon_bitmap_results) {
  SetFaviconRawBitmapResult(icon_url,
                            favicon_base::FAVICON,
                            false /* expired */,
                            favicon_bitmap_results);
}

// This class is used to save the download request for verifying with test case.
// It also will be used to invoke the onDidDownload callback.
class DownloadHandler {
 public:
  explicit DownloadHandler(FaviconHandler* favicon_handler)
      : favicon_handler_(favicon_handler), callback_invoked_(false) {}

  ~DownloadHandler() {}

  void Reset() {
    download_.reset();
    callback_invoked_ = false;
    // Does not affect |should_fail_download_icon_urls_| and
    // |failed_download_icon_urls_|.
  }

  // Make downloads for any of |icon_urls| fail.
  void FailDownloadForIconURLs(const std::set<GURL>& icon_urls) {
    should_fail_download_icon_urls_ = icon_urls;
  }

  // Returns whether a download for |icon_url| did fail.
  bool DidFailDownloadForIconURL(const GURL& icon_url) const {
    return failed_download_icon_urls_.count(icon_url);
  }

  void AddDownload(
      int download_id,
      const GURL& image_url,
      const std::vector<int>& image_sizes,
      int max_image_size) {
    download_.reset(new Download(
        download_id, image_url, image_sizes, max_image_size));
  }

  void InvokeCallback();

  bool HasDownload() const { return download_.get(); }
  const GURL& GetImageUrl() const { return download_->image_url; }
  void SetImageSizes(const std::vector<int>& sizes) {
    download_->image_sizes = sizes; }

 private:
  struct Download {
    Download(int id,
             GURL url,
             const std::vector<int>& sizes,
             int max_size)
        : download_id(id),
          image_url(url),
          image_sizes(sizes),
          max_image_size(max_size) {}
    ~Download() {}
    int download_id;
    GURL image_url;
    std::vector<int> image_sizes;
    int max_image_size;
  };

  FaviconHandler* favicon_handler_;
  std::unique_ptr<Download> download_;
  bool callback_invoked_;

  // The icon URLs for which the download should fail.
  std::set<GURL> should_fail_download_icon_urls_;

  // The icon URLs for which the download did fail. This should be a subset of
  // |should_fail_download_icon_urls_|.
  std::set<GURL> failed_download_icon_urls_;

  DISALLOW_COPY_AND_ASSIGN(DownloadHandler);
};

// This class is used to save the history request for verifying with test case.
// It also will be used to simulate the history response.
class HistoryRequestHandler {
 public:
  HistoryRequestHandler(const GURL& page_url,
                        const GURL& icon_url,
                        int icon_type,
                        const favicon_base::FaviconResultsCallback& callback)
      : page_url_(page_url),
        icon_url_(icon_url),
        icon_type_(icon_type),
        callback_(callback) {
  }

  HistoryRequestHandler(const GURL& page_url,
                        const GURL& icon_url,
                        int icon_type,
                        const std::vector<unsigned char>& bitmap_data,
                        const gfx::Size& size)
      : page_url_(page_url),
        icon_url_(icon_url),
        icon_type_(icon_type),
        bitmap_data_(bitmap_data),
        size_(size) {
  }

  ~HistoryRequestHandler() {}
  void InvokeCallback();

  const GURL page_url_;
  const GURL icon_url_;
  const int icon_type_;
  const std::vector<unsigned char> bitmap_data_;
  const gfx::Size size_;
  std::vector<favicon_base::FaviconRawBitmapResult> history_results_;
  favicon_base::FaviconResultsCallback callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HistoryRequestHandler);
};

}  // namespace

class TestFaviconDriver : public FaviconDriver {
 public:
  TestFaviconDriver()
      : num_notifications_(0u) {}

  ~TestFaviconDriver() override {}

  // FaviconDriver implementation.
  void FetchFavicon(const GURL& url) override {
    ADD_FAILURE() << "TestFaviconDriver::FetchFavicon() "
                  << "should never be called in tests.";
  }

  gfx::Image GetFavicon() const override {
    ADD_FAILURE() << "TestFaviconDriver::GetFavicon() "
                  << "should never be called in tests.";
    return gfx::Image();
  }

  bool FaviconIsValid() const override {
    ADD_FAILURE() << "TestFaviconDriver::FaviconIsValid() "
                  << "should never be called in tests.";
    return false;
  }

  bool HasPendingTasksForTest() override {
    ADD_FAILURE() << "TestFaviconDriver::HasPendingTasksForTest() "
                  << "should never be called in tests.";
    return false;
  }

  int StartDownload(const GURL& url, int max_bitmap_size) override {
    ADD_FAILURE() << "TestFaviconDriver::StartDownload() "
                  << "should never be called in tests.";
    return -1;
  }

  bool IsOffTheRecord() override { return false; }

  bool IsBookmarked(const GURL& url) override { return false; }

  GURL GetActiveURL() override {
    ADD_FAILURE() << "TestFaviconDriver::GetActiveURL() "
                  << "should never be called in tests.";
    return GURL();
  }

  void OnFaviconUpdated(
      const GURL& page_url,
      FaviconDriverObserver::NotificationIconType notification_icon_type,
      const GURL& icon_url,
      bool icon_url_changed,
      const gfx::Image& image) override {
    ++num_notifications_;
    icon_url_ = icon_url;
    image_ = image;
  }

  const GURL& icon_url() const { return icon_url_; }

  const gfx::Image& image() const { return image_; }

  size_t num_notifications() const { return num_notifications_; }
  void ResetNumNotifications() { num_notifications_ = 0; }

 private:
  GURL icon_url_;
  gfx::Image image_;
  size_t num_notifications_;

  DISALLOW_COPY_AND_ASSIGN(TestFaviconDriver);
};

// This class is used to catch the FaviconHandler's download and history
// request, and also provide the methods to access the FaviconHandler
// internals.
class TestFaviconHandler : public FaviconHandler {
 public:
  static int GetMaximalIconSize(favicon_base::IconType icon_type) {
    return FaviconHandler::GetMaximalIconSize(icon_type);
  }

  TestFaviconHandler(TestFaviconDriver* driver,
                     FaviconDriverObserver::NotificationIconType handler_type)
      : FaviconHandler(nullptr, driver, handler_type),
        download_id_(0) {
    download_handler_.reset(new DownloadHandler(this));
  }

  ~TestFaviconHandler() override {}

  HistoryRequestHandler* history_handler() {
    return history_handler_.get();
  }

  // This method will take the ownership of the given handler.
  void set_history_handler(HistoryRequestHandler* handler) {
    history_handler_.reset(handler);
  }

  DownloadHandler* download_handler() {
    return download_handler_.get();
  }

  FaviconURL* current_candidate() {
    return FaviconHandler::current_candidate();
  }

  size_t current_candidate_index() const {
    return current_candidate_index_;
  }

  const FaviconCandidate& best_favicon_candidate() {
    return best_favicon_candidate_;
  }

 protected:
  void UpdateFaviconMappingAndFetch(
      const GURL& page_url,
      const GURL& icon_url,
      favicon_base::IconType icon_type,
      const favicon_base::FaviconResultsCallback& callback,
      base::CancelableTaskTracker* tracker) override {
    history_handler_.reset(new HistoryRequestHandler(page_url, icon_url,
                                                     icon_type, callback));
  }

  void GetFaviconFromFaviconService(
      const GURL& icon_url,
      favicon_base::IconType icon_type,
      const favicon_base::FaviconResultsCallback& callback,
      base::CancelableTaskTracker* tracker) override {
    history_handler_.reset(new HistoryRequestHandler(GURL(), icon_url,
                                                     icon_type, callback));
  }

  void GetFaviconForURLFromFaviconService(
      const GURL& page_url,
      int icon_types,
      const favicon_base::FaviconResultsCallback& callback,
      base::CancelableTaskTracker* tracker) override {
    history_handler_.reset(new HistoryRequestHandler(page_url, GURL(),
                                                     icon_types, callback));
  }

  int DownloadFavicon(const GURL& image_url, int max_bitmap_size) override {
    // Do not do a download if downloading |image_url| failed previously. This
    // emulates the behavior of FaviconDriver::StartDownload()
    if (download_handler_->DidFailDownloadForIconURL(image_url)) {
      download_handler_->AddDownload(download_id_, image_url,
                                     std::vector<int>(), 0);
      return 0;
    }

    download_id_++;
    std::vector<int> sizes;
    sizes.push_back(0);
    download_handler_->AddDownload(
        download_id_, image_url, sizes, max_bitmap_size);
    return download_id_;
  }

  void SetHistoryFavicons(const GURL& page_url,
                          const GURL& icon_url,
                          favicon_base::IconType icon_type,
                          const gfx::Image& image) override {
    scoped_refptr<base::RefCountedMemory> bytes = image.As1xPNGBytes();
    std::vector<unsigned char> bitmap_data(bytes->front(),
                                           bytes->front() + bytes->size());
    history_handler_.reset(new HistoryRequestHandler(
        page_url, icon_url, icon_type, bitmap_data, image.Size()));
  }

  bool ShouldSaveFavicon() override { return true; }

  GURL page_url_;

 private:

  // The unique id of a download request. It will be returned to a
  // FaviconHandler.
  int download_id_;

  std::unique_ptr<DownloadHandler> download_handler_;
  std::unique_ptr<HistoryRequestHandler> history_handler_;

  DISALLOW_COPY_AND_ASSIGN(TestFaviconHandler);
};

namespace {

void HistoryRequestHandler::InvokeCallback() {
  if (!callback_.is_null()) {
    callback_.Run(history_results_);
  }
}

void DownloadHandler::InvokeCallback() {
  if (callback_invoked_)
    return;

  std::vector<gfx::Size> original_bitmap_sizes;
  std::vector<SkBitmap> bitmaps;
  if (should_fail_download_icon_urls_.count(download_->image_url)) {
    failed_download_icon_urls_.insert(download_->image_url);
  } else {
    for (std::vector<int>::const_iterator i = download_->image_sizes.begin();
         i != download_->image_sizes.end(); ++i) {
      int original_size = (*i > 0) ? *i : gfx::kFaviconSize;
      int downloaded_size = original_size;
      if (download_->max_image_size != 0 &&
          downloaded_size > download_->max_image_size) {
        downloaded_size = download_->max_image_size;
      }
      SkBitmap bitmap;
      FillDataToBitmap(downloaded_size, downloaded_size, &bitmap);
      bitmaps.push_back(bitmap);
      original_bitmap_sizes.push_back(gfx::Size(original_size, original_size));
    }
  }
  favicon_handler_->OnDidDownloadFavicon(download_->download_id,
                                         download_->image_url, bitmaps,
                                         original_bitmap_sizes);
  callback_invoked_ = true;
}

class FaviconHandlerTest : public testing::Test {
 public:
  FaviconHandlerTest() {
  }

  ~FaviconHandlerTest() override {}

  // Simulates requesting a favicon for |page_url| given:
  // - We have not previously cached anything in history for |page_url| or for
  //   any of |candidates|.
  // - The page provides favicons at |candidate_icons|.
  // - The favicons at |candidate_icons| have edge pixel sizes of
  //   |candidate_icon_sizes|.
  void DownloadTillDoneIgnoringHistory(
      TestFaviconDriver* favicon_driver,
      TestFaviconHandler* favicon_handler,
      const GURL& page_url,
      const std::vector<FaviconURL>& candidate_icons,
      const int* candidate_icon_sizes) {
    size_t old_num_notifications = favicon_driver->num_notifications();

    UpdateFaviconURL(
        favicon_driver, favicon_handler, page_url, candidate_icons);
    EXPECT_EQ(candidate_icons.size(), favicon_handler->image_urls().size());

    DownloadHandler* download_handler = favicon_handler->download_handler();
    for (size_t i = 0; i < candidate_icons.size(); ++i) {
      favicon_handler->history_handler()->history_results_.clear();
      favicon_handler->history_handler()->InvokeCallback();
      ASSERT_TRUE(download_handler->HasDownload());
      EXPECT_EQ(download_handler->GetImageUrl(),
                candidate_icons[i].icon_url);
      std::vector<int> sizes;
      sizes.push_back(candidate_icon_sizes[i]);
      download_handler->SetImageSizes(sizes);
      download_handler->InvokeCallback();

      download_handler->Reset();

      if (favicon_driver->num_notifications() > old_num_notifications)
        return;
    }
  }

  void UpdateFaviconURL(TestFaviconDriver* favicon_driver,
                        TestFaviconHandler* favicon_handler,
                        const GURL& page_url,
                        const std::vector<FaviconURL>& candidate_icons) {
    favicon_driver->ResetNumNotifications();

    favicon_handler->FetchFavicon(page_url);
    favicon_handler->history_handler()->InvokeCallback();

    favicon_handler->OnUpdateFaviconURL(page_url, candidate_icons);
  }

  void SetUp() override {
    // The score computed by SelectFaviconFrames() is dependent on the supported
    // scale factors of the platform. It is used for determining the goodness of
    // a downloaded bitmap in FaviconHandler::OnDidDownloadFavicon().
    // Force the values of the scale factors so that the tests produce the same
    // results on all platforms.
    std::vector<ui::ScaleFactor> scale_factors;
    scale_factors.push_back(ui::SCALE_FACTOR_100P);
    scoped_set_supported_scale_factors_.reset(
        new ui::test::ScopedSetSupportedScaleFactors(scale_factors));
    testing::Test::SetUp();
  }

 private:
  std::unique_ptr<ui::test::ScopedSetSupportedScaleFactors>
      scoped_set_supported_scale_factors_;
  DISALLOW_COPY_AND_ASSIGN(FaviconHandlerTest);
};

TEST_F(FaviconHandlerTest, GetFaviconFromHistory) {
  const GURL page_url("http://www.google.com");
  const GURL icon_url("http://www.google.com/favicon");

  TestFaviconDriver driver;
  TestFaviconHandler helper(&driver, FaviconDriverObserver::NON_TOUCH_16_DIP);

  helper.FetchFavicon(page_url);
  HistoryRequestHandler* history_handler = helper.history_handler();
  // Ensure the data given to history is correct.
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(page_url, history_handler->page_url_);
  EXPECT_EQ(GURL(), history_handler->icon_url_);
  EXPECT_EQ(favicon_base::FAVICON, history_handler->icon_type_);

  SetFaviconRawBitmapResult(icon_url, &history_handler->history_results_);

  // Send history response.
  history_handler->InvokeCallback();
  // Verify FaviconHandler status
  EXPECT_EQ(1u, driver.num_notifications());
  EXPECT_EQ(icon_url, driver.icon_url());

  // Simulates update favicon url.
  std::vector<FaviconURL> urls;
  urls.push_back(
      FaviconURL(icon_url, favicon_base::FAVICON, std::vector<gfx::Size>()));
  helper.OnUpdateFaviconURL(page_url, urls);

  // Verify FaviconHandler status
  EXPECT_EQ(1u, helper.image_urls().size());
  ASSERT_TRUE(helper.current_candidate());
  ASSERT_EQ(icon_url, helper.current_candidate()->icon_url);
  ASSERT_EQ(favicon_base::FAVICON, helper.current_candidate()->icon_type);

  // Favicon shouldn't request to download icon.
  EXPECT_FALSE(helper.download_handler()->HasDownload());
}

TEST_F(FaviconHandlerTest, DownloadFavicon) {
  const GURL page_url("http://www.google.com");
  const GURL icon_url("http://www.google.com/favicon");

  TestFaviconDriver driver;
  TestFaviconHandler helper(&driver, FaviconDriverObserver::NON_TOUCH_16_DIP);

  helper.FetchFavicon(page_url);
  HistoryRequestHandler* history_handler = helper.history_handler();
  // Ensure the data given to history is correct.
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(page_url, history_handler->page_url_);
  EXPECT_EQ(GURL(), history_handler->icon_url_);
  EXPECT_EQ(favicon_base::FAVICON, history_handler->icon_type_);

  // Set icon data expired
  SetFaviconRawBitmapResult(icon_url,
                            favicon_base::FAVICON,
                            true /* expired */,
                            &history_handler->history_results_);
  // Send history response.
  history_handler->InvokeCallback();
  // Verify FaviconHandler status
  EXPECT_EQ(1u, driver.num_notifications());
  EXPECT_EQ(icon_url, driver.icon_url());

  // Simulates update favicon url.
  std::vector<FaviconURL> urls;
  urls.push_back(
      FaviconURL(icon_url, favicon_base::FAVICON, std::vector<gfx::Size>()));
  helper.OnUpdateFaviconURL(page_url, urls);

  // Verify FaviconHandler status
  EXPECT_EQ(1u, helper.image_urls().size());
  ASSERT_TRUE(helper.current_candidate());
  ASSERT_EQ(icon_url, helper.current_candidate()->icon_url);
  ASSERT_EQ(favicon_base::FAVICON, helper.current_candidate()->icon_type);

  // Favicon should request to download icon now.
  DownloadHandler* download_handler = helper.download_handler();
  EXPECT_TRUE(helper.download_handler()->HasDownload());

  // Verify the download request.
  EXPECT_EQ(icon_url, download_handler->GetImageUrl());

  // Reset the history_handler to verify whether favicon is set.
  helper.set_history_handler(nullptr);

  // Smulates download done.
  download_handler->InvokeCallback();

  // New icon should be saved to history backend and navigation entry.
  history_handler = helper.history_handler();
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(icon_url, history_handler->icon_url_);
  EXPECT_EQ(favicon_base::FAVICON, history_handler->icon_type_);
  EXPECT_LT(0U, history_handler->bitmap_data_.size());
  EXPECT_EQ(page_url, history_handler->page_url_);

  // Verify NavigationEntry.
  EXPECT_EQ(2u, driver.num_notifications());
  EXPECT_EQ(icon_url, driver.icon_url());
  EXPECT_FALSE(driver.image().IsEmpty());
  EXPECT_EQ(gfx::kFaviconSize, driver.image().Width());
}

TEST_F(FaviconHandlerTest, UpdateAndDownloadFavicon) {
  const GURL page_url("http://www.google.com");
  const GURL icon_url("http://www.google.com/favicon");
  const GURL new_icon_url("http://www.google.com/new_favicon");

  TestFaviconDriver driver;
  TestFaviconHandler helper(&driver, FaviconDriverObserver::NON_TOUCH_16_DIP);

  helper.FetchFavicon(page_url);
  HistoryRequestHandler* history_handler = helper.history_handler();
  // Ensure the data given to history is correct.
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(page_url, history_handler->page_url_);
  EXPECT_EQ(GURL(), history_handler->icon_url_);
  EXPECT_EQ(favicon_base::FAVICON, history_handler->icon_type_);

  // Set valid icon data.
  SetFaviconRawBitmapResult(icon_url, &history_handler->history_results_);

  // Send history response.
  history_handler->InvokeCallback();
  // Verify FaviconHandler status.
  EXPECT_EQ(1u, driver.num_notifications());
  EXPECT_EQ(icon_url, driver.icon_url());

  // Reset the history_handler to verify whether new icon is requested from
  // history.
  helper.set_history_handler(nullptr);

  // Simulates update with the different favicon url.
  std::vector<FaviconURL> urls;
  urls.push_back(FaviconURL(
      new_icon_url, favicon_base::FAVICON, std::vector<gfx::Size>()));
  helper.OnUpdateFaviconURL(page_url, urls);

  // Verify FaviconHandler status.
  EXPECT_EQ(1u, helper.image_urls().size());
  ASSERT_TRUE(helper.current_candidate());
  ASSERT_EQ(new_icon_url, helper.current_candidate()->icon_url);
  ASSERT_EQ(favicon_base::FAVICON, helper.current_candidate()->icon_type);

  // Favicon should be requested from history.
  history_handler = helper.history_handler();
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(new_icon_url, history_handler->icon_url_);
  EXPECT_EQ(favicon_base::FAVICON, history_handler->icon_type_);
  EXPECT_EQ(page_url, history_handler->page_url_);

  // Simulate not find icon.
  history_handler->history_results_.clear();
  history_handler->InvokeCallback();

  // Favicon should request to download icon now.
  DownloadHandler* download_handler = helper.download_handler();
  EXPECT_TRUE(helper.download_handler()->HasDownload());

  // Verify the download request.
  EXPECT_EQ(new_icon_url, download_handler->GetImageUrl());

  // Reset the history_handler to verify whether favicon is set.
  helper.set_history_handler(nullptr);

  // Smulates download done.
  download_handler->InvokeCallback();

  // New icon should be saved to history backend and navigation entry.
  history_handler = helper.history_handler();
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(new_icon_url, history_handler->icon_url_);
  EXPECT_EQ(favicon_base::FAVICON, history_handler->icon_type_);
  EXPECT_LT(0U, history_handler->bitmap_data_.size());
  EXPECT_EQ(page_url, history_handler->page_url_);

  // Verify NavigationEntry.
  EXPECT_EQ(2u, driver.num_notifications());
  EXPECT_EQ(new_icon_url, driver.icon_url());
  EXPECT_FALSE(driver.image().IsEmpty());
  EXPECT_EQ(gfx::kFaviconSize, driver.image().Width());
}

TEST_F(FaviconHandlerTest, FaviconInHistoryInvalid) {
  const GURL page_url("http://www.google.com");
  const GURL icon_url("http://www.google.com/favicon");

  TestFaviconDriver driver;
  TestFaviconHandler helper(&driver, FaviconDriverObserver::NON_TOUCH_16_DIP);

  helper.FetchFavicon(page_url);
  HistoryRequestHandler* history_handler = helper.history_handler();
  // Ensure the data given to history is correct.
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(page_url, history_handler->page_url_);
  EXPECT_EQ(GURL(), history_handler->icon_url_);
  EXPECT_EQ(favicon_base::FAVICON, history_handler->icon_type_);

  // Set non empty but invalid data.
  favicon_base::FaviconRawBitmapResult bitmap_result;
  bitmap_result.expired = false;
  // Empty bitmap data is invalid.
  bitmap_result.bitmap_data = new base::RefCountedBytes();
  bitmap_result.pixel_size = gfx::Size(gfx::kFaviconSize, gfx::kFaviconSize);
  bitmap_result.icon_type = favicon_base::FAVICON;
  bitmap_result.icon_url = icon_url;
  history_handler->history_results_.clear();
  history_handler->history_results_.push_back(bitmap_result);

  // Send history response.
  history_handler->InvokeCallback();
  // The NavigationEntry should not be set yet as the history data is invalid.
  EXPECT_EQ(0u, driver.num_notifications());
  EXPECT_EQ(GURL(), driver.icon_url());

  // Reset the history_handler to verify whether new icon is requested from
  // history.
  helper.set_history_handler(nullptr);

  // Simulates update with matching favicon URL.
  std::vector<FaviconURL> urls;
  urls.push_back(
      FaviconURL(icon_url, favicon_base::FAVICON, std::vector<gfx::Size>()));
  helper.OnUpdateFaviconURL(page_url, urls);

  // A download for the favicon should be requested, and we should not do
  // another history request.
  DownloadHandler* download_handler = helper.download_handler();
  EXPECT_TRUE(helper.download_handler()->HasDownload());
  EXPECT_EQ(nullptr, helper.history_handler());

  // Verify the download request.
  EXPECT_EQ(icon_url, download_handler->GetImageUrl());

  // Simulates download done.
  download_handler->InvokeCallback();

  // New icon should be saved to history backend and navigation entry.
  history_handler = helper.history_handler();
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(icon_url, history_handler->icon_url_);
  EXPECT_EQ(favicon_base::FAVICON, history_handler->icon_type_);
  EXPECT_LT(0U, history_handler->bitmap_data_.size());
  EXPECT_EQ(page_url, history_handler->page_url_);

  // Verify NavigationEntry.
  EXPECT_EQ(1u, driver.num_notifications());
  EXPECT_EQ(icon_url, driver.icon_url());
  EXPECT_FALSE(driver.image().IsEmpty());
  EXPECT_EQ(gfx::kFaviconSize, driver.image().Width());
}

TEST_F(FaviconHandlerTest, UpdateFavicon) {
  const GURL page_url("http://www.google.com");
  const GURL icon_url("http://www.google.com/favicon");
  const GURL new_icon_url("http://www.google.com/new_favicon");

  TestFaviconDriver driver;
  TestFaviconHandler helper(&driver, FaviconDriverObserver::NON_TOUCH_16_DIP);

  helper.FetchFavicon(page_url);
  HistoryRequestHandler* history_handler = helper.history_handler();
  // Ensure the data given to history is correct.
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(page_url, history_handler->page_url_);
  EXPECT_EQ(GURL(), history_handler->icon_url_);
  EXPECT_EQ(favicon_base::FAVICON, history_handler->icon_type_);

  SetFaviconRawBitmapResult(icon_url, &history_handler->history_results_);

  // Send history response.
  history_handler->InvokeCallback();
  // Verify FaviconHandler status.
  EXPECT_EQ(1u, driver.num_notifications());
  EXPECT_EQ(icon_url, driver.icon_url());

  // Reset the history_handler to verify whether new icon is requested from
  // history.
  helper.set_history_handler(nullptr);

  // Simulates update with the different favicon url.
  std::vector<FaviconURL> urls;
  urls.push_back(FaviconURL(
      new_icon_url, favicon_base::FAVICON, std::vector<gfx::Size>()));
  helper.OnUpdateFaviconURL(page_url, urls);

  // Verify FaviconHandler status.
  EXPECT_EQ(1u, helper.image_urls().size());
  ASSERT_TRUE(helper.current_candidate());
  ASSERT_EQ(new_icon_url, helper.current_candidate()->icon_url);
  ASSERT_EQ(favicon_base::FAVICON, helper.current_candidate()->icon_type);

  // Favicon should be requested from history.
  history_handler = helper.history_handler();
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(new_icon_url, history_handler->icon_url_);
  EXPECT_EQ(favicon_base::FAVICON, history_handler->icon_type_);
  EXPECT_EQ(page_url, history_handler->page_url_);

  // Simulate find icon.
  SetFaviconRawBitmapResult(new_icon_url, &history_handler->history_results_);
  history_handler->InvokeCallback();

  // Shouldn't request download favicon
  EXPECT_FALSE(helper.download_handler()->HasDownload());

  // Verify the favicon status.
  EXPECT_EQ(2u, driver.num_notifications());
  EXPECT_EQ(new_icon_url, driver.icon_url());
  EXPECT_FALSE(driver.image().IsEmpty());
}

TEST_F(FaviconHandlerTest, Download2ndFaviconURLCandidate) {
  const GURL page_url("http://www.google.com");
  const GURL icon_url("http://www.google.com/favicon");
  const GURL new_icon_url("http://www.google.com/new_favicon");

  TestFaviconDriver driver;
  TestFaviconHandler helper(&driver, FaviconDriverObserver::TOUCH_LARGEST);
  std::set<GURL> fail_downloads;
  fail_downloads.insert(icon_url);
  helper.download_handler()->FailDownloadForIconURLs(fail_downloads);

  helper.FetchFavicon(page_url);
  HistoryRequestHandler* history_handler = helper.history_handler();
  // Ensure the data given to history is correct.
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(page_url, history_handler->page_url_);
  EXPECT_EQ(GURL(), history_handler->icon_url_);
  EXPECT_EQ(favicon_base::TOUCH_PRECOMPOSED_ICON | favicon_base::TOUCH_ICON,
            history_handler->icon_type_);

  // Icon not found.
  history_handler->history_results_.clear();
  // Send history response.
  history_handler->InvokeCallback();
  // Verify FaviconHandler status.
  EXPECT_EQ(0u, driver.num_notifications());
  EXPECT_EQ(GURL(), driver.icon_url());

  // Reset the history_handler to verify whether new icon is requested from
  // history.
  helper.set_history_handler(nullptr);

  // Simulates update with the different favicon url.
  std::vector<FaviconURL> urls;
  urls.push_back(FaviconURL(icon_url,
                            favicon_base::TOUCH_PRECOMPOSED_ICON,
                            std::vector<gfx::Size>()));
  urls.push_back(FaviconURL(
      new_icon_url, favicon_base::TOUCH_ICON, std::vector<gfx::Size>()));
  urls.push_back(FaviconURL(
      new_icon_url, favicon_base::FAVICON, std::vector<gfx::Size>()));
  helper.OnUpdateFaviconURL(page_url, urls);

  // Verify FaviconHandler status.
  EXPECT_EQ(2u, helper.image_urls().size());
  EXPECT_EQ(0u, helper.current_candidate_index());
  ASSERT_TRUE(helper.current_candidate());
  ASSERT_EQ(icon_url, helper.current_candidate()->icon_url);
  ASSERT_EQ(favicon_base::TOUCH_PRECOMPOSED_ICON,
            helper.current_candidate()->icon_type);

  // Favicon should be requested from history.
  history_handler = helper.history_handler();
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(icon_url, history_handler->icon_url_);
  EXPECT_EQ(favicon_base::TOUCH_PRECOMPOSED_ICON, history_handler->icon_type_);
  EXPECT_EQ(page_url, history_handler->page_url_);

  // Simulate not find icon.
  history_handler->history_results_.clear();
  history_handler->InvokeCallback();

  // Should request download favicon.
  DownloadHandler* download_handler = helper.download_handler();
  EXPECT_TRUE(helper.download_handler()->HasDownload());

  // Verify the download request.
  EXPECT_EQ(icon_url, download_handler->GetImageUrl());

  // Reset the history_handler to verify whether favicon is request from
  // history.
  helper.set_history_handler(nullptr);
  download_handler->InvokeCallback();

  // Left 1 url.
  EXPECT_EQ(1u, helper.current_candidate_index());
  ASSERT_TRUE(helper.current_candidate());
  EXPECT_EQ(new_icon_url, helper.current_candidate()->icon_url);
  EXPECT_EQ(favicon_base::TOUCH_ICON, helper.current_candidate()->icon_type);

  // Favicon should be requested from history.
  history_handler = helper.history_handler();
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(new_icon_url, history_handler->icon_url_);
  EXPECT_EQ(favicon_base::TOUCH_ICON, history_handler->icon_type_);
  EXPECT_EQ(page_url, history_handler->page_url_);

  // Reset download handler
  download_handler->Reset();

  // Simulates getting a expired icon from history.
  SetFaviconRawBitmapResult(new_icon_url,
                            favicon_base::TOUCH_ICON,
                            true /* expired */,
                            &history_handler->history_results_);
  history_handler->InvokeCallback();

  // Verify the download request.
  EXPECT_TRUE(helper.download_handler()->HasDownload());
  EXPECT_EQ(new_icon_url, download_handler->GetImageUrl());

  helper.set_history_handler(nullptr);

  // Simulates icon being downloaded.
  download_handler->InvokeCallback();

  // New icon should be saved to history backend.
  history_handler = helper.history_handler();
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(new_icon_url, history_handler->icon_url_);
  EXPECT_EQ(favicon_base::TOUCH_ICON, history_handler->icon_type_);
  EXPECT_LT(0U, history_handler->bitmap_data_.size());
  EXPECT_EQ(page_url, history_handler->page_url_);
}

TEST_F(FaviconHandlerTest, UpdateDuringDownloading) {
  const GURL page_url("http://www.google.com");
  const GURL icon_url("http://www.google.com/favicon");
  const GURL new_icon_url("http://www.google.com/new_favicon");

  TestFaviconDriver driver;
  TestFaviconHandler helper(&driver, FaviconDriverObserver::TOUCH_LARGEST);

  helper.FetchFavicon(page_url);
  HistoryRequestHandler* history_handler = helper.history_handler();
  // Ensure the data given to history is correct.
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(page_url, history_handler->page_url_);
  EXPECT_EQ(GURL(), history_handler->icon_url_);
  EXPECT_EQ(favicon_base::TOUCH_PRECOMPOSED_ICON | favicon_base::TOUCH_ICON,
            history_handler->icon_type_);

  // Icon not found.
  history_handler->history_results_.clear();
  // Send history response.
  history_handler->InvokeCallback();
  // Verify FaviconHandler status.
  EXPECT_EQ(0u, driver.num_notifications());
  EXPECT_EQ(GURL(), driver.icon_url());

  // Reset the history_handler to verify whether new icon is requested from
  // history.
  helper.set_history_handler(nullptr);

  // Simulates update with the different favicon url.
  std::vector<FaviconURL> urls;
  urls.push_back(FaviconURL(icon_url,
                            favicon_base::TOUCH_PRECOMPOSED_ICON,
                            std::vector<gfx::Size>()));
  urls.push_back(FaviconURL(
      new_icon_url, favicon_base::TOUCH_ICON, std::vector<gfx::Size>()));
  urls.push_back(FaviconURL(
      new_icon_url, favicon_base::FAVICON, std::vector<gfx::Size>()));
  helper.OnUpdateFaviconURL(page_url, urls);

  // Verify FaviconHandler status.
  EXPECT_EQ(2u, helper.image_urls().size());
  ASSERT_EQ(0u, helper.current_candidate_index());
  ASSERT_EQ(icon_url, helper.current_candidate()->icon_url);
  ASSERT_EQ(favicon_base::TOUCH_PRECOMPOSED_ICON,
            helper.current_candidate()->icon_type);

  // Favicon should be requested from history.
  history_handler = helper.history_handler();
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(icon_url, history_handler->icon_url_);
  EXPECT_EQ(favicon_base::TOUCH_PRECOMPOSED_ICON, history_handler->icon_type_);
  EXPECT_EQ(page_url, history_handler->page_url_);

  // Simulate not find icon.
  history_handler->history_results_.clear();
  history_handler->InvokeCallback();

  // Should request download favicon.
  DownloadHandler* download_handler = helper.download_handler();
  EXPECT_TRUE(helper.download_handler()->HasDownload());

  // Verify the download request.
  EXPECT_EQ(icon_url, download_handler->GetImageUrl());

  // Reset the history_handler to verify whether favicon is request from
  // history.
  helper.set_history_handler(nullptr);
  const GURL latest_icon_url("http://www.google.com/latest_favicon");
  std::vector<FaviconURL> latest_urls;
  latest_urls.push_back(FaviconURL(
      latest_icon_url, favicon_base::TOUCH_ICON, std::vector<gfx::Size>()));
  helper.OnUpdateFaviconURL(page_url, latest_urls);

  EXPECT_EQ(1u, helper.image_urls().size());
  EXPECT_EQ(0u, helper.current_candidate_index());
  EXPECT_EQ(latest_icon_url, helper.current_candidate()->icon_url);
  EXPECT_EQ(favicon_base::TOUCH_ICON, helper.current_candidate()->icon_type);

  // Whether new icon is requested from history
  history_handler = helper.history_handler();
  ASSERT_TRUE(history_handler);
  EXPECT_EQ(latest_icon_url, history_handler->icon_url_);
  EXPECT_EQ(favicon_base::TOUCH_ICON, history_handler->icon_type_);
  EXPECT_EQ(page_url, history_handler->page_url_);

  // Reset the history_handler to verify whether favicon is request from
  // history.
  // Save the callback for late use.
  favicon_base::FaviconResultsCallback callback = history_handler->callback_;
  helper.set_history_handler(nullptr);

  // Simulates download succeed.
  download_handler->InvokeCallback();
  // The downloaded icon should be thrown away as there is favicon update.
  EXPECT_FALSE(helper.history_handler());

  download_handler->Reset();

  // Simulates getting the icon from history.
  std::unique_ptr<HistoryRequestHandler> handler;
  handler.reset(new HistoryRequestHandler(
      page_url, latest_icon_url, favicon_base::TOUCH_ICON, callback));
  SetFaviconRawBitmapResult(latest_icon_url,
                            favicon_base::TOUCH_ICON,
                            false /* expired */,
                            &handler->history_results_);
  handler->InvokeCallback();

  // No download request.
  EXPECT_FALSE(download_handler->HasDownload());
}

// Test that sending an icon URL update identical to the previous icon URL
// update is a no-op.
TEST_F(FaviconHandlerTest, UpdateSameIconURLs) {
  const GURL page_url("http://www.google.com");
  const GURL icon_url1("http://www.google.com/favicon1");
  const GURL icon_url2("http://www.google.com/favicon2");
  std::vector<FaviconURL> favicon_urls;
  favicon_urls.push_back(FaviconURL(GURL("http://www.google.com/favicon1"),
                                    favicon_base::FAVICON,
                                    std::vector<gfx::Size>()));
  favicon_urls.push_back(FaviconURL(GURL("http://www.google.com/favicon2"),
                                    favicon_base::FAVICON,
                                    std::vector<gfx::Size>()));

  TestFaviconDriver driver;
  TestFaviconHandler helper(&driver, FaviconDriverObserver::NON_TOUCH_16_DIP);

  // Initiate a request for favicon data for |page_url|. History does not know
  // about the page URL or the icon URLs.
  helper.FetchFavicon(page_url);
  helper.history_handler()->InvokeCallback();
  helper.set_history_handler(nullptr);

  // Got icon URLs.
  helper.OnUpdateFaviconURL(page_url, favicon_urls);

  // There should be an ongoing history request for |icon_url1|.
  ASSERT_EQ(2u, helper.image_urls().size());
  ASSERT_EQ(0u, helper.current_candidate_index());
  HistoryRequestHandler* history_handler = helper.history_handler();
  ASSERT_TRUE(history_handler);

  // Calling OnUpdateFaviconURL() with the same icon URLs should have no effect.
  helper.OnUpdateFaviconURL(page_url, favicon_urls);
  EXPECT_EQ(history_handler, helper.history_handler());

  // Complete history request for |icon_url1| and do download.
  helper.history_handler()->InvokeCallback();
  helper.set_history_handler(nullptr);
  helper.download_handler()->SetImageSizes(std::vector<int>(1u, 10));
  helper.download_handler()->InvokeCallback();
  helper.download_handler()->Reset();

  // There should now be an ongoing history request for |icon_url2|.
  ASSERT_EQ(1u, helper.current_candidate_index());
  history_handler = helper.history_handler();
  ASSERT_TRUE(history_handler);

  // Calling OnUpdateFaviconURL() with the same icon URLs should have no effect.
  helper.OnUpdateFaviconURL(page_url, favicon_urls);
  EXPECT_EQ(history_handler, helper.history_handler());
}

// Fixes crbug.com/544560
TEST_F(FaviconHandlerTest,
       OnFaviconAvailableNotificationSentAfterIconURLChange) {
  const GURL kPageURL("http://www.page_which_animates_favicon.com");
  const GURL kIconURL1("http://wwww.page_which_animates_favicon.com/frame1.png");
  const GURL kIconURL2("http://wwww.page_which_animates_favicon.com/frame2.png");

  TestFaviconDriver driver;
  TestFaviconHandler helper(&driver, FaviconDriverObserver::NON_TOUCH_16_DIP);

  // Initial state:
  // - The database does not know about |kPageURL|.
  // - The page uses |kIconURL1| and |kIconURL2|.
  // - The database knows about both |kIconURL1| and |kIconURl2|. Both icons
  //   are expired in the database.
  helper.FetchFavicon(kPageURL);
  ASSERT_TRUE(helper.history_handler());
  helper.history_handler()->InvokeCallback();
  {
    std::vector<FaviconURL> icon_urls;
    icon_urls.push_back(
        FaviconURL(kIconURL1, favicon_base::FAVICON, std::vector<gfx::Size>()));
    icon_urls.push_back(
        FaviconURL(kIconURL2, favicon_base::FAVICON, std::vector<gfx::Size>()));
    helper.OnUpdateFaviconURL(kPageURL, icon_urls);
  }

  // FaviconHandler should request from history and download |kIconURL1| and
  // |kIconURL2|. |kIconURL1| is the better match. A
  // FaviconDriver::OnFaviconUpdated() notification should be sent for
  // |kIconURL1|.
  ASSERT_EQ(0u, driver.num_notifications());
  ASSERT_TRUE(helper.history_handler());
  SetFaviconRawBitmapResult(kIconURL1,
                            favicon_base::FAVICON,
                            true /* expired */,
                            &helper.history_handler()->history_results_);
  helper.history_handler()->InvokeCallback();
  helper.set_history_handler(nullptr);
  ASSERT_TRUE(helper.download_handler()->HasDownload());
  helper.download_handler()->SetImageSizes(std::vector<int>(1u, 15));
  helper.download_handler()->InvokeCallback();
  helper.download_handler()->Reset();

  ASSERT_TRUE(helper.history_handler());
  helper.history_handler()->InvokeCallback();
  SetFaviconRawBitmapResult(kIconURL2,
                            favicon_base::FAVICON,
                            true /* expired */,
                            &helper.history_handler()->history_results_);
  helper.history_handler()->InvokeCallback();
  helper.set_history_handler(nullptr);
  ASSERT_TRUE(helper.download_handler()->HasDownload());
  helper.download_handler()->SetImageSizes(std::vector<int>(1u, 10));
  helper.download_handler()->InvokeCallback();
  helper.download_handler()->Reset();

  ASSERT_LT(0u, driver.num_notifications());
  ASSERT_EQ(kIconURL1, driver.icon_url());

  // Clear the history handler because SetHistoryFavicons() sets it.
  helper.set_history_handler(nullptr);

  // Simulate the page changing it's icon URL to just |kIconURL2| via
  // Javascript.
  helper.OnUpdateFaviconURL(
      kPageURL,
      std::vector<FaviconURL>(1u, FaviconURL(kIconURL2, favicon_base::FAVICON,
                                             std::vector<gfx::Size>())));

  // FaviconHandler should request from history and download |kIconURL2|. A
  // FaviconDriver::OnFaviconUpdated() notification should be sent for
  // |kIconURL2|.
  driver.ResetNumNotifications();
  ASSERT_TRUE(helper.history_handler());
  SetFaviconRawBitmapResult(kIconURL2,
                            favicon_base::FAVICON,
                            true /* expired */,
                            &helper.history_handler()->history_results_);
  helper.history_handler()->InvokeCallback();
  helper.set_history_handler(nullptr);
  ASSERT_TRUE(helper.download_handler()->HasDownload());
  helper.download_handler()->InvokeCallback();
  helper.download_handler()->Reset();
  ASSERT_LT(0u, driver.num_notifications());
  EXPECT_EQ(kIconURL2, driver.icon_url());
}

// Test the favicon which is selected when the web page provides several
// favicons and none of the favicons are cached in history.
// The goal of this test is to be more of an integration test than
// SelectFaviconFramesTest.*.
TEST_F(FaviconHandlerTest, MultipleFavicons) {
  const GURL kPageURL("http://www.google.com");
  const FaviconURL kSourceIconURLs[] = {
      FaviconURL(GURL("http://www.google.com/a"),
                 favicon_base::FAVICON,
                 std::vector<gfx::Size>()),
      FaviconURL(GURL("http://www.google.com/b"),
                 favicon_base::FAVICON,
                 std::vector<gfx::Size>()),
      FaviconURL(GURL("http://www.google.com/c"),
                 favicon_base::FAVICON,
                 std::vector<gfx::Size>()),
      FaviconURL(GURL("http://www.google.com/d"),
                 favicon_base::FAVICON,
                 std::vector<gfx::Size>()),
      FaviconURL(GURL("http://www.google.com/e"),
                 favicon_base::FAVICON,
                 std::vector<gfx::Size>())};

  // Set the supported scale factors to 1x and 2x. This affects the behavior of
  // SelectFaviconFrames().
  std::vector<ui::ScaleFactor> scale_factors;
  scale_factors.push_back(ui::SCALE_FACTOR_100P);
  scale_factors.push_back(ui::SCALE_FACTOR_200P);
  ui::test::ScopedSetSupportedScaleFactors scoped_supported(scale_factors);

  // 1) Test that if there are several single resolution favicons to choose from
  // that the largest exact match is chosen.
  TestFaviconDriver driver1;
  TestFaviconHandler handler1(&driver1,
                              FaviconDriverObserver::NON_TOUCH_16_DIP);

  const int kSizes1[] = { 16, 24, 32, 48, 256 };
  std::vector<FaviconURL> urls1(kSourceIconURLs,
                                kSourceIconURLs + arraysize(kSizes1));
  DownloadTillDoneIgnoringHistory(
      &driver1, &handler1, kPageURL, urls1, kSizes1);

  EXPECT_EQ(nullptr, handler1.current_candidate());
  EXPECT_EQ(1u, driver1.num_notifications());
  EXPECT_FALSE(driver1.image().IsEmpty());
  EXPECT_EQ(gfx::kFaviconSize, driver1.image().Width());

  size_t expected_index = 2u;
  EXPECT_EQ(32, kSizes1[expected_index]);
  EXPECT_EQ(kSourceIconURLs[expected_index].icon_url, driver1.icon_url());

  // 2) Test that if there are several single resolution favicons to choose
  // from, the exact match is preferred even if it results in upsampling.
  TestFaviconDriver driver2;
  TestFaviconHandler handler2(&driver2,
                              FaviconDriverObserver::NON_TOUCH_16_DIP);

  const int kSizes2[] = { 16, 24, 48, 256 };
  std::vector<FaviconURL> urls2(kSourceIconURLs,
                                kSourceIconURLs + arraysize(kSizes2));
  DownloadTillDoneIgnoringHistory(
      &driver2, &handler2, kPageURL, urls2, kSizes2);
  EXPECT_EQ(1u, driver2.num_notifications());
  expected_index = 0u;
  EXPECT_EQ(16, kSizes2[expected_index]);
  EXPECT_EQ(kSourceIconURLs[expected_index].icon_url, driver2.icon_url());

  // 3) Test that favicons which need to be upsampled a little or downsampled
  // a little are preferred over huge favicons.
  TestFaviconDriver driver3;
  TestFaviconHandler handler3(&driver3,
                              FaviconDriverObserver::NON_TOUCH_16_DIP);

  const int kSizes3[] = { 256, 48 };
  std::vector<FaviconURL> urls3(kSourceIconURLs,
                                kSourceIconURLs + arraysize(kSizes3));
  DownloadTillDoneIgnoringHistory(
      &driver3, &handler3, kPageURL, urls3, kSizes3);
  EXPECT_EQ(1u, driver3.num_notifications());
  expected_index = 1u;
  EXPECT_EQ(48, kSizes3[expected_index]);
  EXPECT_EQ(kSourceIconURLs[expected_index].icon_url, driver3.icon_url());

  TestFaviconDriver driver4;
  TestFaviconHandler handler4(&driver4,
                              FaviconDriverObserver::NON_TOUCH_16_DIP);

  const int kSizes4[] = { 17, 256 };
  std::vector<FaviconURL> urls4(kSourceIconURLs,
                                kSourceIconURLs + arraysize(kSizes4));
  DownloadTillDoneIgnoringHistory(
      &driver4, &handler4, kPageURL, urls4, kSizes4);
  EXPECT_EQ(1u, driver4.num_notifications());
  expected_index = 0u;
  EXPECT_EQ(17, kSizes4[expected_index]);
  EXPECT_EQ(kSourceIconURLs[expected_index].icon_url, driver4.icon_url());
}

// Test that the best favicon is selected when:
// - The page provides several favicons.
// - Downloading one of the page's icon URLs previously returned a 404.
// - None of the favicons are cached in the Favicons database.
TEST_F(FaviconHandlerTest, MultipleFavicons404) {
  const GURL kPageURL("http://www.google.com");
  const GURL k404IconURL("http://www.google.com/404.png");
  const FaviconURL k404FaviconURL(
      k404IconURL, favicon_base::FAVICON, std::vector<gfx::Size>());
  const FaviconURL kFaviconURLs[] = {
      FaviconURL(GURL("http://www.google.com/a"),
                 favicon_base::FAVICON,
                 std::vector<gfx::Size>()),
      k404FaviconURL,
      FaviconURL(GURL("http://www.google.com/c"),
                 favicon_base::FAVICON,
                 std::vector<gfx::Size>()),
  };

  TestFaviconDriver driver;
  TestFaviconHandler handler(&driver, FaviconDriverObserver::NON_TOUCH_16_DIP);
  DownloadHandler* download_handler = handler.download_handler();

  std::set<GURL> k404URLs;
  k404URLs.insert(k404IconURL);
  download_handler->FailDownloadForIconURLs(k404URLs);

  // Make the initial download for |k404IconURL| fail.
  const int kSizes1[] = { 0 };
  std::vector<FaviconURL> urls1(1u, k404FaviconURL);
  DownloadTillDoneIgnoringHistory(
      &driver, &handler, kPageURL, urls1, kSizes1);
  EXPECT_TRUE(download_handler->DidFailDownloadForIconURL(k404IconURL));

  // Do a fetch now that the initial download for |k404IconURL| has failed. The
  // behavior is different because OnDidDownloadFavicon() is invoked
  // synchronously from DownloadFavicon().
  const int kSizes2[] = { 10, 0, 16 };
  std::vector<FaviconURL> urls2(kFaviconURLs,
                                kFaviconURLs + arraysize(kFaviconURLs));
  DownloadTillDoneIgnoringHistory(
      &driver, &handler, kPageURL, urls2, kSizes2);

  EXPECT_EQ(nullptr, handler.current_candidate());
  EXPECT_EQ(1u, driver.num_notifications());
  EXPECT_FALSE(driver.image().IsEmpty());
  int expected_index = 2u;
  EXPECT_EQ(16, kSizes2[expected_index]);
  EXPECT_EQ(kFaviconURLs[expected_index].icon_url, driver.icon_url());
}

// Test that no favicon is selected when:
// - The page provides several favicons.
// - Downloading the page's icons has previously returned a 404.
// - None of the favicons are cached in the Favicons database.
TEST_F(FaviconHandlerTest, MultipleFaviconsAll404) {
  const GURL kPageURL("http://www.google.com");
  const GURL k404IconURL1("http://www.google.com/4041.png");
  const GURL k404IconURL2("http://www.google.com/4042.png");
  const FaviconURL kFaviconURLs[] = {
      FaviconURL(k404IconURL1,
                 favicon_base::FAVICON,
                 std::vector<gfx::Size>()),
      FaviconURL(k404IconURL2,
                 favicon_base::FAVICON,
                 std::vector<gfx::Size>()),
  };

  TestFaviconDriver driver;
  TestFaviconHandler handler(&driver, FaviconDriverObserver::NON_TOUCH_16_DIP);
  DownloadHandler* download_handler = handler.download_handler();

  std::set<GURL> k404URLs;
  k404URLs.insert(k404IconURL1);
  k404URLs.insert(k404IconURL2);
  download_handler->FailDownloadForIconURLs(k404URLs);

  // Make the initial downloads for |kFaviconURLs| fail.
  for (const FaviconURL& favicon_url : kFaviconURLs) {
    const int kSizes[] = { 0 };
    std::vector<FaviconURL> urls(1u, favicon_url);
    DownloadTillDoneIgnoringHistory(&driver, &handler, kPageURL, urls, kSizes);
  }
  EXPECT_TRUE(download_handler->DidFailDownloadForIconURL(k404IconURL1));
  EXPECT_TRUE(download_handler->DidFailDownloadForIconURL(k404IconURL2));

  // Do a fetch now that the initial downloads for |kFaviconURLs| have failed.
  // The behavior is different because OnDidDownloadFavicon() is invoked
  // synchronously from DownloadFavicon().
  const int kSizes[] = { 0, 0 };
  std::vector<FaviconURL> urls(kFaviconURLs,
                               kFaviconURLs + arraysize(kFaviconURLs));
  DownloadTillDoneIgnoringHistory(&driver, &handler, kPageURL, urls, kSizes);

  EXPECT_EQ(nullptr, handler.current_candidate());
  EXPECT_EQ(0u, driver.num_notifications());
  EXPECT_TRUE(driver.image().IsEmpty());
}

// Test that no favicon is selected when the page's only icon uses an invalid
// URL syntax.
TEST_F(FaviconHandlerTest, FaviconInvalidURL) {
  const GURL kPageURL("http://www.google.com");
  const GURL kInvalidFormatURL("invalid");
  ASSERT_TRUE(kInvalidFormatURL.is_empty());

  FaviconURL favicon_url(kInvalidFormatURL, favicon_base::FAVICON,
                         std::vector<gfx::Size>());

  TestFaviconDriver driver;
  TestFaviconHandler handler(&driver, FaviconDriverObserver::NON_TOUCH_16_DIP);
  UpdateFaviconURL(&driver, &handler, kPageURL,
                   std::vector<FaviconURL>(1u, favicon_url));
  EXPECT_EQ(0u, handler.image_urls().size());
}

TEST_F(FaviconHandlerTest, TestSortFavicon) {
  const GURL kPageURL("http://www.google.com");
  std::vector<gfx::Size> icon1;
  icon1.push_back(gfx::Size(1024, 1024));
  icon1.push_back(gfx::Size(512, 512));

  std::vector<gfx::Size> icon2;
  icon2.push_back(gfx::Size(15, 15));
  icon2.push_back(gfx::Size(16, 16));

  std::vector<gfx::Size> icon3;
  icon3.push_back(gfx::Size(16, 16));
  icon3.push_back(gfx::Size(14, 14));

  const FaviconURL kSourceIconURLs[] = {
      FaviconURL(GURL("http://www.google.com/a"), favicon_base::FAVICON, icon1),
      FaviconURL(GURL("http://www.google.com/b"), favicon_base::FAVICON, icon2),
      FaviconURL(GURL("http://www.google.com/c"), favicon_base::FAVICON, icon3),
      FaviconURL(GURL("http://www.google.com/d"),
                 favicon_base::FAVICON,
                 std::vector<gfx::Size>()),
      FaviconURL(GURL("http://www.google.com/e"),
                 favicon_base::FAVICON,
                 std::vector<gfx::Size>())};

  TestFaviconDriver driver1;
  TestFaviconHandler handler1(&driver1,
                              FaviconDriverObserver::NON_TOUCH_LARGEST);
  std::vector<FaviconURL> urls1(kSourceIconURLs,
                                kSourceIconURLs + arraysize(kSourceIconURLs));
  UpdateFaviconURL(&driver1, &handler1, kPageURL, urls1);

  struct ExpectedResult {
    // The favicon's index in kSourceIconURLs.
    size_t favicon_index;
    // Width of largest bitmap.
    int width;
  } results[] = {
    // First is icon1, though its size larger than maximal.
    {0, 1024},
    // Second is icon2
    // The 16x16 is largest.
    {1, 16},
    // Third is icon3 though it has same size as icon2.
    // The 16x16 is largest.
    {2, 16},
    // The rest of bitmaps come in order, there is no sizes attribute.
    {3, -1},
    {4, -1},
  };
  const std::vector<FaviconURL>& icons = handler1.image_urls();
  ASSERT_EQ(5u, icons.size());
  for (size_t i = 0; i < icons.size(); ++i) {
    EXPECT_EQ(kSourceIconURLs[results[i].favicon_index].icon_url,
              icons[i].icon_url);
    if (results[i].width != -1)
      EXPECT_EQ(results[i].width, icons[i].icon_sizes[0].width());
  }
}

TEST_F(FaviconHandlerTest, TestDownloadLargestFavicon) {
  const GURL kPageURL("http://www.google.com");
  std::vector<gfx::Size> icon1;
  icon1.push_back(gfx::Size(1024, 1024));
  icon1.push_back(gfx::Size(512, 512));

  std::vector<gfx::Size> icon2;
  icon2.push_back(gfx::Size(15, 15));
  icon2.push_back(gfx::Size(14, 14));

  std::vector<gfx::Size> icon3;
  icon3.push_back(gfx::Size(16, 16));
  icon3.push_back(gfx::Size(512, 512));

  const FaviconURL kSourceIconURLs[] = {
      FaviconURL(
          GURL("http://www.google.com/a"), favicon_base::FAVICON, icon1),
      FaviconURL(
          GURL("http://www.google.com/b"), favicon_base::FAVICON, icon2),
      FaviconURL(
          GURL("http://www.google.com/c"), favicon_base::FAVICON, icon3),
      FaviconURL(GURL("http://www.google.com/d"),
                 favicon_base::FAVICON,
                 std::vector<gfx::Size>()),
      FaviconURL(GURL("http://www.google.com/e"),
                 favicon_base::FAVICON,
                 std::vector<gfx::Size>())};

  TestFaviconDriver driver1;
  TestFaviconHandler handler1(&driver1,
                              FaviconDriverObserver::NON_TOUCH_LARGEST);

  std::set<GURL> fail_icon_urls;
  for (size_t i = 0; i < arraysize(kSourceIconURLs); ++i) {
    fail_icon_urls.insert(kSourceIconURLs[i].icon_url);
  }
  handler1.download_handler()->FailDownloadForIconURLs(fail_icon_urls);

  std::vector<FaviconURL> urls1(kSourceIconURLs,
                                kSourceIconURLs + arraysize(kSourceIconURLs));
  UpdateFaviconURL(&driver1, &handler1, kPageURL, urls1);

  // Simulate the download failed, to check whether the icons were requested
  // to download according their size.
  struct ExpectedResult {
    // The favicon's index in kSourceIconURLs.
    size_t favicon_index;
    // Width of largest bitmap.
    int width;
  } results[] = {
    {0, 1024},
    {2, 512},
    {1, 15},
    // The rest of bitmaps come in order.
    {3, -1},
    {4, -1},
  };

  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(kSourceIconURLs[results[i].favicon_index].icon_url,
              handler1.current_candidate()->icon_url);
    if (results[i].width != -1) {
      EXPECT_EQ(results[i].width, handler1.current_candidate()->
                icon_sizes[0].width());
    }

    // Simulate no favicon from history.
    handler1.history_handler()->history_results_.clear();
    handler1.history_handler()->InvokeCallback();

    // Verify download request
    ASSERT_TRUE(handler1.download_handler()->HasDownload());
    EXPECT_EQ(kSourceIconURLs[results[i].favicon_index].icon_url,
              handler1.download_handler()->GetImageUrl());

    handler1.download_handler()->InvokeCallback();
    handler1.download_handler()->Reset();
  }
}

TEST_F(FaviconHandlerTest, TestSelectLargestFavicon) {
  const GURL kPageURL("http://www.google.com");

  std::vector<gfx::Size> one_icon;
  one_icon.push_back(gfx::Size(15, 15));

  std::vector<gfx::Size> two_icons;
  two_icons.push_back(gfx::Size(14, 14));
  two_icons.push_back(gfx::Size(16, 16));

  const FaviconURL kSourceIconURLs[] = {
      FaviconURL(
          GURL("http://www.google.com/b"), favicon_base::FAVICON, one_icon),
      FaviconURL(
          GURL("http://www.google.com/c"), favicon_base::FAVICON, two_icons)};

  TestFaviconDriver driver1;
  TestFaviconHandler handler1(&driver1,
                              FaviconDriverObserver::NON_TOUCH_LARGEST);
  std::vector<FaviconURL> urls1(kSourceIconURLs,
                                kSourceIconURLs + arraysize(kSourceIconURLs));
  UpdateFaviconURL(&driver1, &handler1, kPageURL, urls1);

  ASSERT_EQ(2u, handler1.image_urls().size());

  // Index of largest favicon in kSourceIconURLs.
  size_t i = 1;
  // The largest bitmap's index in Favicon .
  int b = 1;

  // Verify the icon_bitmaps_ was initialized correctly.
  EXPECT_EQ(kSourceIconURLs[i].icon_url,
            handler1.current_candidate()->icon_url);
  EXPECT_EQ(kSourceIconURLs[i].icon_sizes[b],
            handler1.current_candidate()->icon_sizes[0]);

  // Simulate no favicon from history.
  handler1.history_handler()->history_results_.clear();
  handler1.history_handler()->InvokeCallback();

  // Verify download request
  ASSERT_TRUE(handler1.download_handler()->HasDownload());
  EXPECT_EQ(kSourceIconURLs[i].icon_url,
            handler1.download_handler()->GetImageUrl());

  // Give the correct download result.
  std::vector<int> sizes;
  for (std::vector<gfx::Size>::const_iterator j =
           kSourceIconURLs[i].icon_sizes.begin();
       j != kSourceIconURLs[i].icon_sizes.end(); ++j)
    sizes.push_back(j->width());

  handler1.download_handler()->SetImageSizes(sizes);
  handler1.download_handler()->InvokeCallback();

  // Verify the largest bitmap has been saved into history.
  EXPECT_EQ(kSourceIconURLs[i].icon_url, handler1.history_handler()->icon_url_);
  EXPECT_EQ(kSourceIconURLs[i].icon_sizes[b],
            handler1.history_handler()->size_);
  // Verify NotifyFaviconAvailable().
  EXPECT_EQ(1u, driver1.num_notifications());
  EXPECT_EQ(kSourceIconURLs[i].icon_url, driver1.icon_url());
  EXPECT_EQ(kSourceIconURLs[i].icon_sizes[b], driver1.image().Size());
}

TEST_F(FaviconHandlerTest, TestFaviconWasScaledAfterDownload) {
  const GURL kPageURL("http://www.google.com");
  const int kMaximalSize =
    TestFaviconHandler::GetMaximalIconSize(favicon_base::FAVICON);

  std::vector<gfx::Size> icon1;
  icon1.push_back(gfx::Size(kMaximalSize + 1, kMaximalSize + 1));

  std::vector<gfx::Size> icon2;
  icon2.push_back(gfx::Size(kMaximalSize + 2, kMaximalSize + 2));

  const FaviconURL kSourceIconURLs[] = {
      FaviconURL(
          GURL("http://www.google.com/b"), favicon_base::FAVICON, icon1),
      FaviconURL(
          GURL("http://www.google.com/c"), favicon_base::FAVICON, icon2)};

  TestFaviconDriver driver1;
  TestFaviconHandler handler1(&driver1,
                              FaviconDriverObserver::NON_TOUCH_LARGEST);
  std::vector<FaviconURL> urls1(kSourceIconURLs,
                                kSourceIconURLs + arraysize(kSourceIconURLs));
  UpdateFaviconURL(&driver1, &handler1, kPageURL, urls1);

  ASSERT_EQ(2u, handler1.image_urls().size());

  // Index of largest favicon in kSourceIconURLs.
  size_t i = 1;
  // The largest bitmap's index in Favicon .
  int b = 0;

  // Verify the icon_bitmaps_ was initialized correctly.
  EXPECT_EQ(kSourceIconURLs[i].icon_url,
            handler1.current_candidate()->icon_url);
  EXPECT_EQ(kSourceIconURLs[i].icon_sizes[b],
            handler1.current_candidate()->icon_sizes[0]);

  // Simulate no favicon from history.
  handler1.history_handler()->history_results_.clear();
  handler1.history_handler()->InvokeCallback();

  // Verify download request
  ASSERT_TRUE(handler1.download_handler()->HasDownload());
  EXPECT_EQ(kSourceIconURLs[i].icon_url,
            handler1.download_handler()->GetImageUrl());

  // Give the scaled download bitmap.
  std::vector<int> sizes;
  sizes.push_back(kMaximalSize);

  handler1.download_handler()->SetImageSizes(sizes);
  handler1.download_handler()->InvokeCallback();

  // Verify the largest bitmap has been saved into history though it was
  // scaled down to maximal size and smaller than icon1 now.
  EXPECT_EQ(kSourceIconURLs[i].icon_url, handler1.history_handler()->icon_url_);
  EXPECT_EQ(gfx::Size(kMaximalSize, kMaximalSize),
            handler1.history_handler()->size_);
}

TEST_F(FaviconHandlerTest, TestKeepDownloadedLargestFavicon) {
  const GURL kPageURL("http://www.google.com");

  std::vector<gfx::Size> icon1;
  icon1.push_back(gfx::Size(16, 16));
  const int actual_size1 = 10;

  std::vector<gfx::Size> icon2;
  icon2.push_back(gfx::Size(15, 15));
  const int actual_size2 = 12;

  const FaviconURL kSourceIconURLs[] = {
      FaviconURL(GURL("http://www.google.com/b"), favicon_base::FAVICON, icon1),
      FaviconURL(GURL("http://www.google.com/c"), favicon_base::FAVICON, icon2),
      FaviconURL(GURL("http://www.google.com/d"),
                 favicon_base::FAVICON,
                 std::vector<gfx::Size>())};

  TestFaviconDriver driver1;
  TestFaviconHandler handler1(&driver1,
                              FaviconDriverObserver::NON_TOUCH_LARGEST);
  std::vector<FaviconURL> urls1(kSourceIconURLs,
                                kSourceIconURLs + arraysize(kSourceIconURLs));
  UpdateFaviconURL(&driver1, &handler1, kPageURL, urls1);
  ASSERT_EQ(3u, handler1.image_urls().size());

  // Simulate no favicon from history.
  handler1.history_handler()->history_results_.clear();
  handler1.history_handler()->InvokeCallback();

  // Verify the first icon was request to download
  ASSERT_TRUE(handler1.download_handler()->HasDownload());
  EXPECT_EQ(kSourceIconURLs[0].icon_url,
            handler1.download_handler()->GetImageUrl());

  // Give the incorrect size.
  std::vector<int> sizes;
  sizes.push_back(actual_size1);
  handler1.download_handler()->SetImageSizes(sizes);
  handler1.download_handler()->InvokeCallback();
  handler1.download_handler()->Reset();

  // Simulate no favicon from history.
  handler1.history_handler()->history_results_.clear();
  handler1.history_handler()->InvokeCallback();

  // Verify the 2nd icon was request to download
  ASSERT_TRUE(handler1.download_handler()->HasDownload());
  EXPECT_EQ(kSourceIconURLs[1].icon_url,
            handler1.download_handler()->GetImageUrl());

  // Very the best candidate is icon1
  EXPECT_EQ(kSourceIconURLs[0].icon_url,
            handler1.best_favicon_candidate().image_url);
  EXPECT_EQ(gfx::Size(actual_size1, actual_size1),
            handler1.best_favicon_candidate().image.Size());

  // Give the incorrect size.
  sizes.clear();
  sizes.push_back(actual_size2);
  handler1.download_handler()->SetImageSizes(sizes);
  handler1.download_handler()->InvokeCallback();
  handler1.download_handler()->Reset();

  // Verify icon2 has been saved into history.
  EXPECT_EQ(kSourceIconURLs[1].icon_url, handler1.history_handler()->icon_url_);
  EXPECT_EQ(gfx::Size(actual_size2, actual_size2),
            handler1.history_handler()->size_);
}

}  // namespace
}  // namespace favicon
