// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/filter/filter.h"
#include "net/filter/mock_filter_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class FilterTest : public testing::Test {
};

TEST(FilterTest, ContentTypeId) {
  // Check for basic translation of Content-Encoding, including case variations.
  EXPECT_EQ(Filter::FILTER_TYPE_DEFLATE,
            Filter::ConvertEncodingToType("deflate"));
  EXPECT_EQ(Filter::FILTER_TYPE_DEFLATE,
            Filter::ConvertEncodingToType("deflAte"));
  EXPECT_EQ(Filter::FILTER_TYPE_GZIP,
            Filter::ConvertEncodingToType("gzip"));
  EXPECT_EQ(Filter::FILTER_TYPE_GZIP,
            Filter::ConvertEncodingToType("GzIp"));
  EXPECT_EQ(Filter::FILTER_TYPE_GZIP,
            Filter::ConvertEncodingToType("x-gzip"));
  EXPECT_EQ(Filter::FILTER_TYPE_GZIP,
            Filter::ConvertEncodingToType("X-GzIp"));
  EXPECT_EQ(Filter::FILTER_TYPE_SDCH,
            Filter::ConvertEncodingToType("sdch"));
  EXPECT_EQ(Filter::FILTER_TYPE_SDCH,
            Filter::ConvertEncodingToType("sDcH"));
  EXPECT_EQ(Filter::FILTER_TYPE_UNSUPPORTED,
            Filter::ConvertEncodingToType("weird"));
  EXPECT_EQ(Filter::FILTER_TYPE_UNSUPPORTED,
            Filter::ConvertEncodingToType("strange"));
}

// Check various fixups that modify content encoding lists.
TEST(FilterTest, ApacheGzip) {
  MockFilterContext filter_context;
  filter_context.SetSdchResponse(false);

  // Check that redundant gzip mime type removes only solo gzip encoding.
  const std::string kGzipMime1("application/x-gzip");
  const std::string kGzipMime2("application/gzip");
  const std::string kGzipMime3("application/x-gunzip");
  std::vector<Filter::FilterType> encoding_types;

  // First show it removes the gzip, given any gzip style mime type.
  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_GZIP);
  filter_context.SetMimeType(kGzipMime1);
  Filter::FixupEncodingTypes(filter_context, &encoding_types);
  EXPECT_TRUE(encoding_types.empty());

  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_GZIP);
  filter_context.SetMimeType(kGzipMime2);
  Filter::FixupEncodingTypes(filter_context, &encoding_types);
  EXPECT_TRUE(encoding_types.empty());

  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_GZIP);
  filter_context.SetMimeType(kGzipMime3);
  Filter::FixupEncodingTypes(filter_context, &encoding_types);
  EXPECT_TRUE(encoding_types.empty());

  // Check to be sure it doesn't remove everything when it has such a type.
  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_SDCH);
  filter_context.SetMimeType(kGzipMime1);
  Filter::FixupEncodingTypes(filter_context, &encoding_types);
  ASSERT_EQ(1U, encoding_types.size());
  EXPECT_EQ(Filter::FILTER_TYPE_SDCH, encoding_types.front());

  // Check to be sure that gzip can survive with other mime types.
  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_GZIP);
  filter_context.SetMimeType("other/mime");
  Filter::FixupEncodingTypes(filter_context, &encoding_types);
  ASSERT_EQ(1U, encoding_types.size());
  EXPECT_EQ(Filter::FILTER_TYPE_GZIP, encoding_types.front());
}

TEST(FilterTest, GzipContentDispositionFilename) {
  MockFilterContext filter_context;
  filter_context.SetSdchResponse(false);

  const std::string kGzipMime("application/x-tar");
  const std::string kContentDisposition("attachment; filename=\"foo.tgz\"");
  const std::string kURL("http://foo.com/getfoo.php");
  std::vector<Filter::FilterType> encoding_types;

  encoding_types.push_back(Filter::FILTER_TYPE_GZIP);
  filter_context.SetMimeType(kGzipMime);
  filter_context.SetURL(GURL(kURL));
  filter_context.SetContentDisposition(kContentDisposition);
  Filter::FixupEncodingTypes(filter_context, &encoding_types);
  ASSERT_EQ(0U, encoding_types.size());
}

TEST(FilterTest, SdchEncoding) {
  // Handle content encodings including SDCH.
  const std::string kTextHtmlMime("text/html");
  MockFilterContext filter_context;
  filter_context.SetSdchResponse(true);

  std::vector<Filter::FilterType> encoding_types;

  // Check for most common encoding, and verify it survives unchanged.
  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_SDCH);
  encoding_types.push_back(Filter::FILTER_TYPE_GZIP);
  filter_context.SetMimeType(kTextHtmlMime);
  Filter::FixupEncodingTypes(filter_context, &encoding_types);
  ASSERT_EQ(2U, encoding_types.size());
  EXPECT_EQ(Filter::FILTER_TYPE_SDCH, encoding_types[0]);
  EXPECT_EQ(Filter::FILTER_TYPE_GZIP, encoding_types[1]);

  // Unchanged even with other mime types.
  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_SDCH);
  encoding_types.push_back(Filter::FILTER_TYPE_GZIP);
  filter_context.SetMimeType("other/type");
  Filter::FixupEncodingTypes(filter_context, &encoding_types);
  ASSERT_EQ(2U, encoding_types.size());
  EXPECT_EQ(Filter::FILTER_TYPE_SDCH, encoding_types[0]);
  EXPECT_EQ(Filter::FILTER_TYPE_GZIP, encoding_types[1]);

  // Solo SDCH is extended to include optional gunzip.
  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_SDCH);
  Filter::FixupEncodingTypes(filter_context, &encoding_types);
  ASSERT_EQ(2U, encoding_types.size());
  EXPECT_EQ(Filter::FILTER_TYPE_SDCH, encoding_types[0]);
  EXPECT_EQ(Filter::FILTER_TYPE_GZIP_HELPING_SDCH, encoding_types[1]);
}

TEST(FilterTest, MissingSdchEncoding) {
  // Handle interesting case where entire SDCH encoding assertion "got lost."
  const std::string kTextHtmlMime("text/html");
  MockFilterContext filter_context;
  filter_context.SetSdchResponse(true);

  std::vector<Filter::FilterType> encoding_types;

  // Loss of encoding, but it was an SDCH response with html type.
  encoding_types.clear();
  filter_context.SetMimeType(kTextHtmlMime);
  Filter::FixupEncodingTypes(filter_context, &encoding_types);
  ASSERT_EQ(2U, encoding_types.size());
  EXPECT_EQ(Filter::FILTER_TYPE_SDCH_POSSIBLE, encoding_types[0]);
  EXPECT_EQ(Filter::FILTER_TYPE_GZIP_HELPING_SDCH, encoding_types[1]);

  // Loss of encoding, but it was an SDCH response with a prefix that says it
  // was an html type.  Note that it *should* be the case that a precise match
  // with "text/html" we be collected by GetMimeType() and passed in, but we
  // coded the fixup defensively (scanning for a prefix of "text/html", so this
  // is an example which could survive such confusion in the caller).
  encoding_types.clear();
  filter_context.SetMimeType("text/html; charset=UTF-8");
  Filter::FixupEncodingTypes(filter_context, &encoding_types);
  ASSERT_EQ(2U, encoding_types.size());
  EXPECT_EQ(Filter::FILTER_TYPE_SDCH_POSSIBLE, encoding_types[0]);
  EXPECT_EQ(Filter::FILTER_TYPE_GZIP_HELPING_SDCH, encoding_types[1]);

  // No encoding, but it was an SDCH response with non-html type.
  encoding_types.clear();
  filter_context.SetMimeType("other/mime");
  Filter::FixupEncodingTypes(filter_context, &encoding_types);
  ASSERT_EQ(2U, encoding_types.size());
  EXPECT_EQ(Filter::FILTER_TYPE_SDCH_POSSIBLE, encoding_types[0]);
  EXPECT_EQ(Filter::FILTER_TYPE_GZIP_HELPING_SDCH, encoding_types[1]);
}

TEST(FilterTest, Svgz) {
  MockFilterContext filter_context;

  // Check that svgz files are only decompressed when not downloading.
  const std::string kSvgzMime("image/svg+xml");
  const std::string kSvgzUrl("http://ignore.com/foo.svgz");
  const std::string kSvgUrl("http://ignore.com/foo.svg");
  std::vector<Filter::FilterType> encoding_types;

  // Test svgz extension
  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_GZIP);
  filter_context.SetDownload(false);
  filter_context.SetMimeType(kSvgzMime);
  filter_context.SetURL(GURL(kSvgzUrl));
  Filter::FixupEncodingTypes(filter_context, &encoding_types);
  ASSERT_EQ(1U, encoding_types.size());
  EXPECT_EQ(Filter::FILTER_TYPE_GZIP, encoding_types.front());

  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_GZIP);
  filter_context.SetDownload(true);
  filter_context.SetMimeType(kSvgzMime);
  filter_context.SetURL(GURL(kSvgzUrl));
  Filter::FixupEncodingTypes(filter_context, &encoding_types);
  EXPECT_TRUE(encoding_types.empty());

  // Test svg extension
  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_GZIP);
  filter_context.SetDownload(false);
  filter_context.SetMimeType(kSvgzMime);
  filter_context.SetURL(GURL(kSvgUrl));
  Filter::FixupEncodingTypes(filter_context, &encoding_types);
  ASSERT_EQ(1U, encoding_types.size());
  EXPECT_EQ(Filter::FILTER_TYPE_GZIP, encoding_types.front());

  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_GZIP);
  filter_context.SetDownload(true);
  filter_context.SetMimeType(kSvgzMime);
  filter_context.SetURL(GURL(kSvgUrl));
  Filter::FixupEncodingTypes(filter_context, &encoding_types);
  ASSERT_EQ(1U, encoding_types.size());
  EXPECT_EQ(Filter::FILTER_TYPE_GZIP, encoding_types.front());
}

TEST(FilterTest, UnsupportedMimeGzip) {
  // From issue 8170 - handling files with Content-Encoding: x-gzip
  MockFilterContext filter_context;
  std::vector<Filter::FilterType> encoding_types;
  const std::string kTarMime("application/x-tar");
  const std::string kCpioMime("application/x-cpio");
  const std::string kTarUrl("http://ignore.com/foo.tar");
  const std::string kTargzUrl("http://ignore.com/foo.tar.gz");
  const std::string kTgzUrl("http://ignore.com/foo.tgz");
  const std::string kBadTgzUrl("http://ignore.com/foo.targz");
  const std::string kUrl("http://ignore.com/foo");

  // Firefox 3 does not decompress when we have unsupported mime types for
  // certain filenames.
  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_GZIP);
  filter_context.SetDownload(false);
  filter_context.SetMimeType(kTarMime);
  filter_context.SetURL(GURL(kTargzUrl));
  Filter::FixupEncodingTypes(filter_context, &encoding_types);
  EXPECT_TRUE(encoding_types.empty());

  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_GZIP);
  filter_context.SetDownload(false);
  filter_context.SetMimeType(kTarMime);
  filter_context.SetURL(GURL(kTgzUrl));
  Filter::FixupEncodingTypes(filter_context, &encoding_types);
  EXPECT_TRUE(encoding_types.empty());

  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_GZIP);
  filter_context.SetDownload(false);
  filter_context.SetMimeType(kCpioMime);
  filter_context.SetURL(GURL(kTgzUrl));
  Filter::FixupEncodingTypes(filter_context, &encoding_types);
  EXPECT_TRUE(encoding_types.empty());

  // Same behavior for downloads.
  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_GZIP);
  filter_context.SetDownload(true);
  filter_context.SetMimeType(kCpioMime);
  filter_context.SetURL(GURL(kTgzUrl));
  Filter::FixupEncodingTypes(filter_context, &encoding_types);
  EXPECT_TRUE(encoding_types.empty());

  // Unsupported mime type with wrong file name, decompressed.
  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_GZIP);
  filter_context.SetDownload(false);
  filter_context.SetMimeType(kTarMime);
  filter_context.SetURL(GURL(kUrl));
  Filter::FixupEncodingTypes(filter_context, &encoding_types);
  ASSERT_EQ(1U, encoding_types.size());
  EXPECT_EQ(Filter::FILTER_TYPE_GZIP, encoding_types.front());

  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_GZIP);
  filter_context.SetDownload(false);
  filter_context.SetMimeType(kTarMime);
  filter_context.SetURL(GURL(kTarUrl));
  Filter::FixupEncodingTypes(filter_context, &encoding_types);
  ASSERT_EQ(1U, encoding_types.size());
  EXPECT_EQ(Filter::FILTER_TYPE_GZIP, encoding_types.front());

  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_GZIP);
  filter_context.SetDownload(false);
  filter_context.SetMimeType(kTarMime);
  filter_context.SetURL(GURL(kBadTgzUrl));
  Filter::FixupEncodingTypes(filter_context, &encoding_types);
  ASSERT_EQ(1U, encoding_types.size());
  EXPECT_EQ(Filter::FILTER_TYPE_GZIP, encoding_types.front());

  // Same behavior for downloads.
  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_GZIP);
  filter_context.SetDownload(true);
  filter_context.SetMimeType(kTarMime);
  filter_context.SetURL(GURL(kBadTgzUrl));
  Filter::FixupEncodingTypes(filter_context, &encoding_types);
  ASSERT_EQ(1U, encoding_types.size());
  EXPECT_EQ(Filter::FILTER_TYPE_GZIP, encoding_types.front());
}

TEST(FilterTest, SupportedMimeGzip) {
  // From issue 16430 - Files with supported mime types should be decompressed,
  // even though these files end in .gz/.tgz.
  MockFilterContext filter_context;
  std::vector<Filter::FilterType> encoding_types;
  const std::string kGzUrl("http://ignore.com/foo.gz");
  const std::string kUrl("http://ignore.com/foo");
  const std::string kHtmlMime("text/html");
  const std::string kJavascriptMime("text/javascript");

  // For files that does not end in .gz/.tgz, we always decompress.
  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_GZIP);
  filter_context.SetDownload(false);
  filter_context.SetMimeType(kHtmlMime);
  filter_context.SetURL(GURL(kUrl));
  Filter::FixupEncodingTypes(filter_context, &encoding_types);
  ASSERT_EQ(1U, encoding_types.size());
  EXPECT_EQ(Filter::FILTER_TYPE_GZIP, encoding_types.front());

  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_GZIP);
  filter_context.SetDownload(true);
  filter_context.SetMimeType(kHtmlMime);
  filter_context.SetURL(GURL(kUrl));
  Filter::FixupEncodingTypes(filter_context, &encoding_types);
  ASSERT_EQ(1U, encoding_types.size());
  EXPECT_EQ(Filter::FILTER_TYPE_GZIP, encoding_types.front());

  // And also decompress files that end in .gz/.tgz.
  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_GZIP);
  filter_context.SetDownload(false);
  filter_context.SetMimeType(kHtmlMime);
  filter_context.SetURL(GURL(kGzUrl));
  Filter::FixupEncodingTypes(filter_context, &encoding_types);
  ASSERT_EQ(1U, encoding_types.size());
  EXPECT_EQ(Filter::FILTER_TYPE_GZIP, encoding_types.front());

  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_GZIP);
  filter_context.SetDownload(false);
  filter_context.SetMimeType(kJavascriptMime);
  filter_context.SetURL(GURL(kGzUrl));
  Filter::FixupEncodingTypes(filter_context, &encoding_types);
  ASSERT_EQ(1U, encoding_types.size());
  EXPECT_EQ(Filter::FILTER_TYPE_GZIP, encoding_types.front());

  // Except on downloads, where they just get saved.
  encoding_types.clear();
  encoding_types.push_back(Filter::FILTER_TYPE_GZIP);
  filter_context.SetDownload(true);
  filter_context.SetMimeType(kHtmlMime);
  filter_context.SetURL(GURL(kGzUrl));
  Filter::FixupEncodingTypes(filter_context, &encoding_types);
  EXPECT_TRUE(encoding_types.empty());
}

}  // namespace net
