// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/hpack_decoder.h"

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "net/spdy/hpack_encoder.h"
#include "net/spdy/hpack_input_stream.h"
#include "net/spdy/hpack_output_stream.h"
#include "net/spdy/spdy_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace test {

using base::StringPiece;
using std::string;

class HpackDecoderPeer {
 public:
  explicit HpackDecoderPeer(HpackDecoder* decoder)
      : decoder_(decoder) {}

  void HandleHeaderRepresentation(StringPiece name, StringPiece value) {
    decoder_->HandleHeaderRepresentation(name, value);
  }
  bool DecodeNextName(HpackInputStream* in, StringPiece* out) {
    return decoder_->DecodeNextName(in, out);
  }
  HpackHeaderTable* header_table() {
    return &decoder_->header_table_;
  }
  void set_cookie_value(string value) {
    decoder_->cookie_value_ = value;
  }
  string cookie_value() {
    return decoder_->cookie_value_;
  }
  const std::map<string, string>& decoded_block() const {
    return decoder_->decoded_block_;
  }
  const string& headers_block_buffer() const {
    return decoder_->headers_block_buffer_;
  }

 private:
  HpackDecoder* decoder_;
};

}  // namespace test

namespace {

using base::StringPiece;
using std::string;
using test::a2b_hex;

using testing::ElementsAre;
using testing::Pair;

const size_t kLiteralBound = 1024;

class HpackDecoderTest : public ::testing::Test {
 protected:
  HpackDecoderTest()
      : decoder_(ObtainHpackHuffmanTable()),
        decoder_peer_(&decoder_) {}

  bool DecodeHeaderBlock(StringPiece str) {
    return decoder_.HandleControlFrameHeadersData(0, str.data(), str.size()) &&
        decoder_.HandleControlFrameHeadersComplete(0);
  }

  const std::map<string, string>& decoded_block() const {
    // TODO(jgraettinger): HpackDecoderTest should implement
    // SpdyHeadersHandlerInterface, and collect headers for examination.
    return decoder_peer_.decoded_block();
  }

  const std::map<string, string>& DecodeBlockExpectingSuccess(StringPiece str) {
    EXPECT_TRUE(DecodeHeaderBlock(str));
    return decoded_block();
  }

  void expectEntry(size_t index, size_t size, const string& name,
                   const string& value) {
    HpackEntry* entry = decoder_peer_.header_table()->GetByIndex(index);
    EXPECT_EQ(name, entry->name()) << "index " << index;
    EXPECT_EQ(value, entry->value());
    EXPECT_EQ(size, entry->Size());
    EXPECT_EQ(index, decoder_peer_.header_table()->IndexOf(entry));
  }

  void expectStaticEntry(size_t index) {
    HpackEntry* entry = decoder_peer_.header_table()->GetByIndex(index);
    EXPECT_TRUE(entry->IsStatic()) << "index " << index;
  }

  HpackDecoder decoder_;
  test::HpackDecoderPeer decoder_peer_;
};

TEST_F(HpackDecoderTest, HandleControlFrameHeadersData) {
  // Strings under threshold are concatenated in the buffer.
  EXPECT_TRUE(decoder_.HandleControlFrameHeadersData(
      0, "small string one", 16));
  EXPECT_TRUE(decoder_.HandleControlFrameHeadersData(
      0, "small string two", 16));
  // A string which would push the buffer over the threshold is refused.
  EXPECT_FALSE(decoder_.HandleControlFrameHeadersData(
      0, "fails", kMaxDecodeBufferSize - 32 + 1));

  EXPECT_EQ(decoder_peer_.headers_block_buffer(),
            "small string onesmall string two");
}

TEST_F(HpackDecoderTest, HandleControlFrameHeadersComplete) {
  // Decode a block which toggles two static headers into the reference set.
  EXPECT_TRUE(DecodeHeaderBlock("\x82\x86"));

  decoder_peer_.set_cookie_value("foobar=baz");

  // Headers in the reference set should be emitted.
  // Incremental cookie buffer should be emitted and cleared.
  decoder_.HandleControlFrameHeadersData(0, NULL, 0);
  decoder_.HandleControlFrameHeadersComplete(0);

  EXPECT_THAT(decoded_block(), ElementsAre(
      Pair(":method", "GET"),
      Pair(":path", "/index.html"),
      Pair("cookie", "foobar=baz")));
  EXPECT_EQ(decoder_peer_.cookie_value(), "");
}

TEST_F(HpackDecoderTest, HandleHeaderRepresentation) {
  // All cookie crumbs are joined.
  decoder_peer_.HandleHeaderRepresentation("cookie", " part 1");
  decoder_peer_.HandleHeaderRepresentation("cookie", "part 2 ");
  decoder_peer_.HandleHeaderRepresentation("cookie", "part3");

  // Already-delimited headers are passed through.
  decoder_peer_.HandleHeaderRepresentation("passed-through",
                                           string("foo\0baz", 7));

  // Other headers are joined on \0. Case matters.
  decoder_peer_.HandleHeaderRepresentation("joined", "not joined");
  decoder_peer_.HandleHeaderRepresentation("joineD", "value 1");
  decoder_peer_.HandleHeaderRepresentation("joineD", "value 2");

  // Empty headers remain empty.
  decoder_peer_.HandleHeaderRepresentation("empty", "");

  // Joined empty headers work as expected.
  decoder_peer_.HandleHeaderRepresentation("empty-joined", "");
  decoder_peer_.HandleHeaderRepresentation("empty-joined", "foo");
  decoder_peer_.HandleHeaderRepresentation("empty-joined", "");
  decoder_peer_.HandleHeaderRepresentation("empty-joined", "");

  // Non-contiguous cookie crumb.
  decoder_peer_.HandleHeaderRepresentation("cookie", " fin!");

  // Finish and emit all headers.
  decoder_.HandleControlFrameHeadersComplete(0);

  EXPECT_THAT(decoded_block(), ElementsAre(
      Pair("cookie", " part 1; part 2 ; part3;  fin!"),
      Pair("empty", ""),
      Pair("empty-joined", string("\0foo\0\0", 6)),
      Pair("joineD", string("value 1\0value 2", 15)),
      Pair("joined", "not joined"),
      Pair("passed-through", string("foo\0baz", 7))));
}

// Decoding an encoded name with a valid string literal should work.
TEST_F(HpackDecoderTest, DecodeNextNameLiteral) {
  HpackInputStream input_stream(kLiteralBound, StringPiece("\x00\x04name", 6));

  StringPiece string_piece;
  EXPECT_TRUE(decoder_peer_.DecodeNextName(&input_stream, &string_piece));
  EXPECT_EQ("name", string_piece);
  EXPECT_FALSE(input_stream.HasMoreData());
}

TEST_F(HpackDecoderTest, DecodeNextNameLiteralWithHuffmanEncoding) {
  string input = a2b_hex("0088571c5cdb737b2faf");
  HpackInputStream input_stream(kLiteralBound, input);

  StringPiece string_piece;
  EXPECT_TRUE(decoder_peer_.DecodeNextName(&input_stream, &string_piece));
  EXPECT_EQ("custom-key", string_piece);
  EXPECT_FALSE(input_stream.HasMoreData());
}

// Decoding an encoded name with a valid index should work.
TEST_F(HpackDecoderTest, DecodeNextNameIndexed) {
  HpackInputStream input_stream(kLiteralBound, "\x01");

  StringPiece string_piece;
  EXPECT_TRUE(decoder_peer_.DecodeNextName(&input_stream, &string_piece));
  EXPECT_EQ(":authority", string_piece);
  EXPECT_FALSE(input_stream.HasMoreData());
}

// Decoding an encoded name with an invalid index should fail.
TEST_F(HpackDecoderTest, DecodeNextNameInvalidIndex) {
  // One more than the number of static table entries.
  HpackInputStream input_stream(kLiteralBound, "\x3e");

  StringPiece string_piece;
  EXPECT_FALSE(decoder_peer_.DecodeNextName(&input_stream, &string_piece));
}

// Decoding an indexed header should toggle the index's presence in
// the reference set, making a copy of static table entries if
// necessary. It should also emit the header if toggled on (and only
// as many times as it was toggled on).
TEST_F(HpackDecoderTest, IndexedHeaderBasic) {
  // Toggle on static table entry #2 (and make a copy at index #1),
  // then toggle on static table entry #5 (which is now #6 because of
  // the copy of #2).
  std::map<string, string> header_set1 =
      DecodeBlockExpectingSuccess("\x82\x86");
  std::map<string, string> expected_header_set1;
  expected_header_set1[":method"] = "GET";
  expected_header_set1[":path"] = "/index.html";
  EXPECT_EQ(expected_header_set1, header_set1);

  std::map<string, string> expected_header_set2;
  expected_header_set2[":path"] = "/index.html";
  // Toggle off the copy of static table entry #5.
  std::map<string, string> header_set2 =
      DecodeBlockExpectingSuccess("\x82");
  EXPECT_EQ(expected_header_set2, header_set2);
}

// Test a too-large indexed header.
TEST_F(HpackDecoderTest, InvalidIndexedHeader) {
  // High-bit set, and a prefix of one more than the number of static entries.
  EXPECT_FALSE(DecodeHeaderBlock(StringPiece("\xbe", 1)));
}

TEST_F(HpackDecoderTest, ContextUpdateMaximumSize) {
  EXPECT_EQ(kDefaultHeaderTableSizeSetting,
            decoder_peer_.header_table()->max_size());
  string input;
  {
    // Maximum-size update with size 126. Succeeds.
    HpackOutputStream output_stream;
    output_stream.AppendPrefix(kEncodingContextOpcode);
    output_stream.AppendPrefix(kEncodingContextNewMaximumSize);
    output_stream.AppendUint32(126);

    output_stream.TakeString(&input);
    EXPECT_TRUE(DecodeHeaderBlock(StringPiece(input)));
    EXPECT_EQ(126u, decoder_peer_.header_table()->max_size());
  }
  {
    // Maximum-size update with kDefaultHeaderTableSizeSetting. Succeeds.
    HpackOutputStream output_stream;
    output_stream.AppendPrefix(kEncodingContextOpcode);
    output_stream.AppendPrefix(kEncodingContextNewMaximumSize);
    output_stream.AppendUint32(kDefaultHeaderTableSizeSetting);

    output_stream.TakeString(&input);
    EXPECT_TRUE(DecodeHeaderBlock(StringPiece(input)));
    EXPECT_EQ(kDefaultHeaderTableSizeSetting,
              decoder_peer_.header_table()->max_size());
  }
  {
    // Maximum-size update with kDefaultHeaderTableSizeSetting + 1. Fails.
    HpackOutputStream output_stream;
    output_stream.AppendPrefix(kEncodingContextOpcode);
    output_stream.AppendPrefix(kEncodingContextNewMaximumSize);
    output_stream.AppendUint32(kDefaultHeaderTableSizeSetting + 1);

    output_stream.TakeString(&input);
    EXPECT_FALSE(DecodeHeaderBlock(StringPiece(input)));
    EXPECT_EQ(kDefaultHeaderTableSizeSetting,
              decoder_peer_.header_table()->max_size());
  }
}

TEST_F(HpackDecoderTest, ContextUpdateClearReferenceSet) {
  // Toggle on a couple of headers.
  std::map<string, string> header_set1 =
      DecodeBlockExpectingSuccess("\x82\x86");
  std::map<string, string> expected_header_set1;
  expected_header_set1[":method"] = "GET";
  expected_header_set1[":path"] = "/index.html";
  EXPECT_EQ(expected_header_set1, header_set1);

  // Send a context update to clear the reference set.
  std::map<string, string> header_set2 =
      DecodeBlockExpectingSuccess("\x30");
  std::map<string, string> expected_header_set2;
  EXPECT_EQ(expected_header_set2, header_set2);
}

// Decoding two valid encoded literal headers with no indexing should
// work.
TEST_F(HpackDecoderTest, LiteralHeaderNoIndexing) {
  // First header with indexed name, second header with string literal
  // name.
  const char input[] = "\x04\x0c/sample/path\x00\x06:path2\x0e/sample/path/2";
  std::map<string, string> header_set =
      DecodeBlockExpectingSuccess(StringPiece(input, arraysize(input) - 1));

  std::map<string, string> expected_header_set;
  expected_header_set[":path"] = "/sample/path";
  expected_header_set[":path2"] = "/sample/path/2";
  EXPECT_EQ(expected_header_set, header_set);
}

// Decoding two valid encoded literal headers with incremental
// indexing and string literal names should work and add the headers
// to the reference set.
TEST_F(HpackDecoderTest, LiteralHeaderIncrementalIndexing) {
  const char input[] = "\x44\x0c/sample/path\x40\x06:path2\x0e/sample/path/2";
  std::map<string, string> header_set =
      DecodeBlockExpectingSuccess(StringPiece(input, arraysize(input) - 1));

  std::map<string, string> expected_header_set;
  expected_header_set[":path"] = "/sample/path";
  expected_header_set[":path2"] = "/sample/path/2";
  EXPECT_EQ(expected_header_set, header_set);

  // Decoding an empty string should just return the reference set.
  std::map<string, string> header_set2 = DecodeBlockExpectingSuccess("");
  EXPECT_EQ(expected_header_set, header_set2);
}

TEST_F(HpackDecoderTest, LiteralHeaderWithIndexingInvalidNameIndex) {
  decoder_.ApplyHeaderTableSizeSetting(0);

  // Name is the last static index. Works.
  EXPECT_TRUE(DecodeHeaderBlock(StringPiece("\x7d\x03ooo")));
  // Name is one beyond the last static index. Fails.
  EXPECT_FALSE(DecodeHeaderBlock(StringPiece("\x7e\x03ooo")));
}

TEST_F(HpackDecoderTest, LiteralHeaderNoIndexingInvalidNameIndex) {
  // Name is the last static index. Works.
  EXPECT_TRUE(DecodeHeaderBlock(StringPiece("\x0f\x2e\x03ooo")));
  // Name is one beyond the last static index. Fails.
  EXPECT_FALSE(DecodeHeaderBlock(StringPiece("\x0f\x2f\x03ooo")));
}

TEST_F(HpackDecoderTest, LiteralHeaderNeverIndexedInvalidNameIndex) {
  // Name is the last static index. Works.
  EXPECT_TRUE(DecodeHeaderBlock(StringPiece("\x1f\x2e\x03ooo")));
  // Name is one beyond the last static index. Fails.
  EXPECT_FALSE(DecodeHeaderBlock(StringPiece("\x1f\x2f\x03ooo")));
}

// Round-tripping the header set from E.2.1 should work.
TEST_F(HpackDecoderTest, BasicE21) {
  HpackEncoder encoder(ObtainHpackHuffmanTable());

  std::map<string, string> expected_header_set;
  expected_header_set[":method"] = "GET";
  expected_header_set[":scheme"] = "http";
  expected_header_set[":path"] = "/";
  expected_header_set[":authority"] = "www.example.com";

  string encoded_header_set;
  EXPECT_TRUE(encoder.EncodeHeaderSet(
      expected_header_set, &encoded_header_set));

  EXPECT_TRUE(DecodeHeaderBlock(encoded_header_set));
  EXPECT_EQ(expected_header_set, decoded_block());
}

TEST_F(HpackDecoderTest, SectionD3RequestHuffmanExamples) {
  std::map<string, string> header_set;

  // 82                                      | == Indexed - Add ==
  //                                         |   idx = 2
  //                                         | -> :method: GET
  // 87                                      | == Indexed - Add ==
  //                                         |   idx = 7
  //                                         | -> :scheme: http
  // 86                                      | == Indexed - Add ==
  //                                         |   idx = 6
  //                                         | -> :path: /
  // 44                                      | == Literal indexed ==
  //                                         |   Indexed name (idx = 4)
  //                                         |     :authority
  // 8c                                      |   Literal value (len = 15)
  //                                         |     Huffman encoded:
  // e7cf 9beb e89b 6fb1 6fa9 b6ff           | ......o.o...
  //                                         |     Decoded:
  //                                         | www.example.com
  //                                         | -> :authority: www.example.com
  string first = a2b_hex("828786448ce7cf9bebe89b6fb16fa9b6ff");
  header_set = DecodeBlockExpectingSuccess(first);

  EXPECT_THAT(header_set, ElementsAre(
      Pair(":authority", "www.example.com"),
      Pair(":method", "GET"),
      Pair(":path", "/"),
      Pair(":scheme", "http")));

  expectEntry(1, 57, ":authority", "www.example.com");
  expectEntry(2, 38, ":path", "/");
  expectEntry(3, 43, ":scheme", "http");
  expectEntry(4, 42, ":method", "GET");
  expectStaticEntry(5);
  EXPECT_EQ(180u, decoder_peer_.header_table()->size());

  // 5c                                      | == Literal indexed ==
  //                                         |   Indexed name (idx = 28)
  //                                         |     cache-control
  // 86                                      |   Literal value (len = 8)
  //                                         |     Huffman encoded:
  // b9b9 9495 56bf                          | ....V.
  //                                         |     Decoded:
  //                                         | no-cache
  //                                         | -> cache-control: no-cache
  string second = a2b_hex("5c86b9b9949556bf");
  header_set = DecodeBlockExpectingSuccess(second);

  EXPECT_THAT(header_set, ElementsAre(
      Pair(":authority", "www.example.com"),
      Pair(":method", "GET"),
      Pair(":path", "/"),
      Pair(":scheme", "http"),
      Pair("cache-control", "no-cache")));

  expectEntry(1, 53, "cache-control", "no-cache");
  expectEntry(2, 57, ":authority", "www.example.com");
  expectEntry(3, 38, ":path", "/");
  expectEntry(4, 43, ":scheme", "http");
  expectEntry(5, 42, ":method", "GET");
  expectStaticEntry(6);
  EXPECT_EQ(233u, decoder_peer_.header_table()->size());

  // 30                                      | == Empty reference set ==
  //                                         |   idx = 0
  //                                         |   flag = 1
  // 85                                      | == Indexed - Add ==
  //                                         |   idx = 5
  //                                         | -> :method: GET
  // 8c                                      | == Indexed - Add ==
  //                                         |   idx = 12
  //                                         | -> :scheme: https
  // 8b                                      | == Indexed - Add ==
  //                                         |   idx = 11
  //                                         | -> :path: /index.html
  // 84                                      | == Indexed - Add ==
  //                                         |   idx = 4
  //                                         | -> :authority: www.example.com
  // 40                                      | == Literal indexed ==
  // 88                                      |   Literal name (len = 10)
  //                                         |     Huffman encoded:
  // 571c 5cdb 737b 2faf                     | W.\.s{/.
  //                                         |     Decoded:
  //                                         | custom-key
  // 89                                      |   Literal value (len = 12)
  //                                         |     Huffman encoded:
  // 571c 5cdb 7372 4d9c 57                  | W.\.srM.W
  //                                         |     Decoded:
  //                                         | custom-value
  //                                         | -> custom-key: custom-value
  string third = a2b_hex("30858c8b844088571c5cdb737b2faf89"
                         "571c5cdb73724d9c57");
  header_set = DecodeBlockExpectingSuccess(third);

  EXPECT_THAT(header_set, ElementsAre(
      Pair(":authority", "www.example.com"),
      Pair(":method", "GET"),
      Pair(":path", "/index.html"),
      Pair(":scheme", "https"),
      Pair("custom-key", "custom-value")));

  expectEntry(1, 54, "custom-key", "custom-value");
  expectEntry(2, 48, ":path", "/index.html");
  expectEntry(3, 44, ":scheme", "https");
  expectEntry(4, 53, "cache-control", "no-cache");
  expectEntry(5, 57, ":authority", "www.example.com");
  expectEntry(6, 38, ":path", "/");
  expectEntry(7, 43, ":scheme", "http");
  expectEntry(8, 42, ":method", "GET");
  expectStaticEntry(9);
  EXPECT_EQ(379u, decoder_peer_.header_table()->size());
}

TEST_F(HpackDecoderTest, SectionD5ResponseHuffmanExamples) {
  std::map<string, string> header_set;
  decoder_.ApplyHeaderTableSizeSetting(256);

  // 48                                      | == Literal indexed ==
  //                                         |   Indexed name (idx = 8)
  //                                         |     :status
  // 82                                      |   Literal value (len = 3)
  //                                         |     Huffman encoded:
  // 4017                                    | @.
  //                                         |     Decoded:
  //                                         | 302
  //                                         | -> :status: 302
  // 59                                      | == Literal indexed ==
  //                                         |   Indexed name (idx = 25)
  //                                         |     cache-control
  // 85                                      |   Literal value (len = 7)
  //                                         |     Huffman encoded:
  // bf06 724b 97                            | ..rK.
  //                                         |     Decoded:
  //                                         | private
  //                                         | -> cache-control: private
  // 63                                      | == Literal indexed ==
  //                                         |   Indexed name (idx = 35)
  //                                         |     date
  // 93                                      |   Literal value (len = 29)
  //                                         |     Huffman encoded:
  // d6db b298 84de 2a71 8805 0620 9851 3109 | ......*q... .Q1.
  // b56b a3                                 | .k.
  //                                         |     Decoded:
  //                                         | Mon, 21 Oct 2013 20:13:21
  //                                         | GMT
  //                                         | -> date: Mon, 21 Oct 2013
  //                                         |   20:13:21 GMT
  // 71                                      | == Literal indexed ==
  //                                         |   Indexed name (idx = 49)
  //                                         |     location
  // 91                                      |   Literal value (len = 23)
  //                                         |     Huffman encoded:
  // adce bf19 8e7e 7cf9 bebe 89b6 fb16 fa9b | ......|.........
  // 6f                                      | o
  //                                         |     Decoded:
  //                                         | https://www.example.com
  //                                         | -> location: https://www.e
  //                                         |    xample.com
  string first = a2b_hex("488240175985bf06724b976393d6dbb2"
                         "9884de2a718805062098513109b56ba3"
                         "7191adcebf198e7e7cf9bebe89b6fb16"
                         "fa9b6f");
  header_set = DecodeBlockExpectingSuccess(first);

  EXPECT_THAT(header_set, ElementsAre(
      Pair(":status", "302"),
      Pair("cache-control", "private"),
      Pair("date", "Mon, 21 Oct 2013 20:13:21 GMT"),
      Pair("location", "https://www.example.com")));

  expectEntry(1, 63, "location", "https://www.example.com");
  expectEntry(2, 65, "date", "Mon, 21 Oct 2013 20:13:21 GMT");
  expectEntry(3, 52, "cache-control", "private");
  expectEntry(4, 42, ":status", "302");
  expectStaticEntry(5);
  EXPECT_EQ(222u, decoder_peer_.header_table()->size());

  // 8c                                      | == Indexed - Add ==
  //                                         |   idx = 12
  //                                         | - evict: :status: 302
  //                                         | -> :status: 200
  string second = a2b_hex("8c");
  header_set = DecodeBlockExpectingSuccess(second);

  EXPECT_THAT(header_set, ElementsAre(
      Pair(":status", "200"),
      Pair("cache-control", "private"),
      Pair("date", "Mon, 21 Oct 2013 20:13:21 GMT"),
      Pair("location", "https://www.example.com")));

  expectEntry(1, 42, ":status", "200");
  expectEntry(2, 63, "location", "https://www.example.com");
  expectEntry(3, 65, "date", "Mon, 21 Oct 2013 20:13:21 GMT");
  expectEntry(4, 52, "cache-control", "private");
  expectStaticEntry(5);
  EXPECT_EQ(222u, decoder_peer_.header_table()->size());

  // 84                                      | == Indexed - Remove ==
  //                                         |   idx = 4
  //                                         | -> cache-control: private
  // 84                                      | == Indexed - Add ==
  //                                         |   idx = 4
  //                                         | -> cache-control: private
  // 43                                      | == Literal indexed ==
  //                                         |   Indexed name (idx = 3)
  //                                         |     date
  // 93                                      |   Literal value (len = 29)
  //                                         |     Huffman encoded:
  // d6db b298 84de 2a71 8805 0620 9851 3111 | ......*q... .Q1.
  // b56b a3                                 | .k.
  //                                         |     Decoded:
  //                                         | Mon, 21 Oct 2013 20:13:22
  //                                         | GMT
  //                                         | - evict: cache-control: pr
  //                                         |   ivate
  //                                         | -> date: Mon, 21 Oct 2013
  //                                         |   20:13:22 GMT
  // 5e                                      | == Literal indexed ==
  //                                         |   Indexed name (idx = 30)
  //                                         |     content-encoding
  // 84                                      |   Literal value (len = 4)
  //                                         |     Huffman encoded:
  // abdd 97ff                               | ....
  //                                         |     Decoded:
  //                                         | gzip
  //                                         | - evict: date: Mon, 21 Oct
  //                                         |    2013 20:13:21 GMT
  //                                         | -> content-encoding: gzip
  // 84                                      | == Indexed - Remove ==
  //                                         |   idx = 4
  //                                         | -> location: https://www.e
  //                                         |   xample.com
  // 84                                      | == Indexed - Add ==
  //                                         |   idx = 4
  //                                         | -> location: https://www.e
  //                                         |   xample.com
  // 83                                      | == Indexed - Remove ==
  //                                         |   idx = 3
  //                                         | -> :status: 200
  // 83                                      | == Indexed - Add ==
  //                                         |   idx = 3
  //                                         | -> :status: 200
  // 7b                                      | == Literal indexed ==
  //                                         |   Indexed name (idx = 59)
  //                                         |     set-cookie
  // b1                                      |   Literal value (len = 56)
  //                                         |     Huffman encoded:
  // e0d6 cf9f 6e8f 9fd3 e5f6 fa76 fefd 3c7e | ....n......v....
  // df9e ff1f 2f0f 3cfe 9f6f cf7f 8f87 9f61 | ..../....o.....a
  // ad4f 4cc9 a973 a220 0ec3 725e 18b1 b74e | .OL..s. ..r^...N
  // 3f                                      | ?
  //                                         |     Decoded:
  //                                         | foo=ASDJKHQKBZXOQWEOPIUAXQ
  //                                         | WEOIU; max-age=3600; versi
  //                                         | on=1
  //                                         | - evict: location: https:/
  //                                         |   /www.example.com
  //                                         | - evict: :status: 200
  //                                         | -> set-cookie: foo=ASDJKHQ
  //                                         |   KBZXOQWEOPIUAXQWEOIU; ma
  //                                         |   x-age=3600; version=1
  string third = a2b_hex("84844393d6dbb29884de2a7188050620"
                         "98513111b56ba35e84abdd97ff848483"
                         "837bb1e0d6cf9f6e8f9fd3e5f6fa76fe"
                         "fd3c7edf9eff1f2f0f3cfe9f6fcf7f8f"
                         "879f61ad4f4cc9a973a2200ec3725e18"
                         "b1b74e3f");
  header_set = DecodeBlockExpectingSuccess(third);

  EXPECT_THAT(header_set, ElementsAre(
      Pair(":status", "200"),
      Pair("cache-control", "private"),
      Pair("content-encoding", "gzip"),
      Pair("date", "Mon, 21 Oct 2013 20:13:22 GMT"),
      Pair("location", "https://www.example.com"),
      Pair("set-cookie", "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU;"
           " max-age=3600; version=1")));

  expectEntry(1, 98, "set-cookie", "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU;"
              " max-age=3600; version=1");
  expectEntry(2, 52, "content-encoding", "gzip");
  expectEntry(3, 65, "date", "Mon, 21 Oct 2013 20:13:22 GMT");
  expectStaticEntry(4);
  EXPECT_EQ(215u, decoder_peer_.header_table()->size());
}

}  // namespace

}  // namespace net
