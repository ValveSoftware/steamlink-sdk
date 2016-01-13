// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "webkit/child/multipart_response_delegate.h"

using std::string;
using blink::WebString;
using blink::WebURL;
using blink::WebURLError;
using blink::WebURLLoader;
using blink::WebURLLoaderClient;
using blink::WebURLRequest;
using blink::WebURLResponse;
using content::MultipartResponseDelegateTester;
using webkit_glue::MultipartResponseDelegate;

namespace content {

class MultipartResponseDelegateTester {
 public:
  MultipartResponseDelegateTester(MultipartResponseDelegate* delegate)
      : delegate_(delegate) {
  }

  int PushOverLine(const std::string& data, size_t pos) {
    return delegate_->PushOverLine(data, pos);
  }

  bool ParseHeaders() { return delegate_->ParseHeaders(); }
  size_t FindBoundary() { return delegate_->FindBoundary(); }
  std::string& boundary() { return delegate_->boundary_; }
  std::string& data() { return delegate_->data_; }

 private:
  MultipartResponseDelegate* delegate_;
};

namespace {

class MultipartResponseTest : public testing::Test {
};

class MockWebURLLoaderClient : public WebURLLoaderClient {
 public:
  MockWebURLLoaderClient() { Reset(); }

  virtual void willSendRequest(
      WebURLLoader*, WebURLRequest&, const WebURLResponse&) {}
  virtual void didSendData(
      WebURLLoader*, unsigned long long, unsigned long long) {}

  virtual void didReceiveResponse(WebURLLoader* loader,
                                  const WebURLResponse& response) {
    ++received_response_;
    response_ = response;
    data_.clear();
  }
  virtual void didReceiveData(
      blink::WebURLLoader* loader,
      const char* data,
      int data_length,
      int encoded_data_length) {
    ++received_data_;
    data_.append(data, data_length);
    total_encoded_data_length_ += encoded_data_length;
  }
  virtual void didFinishLoading(
      WebURLLoader*, double finishTime, int64_t total_encoded_data_length) {}
  virtual void didFail(WebURLLoader*, const WebURLError&) {}

  void Reset() {
    received_response_ = received_data_ = total_encoded_data_length_ = 0;
    data_.clear();
    response_.reset();
  }

  string GetResponseHeader(const char* name) const {
    return string(response_.httpHeaderField(WebString::fromUTF8(name)).utf8());
  }

  int received_response_, received_data_, total_encoded_data_length_;
  string data_;
  WebURLResponse response_;
};

// We can't put this in an anonymous function because it's a friend class for
// access to private members.
TEST(MultipartResponseTest, Functions) {
  // PushOverLine tests

  WebURLResponse response;
  response.initialize();
  response.setMIMEType("multipart/x-mixed-replace");
  response.setHTTPHeaderField("Foo", "Bar");
  response.setHTTPHeaderField("Content-type", "text/plain");
  MockWebURLLoaderClient client;
  MultipartResponseDelegate delegate(&client, NULL, response, "bound");
  MultipartResponseDelegateTester delegate_tester(&delegate);

  struct {
    const char* input;
    const int position;
    const int expected;
  } line_tests[] = {
    { "Line", 0, 0 },
    { "Line", 2, 0 },
    { "Line", 10, 0 },
    { "\r\nLine", 0, 2 },
    { "\nLine", 0, 1 },
    { "\n\nLine", 0, 2 },
    { "\rLine", 0, 1 },
    { "Line\r\nLine", 4, 2 },
    { "Line\nLine", 4, 1 },
    { "Line\n\nLine", 4, 2 },
    { "Line\rLine", 4, 1 },
    { "Line\r\rLine", 4, 1 },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(line_tests); ++i) {
    EXPECT_EQ(line_tests[i].expected,
              delegate_tester.PushOverLine(line_tests[i].input,
                                           line_tests[i].position));
  }

  // ParseHeaders tests
  struct {
    const char* data;
    const bool rv;
    const int received_response_calls;
    const char* newdata;
  } header_tests[] = {
    { "This is junk", false, 0, "This is junk" },
    { "Foo: bar\nBaz:\n\nAfter:\n", true, 1, "After:\n" },
    { "Foo: bar\nBaz:\n", false, 0, "Foo: bar\nBaz:\n" },
    { "Foo: bar\r\nBaz:\r\n\r\nAfter:\r\n", true, 1, "After:\r\n" },
    { "Foo: bar\r\nBaz:\r\n", false, 0, "Foo: bar\r\nBaz:\r\n" },
    { "Foo: bar\nBaz:\r\n\r\nAfter:\n\n", true, 1, "After:\n\n" },
    { "Foo: bar\r\nBaz:\n", false, 0, "Foo: bar\r\nBaz:\n" },
    { "\r\n", true, 1, "" },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(header_tests); ++i) {
    client.Reset();
    delegate_tester.data().assign(header_tests[i].data);
    EXPECT_EQ(header_tests[i].rv,
              delegate_tester.ParseHeaders());
    EXPECT_EQ(header_tests[i].received_response_calls,
              client.received_response_);
    EXPECT_EQ(string(header_tests[i].newdata),
              delegate_tester.data());
  }
  // Test that the resource response is filled in correctly when parsing
  // headers.
  client.Reset();
  string test_header("content-type: image/png\ncontent-length: 10\n\n");
  delegate_tester.data().assign(test_header);
  EXPECT_TRUE(delegate_tester.ParseHeaders());
  EXPECT_TRUE(delegate_tester.data().length() == 0);
  EXPECT_EQ(string("image/png"), client.GetResponseHeader("Content-Type"));
  EXPECT_EQ(string("10"), client.GetResponseHeader("content-length"));
  // This header is passed from the original request.
  EXPECT_EQ(string("Bar"), client.GetResponseHeader("foo"));

  // Make sure we parse the right mime-type if a charset is provided.
  client.Reset();
  string test_header2("content-type: text/html; charset=utf-8\n\n");
  delegate_tester.data().assign(test_header2);
  EXPECT_TRUE(delegate_tester.ParseHeaders());
  EXPECT_TRUE(delegate_tester.data().length() == 0);
  EXPECT_EQ(string("text/html; charset=utf-8"),
            client.GetResponseHeader("Content-Type"));
  EXPECT_EQ(string("utf-8"),
            string(client.response_.textEncodingName().utf8()));

  // FindBoundary tests
  struct {
    const char* boundary;
    const char* data;
    const size_t position;
  } boundary_tests[] = {
    { "bound", "bound", 0 },
    { "bound", "--bound", 0 },
    { "bound", "junkbound", 4 },
    { "bound", "junk--bound", 4 },
    { "foo", "bound", string::npos },
    { "bound", "--boundbound", 0 },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(boundary_tests); ++i) {
    delegate_tester.boundary().assign(boundary_tests[i].boundary);
    delegate_tester.data().assign(boundary_tests[i].data);
    EXPECT_EQ(boundary_tests[i].position,
              delegate_tester.FindBoundary());
  }
}

TEST(MultipartResponseTest, MissingBoundaries) {
  WebURLResponse response;
  response.initialize();
  response.setMIMEType("multipart/x-mixed-replace");
  response.setHTTPHeaderField("Foo", "Bar");
  response.setHTTPHeaderField("Content-type", "text/plain");
  MockWebURLLoaderClient client;
  MultipartResponseDelegate delegate(&client, NULL, response, "bound");

  // No start boundary
  string no_start_boundary(
    "Content-type: text/plain\n\n"
    "This is a sample response\n"
    "--bound--"
    "ignore junk after end token --bound\n\nTest2\n");
  delegate.OnReceivedData(no_start_boundary.c_str(),
                          static_cast<int>(no_start_boundary.length()),
                          static_cast<int>(no_start_boundary.length()));
  EXPECT_EQ(1, client.received_response_);
  EXPECT_EQ(1, client.received_data_);
  EXPECT_EQ(string("This is a sample response"), client.data_);
  EXPECT_EQ(static_cast<int>(no_start_boundary.length()),
            client.total_encoded_data_length_);

  delegate.OnCompletedRequest();
  EXPECT_EQ(1, client.received_response_);
  EXPECT_EQ(1, client.received_data_);

  // No end boundary
  client.Reset();
  MultipartResponseDelegate delegate2(&client, NULL, response, "bound");
  string no_end_boundary(
    "bound\nContent-type: text/plain\n\n"
    "This is a sample response\n");
  delegate2.OnReceivedData(no_end_boundary.c_str(),
                          static_cast<int>(no_end_boundary.length()),
                          static_cast<int>(no_end_boundary.length()));
  EXPECT_EQ(1, client.received_response_);
  EXPECT_EQ(1, client.received_data_);
  EXPECT_EQ("This is a sample response\n", client.data_);
  EXPECT_EQ(static_cast<int>(no_end_boundary.length()),
            client.total_encoded_data_length_);

  delegate2.OnCompletedRequest();
  EXPECT_EQ(1, client.received_response_);
  EXPECT_EQ(1, client.received_data_);
  EXPECT_EQ(string("This is a sample response\n"), client.data_);
  EXPECT_EQ(static_cast<int>(no_end_boundary.length()),
            client.total_encoded_data_length_);

  // Neither boundary
  client.Reset();
  MultipartResponseDelegate delegate3(&client, NULL, response, "bound");
  string no_boundaries(
    "Content-type: text/plain\n\n"
    "This is a sample response\n");
  delegate3.OnReceivedData(no_boundaries.c_str(),
                           static_cast<int>(no_boundaries.length()),
                           static_cast<int>(no_boundaries.length()));
  EXPECT_EQ(1, client.received_response_);
  EXPECT_EQ(1, client.received_data_);
  EXPECT_EQ("This is a sample response\n", client.data_);
  EXPECT_EQ(static_cast<int>(no_boundaries.length()),
            client.total_encoded_data_length_);

  delegate3.OnCompletedRequest();
  EXPECT_EQ(1, client.received_response_);
  EXPECT_EQ(1, client.received_data_);
  EXPECT_EQ(string("This is a sample response\n"), client.data_);
  EXPECT_EQ(static_cast<int>(no_boundaries.length()),
            client.total_encoded_data_length_);
}

TEST(MultipartResponseTest, MalformedBoundary) {
  // Some servers send a boundary that is prefixed by "--".  See bug 5786.

  WebURLResponse response;
  response.initialize();
  response.setMIMEType("multipart/x-mixed-replace");
  response.setHTTPHeaderField("Foo", "Bar");
  response.setHTTPHeaderField("Content-type", "text/plain");
  MockWebURLLoaderClient client;
  MultipartResponseDelegate delegate(&client, NULL, response, "--bound");

  string data(
    "--bound\n"
    "Content-type: text/plain\n\n"
    "This is a sample response\n"
    "--bound--"
    "ignore junk after end token --bound\n\nTest2\n");
  delegate.OnReceivedData(data.c_str(),
                          static_cast<int>(data.length()),
                          static_cast<int>(data.length()));
  EXPECT_EQ(1, client.received_response_);
  EXPECT_EQ(1, client.received_data_);
  EXPECT_EQ(string("This is a sample response"), client.data_);
  EXPECT_EQ(static_cast<int>(data.length()), client.total_encoded_data_length_);

  delegate.OnCompletedRequest();
  EXPECT_EQ(1, client.received_response_);
  EXPECT_EQ(1, client.received_data_);
}


// Used in for tests that break the data in various places.
struct TestChunk {
  const int start_pos;  // offset in data
  const int end_pos;    // end offset in data
  const int expected_responses;
  const int expected_received_data;
  const char* expected_data;
  const int expected_encoded_data_length;
};

void VariousChunkSizesTest(const TestChunk chunks[], int chunks_size,
                           int responses, int received_data,
                           const char* completed_data,
                           int completed_encoded_data_length) {
  const string data(
    "--bound\n"                    // 0-7
    "Content-type: image/png\n\n"  // 8-32
    "datadatadatadatadata"         // 33-52
    "--bound\n"                    // 53-60
    "Content-type: image/jpg\n\n"  // 61-85
    "foofoofoofoofoo"              // 86-100
    "--bound--");                  // 101-109

  WebURLResponse response;
  response.initialize();
  response.setMIMEType("multipart/x-mixed-replace");
  MockWebURLLoaderClient client;
  MultipartResponseDelegate delegate(&client, NULL, response, "bound");

  for (int i = 0; i < chunks_size; ++i) {
    ASSERT_TRUE(chunks[i].start_pos < chunks[i].end_pos);
    string chunk = data.substr(chunks[i].start_pos,
                               chunks[i].end_pos - chunks[i].start_pos);
    delegate.OnReceivedData(
        chunk.c_str(),
        static_cast<int>(chunk.length()),
        static_cast<int>(chunk.length()));
    EXPECT_EQ(chunks[i].expected_responses, client.received_response_);
    EXPECT_EQ(chunks[i].expected_received_data, client.received_data_);
    EXPECT_EQ(string(chunks[i].expected_data), client.data_);
    EXPECT_EQ(chunks[i].expected_encoded_data_length,
              client.total_encoded_data_length_);
  }
  // Check final state
  delegate.OnCompletedRequest();
  EXPECT_EQ(responses, client.received_response_);
  EXPECT_EQ(received_data, client.received_data_);
  string completed_data_string(completed_data);
  EXPECT_EQ(completed_data_string, client.data_);
  EXPECT_EQ(completed_encoded_data_length, client.total_encoded_data_length_);
}

TEST(MultipartResponseTest, BreakInBoundary) {
  // Break in the first boundary
  const TestChunk bound1[] = {
    { 0, 4, 0, 0, "", 0 },
    { 4, 110, 2, 2, "foofoofoofoofoo", 110 },
  };
  VariousChunkSizesTest(bound1, arraysize(bound1),
                        2, 2, "foofoofoofoofoo", 110);

  // Break in first and second
  const TestChunk bound2[] = {
    { 0, 4, 0, 0, "", 0 },
    { 4, 55, 1, 1, "datadatadatadat", 55 },
    { 55, 65, 1, 2, "datadatadatadatadata", 65 },
    { 65, 110, 2, 3, "foofoofoofoofoo", 110 },
  };
  VariousChunkSizesTest(bound2, arraysize(bound2),
                        2, 3, "foofoofoofoofoo", 110);

  // Break in second only
  const TestChunk bound3[] = {
    { 0, 55, 1, 1, "datadatadatadat", 55 },
    { 55, 110, 2, 3, "foofoofoofoofoo", 110 },
  };
  VariousChunkSizesTest(bound3, arraysize(bound3),
                        2, 3, "foofoofoofoofoo", 110);
}

TEST(MultipartResponseTest, BreakInHeaders) {
  // Break in first header
  const TestChunk header1[] = {
    { 0, 10, 0, 0, "", 0 },
    { 10, 35, 1, 0, "", 0 },
    { 35, 110, 2, 2, "foofoofoofoofoo", 110 },
  };
  VariousChunkSizesTest(header1, arraysize(header1),
                        2, 2, "foofoofoofoofoo", 110);

  // Break in both headers
  const TestChunk header2[] = {
    { 0, 10, 0, 0, "", 0 },
    { 10, 65, 1, 1, "datadatadatadatadata", 65 },
    { 65, 110, 2, 2, "foofoofoofoofoo", 110 },
  };
  VariousChunkSizesTest(header2, arraysize(header2),
                        2, 2, "foofoofoofoofoo", 110);

  // Break at end of a header
  const TestChunk header3[] = {
    { 0, 33, 1, 0, "", 0 },
    { 33, 65, 1, 1, "datadatadatadatadata", 65 },
    { 65, 110, 2, 2, "foofoofoofoofoo", 110 },
  };
  VariousChunkSizesTest(header3, arraysize(header3),
                        2, 2, "foofoofoofoofoo", 110);
}

TEST(MultipartResponseTest, BreakInData) {
  // All data as one chunk
  const TestChunk data1[] = {
    { 0, 110, 2, 2, "foofoofoofoofoo", 110 },
  };
  VariousChunkSizesTest(data1, arraysize(data1),
                        2, 2, "foofoofoofoofoo", 110);

  // breaks in data segment
  const TestChunk data2[] = {
    { 0, 35, 1, 0, "", 0 },
    { 35, 65, 1, 1, "datadatadatadatadata", 65 },
    { 65, 90, 2, 1, "", 65 },
    { 90, 110, 2, 2, "foofoofoofoofoo", 110 },
  };
  VariousChunkSizesTest(data2, arraysize(data2),
                        2, 2, "foofoofoofoofoo", 110);

  // Incomplete send
  const TestChunk data3[] = {
    { 0, 35, 1, 0, "", 0 },
    { 35, 90, 2, 1, "", 90 },
  };
  VariousChunkSizesTest(data3, arraysize(data3),
                        2, 2, "foof", 90);
}

TEST(MultipartResponseTest, SmallChunk) {
  WebURLResponse response;
  response.initialize();
  response.setMIMEType("multipart/x-mixed-replace");
  response.setHTTPHeaderField("Content-type", "text/plain");
  MockWebURLLoaderClient client;
  MultipartResponseDelegate delegate(&client, NULL, response, "bound");

  // Test chunks of size 1, 2, and 0.
  string data(
    "--boundContent-type: text/plain\n\n"
    "\n--boundContent-type: text/plain\n\n"
    "\n\n--boundContent-type: text/plain\n\n"
    "--boundContent-type: text/plain\n\n"
    "end--bound--");
  delegate.OnReceivedData(data.c_str(),
                          static_cast<int>(data.length()),
                          static_cast<int>(data.length()));
  EXPECT_EQ(4, client.received_response_);
  EXPECT_EQ(2, client.received_data_);
  EXPECT_EQ(string("end"), client.data_);
  EXPECT_EQ(static_cast<int>(data.length()), client.total_encoded_data_length_);

  delegate.OnCompletedRequest();
  EXPECT_EQ(4, client.received_response_);
  EXPECT_EQ(2, client.received_data_);
}

TEST(MultipartResponseTest, MultipleBoundaries) {
  // Test multiple boundaries back to back
  WebURLResponse response;
  response.initialize();
  response.setMIMEType("multipart/x-mixed-replace");
  MockWebURLLoaderClient client;
  MultipartResponseDelegate delegate(&client, NULL, response, "bound");

  string data("--bound\r\n\r\n--bound\r\n\r\nfoofoo--bound--");
  delegate.OnReceivedData(data.c_str(),
                          static_cast<int>(data.length()),
                          static_cast<int>(data.length()));
  EXPECT_EQ(2, client.received_response_);
  EXPECT_EQ(1, client.received_data_);
  EXPECT_EQ(string("foofoo"), client.data_);
  EXPECT_EQ(static_cast<int>(data.length()), client.total_encoded_data_length_);
}

TEST(MultipartResponseTest, MultipartByteRangeParsingTest) {
  // Test multipart/byteranges based boundary parsing.
  WebURLResponse response1;
  response1.initialize();
  response1.setMIMEType("multipart/x-mixed-replace");
  response1.setHTTPHeaderField("Content-Length", "200");
  response1.setHTTPHeaderField("Content-type",
                               "multipart/byteranges; boundary=--bound--");

  std::string multipart_boundary;
  bool result = MultipartResponseDelegate::ReadMultipartBoundary(
      response1, &multipart_boundary);
  EXPECT_EQ(result, true);
  EXPECT_EQ(string("--bound--"),
            multipart_boundary);

  WebURLResponse response2;
  response2.initialize();
  response2.setMIMEType("image/png");

  response2.setHTTPHeaderField("Content-Length", "300");
  response2.setHTTPHeaderField("Last-Modified",
                               "Mon, 04 Apr 2005 20:36:01 GMT");
  response2.setHTTPHeaderField("Date", "Thu, 11 Sep 2008 18:21:42 GMT");

  multipart_boundary.clear();
  result = MultipartResponseDelegate::ReadMultipartBoundary(
      response2, &multipart_boundary);
  EXPECT_EQ(result, false);

  WebURLResponse response3;
  response3.initialize();
  response3.setMIMEType("multipart/byteranges");

  response3.setHTTPHeaderField("Content-Length", "300");
  response3.setHTTPHeaderField("Last-Modified",
                               "Mon, 04 Apr 2005 20:36:01 GMT");
  response3.setHTTPHeaderField("Date", "Thu, 11 Sep 2008 18:21:42 GMT");
  response3.setHTTPHeaderField("Content-type", "multipart/byteranges");

  multipart_boundary.clear();
  result = MultipartResponseDelegate::ReadMultipartBoundary(
      response3, &multipart_boundary);
  EXPECT_EQ(result, false);
  EXPECT_EQ(multipart_boundary.length(), 0U);

  WebURLResponse response4;
  response4.initialize();
  response4.setMIMEType("multipart/byteranges");
  response4.setHTTPHeaderField("Content-Length", "200");
  response4.setHTTPHeaderField("Content-type",
      "multipart/byteranges; boundary=--bound--; charSet=utf8");

  multipart_boundary.clear();

  result = MultipartResponseDelegate::ReadMultipartBoundary(
      response4, &multipart_boundary);
  EXPECT_EQ(result, true);
  EXPECT_EQ(string("--bound--"), multipart_boundary);

  WebURLResponse response5;
  response5.initialize();
  response5.setMIMEType("multipart/byteranges");
  response5.setHTTPHeaderField("Content-Length", "200");
  response5.setHTTPHeaderField("Content-type",
      "multipart/byteranges; boundary=\"--bound--\"; charSet=utf8");

  multipart_boundary.clear();

  result = MultipartResponseDelegate::ReadMultipartBoundary(
      response5, &multipart_boundary);
  EXPECT_EQ(result, true);
  EXPECT_EQ(string("--bound--"), multipart_boundary);
}

TEST(MultipartResponseTest, MultipartContentRangesTest) {
  WebURLResponse response1;
  response1.initialize();
  response1.setMIMEType("application/pdf");
  response1.setHTTPHeaderField("Content-Length", "200");  // Ignored!
  // Use intentionally >32bit values to check they are handled correctly.
  response1.setHTTPHeaderField("Content-Range",
                               "bytes 5000000000-5000000050/6000000000");

  int64 content_range_lower_bound = 0;
  int64 content_range_upper_bound = 0;
  int64 content_range_instance_size = 0;

  bool result = MultipartResponseDelegate::ReadContentRanges(
      response1, &content_range_lower_bound,
      &content_range_upper_bound,
      &content_range_instance_size);

  EXPECT_EQ(result, true);
  EXPECT_EQ(content_range_lower_bound, 5e9);
  EXPECT_EQ(content_range_upper_bound, 5e9+50);
  EXPECT_EQ(content_range_instance_size, 6e9);

  WebURLResponse response2;
  response2.initialize();
  response2.setMIMEType("application/pdf");
  response2.setHTTPHeaderField("Content-Length", "200");
  response2.setHTTPHeaderField("Content-Range", "bytes 1000/1050");

  content_range_lower_bound = 0;
  content_range_upper_bound = 0;
  content_range_instance_size = 0;

  result = MultipartResponseDelegate::ReadContentRanges(
      response2, &content_range_lower_bound,
      &content_range_upper_bound,
      &content_range_instance_size);

  EXPECT_EQ(result, false);

  WebURLResponse response3;
  response3.initialize();
  response3.setMIMEType("application/pdf");
  response3.setHTTPHeaderField("Content-Length", "200");
  response3.setHTTPHeaderField("Range", "bytes 1000-1050/5000");

  content_range_lower_bound = 0;
  content_range_upper_bound = 0;
  content_range_instance_size = 0;

  result = MultipartResponseDelegate::ReadContentRanges(
      response3, &content_range_lower_bound,
      &content_range_upper_bound,
      &content_range_instance_size);

  EXPECT_EQ(result, true);
  EXPECT_EQ(content_range_lower_bound, 1000);
  EXPECT_EQ(content_range_upper_bound, 1050);

  WebURLResponse response4;
  response4.initialize();
  response4.setMIMEType("application/pdf");
  response4.setHTTPHeaderField("Content-Length", "200");

  content_range_lower_bound = 0;
  content_range_upper_bound = 0;
  content_range_instance_size = 0;

  result = MultipartResponseDelegate::ReadContentRanges(
      response4, &content_range_lower_bound,
      &content_range_upper_bound,
      &content_range_instance_size);

  EXPECT_EQ(result, false);
}

TEST(MultipartResponseTest, MultipartPayloadSet) {
  WebURLResponse response;
  response.initialize();
  response.setMIMEType("multipart/x-mixed-replace");
  MockWebURLLoaderClient client;
  MultipartResponseDelegate delegate(&client, NULL, response, "bound");

  string data(
      "--bound\n"
      "Content-type: text/plain\n\n"
      "response data\n"
      "--bound\n");
  delegate.OnReceivedData(data.c_str(),
                          static_cast<int>(data.length()),
                          static_cast<int>(data.length()));
  EXPECT_EQ(1, client.received_response_);
  EXPECT_EQ(string("response data"), client.data_);
  EXPECT_EQ(static_cast<int>(data.length()), client.total_encoded_data_length_);
  EXPECT_FALSE(client.response_.isMultipartPayload());

  string data2(
      "Content-type: text/plain\n\n"
      "response data2\n"
      "--bound\n");
  delegate.OnReceivedData(data2.c_str(),
                          static_cast<int>(data2.length()),
                          static_cast<int>(data2.length()));
  EXPECT_EQ(2, client.received_response_);
  EXPECT_EQ(string("response data2"), client.data_);
  EXPECT_EQ(static_cast<int>(data.length()) + static_cast<int>(data2.length()),
            client.total_encoded_data_length_);
  EXPECT_TRUE(client.response_.isMultipartPayload());
}

}  // namespace

}  // namespace content
