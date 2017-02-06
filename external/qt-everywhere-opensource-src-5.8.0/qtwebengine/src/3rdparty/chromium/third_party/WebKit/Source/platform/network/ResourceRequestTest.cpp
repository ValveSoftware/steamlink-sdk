// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/network/ResourceRequest.h"

#include "platform/network/EncodedFormData.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/Referrer.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebURLRequest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/AtomicString.h"
#include <memory>

namespace blink {

TEST(ResourceRequestTest, RequestorOriginNonNull)
{
    ResourceRequest req;
    EXPECT_NE(nullptr, req.requestorOrigin().get());
    EXPECT_TRUE(req.requestorOrigin()->isUnique());
}

TEST(ResourceRequestTest, CrossThreadResourceRequestData)
{
    ResourceRequest original;
    original.setURL(KURL(ParsedURLString, "http://www.example.com/test.htm"));
    original.setCachePolicy(WebCachePolicy::UseProtocolCachePolicy);
    original.setTimeoutInterval(10);
    original.setFirstPartyForCookies(KURL(ParsedURLString, "http://www.example.com/first_party.htm"));
    original.setRequestorOrigin(SecurityOrigin::create(KURL(ParsedURLString, "http://www.example.com/first_party.htm")));
    original.setHTTPMethod(HTTPNames::GET);
    original.setHTTPHeaderField(AtomicString("Foo"), AtomicString("Bar"));
    original.setHTTPHeaderField(AtomicString("Piyo"), AtomicString("Fuga"));
    original.setPriority(ResourceLoadPriorityLow, 20);

    RefPtr<EncodedFormData> originalBody(EncodedFormData::create("Test Body"));
    original.setHTTPBody(originalBody);
    original.setAllowStoredCredentials(false);
    original.setReportUploadProgress(false);
    original.setHasUserGesture(false);
    original.setDownloadToFile(false);
    original.setSkipServiceWorker(WebURLRequest::SkipServiceWorker::None);
    original.setFetchRequestMode(WebURLRequest::FetchRequestModeCORS);
    original.setFetchCredentialsMode(WebURLRequest::FetchCredentialsModeSameOrigin);
    original.setRequestorID(30);
    original.setRequestorProcessID(40);
    original.setAppCacheHostID(50);
    original.setRequestContext(WebURLRequest::RequestContextAudio);
    original.setFrameType(WebURLRequest::FrameTypeNested);
    original.setHTTPReferrer(Referrer("http://www.example.com/referrer.htm", ReferrerPolicyDefault));

    EXPECT_STREQ("http://www.example.com/test.htm", original.url().getString().utf8().data());
    EXPECT_EQ(WebCachePolicy::UseProtocolCachePolicy, original.getCachePolicy());
    EXPECT_EQ(10, original.timeoutInterval());
    EXPECT_STREQ("http://www.example.com/first_party.htm", original.firstPartyForCookies().getString().utf8().data());
    EXPECT_STREQ("www.example.com", original.requestorOrigin()->host().utf8().data());
    EXPECT_STREQ("GET", original.httpMethod().utf8().data());
    EXPECT_STREQ("Bar", original.httpHeaderFields().get("Foo").utf8().data());
    EXPECT_STREQ("Fuga", original.httpHeaderFields().get("Piyo").utf8().data());
    EXPECT_EQ(ResourceLoadPriorityLow, original.priority());
    EXPECT_STREQ("Test Body", original.httpBody()->flattenToString().utf8().data());
    EXPECT_FALSE(original.allowStoredCredentials());
    EXPECT_FALSE(original.reportUploadProgress());
    EXPECT_FALSE(original.hasUserGesture());
    EXPECT_FALSE(original.downloadToFile());
    EXPECT_EQ(WebURLRequest::SkipServiceWorker::None, original.skipServiceWorker());
    EXPECT_EQ(WebURLRequest::FetchRequestModeCORS, original.fetchRequestMode());
    EXPECT_EQ(WebURLRequest::FetchCredentialsModeSameOrigin, original.fetchCredentialsMode());
    EXPECT_EQ(30, original.requestorID());
    EXPECT_EQ(40, original.requestorProcessID());
    EXPECT_EQ(50, original.appCacheHostID());
    EXPECT_EQ(WebURLRequest::RequestContextAudio, original.requestContext());
    EXPECT_EQ(WebURLRequest::FrameTypeNested, original.frameType());
    EXPECT_STREQ("http://www.example.com/referrer.htm", original.httpReferrer().utf8().data());
    EXPECT_EQ(ReferrerPolicyDefault, original.getReferrerPolicy());

    std::unique_ptr<CrossThreadResourceRequestData> data1(original.copyData());
    ResourceRequest copy1(data1.get());

    EXPECT_STREQ("http://www.example.com/test.htm", copy1.url().getString().utf8().data());
    EXPECT_EQ(WebCachePolicy::UseProtocolCachePolicy, copy1.getCachePolicy());
    EXPECT_EQ(10, copy1.timeoutInterval());
    EXPECT_STREQ("http://www.example.com/first_party.htm", copy1.firstPartyForCookies().getString().utf8().data());
    EXPECT_STREQ("www.example.com", copy1.requestorOrigin()->host().utf8().data());
    EXPECT_STREQ("GET", copy1.httpMethod().utf8().data());
    EXPECT_STREQ("Bar", copy1.httpHeaderFields().get("Foo").utf8().data());
    EXPECT_EQ(ResourceLoadPriorityLow, copy1.priority());
    EXPECT_STREQ("Test Body", copy1.httpBody()->flattenToString().utf8().data());
    EXPECT_FALSE(copy1.allowStoredCredentials());
    EXPECT_FALSE(copy1.reportUploadProgress());
    EXPECT_FALSE(copy1.hasUserGesture());
    EXPECT_FALSE(copy1.downloadToFile());
    EXPECT_EQ(WebURLRequest::SkipServiceWorker::None, copy1.skipServiceWorker());
    EXPECT_EQ(WebURLRequest::FetchRequestModeCORS, copy1.fetchRequestMode());
    EXPECT_EQ(WebURLRequest::FetchCredentialsModeSameOrigin, copy1.fetchCredentialsMode());
    EXPECT_EQ(30, copy1.requestorID());
    EXPECT_EQ(40, copy1.requestorProcessID());
    EXPECT_EQ(50, copy1.appCacheHostID());
    EXPECT_EQ(WebURLRequest::RequestContextAudio, copy1.requestContext());
    EXPECT_EQ(WebURLRequest::FrameTypeNested, copy1.frameType());
    EXPECT_STREQ("http://www.example.com/referrer.htm", copy1.httpReferrer().utf8().data());
    EXPECT_EQ(ReferrerPolicyDefault, copy1.getReferrerPolicy());

    copy1.setAllowStoredCredentials(true);
    copy1.setReportUploadProgress(true);
    copy1.setHasUserGesture(true);
    copy1.setDownloadToFile(true);
    copy1.setSkipServiceWorker(WebURLRequest::SkipServiceWorker::All);
    copy1.setFetchRequestMode(WebURLRequest::FetchRequestModeNoCORS);
    copy1.setFetchCredentialsMode(WebURLRequest::FetchCredentialsModeInclude);

    std::unique_ptr<CrossThreadResourceRequestData> data2(copy1.copyData());
    ResourceRequest copy2(data2.get());
    EXPECT_TRUE(copy2.allowStoredCredentials());
    EXPECT_TRUE(copy2.reportUploadProgress());
    EXPECT_TRUE(copy2.hasUserGesture());
    EXPECT_TRUE(copy2.downloadToFile());
    EXPECT_EQ(WebURLRequest::SkipServiceWorker::All, copy2.skipServiceWorker());
    EXPECT_EQ(WebURLRequest::FetchRequestModeNoCORS, copy1.fetchRequestMode());
    EXPECT_EQ(WebURLRequest::FetchCredentialsModeInclude, copy1.fetchCredentialsMode());
}

TEST(ResourceRequestTest, SetHasUserGesture)
{
    ResourceRequest original;
    EXPECT_FALSE(original.hasUserGesture());
    original.setHasUserGesture(true);
    EXPECT_TRUE(original.hasUserGesture());
    original.setHasUserGesture(false);
    EXPECT_TRUE(original.hasUserGesture());
}

} // namespace blink
