// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_event_details.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

TEST(WebRequestEventDetailsTest, WhitelistedCopyForPublicSession) {
  // Create original and copy, and populate them with some values.
  std::unique_ptr<WebRequestEventDetails> orig(new WebRequestEventDetails);
  std::unique_ptr<WebRequestEventDetails> copy(new WebRequestEventDetails);

  const char* const safe_attributes[] = {
    "method", "requestId", "timeStamp", "type", "tabId", "frameId",
    "parentFrameId", "fromCache", "error", "ip", "statusLine", "statusCode"
  };

  for (WebRequestEventDetails* ptr : {orig.get(), copy.get()}) {
    ptr->render_process_id_ = 1;
    ptr->render_frame_id_ = 2;
    ptr->extra_info_spec_ = 3;

    ptr->request_body_.reset(new base::DictionaryValue);
    ptr->request_headers_.reset(new base::ListValue);
    ptr->response_headers_.reset(new base::ListValue);

    for (const char* safe_attr : safe_attributes) {
      ptr->dict_.SetString(safe_attr, safe_attr);
    }

    ptr->dict_.SetString("url", "http://www.foo.bar/baz");

    // Add some extra dict_ values that should be filtered out.
    ptr->dict_.SetString("requestBody", "request body value");
    ptr->dict_.SetString("requestHeaders", "request headers value");
  }

  // Filter the copy out then check that filtering really works.
  copy->FilterForPublicSession();

  EXPECT_EQ(orig->render_process_id_, copy->render_process_id_);
  EXPECT_EQ(orig->render_frame_id_, copy->render_frame_id_);
  EXPECT_EQ(0, copy->extra_info_spec_);

  EXPECT_EQ(nullptr, copy->request_body_);
  EXPECT_EQ(nullptr, copy->request_headers_);
  EXPECT_EQ(nullptr, copy->response_headers_);

  for (const char* safe_attr : safe_attributes) {
    std::string copy_str;
    copy->dict_.GetString(safe_attr, &copy_str);
    EXPECT_EQ(safe_attr, copy_str);
  }

  // URL is stripped down to origin.
  std::string url;
  copy->dict_.GetString("url", &url);
  EXPECT_EQ("http://www.foo.bar/", url);

  // Extras are filtered out (+1 for url).
  EXPECT_EQ(arraysize(safe_attributes) + 1, copy->dict_.size());
}

}  // namespace extensions
