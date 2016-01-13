// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/webui/web_ui_data_source_impl.h"
#include "content/test/test_content_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

const int kDummyStringId = 123;
const int kDummyDefaultResourceId = 456;
const int kDummyResourceId = 789;

const char kDummyString[] = "foo";
const char kDummyDefaultResource[] = "<html>foo</html>";
const char kDummytResource[] = "<html>blah</html>";

class TestClient : public TestContentClient {
 public:
  TestClient() {}
  virtual ~TestClient() {}

  virtual base::string16 GetLocalizedString(int message_id) const OVERRIDE {
    if (message_id == kDummyStringId)
      return base::UTF8ToUTF16(kDummyString);
    return base::string16();

  }

  virtual base::RefCountedStaticMemory* GetDataResourceBytes(
      int resource_id) const OVERRIDE {
    base::RefCountedStaticMemory* bytes = NULL;
    if (resource_id == kDummyDefaultResourceId) {
      bytes = new base::RefCountedStaticMemory(
          kDummyDefaultResource, arraysize(kDummyDefaultResource));
    } else if (resource_id == kDummyResourceId) {
      bytes = new base::RefCountedStaticMemory(
          kDummytResource, arraysize(kDummytResource));
    }
    return bytes;
  }
};

}

class WebUIDataSourceTest : public testing::Test {
 public:
  WebUIDataSourceTest() : result_data_(NULL) {}
  virtual ~WebUIDataSourceTest() {}
  WebUIDataSourceImpl* source() { return source_.get(); }

  void StartDataRequest(const std::string& path) {
     source_->StartDataRequest(
        path,
        0, 0,
        base::Bind(&WebUIDataSourceTest::SendResult,
        base::Unretained(this)));
  }

  std::string GetMimeType(const std::string& path) const {
    return source_->GetMimeType(path);
  }

  scoped_refptr<base::RefCountedMemory> result_data_;

 private:
  virtual void SetUp() {
    SetContentClient(&client_);
    WebUIDataSource* source = WebUIDataSourceImpl::Create("host");
    WebUIDataSourceImpl* source_impl = static_cast<WebUIDataSourceImpl*>(
        source);
    source_impl->disable_set_font_strings_for_testing();
    source_ = make_scoped_refptr(source_impl);
  }

  // Store response for later comparisons.
  void SendResult(base::RefCountedMemory* data) {
    result_data_ = data;
  }

  scoped_refptr<WebUIDataSourceImpl> source_;
  TestClient client_;
};

TEST_F(WebUIDataSourceTest, EmptyStrings) {
  source()->SetJsonPath("strings.js");
  StartDataRequest("strings.js");
  std::string result(result_data_->front_as<char>(), result_data_->size());
  EXPECT_NE(result.find("var templateData = {"), std::string::npos);
  EXPECT_NE(result.find("};"), std::string::npos);
}

TEST_F(WebUIDataSourceTest, SomeStrings) {
  source()->SetJsonPath("strings.js");
  source()->AddString("planet", base::ASCIIToUTF16("pluto"));
  source()->AddLocalizedString("button", kDummyStringId);
  StartDataRequest("strings.js");
  std::string result(result_data_->front_as<char>(), result_data_->size());
  EXPECT_NE(result.find("\"planet\":\"pluto\""), std::string::npos);
  EXPECT_NE(result.find("\"button\":\"foo\""), std::string::npos);
}

TEST_F(WebUIDataSourceTest, DefaultResource) {
  source()->SetDefaultResource(kDummyDefaultResourceId);
  StartDataRequest("foobar" );
  std::string result(result_data_->front_as<char>(), result_data_->size());
  EXPECT_NE(result.find(kDummyDefaultResource), std::string::npos);
  StartDataRequest("strings.js");
  result = std::string(result_data_->front_as<char>(), result_data_->size());
  EXPECT_NE(result.find(kDummyDefaultResource), std::string::npos);
}

TEST_F(WebUIDataSourceTest, NamedResource) {
  source()->SetDefaultResource(kDummyDefaultResourceId);
  source()->AddResourcePath("foobar", kDummyResourceId);
  StartDataRequest("foobar");
  std::string result(result_data_->front_as<char>(), result_data_->size());
  EXPECT_NE(result.find(kDummytResource), std::string::npos);
  StartDataRequest("strings.js");
  result = std::string(result_data_->front_as<char>(), result_data_->size());
  EXPECT_NE(result.find(kDummyDefaultResource), std::string::npos);
}

TEST_F(WebUIDataSourceTest, MimeType) {
  const char* html = "text/html";
  const char* js = "application/javascript";
  EXPECT_EQ(GetMimeType(std::string()), html);
  EXPECT_EQ(GetMimeType("foo"), html);
  EXPECT_EQ(GetMimeType("foo.html"), html);
  EXPECT_EQ(GetMimeType(".js"), js);
  EXPECT_EQ(GetMimeType("foo.js"), js);
  EXPECT_EQ(GetMimeType("js"), html);
  EXPECT_EQ(GetMimeType("foojs"), html);
  EXPECT_EQ(GetMimeType("foo.jsp"), html);
}

}  // namespace content
