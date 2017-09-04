// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/content/content_favicon_driver.h"

#include <memory>

#include "base/macros.h"
#include "components/favicon/core/favicon_client.h"
#include "components/favicon/core/favicon_handler.h"
#include "components/favicon/core/favicon_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/favicon_url.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/favicon_size.h"

namespace favicon {
namespace {

class ContentFaviconDriverTest : public content::RenderViewHostTestHarness {
 public:
  ContentFaviconDriverTest() {}

  ~ContentFaviconDriverTest() override {}

  // content::RenderViewHostTestHarness:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    favicon_service_.reset(new FaviconService(nullptr, nullptr));
    ContentFaviconDriver::CreateForWebContents(
        web_contents(), favicon_service(), nullptr, nullptr);
  }

  FaviconService* favicon_service() {
    return favicon_service_.get();
  }

 private:
  std::unique_ptr<FaviconService> favicon_service_;

  DISALLOW_COPY_AND_ASSIGN(ContentFaviconDriverTest);
};

// Test that Favicon is not requested repeatedly during the same session if
// server returns HTTP 404 status.
TEST_F(ContentFaviconDriverTest, UnableToDownloadFavicon) {
  const GURL missing_icon_url("http://www.google.com/favicon.ico");
  const GURL another_icon_url("http://www.youtube.com/favicon.ico");

  ContentFaviconDriver* content_favicon_driver =
      ContentFaviconDriver::FromWebContents(web_contents());

  std::vector<SkBitmap> empty_icons;
  std::vector<gfx::Size> empty_icon_sizes;
  int download_id = 0;

  // Try to download missing icon.
  download_id = content_favicon_driver->StartDownload(missing_icon_url, 0);
  EXPECT_NE(0, download_id);
  EXPECT_FALSE(favicon_service()->WasUnableToDownloadFavicon(missing_icon_url));

  // Report download failure with HTTP 503 status.
  content_favicon_driver->DidDownloadFavicon(download_id, 503, missing_icon_url,
                                             empty_icons, empty_icon_sizes);
  // Icon is not marked as UnableToDownload as HTTP status is not 404.
  EXPECT_FALSE(favicon_service()->WasUnableToDownloadFavicon(missing_icon_url));

  // Try to download again.
  download_id = content_favicon_driver->StartDownload(missing_icon_url, 0);
  EXPECT_NE(0, download_id);
  EXPECT_FALSE(favicon_service()->WasUnableToDownloadFavicon(missing_icon_url));

  // Report download failure with HTTP 404 status.
  content_favicon_driver->DidDownloadFavicon(download_id, 404, missing_icon_url,
                                             empty_icons, empty_icon_sizes);
  // Icon is marked as UnableToDownload.
  EXPECT_TRUE(favicon_service()->WasUnableToDownloadFavicon(missing_icon_url));

  // Try to download again.
  download_id = content_favicon_driver->StartDownload(missing_icon_url, 0);
  // Download is not started and Icon is still marked as UnableToDownload.
  EXPECT_EQ(0, download_id);
  EXPECT_TRUE(favicon_service()->WasUnableToDownloadFavicon(missing_icon_url));

  // Try to download another icon.
  download_id = content_favicon_driver->StartDownload(another_icon_url, 0);
  // Download is started as another icon URL is not same as missing_icon_url.
  EXPECT_NE(0, download_id);
  EXPECT_FALSE(favicon_service()->WasUnableToDownloadFavicon(another_icon_url));

  // Clear the list of missing icons.
  favicon_service()->ClearUnableToDownloadFavicons();
  EXPECT_FALSE(favicon_service()->WasUnableToDownloadFavicon(missing_icon_url));
  EXPECT_FALSE(favicon_service()->WasUnableToDownloadFavicon(another_icon_url));

  // Try to download again.
  download_id = content_favicon_driver->StartDownload(missing_icon_url, 0);
  EXPECT_NE(0, download_id);
  // Report download success with HTTP 200 status.
  content_favicon_driver->DidDownloadFavicon(download_id, 200, missing_icon_url,
                                             empty_icons, empty_icon_sizes);
  // Icon is not marked as UnableToDownload as HTTP status is not 404.
  EXPECT_FALSE(favicon_service()->WasUnableToDownloadFavicon(missing_icon_url));

  favicon_service()->Shutdown();
}

// Test that ContentFaviconDriver ignores updated favicon URLs if there is no
// last committed entry. This occurs when script is injected in about:blank.
// See crbug.com/520759 for more details
TEST_F(ContentFaviconDriverTest, FaviconUpdateNoLastCommittedEntry) {
  ASSERT_EQ(nullptr, web_contents()->GetController().GetLastCommittedEntry());

  std::vector<content::FaviconURL> favicon_urls;
  favicon_urls.push_back(content::FaviconURL(
      GURL("http://www.google.ca/favicon.ico"), content::FaviconURL::FAVICON,
      std::vector<gfx::Size>()));
  favicon::ContentFaviconDriver* driver =
      favicon::ContentFaviconDriver::FromWebContents(web_contents());
  static_cast<content::WebContentsObserver*>(driver)
      ->DidUpdateFaviconURL(favicon_urls);

  // Test that ContentFaviconDriver ignored the favicon url update.
  EXPECT_TRUE(driver->favicon_urls().empty());
}

}  // namespace
}  // namespace favicon
