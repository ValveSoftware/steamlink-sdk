// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "content/public/common/user_agent.h"
#include "content/public/test/render_view_test.h"
#include "content/renderer/pepper/url_request_info_util.h"
#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/url_request_info_resource.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/test_globals.h"
#include "ppapi/shared_impl/url_request_info_data.h"
#include "ppapi/thunk/thunk.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

// This test is a end-to-end test from the resource to the WebKit request
// object. The actual resource implementation is so simple, it makes sense to
// test it by making sure the conversion routines actually work at the same
// time.

using blink::WebCString;
using blink::WebFrameClient;
using blink::WebString;
using blink::WebView;
using blink::WebURL;
using blink::WebURLRequest;

namespace {

bool IsExpected(const WebCString& web_string, const char* expected) {
  const char* result = web_string.data();
  return strcmp(result, expected) == 0;
}

bool IsExpected(const WebString& web_string, const char* expected) {
  return IsExpected(web_string.utf8(), expected);
}

// The base class destructor is protected, so derive.
class TestWebFrameClient : public WebFrameClient {};

}  // namespace

using ppapi::proxy::URLRequestInfoResource;
using ppapi::URLRequestInfoData;

namespace content {

class URLRequestInfoTest : public RenderViewTest {
 public:
  URLRequestInfoTest() : pp_instance_(1234) {}

  virtual void SetUp() OVERRIDE {
    RenderViewTest::SetUp();
    ppapi::ProxyLock::DisableLockingOnThreadForTest();

    test_globals_.GetResourceTracker()->DidCreateInstance(pp_instance_);

    // This resource doesn't do IPC, so a null connection is fine.
    info_ = new URLRequestInfoResource(
        ppapi::proxy::Connection(), pp_instance_, URLRequestInfoData());
  }

  virtual void TearDown() OVERRIDE {
    test_globals_.GetResourceTracker()->DidDeleteInstance(pp_instance_);
    RenderViewTest::TearDown();
  }

  bool GetDownloadToFile() {
    WebURLRequest web_request;
    URLRequestInfoData data = info_->GetData();
    if (!CreateWebURLRequest(pp_instance_, &data, GetMainFrame(), &web_request))
      return false;
    return web_request.downloadToFile();
  }

  WebCString GetURL() {
    WebURLRequest web_request;
    URLRequestInfoData data = info_->GetData();
    if (!CreateWebURLRequest(pp_instance_, &data, GetMainFrame(), &web_request))
      return WebCString();
    return web_request.url().spec();
  }

  WebString GetMethod() {
    WebURLRequest web_request;
    URLRequestInfoData data = info_->GetData();
    if (!CreateWebURLRequest(pp_instance_, &data, GetMainFrame(), &web_request))
      return WebString();
    return web_request.httpMethod();
  }

  WebString GetHeaderValue(const char* field) {
    WebURLRequest web_request;
    URLRequestInfoData data = info_->GetData();
    if (!CreateWebURLRequest(pp_instance_, &data, GetMainFrame(), &web_request))
      return WebString();
    return web_request.httpHeaderField(WebString::fromUTF8(field));
  }

  bool SetBooleanProperty(PP_URLRequestProperty prop, bool b) {
    return info_->SetBooleanProperty(prop, b);
  }
  bool SetStringProperty(PP_URLRequestProperty prop, const std::string& s) {
    return info_->SetStringProperty(prop, s);
  }

  PP_Instance pp_instance_;

  // Needs to be alive for resource tracking to work.
  ppapi::TestGlobals test_globals_;

  scoped_refptr<URLRequestInfoResource> info_;
};

TEST_F(URLRequestInfoTest, GetInterface) {
  const PPB_URLRequestInfo* request_info =
      ppapi::thunk::GetPPB_URLRequestInfo_1_0_Thunk();
  EXPECT_TRUE(request_info);
  EXPECT_TRUE(request_info->Create);
  EXPECT_TRUE(request_info->IsURLRequestInfo);
  EXPECT_TRUE(request_info->SetProperty);
  EXPECT_TRUE(request_info->AppendDataToBody);
  EXPECT_TRUE(request_info->AppendFileToBody);
}

TEST_F(URLRequestInfoTest, AsURLRequestInfo) {
  EXPECT_EQ(info_, info_->AsPPB_URLRequestInfo_API());
}

TEST_F(URLRequestInfoTest, StreamToFile) {
  SetStringProperty(PP_URLREQUESTPROPERTY_URL, "http://www.google.com");

  EXPECT_FALSE(GetDownloadToFile());

  EXPECT_TRUE(SetBooleanProperty(PP_URLREQUESTPROPERTY_STREAMTOFILE, true));
  EXPECT_TRUE(GetDownloadToFile());

  EXPECT_TRUE(SetBooleanProperty(PP_URLREQUESTPROPERTY_STREAMTOFILE, false));
  EXPECT_FALSE(GetDownloadToFile());
}

TEST_F(URLRequestInfoTest, FollowRedirects) {
  EXPECT_TRUE(info_->GetData().follow_redirects);

  EXPECT_TRUE(SetBooleanProperty(PP_URLREQUESTPROPERTY_FOLLOWREDIRECTS, false));
  EXPECT_FALSE(info_->GetData().follow_redirects);

  EXPECT_TRUE(SetBooleanProperty(PP_URLREQUESTPROPERTY_FOLLOWREDIRECTS, true));
  EXPECT_TRUE(info_->GetData().follow_redirects);
}

TEST_F(URLRequestInfoTest, RecordDownloadProgress) {
  EXPECT_FALSE(info_->GetData().record_download_progress);

  EXPECT_TRUE(
      SetBooleanProperty(PP_URLREQUESTPROPERTY_RECORDDOWNLOADPROGRESS, true));
  EXPECT_TRUE(info_->GetData().record_download_progress);

  EXPECT_TRUE(
      SetBooleanProperty(PP_URLREQUESTPROPERTY_RECORDDOWNLOADPROGRESS, false));
  EXPECT_FALSE(info_->GetData().record_download_progress);
}

TEST_F(URLRequestInfoTest, RecordUploadProgress) {
  EXPECT_FALSE(info_->GetData().record_upload_progress);

  EXPECT_TRUE(
      SetBooleanProperty(PP_URLREQUESTPROPERTY_RECORDUPLOADPROGRESS, true));
  EXPECT_TRUE(info_->GetData().record_upload_progress);

  EXPECT_TRUE(
      SetBooleanProperty(PP_URLREQUESTPROPERTY_RECORDUPLOADPROGRESS, false));
  EXPECT_FALSE(info_->GetData().record_upload_progress);
}

TEST_F(URLRequestInfoTest, AllowCrossOriginRequests) {
  EXPECT_FALSE(info_->GetData().allow_cross_origin_requests);

  EXPECT_TRUE(
      SetBooleanProperty(PP_URLREQUESTPROPERTY_ALLOWCROSSORIGINREQUESTS, true));
  EXPECT_TRUE(info_->GetData().allow_cross_origin_requests);

  EXPECT_TRUE(SetBooleanProperty(PP_URLREQUESTPROPERTY_ALLOWCROSSORIGINREQUESTS,
                                 false));
  EXPECT_FALSE(info_->GetData().allow_cross_origin_requests);
}

TEST_F(URLRequestInfoTest, AllowCredentials) {
  EXPECT_FALSE(info_->GetData().allow_credentials);

  EXPECT_TRUE(SetBooleanProperty(PP_URLREQUESTPROPERTY_ALLOWCREDENTIALS, true));
  EXPECT_TRUE(info_->GetData().allow_credentials);

  EXPECT_TRUE(
      SetBooleanProperty(PP_URLREQUESTPROPERTY_ALLOWCREDENTIALS, false));
  EXPECT_FALSE(info_->GetData().allow_credentials);
}

TEST_F(URLRequestInfoTest, SetURL) {
  const char* url = "http://www.google.com/";
  EXPECT_TRUE(SetStringProperty(PP_URLREQUESTPROPERTY_URL, url));
  EXPECT_TRUE(IsExpected(GetURL(), url));
}

TEST_F(URLRequestInfoTest, JavascriptURL) {
  const char* url = "javascript:foo = bar";
  EXPECT_FALSE(URLRequestRequiresUniversalAccess(info_->GetData()));
  SetStringProperty(PP_URLREQUESTPROPERTY_URL, url);
  EXPECT_TRUE(URLRequestRequiresUniversalAccess(info_->GetData()));
}

TEST_F(URLRequestInfoTest, SetMethod) {
  // Test default method is "GET".
  EXPECT_TRUE(IsExpected(GetMethod(), "GET"));
  EXPECT_TRUE(SetStringProperty(PP_URLREQUESTPROPERTY_METHOD, "POST"));
  EXPECT_TRUE(IsExpected(GetMethod(), "POST"));
}

TEST_F(URLRequestInfoTest, SetHeaders) {
  // Test default header field.
  EXPECT_TRUE(IsExpected(GetHeaderValue("foo"), ""));
  // Test that we can set a header field.
  EXPECT_TRUE(SetStringProperty(PP_URLREQUESTPROPERTY_HEADERS, "foo: bar"));
  EXPECT_TRUE(IsExpected(GetHeaderValue("foo"), "bar"));
  // Test that we can set multiple header fields using \n delimiter.
  EXPECT_TRUE(
      SetStringProperty(PP_URLREQUESTPROPERTY_HEADERS, "foo: bar\nbar: baz"));
  EXPECT_TRUE(IsExpected(GetHeaderValue("foo"), "bar"));
  EXPECT_TRUE(IsExpected(GetHeaderValue("bar"), "baz"));
}

// TODO(bbudge) Unit tests for AppendDataToBody, AppendFileToBody.

}  // namespace content
