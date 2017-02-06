// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/api/web_request/web_request_permissions.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ipc/ipc_message.h"
#include "net/base/request_priority.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::ResourceRequestInfo;
using content::ResourceType;
using extensions::Extension;
using extensions::Manifest;
using extensions::PermissionsData;
using extension_test_util::LoadManifestUnchecked;

class ExtensionWebRequestHelpersTestWithThreadsTest : public testing::Test {
 public:
  ExtensionWebRequestHelpersTestWithThreadsTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}

 protected:
  void SetUp() override;

 private:
  content::TestBrowserThreadBundle thread_bundle_;

 protected:
  net::TestURLRequestContext context;

  // This extension has Web Request permissions, but no host permission.
  scoped_refptr<Extension> permissionless_extension_;
  // This extension has Web Request permissions, and *.com a host permission.
  scoped_refptr<Extension> com_extension_;
  scoped_refptr<extensions::InfoMap> extension_info_map_;
};

void ExtensionWebRequestHelpersTestWithThreadsTest::SetUp() {
  testing::Test::SetUp();

  std::string error;
  permissionless_extension_ = LoadManifestUnchecked("permissions",
                                                    "web_request_no_host.json",
                                                    Manifest::INVALID_LOCATION,
                                                    Extension::NO_FLAGS,
                                                    "ext_id_1",
                                                    &error);
  ASSERT_TRUE(permissionless_extension_.get()) << error;
  com_extension_ =
      LoadManifestUnchecked("permissions",
                            "web_request_com_host_permissions.json",
                            Manifest::INVALID_LOCATION,
                            Extension::NO_FLAGS,
                            "ext_id_2",
                            &error);
  ASSERT_TRUE(com_extension_.get()) << error;
  extension_info_map_ = new extensions::InfoMap;
  extension_info_map_->AddExtension(permissionless_extension_.get(),
                                    base::Time::Now(),
                                    false /*incognito_enabled*/,
                                    false /*notifications_disabled*/);
  extension_info_map_->AddExtension(
      com_extension_.get(),
      base::Time::Now(),
      false /*incognito_enabled*/,
      false /*notifications_disabled*/);
}

TEST_F(ExtensionWebRequestHelpersTestWithThreadsTest, TestHideRequestForURL) {
  net::TestURLRequestContext context;
  const char* const sensitive_urls[] = {
      "http://clients2.google.com",
      "http://clients22.google.com",
      "https://clients2.google.com",
      "http://clients2.google.com/service/update2/crx",
      "https://clients.google.com",
      "https://test.clients.google.com",
      "https://clients2.google.com/service/update2/crx",
      "http://www.gstatic.com/chrome/extensions/blacklist",
      "https://www.gstatic.com/chrome/extensions/blacklist",
      "notregisteredscheme://www.foobar.com",
      "https://chrome.google.com/webstore/",
      "https://chrome.google.com/webstore/"
          "inlineinstall/detail/kcnhkahnjcbndmmehfkdnkjomaanaooo"
  };
  const char* const non_sensitive_urls[] = {
      "http://www.google.com/"
  };

  // Check that requests are rejected based on the destination
  for (size_t i = 0; i < arraysize(sensitive_urls); ++i) {
    GURL sensitive_url(sensitive_urls[i]);
    std::unique_ptr<net::URLRequest> request(
        context.CreateRequest(sensitive_url, net::DEFAULT_PRIORITY, NULL));
    EXPECT_TRUE(WebRequestPermissions::HideRequest(
        extension_info_map_.get(), request.get())) << sensitive_urls[i];
  }
  // Check that requests are accepted if they don't touch sensitive urls.
  for (size_t i = 0; i < arraysize(non_sensitive_urls); ++i) {
    GURL non_sensitive_url(non_sensitive_urls[i]);
    std::unique_ptr<net::URLRequest> request(
        context.CreateRequest(non_sensitive_url, net::DEFAULT_PRIORITY, NULL));
    EXPECT_FALSE(WebRequestPermissions::HideRequest(
        extension_info_map_.get(), request.get())) << non_sensitive_urls[i];
  }

  // Check protection of requests originating from the frame showing the Chrome
  // WebStore.
  // Normally this request is not protected:
  GURL non_sensitive_url("http://www.google.com/test.js");
  std::unique_ptr<net::URLRequest> non_sensitive_request(
      context.CreateRequest(non_sensitive_url, net::DEFAULT_PRIORITY, NULL));
  EXPECT_FALSE(WebRequestPermissions::HideRequest(
      extension_info_map_.get(), non_sensitive_request.get()));
  // If the origin is labeled by the WebStoreAppId, it becomes protected.
  {
    int process_id = 42;
    int site_instance_id = 23;
    int view_id = 17;
    std::unique_ptr<net::URLRequest> sensitive_request(
        context.CreateRequest(non_sensitive_url, net::DEFAULT_PRIORITY, NULL));
    ResourceRequestInfo::AllocateForTesting(sensitive_request.get(),
                                            content::RESOURCE_TYPE_SCRIPT,
                                            NULL,
                                            process_id,
                                            view_id,
                                            MSG_ROUTING_NONE,
                                            false,   // is_main_frame
                                            false,   // parent_is_main_frame
                                            true,    // allow_download
                                            false,   // is_async
                                            false);  // is_using_lofi
    extension_info_map_->RegisterExtensionProcess(
        extensions::kWebStoreAppId, process_id, site_instance_id);
    EXPECT_TRUE(WebRequestPermissions::HideRequest(
        extension_info_map_.get(), sensitive_request.get()));
  }
}

TEST_F(ExtensionWebRequestHelpersTestWithThreadsTest,
       TestCanExtensionAccessURL_HostPermissions) {
  std::unique_ptr<net::URLRequest> request(context.CreateRequest(
      GURL("http://example.com"), net::DEFAULT_PRIORITY, NULL));

  EXPECT_EQ(PermissionsData::ACCESS_ALLOWED,
            WebRequestPermissions::CanExtensionAccessURL(
                extension_info_map_.get(), permissionless_extension_->id(),
                request->url(),
                -1,  // No tab id.
                false /*crosses_incognito*/,
                WebRequestPermissions::DO_NOT_CHECK_HOST));
  EXPECT_EQ(PermissionsData::ACCESS_DENIED,
            WebRequestPermissions::CanExtensionAccessURL(
                extension_info_map_.get(), permissionless_extension_->id(),
                request->url(),
                -1,  // No tab id.
                false /*crosses_incognito*/,
                WebRequestPermissions::REQUIRE_HOST_PERMISSION));
  EXPECT_EQ(PermissionsData::ACCESS_ALLOWED,
            WebRequestPermissions::CanExtensionAccessURL(
                extension_info_map_.get(), com_extension_->id(), request->url(),
                -1,  // No tab id.
                false /*crosses_incognito*/,
                WebRequestPermissions::REQUIRE_HOST_PERMISSION));
  EXPECT_EQ(PermissionsData::ACCESS_DENIED,
            WebRequestPermissions::CanExtensionAccessURL(
                extension_info_map_.get(), com_extension_->id(), request->url(),
                -1,  // No tab id.
                false /*crosses_incognito*/,
                WebRequestPermissions::REQUIRE_ALL_URLS));
}
