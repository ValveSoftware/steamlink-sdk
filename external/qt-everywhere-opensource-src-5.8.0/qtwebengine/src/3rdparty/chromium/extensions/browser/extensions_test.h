// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSIONS_TEST_H_
#define EXTENSIONS_BROWSER_EXTENSIONS_TEST_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/test/test_renderer_host.h"
#include "extensions/browser/mock_extension_system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
class BrowserContext;
class ContentBrowserClient;
class ContentClient;
class ContentUtilityClient;
class RenderViewHostTestEnabler;
}

namespace extensions {
class TestExtensionsBrowserClient;

// Base class for extensions module unit tests of browser process code. Sets up
// the content module and extensions module client interfaces. Initializes
// services for a browser context.
//
// NOTE: Use this class only in extensions_unittests, not in Chrome unit_tests.
// BrowserContextKeyedServiceFactory singletons persist between tests.
// In Chrome those factories assume any BrowserContext is a Profile and will
// cause crashes if it is not. http://crbug.com/395820
class ExtensionsTest : public testing::Test {
 public:
  ExtensionsTest();
  ~ExtensionsTest() override;

  // Returned as a BrowserContext since most users don't need methods from
  // TestBrowserContext.
  content::BrowserContext* browser_context() { return browser_context_.get(); }

  // Returned as a TestExtensionsBrowserClient since most users need to call
  // test-specific methods on it.
  TestExtensionsBrowserClient* extensions_browser_client() {
    return extensions_browser_client_.get();
  }

  // testing::Test overrides:
  void SetUp() override;
  void TearDown() override;

 private:
  // TODO(yoz): Add a NotificationService here; it's used widely enough.
  std::unique_ptr<content::ContentClient> content_client_;
  std::unique_ptr<content::ContentUtilityClient> content_utility_client_;
  std::unique_ptr<content::ContentBrowserClient> content_browser_client_;
  std::unique_ptr<content::BrowserContext> browser_context_;
  std::unique_ptr<TestExtensionsBrowserClient> extensions_browser_client_;

  // The existence of this object enables tests via
  // RenderViewHostTester.
  content::RenderViewHostTestEnabler rvh_test_enabler_;

  MockExtensionSystemFactory<MockExtensionSystem> extension_system_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsTest);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSIONS_TEST_H_
