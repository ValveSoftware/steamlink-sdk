// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_chunked_decoder.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

typedef testing::Test HttpChunkedDecoderTest;

void RunTest(const char* inputs[], size_t num_inputs,
             const char* expected_output,
             bool expected_eof,
             int bytes_after_eof) {
  HttpChunkedDecoder decoder;
  EXPECT_FALSE(decoder.reached_eof());

  std::string result;

  for (size_t i = 0; i < num_inputs; ++i) {
    std::string input = inputs[i];
    int n = decoder.FilterBuf(&input[0], static_cast<int>(input.size()));
    EXPECT_GE(n, 0);
    if (n > 0)
      result.append(input.data(), n);
  }

  EXPECT_EQ(expected_output, result);
  EXPECT_EQ(expected_eof, decoder.reached_eof());
  EXPECT_EQ(bytes_after_eof, decoder.bytes_after_eof());
}

// Feed the inputs to the decoder, until it returns an error.
void RunTestUntilFailure(const char* inputs[],
                         size_t num_inputs,
                         size_t fail_index) {
  HttpChunkedDecoder decoder;
  EXPECT_FALSE(decoder.reached_eof());

  for (size_t i = 0; i < num_inputs; ++i) {
    std::string input = inputs[i];
    int n = decoder.FilterBuf(&input[0], static_cast<int>(input.size()));
    if (n < 0) {
      EXPECT_EQ(ERR_INVALID_CHUNKED_ENCODING, n);
      EXPECT_EQ(fail_index, i);
      return;
    }
  }
  FAIL();  // We should have failed on the |fail_index| iteration of the loop.
}

TEST(HttpChunkedDecoderTest, Basic) {
  const char* inputs[] = {
    "B\r\nhello hello\r\n0\r\n\r\n"
  };
  RunTest(inputs, arraysize(inputs), "hello hello", true, 0);
}

TEST(HttpChunkedDecoderTest, OneChunk) {
  const char* inputs[] = {
    "5\r\nhello\r\n"
  };
  RunTest(inputs, arraysize(inputs), "hello", false, 0);
}

TEST(HttpChunkedDecoderTest, Typical) {
  const char* inputs[] = {
    "5\r\nhello\r\n",
    "1\r\n \r\n",
    "5\r\nworld\r\n",
    "0\r\n\r\n"
  };
  RunTest(inputs, arraysize(inputs), "hello world", true, 0);
}

TEST(HttpChunkedDecoderTest, Incremental) {
  const char* inputs[] = {
    "5",
    "\r",
    "\n",
    "hello",
    "\r",
    "\n",
    "0",
    "\r",
    "\n",
    "\r",
    "\n"
  };
  RunTest(inputs, arraysize(inputs), "hello", true, 0);
}

// Same as above, but group carriage returns with previous input.
TEST(HttpChunkedDecoderTest, Incremental2) {
  const char* inputs[] = {
    "5\r",
    "\n",
    "hello\r",
    "\n",
    "0\r",
    "\n\r",
    "\n"
  };
  RunTest(inputs, arraysize(inputs), "hello", true, 0);
}

TEST(HttpChunkedDecoderTest, LF_InsteadOf_CRLF) {
  // Compatibility: [RFC 2616 - Invalid]
  // {Firefox3} - Valid
  // {IE7, Safari3.1, Opera9.51} - Invalid
  const char* inputs[] = {
    "5\nhello\n",
    "1\n \n",
    "5\nworld\n",
    "0\n\n"
  };
  RunTest(inputs, arraysize(inputs), "hello world", true, 0);
}

TEST(HttpChunkedDecoderTest, Extensions) {
  const char* inputs[] = {
    "5;x=0\r\nhello\r\n",
    "0;y=\"2 \"\r\n\r\n"
  };
  RunTest(inputs, arraysize(inputs), "hello", true, 0);
}

TEST(HttpChunkedDecoderTest, Trailers) {
  const char* inputs[] = {
    "5\r\nhello\r\n",
    "0\r\n",
    "Foo: 1\r\n",
    "Bar: 2\r\n",
    "\r\n"
  };
  RunTest(inputs, arraysize(inputs), "hello", true, 0);
}

TEST(HttpChunkedDecoderTest, TrailersUnfinished) {
  const char* inputs[] = {
    "5\r\nhello\r\n",
    "0\r\n",
    "Foo: 1\r\n"
  };
  RunTest(inputs, arraysize(inputs), "hello", false, 0);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_TooBig) {
  const char* inputs[] = {
    // This chunked body is not terminated.
    // However we will fail decoding because the chunk-size
    // number is larger than we can handle.
    "48469410265455838241\r\nhello\r\n",
    "0\r\n\r\n"
  };
  RunTestUntilFailure(inputs, arraysize(inputs), 0);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_0X) {
  const char* inputs[] = {
    // Compatibility [RFC 2616 - Invalid]:
    // {Safari3.1, IE7} - Invalid
    // {Firefox3, Opera 9.51} - Valid
    "0x5\r\nhello\r\n",
    "0\r\n\r\n"
  };
  RunTestUntilFailure(inputs, arraysize(inputs), 0);
}

TEST(HttpChunkedDecoderTest, ChunkSize_TrailingSpace) {
  const char* inputs[] = {
    // Compatibility [RFC 2616 - Invalid]:
    // {IE7, Safari3.1, Firefox3, Opera 9.51} - Valid
    //
    // At least yahoo.com depends on this being valid.
    "5      \r\nhello\r\n",
    "0\r\n\r\n"
  };
  RunTest(inputs, arraysize(inputs), "hello", true, 0);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_TrailingTab) {
  const char* inputs[] = {
    // Compatibility [RFC 2616 - Invalid]:
    // {IE7, Safari3.1, Firefox3, Opera 9.51} - Valid
    "5\t\r\nhello\r\n",
    "0\r\n\r\n"
  };
  RunTestUntilFailure(inputs, arraysize(inputs), 0);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_TrailingFormFeed) {
  const char* inputs[] = {
    // Compatibility [RFC 2616- Invalid]:
    // {Safari3.1} - Invalid
    // {IE7, Firefox3, Opera 9.51} - Valid
    "5\f\r\nhello\r\n",
    "0\r\n\r\n"
  };
  RunTestUntilFailure(inputs, arraysize(inputs), 0);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_TrailingVerticalTab) {
  const char* inputs[] = {
    // Compatibility [RFC 2616 - Invalid]:
    // {Safari 3.1} - Invalid
    // {IE7, Firefox3, Opera 9.51} - Valid
    "5\v\r\nhello\r\n",
    "0\r\n\r\n"
  };
  RunTestUntilFailure(inputs, arraysize(inputs), 0);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_TrailingNonHexDigit) {
  const char* inputs[] = {
    // Compatibility [RFC 2616 - Invalid]:
    // {Safari 3.1} - Invalid
    // {IE7, Firefox3, Opera 9.51} - Valid
    "5H\r\nhello\r\n",
    "0\r\n\r\n"
  };
  RunTestUntilFailure(inputs, arraysize(inputs), 0);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_LeadingSpace) {
  const char* inputs[] = {
    // Compatibility [RFC 2616 - Invalid]:
    // {IE7} - Invalid
    // {Safari 3.1, Firefox3, Opera 9.51} - Valid
    " 5\r\nhello\r\n",
    "0\r\n\r\n"
  };
  RunTestUntilFailure(inputs, arraysize(inputs), 0);
}

TEST(HttpChunkedDecoderTest, InvalidLeadingSeparator) {
  const char* inputs[] = {
    "\r\n5\r\nhello\r\n",
    "0\r\n\r\n"
  };
  RunTestUntilFailure(inputs, arraysize(inputs), 0);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_NoSeparator) {
  const char* inputs[] = {
    "5\r\nhello",
    "1\r\n \r\n",
    "0\r\n\r\n"
  };
  RunTestUntilFailure(inputs, arraysize(inputs), 1);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_Negative) {
  const char* inputs[] = {
    "8\r\n12345678\r\n-5\r\nhello\r\n",
    "0\r\n\r\n"
  };
  RunTestUntilFailure(inputs, arraysize(inputs), 0);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_Plus) {
  const char* inputs[] = {
    // Compatibility [RFC 2616 - Invalid]:
    // {IE7, Safari 3.1} - Invalid
    // {Firefox3, Opera 9.51} - Valid
    "+5\r\nhello\r\n",
    "0\r\n\r\n"
  };
  RunTestUntilFailure(inputs, arraysize(inputs), 0);
}

TEST(HttpChunkedDecoderTest, InvalidConsecutiveCRLFs) {
  const char* inputs[] = {
    "5\r\nhello\r\n",
    "\r\n\r\n\r\n\r\n",
    "0\r\n\r\n"
  };
  RunTestUntilFailure(inputs, arraysize(inputs), 1);
}

TEST(HttpChunkedDecoderTest, ExcessiveChunkLen) {
  const char* inputs[] = {
    "c0000000\r\nhello\r\n"
  };
  RunTestUntilFailure(inputs, arraysize(inputs), 0);
}

TEST(HttpChunkedDecoderTest, BasicExtraData) {
  const char* inputs[] = {
    "5\r\nhello\r\n0\r\n\r\nextra bytes"
  };
  RunTest(inputs, arraysize(inputs), "hello", true, 11);
}

TEST(HttpChunkedDecoderTest, IncrementalExtraData) {
  const char* inputs[] = {
    "5",
    "\r",
    "\n",
    "hello",
    "\r",
    "\n",
    "0",
    "\r",
    "\n",
    "\r",
    "\nextra bytes"
  };
  RunTest(inputs, arraysize(inputs), "hello", true, 11);
}

TEST(HttpChunkedDecoderTest, MultipleExtraDataBlocks) {
  const char* inputs[] = {
    "5\r\nhello\r\n0\r\n\r\nextra",
    " bytes"
  };
  RunTest(inputs, arraysize(inputs), "hello", true, 11);
}

// Test when the line with the chunk length is too long.
TEST(HttpChunkedDecoderTest, LongChunkLengthLine) {
  int big_chunk_length = HttpChunkedDecoder::kMaxLineBufLen;
  scoped_ptr<char[]> big_chunk(new char[big_chunk_length + 1]);
  memset(big_chunk.get(), '0', big_chunk_length);
  big_chunk[big_chunk_length] = 0;
  const char* inputs[] = {
    big_chunk.get(),
    "5"
  };
  RunTestUntilFailure(inputs, arraysize(inputs), 1);
}

// Test when the extension portion of the line with the chunk length is too
// long.
TEST(HttpChunkedDecoderTest, LongLengthLengthLine) {
  int big_chunk_length = HttpChunkedDecoder::kMaxLineBufLen;
  scoped_ptr<char[]> big_chunk(new char[big_chunk_length + 1]);
  memset(big_chunk.get(), '0', big_chunk_length);
  big_chunk[big_chunk_length] = 0;
  const char* inputs[] = {
    "5;",
    big_chunk.get()
  };
  RunTestUntilFailure(inputs, arraysize(inputs), 1);
}

}  // namespace

}  // namespace net
