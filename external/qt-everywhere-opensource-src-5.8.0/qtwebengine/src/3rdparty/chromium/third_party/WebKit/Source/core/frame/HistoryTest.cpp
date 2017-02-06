// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/History.h"

#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class HistoryTest : public ::testing::Test {
};

TEST_F(HistoryTest, CanChangeToURL)
{
    struct TestCase {
        const char* url;
        const char* documentURL;
        bool expected;
    } cases[] = {
        {"http://example.com/", "http://example.com/", true},
        {"http://example.com/#hash", "http://example.com/", true},
        {"http://example.com/path", "http://example.com/", true},
        {"http://example.com/path#hash", "http://example.com/", true},
        {"http://example.com/path?query", "http://example.com/", true},
        {"http://example.com/path?query#hash", "http://example.com/", true},
        {"http://example.com:80/", "http://example.com/", true},
        {"http://example.com:80/#hash", "http://example.com/", true},
        {"http://example.com:80/path", "http://example.com/", true},
        {"http://example.com:80/path#hash", "http://example.com/", true},
        {"http://example.com:80/path?query", "http://example.com/", true},
        {"http://example.com:80/path?query#hash", "http://example.com/", true},
        {"http://not-example.com:80/", "http://example.com/", false},
        {"http://not-example.com:80/#hash", "http://example.com/", false},
        {"http://not-example.com:80/path", "http://example.com/", false},
        {"http://not-example.com:80/path#hash", "http://example.com/", false},
        {"http://not-example.com:80/path?query", "http://example.com/", false},
        {"http://not-example.com:80/path?query#hash", "http://example.com/", false},
        {"http://example.com:81/", "http://example.com/", false},
        {"http://example.com:81/#hash", "http://example.com/", false},
        {"http://example.com:81/path", "http://example.com/", false},
        {"http://example.com:81/path#hash", "http://example.com/", false},
        {"http://example.com:81/path?query", "http://example.com/", false},
        {"http://example.com:81/path?query#hash", "http://example.com/", false},
    };

    for (const auto& test : cases) {
        KURL url(ParsedURLString, test.url);
        KURL documentURL(ParsedURLString, test.documentURL);
        RefPtr<SecurityOrigin> documentOrigin = SecurityOrigin::create(documentURL);
        EXPECT_EQ(test.expected, History::canChangeToUrl(url, documentOrigin.get(), documentURL));
    }
}

TEST_F(HistoryTest, CanChangeToURLInFileOrigin)
{
    struct TestCase {
        const char* url;
        const char* documentURL;
        bool expected;
    } cases[] = {
        {"file:///path/to/file/", "file:///path/to/file/", true},
        {"file:///path/to/file/#hash", "file:///path/to/file/", true},
        {"file:///path/to/file/path", "file:///path/to/file/", false},
        {"file:///path/to/file/path#hash", "file:///path/to/file/", false},
        {"file:///path/to/file/path?query", "file:///path/to/file/", false},
        {"file:///path/to/file/path?query#hash", "file:///path/to/file/", false},
    };

    for (const auto& test : cases) {
        KURL url(ParsedURLString, test.url);
        KURL documentURL(ParsedURLString, test.documentURL);
        RefPtr<SecurityOrigin> documentOrigin = SecurityOrigin::create(documentURL);
        EXPECT_EQ(test.expected, History::canChangeToUrl(url, documentOrigin.get(), documentURL));
    }
}

TEST_F(HistoryTest, CanChangeToURLInUniqueOrigin)
{
    struct TestCase {
        const char* url;
        const char* documentURL;
        bool expected;
    } cases[] = {
        {"http://example.com/", "http://example.com/", true},
        {"http://example.com/#hash", "http://example.com/", true},
        {"http://example.com/path", "http://example.com/", false},
        {"http://example.com/path#hash", "http://example.com/", false},
        {"http://example.com/path?query", "http://example.com/", false},
        {"http://example.com/path?query#hash", "http://example.com/", false},
        {"http://example.com:80/", "http://example.com/", true},
        {"http://example.com:80/#hash", "http://example.com/", true},
        {"http://example.com:80/path", "http://example.com/", false},
        {"http://example.com:80/path#hash", "http://example.com/", false},
        {"http://example.com:80/path?query", "http://example.com/", false},
        {"http://example.com:80/path?query#hash", "http://example.com/", false},
        {"http://example.com:81/", "http://example.com/", false},
        {"http://example.com:81/#hash", "http://example.com/", false},
        {"http://example.com:81/path", "http://example.com/", false},
        {"http://example.com:81/path#hash", "http://example.com/", false},
        {"http://example.com:81/path?query", "http://example.com/", false},
        {"http://example.com:81/path?query#hash", "http://example.com/", false},
    };

    for (const auto& test : cases) {
        KURL url(ParsedURLString, test.url);
        KURL documentURL(ParsedURLString, test.documentURL);
        RefPtr<SecurityOrigin> documentOrigin = SecurityOrigin::createUnique();
        EXPECT_EQ(test.expected, History::canChangeToUrl(url, documentOrigin.get(), documentURL));
    }
}

} // namespace blink

