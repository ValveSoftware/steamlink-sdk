// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extensions_test.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/test/test_browser_context.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "extensions/test/test_content_utility_client.h"

namespace extensions {

// This class does work in the constructor instead of SetUp() to give subclasses
// a valid BrowserContext to use while initializing their members. For example:
//
// class MyExtensionsTest : public ExtensionsTest {
//   MyExtensionsTest()
//     : my_object_(browser_context())) {
//   }
// };
ExtensionsTest::ExtensionsTest()
    : content_client_(new content::ContentClient),
      content_utility_client_(new TestContentUtilityClient),
      content_browser_client_(new content::ContentBrowserClient),
      browser_context_(new content::TestBrowserContext),
      extensions_browser_client_(
          new TestExtensionsBrowserClient(browser_context_.get())) {
  content::SetContentClient(content_client_.get());
  content::SetUtilityClientForTesting(content_utility_client_.get());
  content::SetBrowserClientForTesting(content_browser_client_.get());
  ExtensionsBrowserClient::Set(extensions_browser_client_.get());
  extensions_browser_client_->set_extension_system_factory(
      &extension_system_factory_);
}

ExtensionsTest::~ExtensionsTest() {
  ExtensionsBrowserClient::Set(NULL);
  content::SetBrowserClientForTesting(NULL);
  content::SetUtilityClientForTesting(NULL);
  content::SetContentClient(NULL);
}

void ExtensionsTest::SetUp() {
  // Crashing here? Don't use this class in Chrome's unit_tests. See header.
  BrowserContextDependencyManager::GetInstance()
      ->CreateBrowserContextServicesForTest(browser_context_.get());
}

void ExtensionsTest::TearDown() {
  // Allows individual tests to have BrowserContextKeyedServiceFactory objects
  // as member variables instead of singletons. The individual services will be
  // cleaned up before the factories are destroyed.
  BrowserContextDependencyManager::GetInstance()->DestroyBrowserContextServices(
      browser_context_.get());
  extensions_browser_client_.reset();
  browser_context_.reset();
}

}  // namespace extensions
