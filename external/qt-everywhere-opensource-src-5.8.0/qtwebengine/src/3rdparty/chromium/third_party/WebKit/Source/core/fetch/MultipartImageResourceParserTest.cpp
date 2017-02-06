// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fetch/MultipartImageResourceParser.h"

#include "platform/network/ResourceResponse.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLResponse.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace blink {

namespace {

String toString(const Vector<char>& data)
{
    if (data.isEmpty())
        return String("");
    return String(data.data(), data.size());
}

class MockClient final : public GarbageCollectedFinalized<MockClient>, public MultipartImageResourceParser::Client {
    USING_GARBAGE_COLLECTED_MIXIN(MockClient);

public:
    void onePartInMultipartReceived(const ResourceResponse& response) override
    {
        m_responses.append(response);
        m_data.append(Vector<char>());
    }
    void multipartDataReceived(const char* bytes, size_t size) override
    {
        m_data.last().append(bytes, size);
    }

    Vector<ResourceResponse> m_responses;
    Vector<Vector<char>> m_data;
};

TEST(MultipartResponseTest, SkippableLength)
{
    struct {
        const char* input;
        const size_t position;
        const size_t expected;
    } lineTests[] = {
        { "Line", 0, 0 },
        { "Line", 2, 0 },
        { "Line", 10, 0 },
        { "\r\nLine", 0, 2 },
        { "\nLine", 0, 1 },
        { "\n\nLine", 0, 1 },
        { "\rLine", 0, 0 },
        { "Line\r\nLine", 4, 2 },
        { "Line\nLine", 4, 1 },
        { "Line\n\nLine", 4, 1 },
        { "Line\rLine", 4, 0 },
        { "Line\r\rLine", 4, 0 },
    };
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(lineTests); ++i) {
        Vector<char> input;
        input.append(lineTests[i].input, strlen(lineTests[i].input));
        EXPECT_EQ(lineTests[i].expected,
            MultipartImageResourceParser::skippableLengthForTest(input, lineTests[i].position));
    }
}

TEST(MultipartResponseTest, ParseMultipartHeadersResult)
{
    struct {
        const char* data;
        const bool result;
        const size_t end;
    } tests[] = {
        { "This is junk", false, 0 },
        { "Foo: bar\nBaz:\n\nAfter:\n", true, 15 },
        { "Foo: bar\nBaz:\n", false, 0},
        { "Foo: bar\r\nBaz:\r\n\r\nAfter:\r\n", true, 18 },
        { "Foo: bar\r\nBaz:\r\n", false, 0 },
        { "Foo: bar\nBaz:\r\n\r\nAfter:\n\n", true, 17 },
        { "Foo: bar\r\nBaz:\n", false,  0 },
        { "\r\n", true, 2 },
    };
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(tests); ++i) {
        WebURLResponse response;
        response.initialize();
        size_t end = 0;
        bool result = Platform::current()->parseMultipartHeadersFromBody(tests[i].data, strlen(tests[i].data), &response, &end);
        EXPECT_EQ(tests[i].result, result);
        EXPECT_EQ(tests[i].end, end);
    }
}

TEST(MultipartResponseTest, ParseMultipartHeaders)
{
    WebURLResponse webResponse;
    webResponse.initialize();
    webResponse.addHTTPHeaderField(WebString::fromLatin1("foo"), WebString::fromLatin1("bar"));
    webResponse.addHTTPHeaderField(WebString::fromLatin1("range"), WebString::fromLatin1("piyo"));
    webResponse.addHTTPHeaderField(WebString::fromLatin1("content-length"), WebString::fromLatin1("999"));

    const char data[] = "content-type: image/png\ncontent-length: 10\n\n";
    size_t end = 0;
    bool result = Platform::current()->parseMultipartHeadersFromBody(data, strlen(data), &webResponse, &end);
    const ResourceResponse& response = webResponse.toResourceResponse();

    EXPECT_TRUE(result);
    EXPECT_EQ(strlen(data), end);
    EXPECT_EQ("image/png", response.httpHeaderField("content-type"));
    EXPECT_EQ("10", response.httpHeaderField("content-length"));
    EXPECT_EQ("bar", response.httpHeaderField("foo"));
    EXPECT_EQ(AtomicString(), response.httpHeaderField("range"));
}

TEST(MultipartResponseTest, ParseMultipartHeadersContentCharset)
{
    WebURLResponse webResponse;
    webResponse.initialize();

    const char data[] = "content-type: text/html; charset=utf-8\n\n";
    size_t end = 0;
    bool result = Platform::current()->parseMultipartHeadersFromBody(data, strlen(data), &webResponse, &end);
    const ResourceResponse& response = webResponse.toResourceResponse();

    EXPECT_TRUE(result);
    EXPECT_EQ(strlen(data), end);
    EXPECT_EQ("text/html; charset=utf-8", response.httpHeaderField("content-type"));
    EXPECT_EQ("utf-8", response.textEncodingName());
}

TEST(MultipartResponseTest, FindBoundary)
{
    struct {
        const char* boundary;
        const char* data;
        const size_t position;
    } boundaryTests[] = {
        { "bound", "bound", 0 },
        { "bound", "--bound", 0 },
        { "bound", "junkbound", 4 },
        { "bound", "junk--bound", 4 },
        { "foo", "bound", kNotFound },
        { "bound", "--boundbound", 0 },
    };

    for (size_t i = 0; i < WTF_ARRAY_LENGTH(boundaryTests); ++i) {
        Vector<char> boundary, data;
        boundary.append(boundaryTests[i].boundary, strlen(boundaryTests[i].boundary));
        data.append(boundaryTests[i].data, strlen(boundaryTests[i].data));
        EXPECT_EQ(boundaryTests[i].position, MultipartImageResourceParser::findBoundaryForTest(data, &boundary));
    }
}

TEST(MultipartResponseTest, NoStartBoundary)
{
    ResourceResponse response;
    response.setMimeType("multipart/x-mixed-replace");
    response.setHTTPHeaderField("Foo", "Bar");
    response.setHTTPHeaderField("Content-type", "text/plain");
    MockClient* client = new MockClient;
    Vector<char> boundary;
    boundary.append("bound", 5);

    MultipartImageResourceParser* parser = new MultipartImageResourceParser(response, boundary, client);
    const char data[] =
        "Content-type: text/plain\n\n"
        "This is a sample response\n"
        "--bound--"
        "ignore junk after end token --bound\n\nTest2\n";
    parser->appendData(data, strlen(data));
    ASSERT_EQ(1u, client->m_responses.size());
    ASSERT_EQ(1u, client->m_data.size());
    EXPECT_EQ("This is a sample response", toString(client->m_data[0]));

    parser->finish();
    ASSERT_EQ(1u, client->m_responses.size());
    ASSERT_EQ(1u, client->m_data.size());
    EXPECT_EQ("This is a sample response", toString(client->m_data[0]));
}

TEST(MultipartResponseTest, NoEndBoundary)
{
    ResourceResponse response;
    response.setMimeType("multipart/x-mixed-replace");
    response.setHTTPHeaderField("Foo", "Bar");
    response.setHTTPHeaderField("Content-type", "text/plain");
    MockClient* client = new MockClient;
    Vector<char> boundary;
    boundary.append("bound", 5);

    MultipartImageResourceParser* parser = new MultipartImageResourceParser(response, boundary, client);
    const char data[] =
        "bound\nContent-type: text/plain\n\n"
        "This is a sample response\n";
    parser->appendData(data, strlen(data));
    ASSERT_EQ(1u, client->m_responses.size());
    ASSERT_EQ(1u, client->m_data.size());
    EXPECT_EQ("This is a sample ", toString(client->m_data[0]));

    parser->finish();
    ASSERT_EQ(1u, client->m_responses.size());
    ASSERT_EQ(1u, client->m_data.size());
    EXPECT_EQ("This is a sample response\n", toString(client->m_data[0]));
}

TEST(MultipartResponseTest, NoStartAndEndBoundary)
{
    ResourceResponse response;
    response.setMimeType("multipart/x-mixed-replace");
    response.setHTTPHeaderField("Foo", "Bar");
    response.setHTTPHeaderField("Content-type", "text/plain");
    MockClient* client = new MockClient;
    Vector<char> boundary;
    boundary.append("bound", 5);

    MultipartImageResourceParser* parser = new MultipartImageResourceParser(response, boundary, client);
    const char data[] =
        "Content-type: text/plain\n\n"
        "This is a sample response\n";
    parser->appendData(data, strlen(data));
    ASSERT_EQ(1u, client->m_responses.size());
    ASSERT_EQ(1u, client->m_data.size());
    EXPECT_EQ("This is a sample ", toString(client->m_data[0]));

    parser->finish();
    ASSERT_EQ(1u, client->m_responses.size());
    ASSERT_EQ(1u, client->m_data.size());
    EXPECT_EQ("This is a sample response\n", toString(client->m_data[0]));
}

TEST(MultipartResponseTest, MalformedBoundary)
{
    // Some servers send a boundary that is prefixed by "--".  See bug 5786.
    ResourceResponse response;
    response.setMimeType("multipart/x-mixed-replace");
    response.setHTTPHeaderField("Foo", "Bar");
    response.setHTTPHeaderField("Content-type", "text/plain");
    MockClient* client = new MockClient;
    Vector<char> boundary;
    boundary.append("--bound", 7);

    MultipartImageResourceParser* parser = new MultipartImageResourceParser(response, boundary, client);
    const char data[] =
        "--bound\n"
        "Content-type: text/plain\n\n"
        "This is a sample response\n"
        "--bound--"
        "ignore junk after end token --bound\n\nTest2\n";
    parser->appendData(data, strlen(data));
    ASSERT_EQ(1u, client->m_responses.size());
    ASSERT_EQ(1u, client->m_data.size());
    EXPECT_EQ("This is a sample response", toString(client->m_data[0]));

    parser->finish();
    ASSERT_EQ(1u, client->m_responses.size());
    ASSERT_EQ(1u, client->m_data.size());
    EXPECT_EQ("This is a sample response", toString(client->m_data[0]));
}

// Used in for tests that break the data in various places.
struct TestChunk {
    const int startPosition; // offset in data
    const int endPosition; // end offset in data
    const size_t expectedResponses;
    const char* expectedData;
};

void variousChunkSizesTest(const TestChunk chunks[], int chunksSize,
    size_t responses, int receivedData,
    const char* completedData)
{
    const char data[] =
        "--bound\n" // 0-7
        "Content-type: image/png\n\n" // 8-32
        "datadatadatadatadata" // 33-52
        "--bound\n" // 53-60
        "Content-type: image/jpg\n\n" // 61-85
        "foofoofoofoofoo" // 86-100
        "--bound--"; // 101-109

    ResourceResponse response;
    response.setMimeType("multipart/x-mixed-replace");
    MockClient* client = new MockClient;
    Vector<char> boundary;
    boundary.append("bound", 5);

    MultipartImageResourceParser* parser = new MultipartImageResourceParser(response, boundary, client);

    for (int i = 0; i < chunksSize; ++i) {
        ASSERT_LT(chunks[i].startPosition, chunks[i].endPosition);
        parser->appendData(data + chunks[i].startPosition, chunks[i].endPosition - chunks[i].startPosition);
        EXPECT_EQ(chunks[i].expectedResponses, client->m_responses.size());
        EXPECT_EQ(String(chunks[i].expectedData), client->m_data.size() > 0 ? toString(client->m_data.last()) : String(""));
    }
    // Check final state
    parser->finish();
    EXPECT_EQ(responses, client->m_responses.size());
    EXPECT_EQ(completedData, toString(client->m_data.last()));
}

template <size_t N>
void variousChunkSizesTest(const TestChunk (&chunks)[N], size_t responses, int receivedData, const char* completedData)
{
    variousChunkSizesTest(chunks, N, responses, receivedData, completedData);
}

TEST(MultipartResponseTest, BreakInBoundary)
{
    // Break in the first boundary
    const TestChunk bound1[] = {
        { 0, 4, 0, "" },
        { 4, 110, 2, "foofoofoofoofoo" },
    };
    variousChunkSizesTest(bound1, 2, 2, "foofoofoofoofoo");

    // Break in first and second
    const TestChunk bound2[] = {
        { 0, 4, 0, "" },
        { 4, 55, 1, "datadatadatad" },
        { 55, 65, 1, "datadatadatadatadata" },
        { 65, 110, 2, "foofoofoofoofoo" },
    };
    variousChunkSizesTest(bound2, 2, 3, "foofoofoofoofoo");

    // Break in second only
    const TestChunk bound3[] = {
        { 0, 55, 1, "datadatadatad" },
        { 55, 110, 2, "foofoofoofoofoo" },
    };
    variousChunkSizesTest(bound3, 2, 3, "foofoofoofoofoo");
}

TEST(MultipartResponseTest, BreakInHeaders)
{
    // Break in first header
    const TestChunk header1[] = {
        { 0, 10, 0, "" },
        { 10, 35, 1, "" },
        { 35, 110, 2, "foofoofoofoofoo" },
    };
    variousChunkSizesTest(header1, 2, 2, "foofoofoofoofoo");

    // Break in both headers
    const TestChunk header2[] = {
        { 0, 10, 0, "" },
        { 10, 65, 1, "datadatadatadatadata" },
        { 65, 110, 2, "foofoofoofoofoo" },
    };
    variousChunkSizesTest(header2, 2, 2, "foofoofoofoofoo");

    // Break at end of a header
    const TestChunk header3[] = {
        { 0, 33, 1, "" },
        { 33, 65, 1, "datadatadatadatadata" },
        { 65, 110, 2, "foofoofoofoofoo" },
    };
    variousChunkSizesTest(header3, 2, 2, "foofoofoofoofoo");
}

TEST(MultipartResponseTest, BreakInData)
{
    // All data as one chunk
    const TestChunk data1[] = {
        { 0, 110, 2, "foofoofoofoofoo" },
    };
    variousChunkSizesTest(data1, 2, 2, "foofoofoofoofoo");

    // breaks in data segment
    const TestChunk data2[] = {
        { 0, 35, 1, "" },
        { 35, 65, 1, "datadatadatadatadata" },
        { 65, 90, 2, "" },
        { 90, 110, 2, "foofoofoofoofoo" },
    };
    variousChunkSizesTest(data2, 2, 2, "foofoofoofoofoo");

    // Incomplete send
    const TestChunk data3[] = {
        { 0, 35, 1, "" },
        { 35, 90, 2, "" },
    };
    variousChunkSizesTest(data3, 2, 2, "foof");
}

TEST(MultipartResponseTest, SmallChunk)
{
    ResourceResponse response;
    response.setMimeType("multipart/x-mixed-replace");
    response.setHTTPHeaderField("Content-type", "text/plain");
    MockClient* client = new MockClient;
    Vector<char> boundary;
    boundary.append("bound", 5);

    MultipartImageResourceParser* parser = new MultipartImageResourceParser(response, boundary, client);

    // Test chunks of size 1, 2, and 0.
    const char data[] =
        "--boundContent-type: text/plain\n\n"
        "\n--boundContent-type: text/plain\n\n"
        "\n\n--boundContent-type: text/plain\n\n"
        "--boundContent-type: text/plain\n\n"
        "end--bound--";
    parser->appendData(data, strlen(data));
    ASSERT_EQ(4u, client->m_responses.size());
    ASSERT_EQ(4u, client->m_data.size());
    EXPECT_EQ("", toString(client->m_data[0]));
    EXPECT_EQ("\n", toString(client->m_data[1]));
    EXPECT_EQ("", toString(client->m_data[2]));
    EXPECT_EQ("end", toString(client->m_data[3]));

    parser->finish();
    ASSERT_EQ(4u, client->m_responses.size());
    ASSERT_EQ(4u, client->m_data.size());
    EXPECT_EQ("", toString(client->m_data[0]));
    EXPECT_EQ("\n", toString(client->m_data[1]));
    EXPECT_EQ("", toString(client->m_data[2]));
    EXPECT_EQ("end", toString(client->m_data[3]));
}

TEST(MultipartResponseTest, MultipleBoundaries)
{
    // Test multiple boundaries back to back
    ResourceResponse response;
    response.setMimeType("multipart/x-mixed-replace");
    MockClient* client = new MockClient;
    Vector<char> boundary;
    boundary.append("bound", 5);

    MultipartImageResourceParser* parser = new MultipartImageResourceParser(response, boundary, client);

    const char data[] = "--bound\r\n\r\n--bound\r\n\r\nfoofoo--bound--";
    parser->appendData(data, strlen(data));
    ASSERT_EQ(2u, client->m_responses.size());
    ASSERT_EQ(2u, client->m_data.size());
    EXPECT_EQ("", toString(client->m_data[0]));
    EXPECT_EQ("foofoo", toString(client->m_data[1]));
}

TEST(MultipartResponseTest, EatLeadingLF)
{
    ResourceResponse response;
    response.setMimeType("multipart/x-mixed-replace");
    MockClient* client = new MockClient;
    Vector<char> boundary;
    boundary.append("bound", 5);

    const char data[] =
        "\n\n\n--bound\n\n\ncontent-type: 1\n\n"
        "\n\n\n--bound\n\ncontent-type: 2\n\n"
        "\n\n\n--bound\ncontent-type: 3\n\n";
    MultipartImageResourceParser* parser = new MultipartImageResourceParser(response, boundary, client);

    for (size_t i = 0; i < strlen(data); ++i)
        parser->appendData(&data[i], 1);
    parser->finish();

    ASSERT_EQ(4u, client->m_responses.size());
    ASSERT_EQ(4u, client->m_data.size());
    EXPECT_EQ(String(), client->m_responses[0].httpHeaderField("content-type"));
    EXPECT_EQ("", toString(client->m_data[0]));
    EXPECT_EQ(String(), client->m_responses[1].httpHeaderField("content-type"));
    EXPECT_EQ("\ncontent-type: 1\n\n\n\n", toString(client->m_data[1]));
    EXPECT_EQ(String(), client->m_responses[2].httpHeaderField("content-type"));
    EXPECT_EQ("content-type: 2\n\n\n\n", toString(client->m_data[2]));
    EXPECT_EQ("3", client->m_responses[3].httpHeaderField("content-type"));
    EXPECT_EQ("", toString(client->m_data[3]));
}

TEST(MultipartResponseTest, EatLeadingCRLF)
{
    ResourceResponse response;
    response.setMimeType("multipart/x-mixed-replace");
    MockClient* client = new MockClient;
    Vector<char> boundary;
    boundary.append("bound", 5);

    const char data[] =
        "\r\n\r\n\r\n--bound\r\n\r\n\r\ncontent-type: 1\r\n\r\n"
        "\r\n\r\n\r\n--bound\r\n\r\ncontent-type: 2\r\n\r\n"
        "\r\n\r\n\r\n--bound\r\ncontent-type: 3\r\n\r\n";
    MultipartImageResourceParser* parser = new MultipartImageResourceParser(response, boundary, client);

    for (size_t i = 0; i < strlen(data); ++i)
        parser->appendData(&data[i], 1);
    parser->finish();

    ASSERT_EQ(4u, client->m_responses.size());
    ASSERT_EQ(4u, client->m_data.size());
    EXPECT_EQ(String(), client->m_responses[0].httpHeaderField("content-type"));
    EXPECT_EQ("", toString(client->m_data[0]));
    EXPECT_EQ(String(), client->m_responses[1].httpHeaderField("content-type"));
    EXPECT_EQ("\r\ncontent-type: 1\r\n\r\n\r\n\r\n", toString(client->m_data[1]));
    EXPECT_EQ(String(), client->m_responses[2].httpHeaderField("content-type"));
    EXPECT_EQ("content-type: 2\r\n\r\n\r\n\r\n", toString(client->m_data[2]));
    EXPECT_EQ("3", client->m_responses[3].httpHeaderField("content-type"));
    EXPECT_EQ("", toString(client->m_data[3]));
}

} // namespace

} // namespace blink
