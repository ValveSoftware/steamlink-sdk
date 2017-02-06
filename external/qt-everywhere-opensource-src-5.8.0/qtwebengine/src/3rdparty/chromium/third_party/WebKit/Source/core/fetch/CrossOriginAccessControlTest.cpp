// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fetch/CrossOriginAccessControl.h"

#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace {

class CreateAccessControlPreflightRequestTest : public ::testing::Test {
protected:
    virtual void SetUp()
    {
        m_securityOrigin = SecurityOrigin::createFromString("http://example.com");
    }

    RefPtr<SecurityOrigin> m_securityOrigin;
};

TEST_F(CreateAccessControlPreflightRequestTest, LexicographicalOrder)
{
    ResourceRequest request;
    request.addHTTPHeaderField("Orange", "Orange");
    request.addHTTPHeaderField("Apple", "Red");
    request.addHTTPHeaderField("Kiwifruit", "Green");
    request.addHTTPHeaderField("Content-Type", "application/octet-stream");
    request.addHTTPHeaderField("Strawberry", "Red");

    ResourceRequest preflight = createAccessControlPreflightRequest(request, m_securityOrigin.get());

    EXPECT_EQ("apple, content-type, kiwifruit, orange, strawberry", preflight.httpHeaderField("Access-Control-Request-Headers"));
}

TEST_F(CreateAccessControlPreflightRequestTest, ExcludeSimpleHeaders)
{
    ResourceRequest request;
    request.addHTTPHeaderField("Accept", "everything");
    request.addHTTPHeaderField("Accept-Language", "everything");
    request.addHTTPHeaderField("Content-Language", "everything");
    request.addHTTPHeaderField("Save-Data", "on");

    ResourceRequest preflight = createAccessControlPreflightRequest(request, m_securityOrigin.get());

    EXPECT_EQ("", preflight.httpHeaderField("Access-Control-Request-Headers"));
}

TEST_F(CreateAccessControlPreflightRequestTest, ExcludeSimpleContentTypeHeader)
{
    ResourceRequest request;
    request.addHTTPHeaderField("Content-Type", "text/plain");

    ResourceRequest preflight = createAccessControlPreflightRequest(request, m_securityOrigin.get());

    EXPECT_EQ("", preflight.httpHeaderField("Access-Control-Request-Headers"));
}

TEST_F(CreateAccessControlPreflightRequestTest, IncludeNonSimpleHeader)
{
    ResourceRequest request;
    request.addHTTPHeaderField("X-Custom-Header", "foobar");

    ResourceRequest preflight = createAccessControlPreflightRequest(request, m_securityOrigin.get());

    EXPECT_EQ("x-custom-header", preflight.httpHeaderField("Access-Control-Request-Headers"));
}

TEST_F(CreateAccessControlPreflightRequestTest, IncludeNonSimpleContentTypeHeader)
{
    ResourceRequest request;
    request.addHTTPHeaderField("Content-Type", "application/octet-stream");

    ResourceRequest preflight = createAccessControlPreflightRequest(request, m_securityOrigin.get());

    EXPECT_EQ("content-type", preflight.httpHeaderField("Access-Control-Request-Headers"));
}

} // namespace

} // namespace blink
