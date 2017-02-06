// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/network/LinkHeader.h"

#include "testing/gtest/include/gtest/gtest.h"
#include <base/macros.h>

namespace blink {
namespace {

TEST(LinkHeaderTest, Empty)
{
    String nullString;
    LinkHeaderSet nullHeaderSet(nullString);
    ASSERT_EQ(nullHeaderSet.size(), unsigned(0));
    String emptyString("");
    LinkHeaderSet emptyHeaderSet(emptyString);
    ASSERT_EQ(emptyHeaderSet.size(), unsigned(0));
}

struct SingleTestCase {
    const char* headerValue;
    bool valid;
    const char* url;
    const char* rel;
    const char* as;
    const char* media;
} singleTestCases[] = {
    {"</images/cat.jpg>; rel=prefetch", true, "/images/cat.jpg", "prefetch", "", ""},
    {"</images/cat.jpg>;rel=prefetch", true, "/images/cat.jpg", "prefetch", "", ""},
    {"</images/cat.jpg>   ;rel=prefetch", true, "/images/cat.jpg", "prefetch", "", ""},
    {"</images/cat.jpg>   ;   rel=prefetch", true, "/images/cat.jpg", "prefetch", "", ""},
    {"< /images/cat.jpg>   ;   rel=prefetch", true, "/images/cat.jpg", "prefetch", "", ""},
    {"</images/cat.jpg >   ;   rel=prefetch", true, "/images/cat.jpg", "prefetch", "", ""},
    {"</images/cat.jpg wutwut>   ;   rel=prefetch", true, "/images/cat.jpg wutwut", "prefetch", "", ""},
    {"</images/cat.jpg wutwut  \t >   ;   rel=prefetch", true, "/images/cat.jpg wutwut", "prefetch", "", ""},
    {"</images/cat.jpg>; rel=prefetch   ", true, "/images/cat.jpg", "prefetch", "", ""},
    {"</images/cat.jpg>; Rel=prefetch   ", true, "/images/cat.jpg", "prefetch", "", ""},
    {"</images/cat.jpg>; Rel=PReFetCh   ", true, "/images/cat.jpg", "prefetch", "", ""},
    {"</images/cat.jpg>; rel=prefetch; rel=somethingelse", true, "/images/cat.jpg", "prefetch", "", ""},
    {"  </images/cat.jpg>; rel=prefetch   ", true, "/images/cat.jpg", "prefetch", "", ""},
    {"\t  </images/cat.jpg>; rel=prefetch   ", true, "/images/cat.jpg", "prefetch", "", ""},
    {"</images/cat.jpg>\t\t ; \trel=prefetch \t  ", true, "/images/cat.jpg", "prefetch", "", ""},
    {"\f</images/cat.jpg>\t\t ; \trel=prefetch \t  ", false},
    {"</images/cat.jpg>; rel= prefetch", true, "/images/cat.jpg", "prefetch", "", ""},
    {"<../images/cat.jpg?dog>; rel= prefetch", true, "../images/cat.jpg?dog", "prefetch", "", ""},
    {"</images/cat.jpg>; rel =prefetch", true, "/images/cat.jpg", "prefetch", "", ""},
    {"</images/cat.jpg>; rel pel=prefetch", false},
    {"< /images/cat.jpg>", true, "/images/cat.jpg", "", "", ""},
    {"</images/cat.jpg>; rel =", false},
    {"</images/cat.jpg>; wut=sup; rel =prefetch", true, "/images/cat.jpg", "prefetch", "", ""},
    {"</images/cat.jpg>; wut=sup ; rel =prefetch", true, "/images/cat.jpg", "prefetch", "", ""},
    {"</images/cat.jpg>; wut=sup ; rel =prefetch  \t  ;", true, "/images/cat.jpg", "prefetch", "", ""},
    {"</images/cat.jpg> wut=sup ; rel =prefetch  \t  ;", false},
    {"<   /images/cat.jpg", false},
    {"<   http://wut.com/  sdfsdf ?sd>; rel=dns-prefetch", true, "http://wut.com/  sdfsdf ?sd", "dns-prefetch", "", ""},
    {"<   http://wut.com/%20%20%3dsdfsdf?sd>; rel=dns-prefetch", true, "http://wut.com/%20%20%3dsdfsdf?sd", "dns-prefetch", "", ""},
    {"<   http://wut.com/dfsdf?sdf=ghj&wer=rty>; rel=prefetch", true, "http://wut.com/dfsdf?sdf=ghj&wer=rty", "prefetch", "", ""},
    {"<   http://wut.com/dfsdf?sdf=ghj&wer=rty>;;;;; rel=prefetch", true, "http://wut.com/dfsdf?sdf=ghj&wer=rty", "prefetch", "", ""},
    {"<   http://wut.com/%20%20%3dsdfsdf?sd>; rel=preload;as=image", true, "http://wut.com/%20%20%3dsdfsdf?sd", "preload", "image", ""},
    {"<   http://wut.com/%20%20%3dsdfsdf?sd>; rel=preload;as=whatever", true, "http://wut.com/%20%20%3dsdfsdf?sd", "preload", "whatever", ""},
    {"</images/cat.jpg>; anchor=foo; rel=prefetch;", false},
    {"</images/cat.jpg>; rel=prefetch;anchor=foo ", false},
    {"</images/cat.jpg>; anchor='foo'; rel=prefetch;", false},
    {"</images/cat.jpg>; rel=prefetch;anchor='foo' ", false},
    {"</images/cat.jpg>; rel=prefetch;anchor='' ", false},
    {"</images/cat.jpg>; rel=prefetch;", true, "/images/cat.jpg", "prefetch", "", ""},
    {"</images/cat.jpg>; rel=prefetch    ;", true, "/images/cat.jpg", "prefetch", "", ""},
    {"</images/ca,t.jpg>; rel=prefetch    ;", true, "/images/ca,t.jpg", "prefetch", "", ""},
    {"<simple.css>; rel=stylesheet; title=\"title with a DQUOTE and backslash\"", true, "simple.css", "stylesheet", "", ""},
    {"<simple.css>; rel=stylesheet; title=\"title with a DQUOTE \\\" and backslash: \\\"", false},
    {"<simple.css>; title=\"title with a DQUOTE \\\" and backslash: \"; rel=stylesheet; ", true, "simple.css", "stylesheet", "", ""},
    {"<simple.css>; title=\'title with a DQUOTE \\\' and backslash: \'; rel=stylesheet; ", true, "simple.css", "stylesheet", "", ""},
    {"<simple.css>; title=\"title with a DQUOTE \\\" and ;backslash,: \"; rel=stylesheet; ", true, "simple.css", "stylesheet", "", ""},
    {"<simple.css>; title=\"title with a DQUOTE \' and ;backslash,: \"; rel=stylesheet; ", true, "simple.css", "stylesheet", "", ""},
    {"<simple.css>; title=\"\"; rel=stylesheet; ", true, "simple.css", "stylesheet", "", ""},
    {"<simple.css>; title=\"\"; rel=\"stylesheet\"; ", true, "simple.css", "stylesheet", "", ""},
    {"<simple.css>; rel=stylesheet; title=\"", false},
    {"<simple.css>; rel=stylesheet; title=\"\"", true, "simple.css", "stylesheet", "", ""},
    {"<simple.css>; rel=\"stylesheet\"; title=\"", false},
    {"<simple.css>; rel=\";style,sheet\"; title=\"", false},
    {"<simple.css>; rel=\"bla'sdf\"; title=\"", false},
    {"<simple.css>; rel=\"\"; title=\"\"", true, "simple.css", "", "", ""},
    {"<simple.css>; rel=''; title=\"\"", true, "simple.css", "''", "", ""},
    {"<simple.css>; rel=''; title=", false},
    {"<simple.css>; rel=''; title", false},
    {"<simple.css>; rel=''; media", false},
    {"<simple.css>; rel=''; hreflang", false},
    {"<simple.css>; rel=''; type", false},
    {"<simple.css>; rel=''; rev", false},
    {"<simple.css>; rel=''; bla", true, "simple.css", "''", "", ""},
    {"<simple.css>; rel='prefetch", true, "simple.css", "'prefetch", "", ""},
    {"<simple.css>; rel=\"prefetch", false},
    {"<simple.css>; rel=\"", false},
    {"<http://whatever.com>; rel=preconnect; valid!", true, "http://whatever.com", "preconnect", "", ""},
    {"<http://whatever.com>; rel=preconnect; valid$", true, "http://whatever.com", "preconnect", "", ""},
    {"<http://whatever.com>; rel=preconnect; invalid@", false},
    {"<http://whatever.com>; rel=preconnect; invalid*", false},
    {"</images/cat.jpg>; rel=prefetch;media='(max-width: 5000px)'", true, "/images/cat.jpg", "prefetch", "", "'(max-width: 5000px)'"},
    {"</images/cat.jpg>; rel=prefetch;media=\"(max-width: 5000px)\"", true, "/images/cat.jpg", "prefetch", "", "(max-width: 5000px)"},
    {"</images/cat.jpg>; rel=prefetch;media=(max-width:5000px)", true, "/images/cat.jpg", "prefetch", "", "(max-width:5000px)"},
};

void PrintTo(const SingleTestCase& test, std::ostream* os)
{
    *os << ::testing::PrintToString(test.headerValue);
}

class SingleLinkHeaderTest : public ::testing::TestWithParam<SingleTestCase> {};

// Test the cases with a single header
TEST_P(SingleLinkHeaderTest, Single)
{
    const SingleTestCase testCase = GetParam();
    LinkHeaderSet headerSet(testCase.headerValue);
    ASSERT_EQ(1u, headerSet.size());
    LinkHeader& header = headerSet[0];
    EXPECT_EQ(testCase.valid, header.valid());
    if (testCase.valid) {
        EXPECT_STREQ(testCase.url, header.url().ascii().data());
        EXPECT_STREQ(testCase.rel, header.rel().ascii().data());
        EXPECT_STREQ(testCase.as, header.as().ascii().data());
        EXPECT_STREQ(testCase.media, header.media().ascii().data());
    }
}

INSTANTIATE_TEST_CASE_P(LinkHeaderTest, SingleLinkHeaderTest, testing::ValuesIn(singleTestCases));

struct DoubleTestCase {
    const char* headerValue;
    const char* url;
    const char* rel;
    bool valid;
    const char* url2;
    const char* rel2;
    bool valid2;
} doubleTestCases[] = {
    {"<ybg.css>; rel=stylesheet, <simple.css>; rel=stylesheet", "ybg.css", "stylesheet", true, "simple.css", "stylesheet", true},
    {"<ybg.css>; rel=stylesheet,<simple.css>; rel=stylesheet", "ybg.css", "stylesheet", true, "simple.css", "stylesheet", true},
    {"<ybg.css>; rel=stylesheet;crossorigin,<simple.css>; rel=stylesheet", "ybg.css", "stylesheet", true, "simple.css", "stylesheet", true},
    {"<hel,lo.css>; rel=stylesheet; title=\"foo,bar\", <simple.css>; rel=stylesheet; title=\"foo;bar\"", "hel,lo.css", "stylesheet", true, "simple.css", "stylesheet", true},
};

void PrintTo(const DoubleTestCase& test, std::ostream* os)
{
    *os << ::testing::PrintToString(test.headerValue);
}

class DoubleLinkHeaderTest : public ::testing::TestWithParam<DoubleTestCase> {};

TEST_P(DoubleLinkHeaderTest, Double)
{
    const DoubleTestCase testCase = GetParam();
    LinkHeaderSet headerSet(testCase.headerValue);
    ASSERT_EQ(2u, headerSet.size());
    LinkHeader& header1 = headerSet[0];
    LinkHeader& header2 = headerSet[1];
    EXPECT_STREQ(testCase.url, header1.url().ascii().data());
    EXPECT_STREQ(testCase.rel, header1.rel().ascii().data());
    EXPECT_EQ(testCase.valid, header1.valid());
    EXPECT_STREQ(testCase.url2, header2.url().ascii().data());
    EXPECT_STREQ(testCase.rel2, header2.rel().ascii().data());
    EXPECT_EQ(testCase.valid2, header2.valid());
}

INSTANTIATE_TEST_CASE_P(LinkHeaderTest, DoubleLinkHeaderTest, testing::ValuesIn(doubleTestCases));

struct CrossOriginTestCase {
    const char* headerValue;
    const char* url;
    const char* rel;
    const char* crossorigin;
    bool valid;
} crossOriginTestCases[] = {
    {"<http://whatever.com>; rel=preconnect", "http://whatever.com", "preconnect", nullptr, true},
    {"<http://whatever.com>; rel=preconnect; crossorigin=", "", "", "", false},
    {"<http://whatever.com>; rel=preconnect; crossorigin", "http://whatever.com", "preconnect", "", true},
    {"<http://whatever.com>; rel=preconnect; crossorigin ", "http://whatever.com", "preconnect", "", true},
    {"<http://whatever.com>; rel=preconnect; crossorigin;", "http://whatever.com", "preconnect", "", true},
    {"<http://whatever.com>; rel=preconnect; crossorigin, <http://whatever2.com>; rel=preconnect", "http://whatever.com", "preconnect", "", true},
    {"<http://whatever.com>; rel=preconnect; crossorigin , <http://whatever2.com>; rel=preconnect", "http://whatever.com", "preconnect", "", true},
    {"<http://whatever.com>; rel=preconnect; crossorigin,<http://whatever2.com>; rel=preconnect", "http://whatever.com", "preconnect", "", true},
    {"<http://whatever.com>; rel=preconnect; crossorigin=anonymous", "http://whatever.com", "preconnect", "anonymous", true},
    {"<http://whatever.com>; rel=preconnect; crossorigin=use-credentials", "http://whatever.com", "preconnect", "use-credentials", true},
    {"<http://whatever.com>; rel=preconnect; crossorigin=whatever", "http://whatever.com", "preconnect", "whatever", true},
    {"<http://whatever.com>; rel=preconnect; crossorig|in=whatever", "http://whatever.com", "preconnect", nullptr, true},
    {"<http://whatever.com>; rel=preconnect; crossorigin|=whatever", "http://whatever.com", "preconnect", nullptr, true},
};

void PrintTo(const CrossOriginTestCase& test, std::ostream* os)
{
    *os << ::testing::PrintToString(test.headerValue);
}

class CrossOriginLinkHeaderTest : public ::testing::TestWithParam<CrossOriginTestCase> {};

TEST_P(CrossOriginLinkHeaderTest, CrossOrigin)
{
    const CrossOriginTestCase testCase = GetParam();
    LinkHeaderSet headerSet(testCase.headerValue);
    ASSERT_GE(headerSet.size(), 1u);
    LinkHeader& header = headerSet[0];
    EXPECT_STREQ(testCase.url, header.url().ascii().data());
    EXPECT_STREQ(testCase.rel, header.rel().ascii().data());
    EXPECT_EQ(testCase.valid, header.valid());
    if (!testCase.crossorigin)
        EXPECT_TRUE(header.crossOrigin().isNull());
    else
        EXPECT_STREQ(testCase.crossorigin, header.crossOrigin().ascii().data());
}

INSTANTIATE_TEST_CASE_P(LinkHeaderTest, CrossOriginLinkHeaderTest, testing::ValuesIn(crossOriginTestCases));

} // namespace
} // namespace blink
