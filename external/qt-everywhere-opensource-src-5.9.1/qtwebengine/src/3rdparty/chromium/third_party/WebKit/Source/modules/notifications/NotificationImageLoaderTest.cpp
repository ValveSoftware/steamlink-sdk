// Copyright 2016 Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/notifications/NotificationImageLoader.h"

#include "core/dom/ExecutionContext.h"
#include "core/fetch/MemoryCache.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/testing/HistogramTester.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/testing/URLTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/platform/weborigin/KURL.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "wtf/Functional.h"

namespace blink {
namespace {

enum class LoadState { kNotLoaded, kLoadFailed, kLoadSuccessful };

const char kBaseUrl[] = "http://test.com/";
const char kIcon500x500[] = "500x500.png";

// This mirrors the definition in NotificationImageLoader.cpp.
const unsigned long kImageFetchTimeoutInMs = 90000;

static_assert(kImageFetchTimeoutInMs > 1000.0,
              "kImageFetchTimeoutInMs must be greater than 1000ms.");

class NotificationImageLoaderTest : public ::testing::Test {
 public:
  NotificationImageLoaderTest()
      : m_page(DummyPageHolder::create()),
        // Use an arbitrary type, since it only affects which UMA bucket we use.
        m_loader(
            new NotificationImageLoader(NotificationImageLoader::Type::Icon)) {}

  ~NotificationImageLoaderTest() override {
    m_loader->stop();
    Platform::current()->getURLLoaderMockFactory()->unregisterAllURLs();
    memoryCache()->evictResources();
  }

  // Registers a mocked URL. When fetched it will be loaded form the test data
  // directory.
  WebURL registerMockedURL(const String& fileName) {
    WebURL url(KURL(ParsedURLString, kBaseUrl + fileName));
    URLTestHelpers::registerMockedURLLoad(url, fileName, "notifications/",
                                          "image/png");
    return url;
  }

  // Callback for the NotificationImageLoader. This will set the state of the
  // load as either success or failed based on whether the bitmap is empty.
  void imageLoaded(const SkBitmap& bitmap) {
    if (!bitmap.empty())
      m_loaded = LoadState::kLoadSuccessful;
    else
      m_loaded = LoadState::kLoadFailed;
  }

  void loadImage(const KURL& url) {
    m_loader->start(
        context(), url,
        bind(&NotificationImageLoaderTest::imageLoaded, WTF::unretained(this)));
  }

  ExecutionContext* context() const { return &m_page->document(); }
  LoadState loaded() const { return m_loaded; }

 protected:
  HistogramTester m_histogramTester;

 private:
  std::unique_ptr<DummyPageHolder> m_page;
  Persistent<NotificationImageLoader> m_loader;
  LoadState m_loaded = LoadState::kNotLoaded;
};

TEST_F(NotificationImageLoaderTest, SuccessTest) {
  KURL url = registerMockedURL(kIcon500x500);
  loadImage(url);
  m_histogramTester.expectTotalCount("Notifications.LoadFinishTime.Icon", 0);
  m_histogramTester.expectTotalCount("Notifications.LoadFileSize.Icon", 0);
  m_histogramTester.expectTotalCount("Notifications.LoadFailTime.Icon", 0);
  Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
  EXPECT_EQ(LoadState::kLoadSuccessful, loaded());
  m_histogramTester.expectTotalCount("Notifications.LoadFinishTime.Icon", 1);
  m_histogramTester.expectUniqueSample("Notifications.LoadFileSize.Icon", 7439,
                                       1);
  m_histogramTester.expectTotalCount("Notifications.LoadFailTime.Icon", 0);
}

TEST_F(NotificationImageLoaderTest, TimeoutTest) {
  // To test for a timeout, this needs to override the clock in the platform.
  // Just creating the mock platform will do everything to set it up.
  TestingPlatformSupportWithMockScheduler testingPlatform;
  KURL url = registerMockedURL(kIcon500x500);
  loadImage(url);

  // Run the platform for kImageFetchTimeoutInMs-1 seconds. This should not
  // result in a timeout.
  testingPlatform.runForPeriodSeconds(kImageFetchTimeoutInMs / 1000 - 1);
  EXPECT_EQ(LoadState::kNotLoaded, loaded());
  m_histogramTester.expectTotalCount("Notifications.LoadFinishTime.Icon", 0);
  m_histogramTester.expectTotalCount("Notifications.LoadFileSize.Icon", 0);
  m_histogramTester.expectTotalCount("Notifications.LoadFailTime.Icon", 0);

  // Now advance time until a timeout should be expected.
  testingPlatform.runForPeriodSeconds(2);

  // If the loader times out, it calls the callback and returns an empty bitmap.
  EXPECT_EQ(LoadState::kLoadFailed, loaded());
  m_histogramTester.expectTotalCount("Notifications.LoadFinishTime.Icon", 0);
  m_histogramTester.expectTotalCount("Notifications.LoadFileSize.Icon", 0);
  // Should log a non-zero failure time.
  m_histogramTester.expectTotalCount("Notifications.LoadFailTime.Icon", 1);
  m_histogramTester.expectBucketCount("Notifications.LoadFailTime.Icon", 0, 0);
}

}  // namspace
}  // namespace blink
