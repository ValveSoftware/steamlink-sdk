// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/PingLoader.h"

#include "core/fetch/SubstituteData.h"
#include "core/frame/LocalFrame.h"
#include "core/loader/EmptyClients.h"
#include "core/loader/FrameLoader.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class PingFrameLoaderClient : public EmptyFrameLoaderClient {
 public:
  void dispatchWillSendRequest(ResourceRequest& request) override {
    if (request.httpContentType() == "text/ping")
      m_pingRequest = request;
  }

  const ResourceRequest& pingRequest() const { return m_pingRequest; }

 private:
  ResourceRequest m_pingRequest;
};

class PingLoaderTest : public ::testing::Test {
 public:
  void SetUp() override {
    m_client = new PingFrameLoaderClient;
    m_pageHolder = DummyPageHolder::create(IntSize(1, 1), nullptr, m_client);
  }

  void TearDown() override {
    Platform::current()->getURLLoaderMockFactory()->unregisterAllURLs();
  }

  void setDocumentURL(const KURL& url) {
    FrameLoadRequest request(
        nullptr, url,
        SubstituteData(SharedBuffer::create(), "text/html", "UTF-8", KURL()));
    m_pageHolder->frame().loader().load(request);
    blink::testing::runPendingTasks();
    ASSERT_EQ(url.getString(), m_pageHolder->document().url().getString());
  }

  const ResourceRequest& pingAndGetRequest(const KURL& pingURL) {
    KURL destinationURL(ParsedURLString, "http://navigation.destination");
    URLTestHelpers::registerMockedURLLoad(pingURL, "bar.html", "text/html");
    PingLoader::sendLinkAuditPing(&m_pageHolder->frame(), pingURL,
                                  destinationURL);
    const ResourceRequest& pingRequest = m_client->pingRequest();
    if (!pingRequest.isNull()) {
      EXPECT_EQ(destinationURL.getString(),
                pingRequest.httpHeaderField("Ping-To"));
    }
    return pingRequest;
  }

 private:
  Persistent<PingFrameLoaderClient> m_client;
  std::unique_ptr<DummyPageHolder> m_pageHolder;
};

TEST_F(PingLoaderTest, HTTPSToHTTPS) {
  KURL pingURL(ParsedURLString, "https://localhost/bar.html");
  setDocumentURL(KURL(ParsedURLString, "https://127.0.0.1:8000/foo.html"));
  const ResourceRequest& pingRequest = pingAndGetRequest(pingURL);
  ASSERT_FALSE(pingRequest.isNull());
  EXPECT_EQ(pingURL, pingRequest.url());
  EXPECT_EQ(String(), pingRequest.httpHeaderField("Ping-From"));
}

TEST_F(PingLoaderTest, HTTPToHTTPS) {
  KURL documentURL(ParsedURLString, "http://127.0.0.1:8000/foo.html");
  KURL pingURL(ParsedURLString, "https://localhost/bar.html");
  setDocumentURL(documentURL);
  const ResourceRequest& pingRequest = pingAndGetRequest(pingURL);
  ASSERT_FALSE(pingRequest.isNull());
  EXPECT_EQ(pingURL, pingRequest.url());
  EXPECT_EQ(documentURL.getString(), pingRequest.httpHeaderField("Ping-From"));
}

TEST_F(PingLoaderTest, NonHTTPPingTarget) {
  setDocumentURL(KURL(ParsedURLString, "http://127.0.0.1:8000/foo.html"));
  const ResourceRequest& pingRequest =
      pingAndGetRequest(KURL(ParsedURLString, "ftp://localhost/bar.html"));
  ASSERT_TRUE(pingRequest.isNull());
}

}  // namespace

}  // namespace blink
