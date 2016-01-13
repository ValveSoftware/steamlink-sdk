// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>

#include "base/base64.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/pickle.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/page_state_serialization.h"
#include "content/public/common/content_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

#if defined(OS_WIN)
inline bool isnan(double num) { return !!_isnan(num); }
#endif

base::NullableString16 NS16(const char* s) {
  return s ? base::NullableString16(base::ASCIIToUTF16(s), false) :
             base::NullableString16();
}

//-----------------------------------------------------------------------------

template <typename T>
void ExpectEquality(const T& a, const T& b) {
  EXPECT_EQ(a, b);
}

template <typename T>
void ExpectEquality(const std::vector<T>& a, const std::vector<T>& b) {
  EXPECT_EQ(a.size(), b.size());
  for (size_t i = 0; i < std::min(a.size(), b.size()); ++i)
    ExpectEquality(a[i], b[i]);
}

template <>
void ExpectEquality(const ExplodedHttpBodyElement& a,
                    const ExplodedHttpBodyElement& b) {
  EXPECT_EQ(a.type, b.type);
  EXPECT_EQ(a.data, b.data);
  EXPECT_EQ(a.file_path, b.file_path);
  EXPECT_EQ(a.filesystem_url, b.filesystem_url);
  EXPECT_EQ(a.file_start, b.file_start);
  EXPECT_EQ(a.file_length, b.file_length);
  if (!(isnan(a.file_modification_time) && isnan(b.file_modification_time)))
    EXPECT_DOUBLE_EQ(a.file_modification_time, b.file_modification_time);
  EXPECT_EQ(a.blob_uuid, b.blob_uuid);
}

template <>
void ExpectEquality(const ExplodedHttpBody& a, const ExplodedHttpBody& b) {
  EXPECT_EQ(a.http_content_type, b.http_content_type);
  EXPECT_EQ(a.identifier, b.identifier);
  EXPECT_EQ(a.contains_passwords, b.contains_passwords);
  EXPECT_EQ(a.is_null, b.is_null);
  ExpectEquality(a.elements, b.elements);
}

template <>
void ExpectEquality(const ExplodedFrameState& a, const ExplodedFrameState& b) {
  EXPECT_EQ(a.url_string, b.url_string);
  EXPECT_EQ(a.referrer, b.referrer);
  EXPECT_EQ(a.referrer_policy, b.referrer_policy);
  EXPECT_EQ(a.target, b.target);
  EXPECT_EQ(a.state_object, b.state_object);
  ExpectEquality(a.document_state, b.document_state);
  EXPECT_EQ(a.pinch_viewport_scroll_offset, b.pinch_viewport_scroll_offset);
  EXPECT_EQ(a.scroll_offset, b.scroll_offset);
  EXPECT_EQ(a.item_sequence_number, b.item_sequence_number);
  EXPECT_EQ(a.document_sequence_number, b.document_sequence_number);
  EXPECT_EQ(a.page_scale_factor, b.page_scale_factor);
  ExpectEquality(a.http_body, b.http_body);
  ExpectEquality(a.children, b.children);
}

void ExpectEquality(const ExplodedPageState& a, const ExplodedPageState& b) {
  ExpectEquality(a.referenced_files, b.referenced_files);
  ExpectEquality(a.top, b.top);
}

//-----------------------------------------------------------------------------

class PageStateSerializationTest : public testing::Test {
 public:
  void PopulateFrameState(ExplodedFrameState* frame_state) {
    // Invent some data for the various fields.
    frame_state->url_string = NS16("http://dev.chromium.org/");
    frame_state->referrer = NS16("https://www.google.com/search?q=dev.chromium.org");
    frame_state->referrer_policy = blink::WebReferrerPolicyAlways;
    frame_state->target = NS16("foo");
    frame_state->state_object = NS16(NULL);
    frame_state->document_state.push_back(NS16("1"));
    frame_state->document_state.push_back(NS16("q"));
    frame_state->document_state.push_back(NS16("text"));
    frame_state->document_state.push_back(NS16("dev.chromium.org"));
    frame_state->pinch_viewport_scroll_offset = gfx::PointF(10, 15);
    frame_state->scroll_offset = gfx::Point(0, 100);
    frame_state->item_sequence_number = 1;
    frame_state->document_sequence_number = 2;
    frame_state->page_scale_factor = 2.0;
  }

  void PopulateHttpBody(ExplodedHttpBody* http_body,
                        std::vector<base::NullableString16>* referenced_files) {
    http_body->is_null = false;
    http_body->identifier = 12345;
    http_body->contains_passwords = false;
    http_body->http_content_type = NS16("text/foo");

    ExplodedHttpBodyElement e1;
    e1.type = blink::WebHTTPBody::Element::TypeData;
    e1.data = "foo";
    http_body->elements.push_back(e1);

    ExplodedHttpBodyElement e2;
    e2.type = blink::WebHTTPBody::Element::TypeFile;
    e2.file_path = NS16("file.txt");
    e2.file_start = 100;
    e2.file_length = 1024;
    e2.file_modification_time = 9999.0;
    http_body->elements.push_back(e2);

    referenced_files->push_back(e2.file_path);
  }

  void PopulateFrameStateForBackwardsCompatTest(
      ExplodedFrameState* frame_state,
      bool is_child) {
    frame_state->url_string = NS16("http://chromium.org/");
    frame_state->referrer = NS16("http://google.com/");
    frame_state->referrer_policy = blink::WebReferrerPolicyDefault;
    if (!is_child)
      frame_state->target = NS16("target");
    frame_state->pinch_viewport_scroll_offset = gfx::PointF(-1, -1);
    frame_state->scroll_offset = gfx::Point(42, -42);
    frame_state->item_sequence_number = 123;
    frame_state->document_sequence_number = 456;
    frame_state->page_scale_factor = 2.0f;

    frame_state->document_state.push_back(
        NS16("\n\r?% WebKit serialized form state version 8 \n\r=&"));
    frame_state->document_state.push_back(NS16("form key"));
    frame_state->document_state.push_back(NS16("1"));
    frame_state->document_state.push_back(NS16("foo"));
    frame_state->document_state.push_back(NS16("file"));
    frame_state->document_state.push_back(NS16("2"));
    frame_state->document_state.push_back(NS16("file.txt"));
    frame_state->document_state.push_back(NS16("displayName"));

    if (!is_child) {
      frame_state->http_body.http_content_type = NS16("foo/bar");
      frame_state->http_body.identifier = 789;
      frame_state->http_body.is_null = false;

      ExplodedHttpBodyElement e1;
      e1.type = blink::WebHTTPBody::Element::TypeData;
      e1.data = "first data block";
      frame_state->http_body.elements.push_back(e1);

      ExplodedHttpBodyElement e2;
      e2.type = blink::WebHTTPBody::Element::TypeFile;
      e2.file_path = NS16("file.txt");
      frame_state->http_body.elements.push_back(e2);

      ExplodedHttpBodyElement e3;
      e3.type = blink::WebHTTPBody::Element::TypeData;
      e3.data = "data the second";
      frame_state->http_body.elements.push_back(e3);

      ExplodedFrameState child_state;
      PopulateFrameStateForBackwardsCompatTest(&child_state, true);
      frame_state->children.push_back(child_state);
    }
  }

  void PopulatePageStateForBackwardsCompatTest(ExplodedPageState* page_state) {
    page_state->referenced_files.push_back(NS16("file.txt"));
    PopulateFrameStateForBackwardsCompatTest(&page_state->top, false);
  }

  void TestBackwardsCompat(int version) {
    const char* suffix = "";

#if defined(OS_ANDROID)
    // Unfortunately, the format of version 11 is different on Android, so we
    // need to use a special reference file.
    if (version == 11)
      suffix = "_android";
#endif

    base::FilePath path;
    PathService::Get(content::DIR_TEST_DATA, &path);
    path = path.AppendASCII("page_state").AppendASCII(
        base::StringPrintf("serialized_v%d%s.dat", version, suffix));

    std::string file_contents;
    if (!base::ReadFileToString(path, &file_contents)) {
      ADD_FAILURE() << "File not found: " << path.value();
      return;
    }

    std::string trimmed_contents;
    EXPECT_TRUE(base::RemoveChars(file_contents, "\r\n", &trimmed_contents));

    std::string encoded;
    EXPECT_TRUE(base::Base64Decode(trimmed_contents, &encoded));

    ExplodedPageState output;
#if defined(OS_ANDROID)
    // Because version 11 of the file format unfortunately bakes in the device
    // scale factor on Android, perform this test by assuming a preset device
    // scale factor, ignoring the device scale factor of the current device.
    const float kPresetDeviceScaleFactor = 2.0f;
    EXPECT_TRUE(DecodePageStateWithDeviceScaleFactorForTesting(
        encoded,
        kPresetDeviceScaleFactor,
        &output));
#else
    EXPECT_TRUE(DecodePageState(encoded, &output));
#endif

    ExplodedPageState expected;
    PopulatePageStateForBackwardsCompatTest(&expected);

    ExpectEquality(expected, output);
  }
};

TEST_F(PageStateSerializationTest, BasicEmpty) {
  ExplodedPageState input;

  std::string encoded;
  EXPECT_TRUE(EncodePageState(input, &encoded));

  ExplodedPageState output;
  EXPECT_TRUE(DecodePageState(encoded, &output));

  ExpectEquality(input, output);
}

TEST_F(PageStateSerializationTest, BasicFrame) {
  ExplodedPageState input;
  PopulateFrameState(&input.top);

  std::string encoded;
  EXPECT_TRUE(EncodePageState(input, &encoded));

  ExplodedPageState output;
  EXPECT_TRUE(DecodePageState(encoded, &output));

  ExpectEquality(input, output);
}

TEST_F(PageStateSerializationTest, BasicFramePOST) {
  ExplodedPageState input;
  PopulateFrameState(&input.top);
  PopulateHttpBody(&input.top.http_body, &input.referenced_files);

  std::string encoded;
  EXPECT_TRUE(EncodePageState(input, &encoded));

  ExplodedPageState output;
  EXPECT_TRUE(DecodePageState(encoded, &output));

  ExpectEquality(input, output);
}

TEST_F(PageStateSerializationTest, BasicFrameSet) {
  ExplodedPageState input;
  PopulateFrameState(&input.top);

  // Add some child frames.
  for (int i = 0; i < 4; ++i) {
    ExplodedFrameState child_state;
    PopulateFrameState(&child_state);
    input.top.children.push_back(child_state);
  }

  std::string encoded;
  EXPECT_TRUE(EncodePageState(input, &encoded));

  ExplodedPageState output;
  EXPECT_TRUE(DecodePageState(encoded, &output));

  ExpectEquality(input, output);
}

TEST_F(PageStateSerializationTest, BasicFrameSetPOST) {
  ExplodedPageState input;
  PopulateFrameState(&input.top);

  // Add some child frames.
  for (int i = 0; i < 4; ++i) {
    ExplodedFrameState child_state;
    PopulateFrameState(&child_state);

    // Simulate a form POST on a subframe.
    if (i == 2)
      PopulateHttpBody(&child_state.http_body, &input.referenced_files);

    input.top.children.push_back(child_state);
  }

  std::string encoded;
  EncodePageState(input, &encoded);

  ExplodedPageState output;
  DecodePageState(encoded, &output);

  ExpectEquality(input, output);
}

TEST_F(PageStateSerializationTest, BadMessagesTest1) {
  Pickle p;
  // Version 14
  p.WriteInt(14);
  // Empty strings.
  for (int i = 0; i < 6; ++i)
    p.WriteInt(-1);
  // Bad real number.
  p.WriteInt(-1);

  std::string s(static_cast<const char*>(p.data()), p.size());

  ExplodedPageState output;
  EXPECT_FALSE(DecodePageState(s, &output));
}

TEST_F(PageStateSerializationTest, BadMessagesTest2) {
  double d = 0;
  Pickle p;
  // Version 14
  p.WriteInt(14);
  // Empty strings.
  for (int i = 0; i < 6; ++i)
    p.WriteInt(-1);
  // More misc fields.
  p.WriteData(reinterpret_cast<const char*>(&d), sizeof(d));
  p.WriteInt(1);
  p.WriteInt(1);
  p.WriteInt(0);
  p.WriteInt(0);
  p.WriteInt(-1);
  p.WriteInt(0);
  // WebForm
  p.WriteInt(1);
  p.WriteInt(blink::WebHTTPBody::Element::TypeData);

  std::string s(static_cast<const char*>(p.data()), p.size());

  ExplodedPageState output;
  EXPECT_FALSE(DecodePageState(s, &output));
}

TEST_F(PageStateSerializationTest, DumpExpectedPageStateForBackwardsCompat) {
  // Change to #if 1 to enable this code.  Use this code to generate data, based
  // on the current serialization format, for the BackwardsCompat_vXX tests.
#if 0
  ExplodedPageState state;
  PopulatePageStateForBackwardsCompatTest(&state);

  std::string encoded;
  EXPECT_TRUE(EncodePageState(state, &encoded));

  std::string base64;
  base::Base64Encode(encoded, &base64);

  base::FilePath path;
  PathService::Get(base::DIR_TEMP, &path);
  path = path.AppendASCII("expected.dat");

  FILE* fp = base::OpenFile(path, "wb");
  ASSERT_TRUE(fp);

  const size_t kRowSize = 76;
  for (size_t offset = 0; offset < base64.size(); offset += kRowSize) {
    size_t length = std::min(base64.size() - offset, kRowSize);
    std::string segment(&base64[offset], length);
    segment.push_back('\n');
    ASSERT_EQ(1U, fwrite(segment.data(), segment.size(), 1, fp));
  }

  fclose(fp);
#endif
}

#if !defined(OS_ANDROID)
// TODO(darin): Re-enable for Android once this test accounts for systems with
//              a device scale factor not equal to 2.
TEST_F(PageStateSerializationTest, BackwardsCompat_v11) {
  TestBackwardsCompat(11);
}
#endif

TEST_F(PageStateSerializationTest, BackwardsCompat_v12) {
  TestBackwardsCompat(12);
}

TEST_F(PageStateSerializationTest, BackwardsCompat_v13) {
  TestBackwardsCompat(13);
}

TEST_F(PageStateSerializationTest, BackwardsCompat_v14) {
  TestBackwardsCompat(14);
}

TEST_F(PageStateSerializationTest, BackwardsCompat_v15) {
  TestBackwardsCompat(15);
}

TEST_F(PageStateSerializationTest, BackwardsCompat_v16) {
  TestBackwardsCompat(16);
}

TEST_F(PageStateSerializationTest, BackwardsCompat_v18) {
  TestBackwardsCompat(18);
}

}  // namespace
}  // namespace content
