/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/fetch/RawResource.h"

#include "core/fetch/MemoryCache.h"
#include "core/fetch/ResourceFetcher.h"
#include "platform/SharedBuffer.h"
#include "platform/heap/Handle.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLResponse.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(RawResourceTest, DontIgnoreAcceptForCacheReuse)
{
    ResourceRequest jpegRequest;
    jpegRequest.setHTTPAccept("image/jpeg");

    RawResource* jpegResource(RawResource::create(jpegRequest, Resource::Raw));

    ResourceRequest pngRequest;
    pngRequest.setHTTPAccept("image/png");

    ASSERT_FALSE(jpegResource->canReuse(pngRequest));
}

class DummyClient final : public GarbageCollectedFinalized<DummyClient>, public RawResourceClient {
public:
    DummyClient() : m_called(false), m_numberOfRedirectsReceived(0) {}
    ~DummyClient() override {}

    // ResourceClient implementation.
    void notifyFinished(Resource* resource) override
    {
        m_called = true;
    }
    String debugName() const override { return "DummyClient"; }

    void dataReceived(Resource*, const char* data, size_t length) override
    {
        m_data.append(data, length);
    }

    void redirectReceived(Resource*, ResourceRequest&, const ResourceResponse&) override
    {
        ++m_numberOfRedirectsReceived;
    }

    bool called() { return m_called; }
    int numberOfRedirectsReceived() const { return m_numberOfRedirectsReceived; }
    const Vector<char>& data() { return m_data; }
    DEFINE_INLINE_TRACE() {}

private:
    bool m_called;
    int m_numberOfRedirectsReceived;
    Vector<char> m_data;
};

// This client adds another client when notified.
class AddingClient final : public GarbageCollectedFinalized<AddingClient>, public RawResourceClient {
public:
    AddingClient(DummyClient* client, Resource* resource)
        : m_dummyClient(client)
        , m_resource(resource)
        , m_removeClientTimer(this, &AddingClient::removeClient) {}

    ~AddingClient() override {}

    // ResourceClient implementation.
    void notifyFinished(Resource* resource) override
    {
        // First schedule an asynchronous task to remove the client.
        // We do not expect the client to be called.
        m_removeClientTimer.startOneShot(0, BLINK_FROM_HERE);
        resource->addClient(m_dummyClient);
    }
    String debugName() const override { return "AddingClient"; }

    void removeClient(Timer<AddingClient>* timer)
    {
        m_resource->removeClient(m_dummyClient);
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_dummyClient);
        visitor->trace(m_resource);
    }

private:
    Member<DummyClient> m_dummyClient;
    Member<Resource> m_resource;
    Timer<AddingClient> m_removeClientTimer;
};

TEST(RawResourceTest, RevalidationSucceeded)
{
    Resource* resource = RawResource::create(ResourceRequest("data:text/html,"), Resource::Raw);
    ResourceResponse response;
    response.setHTTPStatusCode(200);
    resource->responseReceived(response, nullptr);
    const char data[5] = "abcd";
    resource->appendData(data, 4);
    resource->finish();
    memoryCache()->add(resource);

    // Simulate a successful revalidation.
    resource->setRevalidatingRequest(ResourceRequest("data:text/html,"));

    Persistent<DummyClient> client = new DummyClient;
    resource->addClient(client);

    ResourceResponse revalidatingResponse;
    revalidatingResponse.setHTTPStatusCode(304);
    resource->responseReceived(revalidatingResponse, nullptr);
    EXPECT_FALSE(resource->isCacheValidator());
    EXPECT_EQ(200, resource->response().httpStatusCode());
    EXPECT_EQ(4u, resource->resourceBuffer()->size());
    EXPECT_EQ(memoryCache()->resourceForURL(KURL(ParsedURLString, "data:text/html,")), resource);
    memoryCache()->remove(resource);

    resource->removeClient(client);
    EXPECT_FALSE(resource->hasClientsOrObservers());
    EXPECT_FALSE(client->called());
    EXPECT_EQ("abcd", String(client->data().data(), client->data().size()));
}

TEST(RawResourceTest, RevalidationSucceededForResourceWithoutBody)
{
    Resource* resource = RawResource::create(ResourceRequest("data:text/html,"), Resource::Raw);
    ResourceResponse response;
    response.setHTTPStatusCode(200);
    resource->responseReceived(response, nullptr);
    resource->finish();
    memoryCache()->add(resource);

    // Simulate a successful revalidation.
    resource->setRevalidatingRequest(ResourceRequest("data:text/html,"));

    Persistent<DummyClient> client = new DummyClient;
    resource->addClient(client);

    ResourceResponse revalidatingResponse;
    revalidatingResponse.setHTTPStatusCode(304);
    resource->responseReceived(revalidatingResponse, nullptr);
    EXPECT_FALSE(resource->isCacheValidator());
    EXPECT_EQ(200, resource->response().httpStatusCode());
    EXPECT_EQ(nullptr, resource->resourceBuffer());
    EXPECT_EQ(memoryCache()->resourceForURL(KURL(ParsedURLString, "data:text/html,")), resource);
    memoryCache()->remove(resource);

    resource->removeClient(client);
    EXPECT_FALSE(resource->hasClientsOrObservers());
    EXPECT_FALSE(client->called());
    EXPECT_EQ(0u, client->data().size());
}

TEST(RawResourceTest, RevalidationSucceededUpdateHeaders)
{
    Resource* resource = RawResource::create(ResourceRequest("data:text/html,"), Resource::Raw);
    ResourceResponse response;
    response.setHTTPStatusCode(200);
    response.addHTTPHeaderField("keep-alive", "keep-alive value");
    response.addHTTPHeaderField("expires", "expires value");
    response.addHTTPHeaderField("last-modified", "last-modified value");
    response.addHTTPHeaderField("proxy-authenticate", "proxy-authenticate value");
    response.addHTTPHeaderField("proxy-connection", "proxy-connection value");
    response.addHTTPHeaderField("x-custom", "custom value");
    resource->responseReceived(response, nullptr);
    resource->finish();
    memoryCache()->add(resource);

    // Simulate a successful revalidation.
    resource->setRevalidatingRequest(ResourceRequest("data:text/html,"));

    // Validate that these headers pre-update.
    EXPECT_EQ("keep-alive value", resource->response().httpHeaderField("keep-alive"));
    EXPECT_EQ("expires value", resource->response().httpHeaderField("expires"));
    EXPECT_EQ("last-modified value", resource->response().httpHeaderField("last-modified"));
    EXPECT_EQ("proxy-authenticate value", resource->response().httpHeaderField("proxy-authenticate"));
    EXPECT_EQ("proxy-authenticate value", resource->response().httpHeaderField("proxy-authenticate"));
    EXPECT_EQ("proxy-connection value", resource->response().httpHeaderField("proxy-connection"));
    EXPECT_EQ("custom value", resource->response().httpHeaderField("x-custom"));

    Persistent<DummyClient> client = new DummyClient;
    resource->addClient(client.get());

    // Perform a revalidation step.
    ResourceResponse revalidatingResponse;
    revalidatingResponse.setHTTPStatusCode(304);
    // Headers that aren't copied with an 304 code.
    revalidatingResponse.addHTTPHeaderField("keep-alive", "garbage");
    revalidatingResponse.addHTTPHeaderField("expires", "garbage");
    revalidatingResponse.addHTTPHeaderField("last-modified", "garbage");
    revalidatingResponse.addHTTPHeaderField("proxy-authenticate", "garbage");
    revalidatingResponse.addHTTPHeaderField("proxy-connection", "garbage");
    // Header that is updated with 304 code.
    revalidatingResponse.addHTTPHeaderField("x-custom", "updated");
    resource->responseReceived(revalidatingResponse, nullptr);

    // Validate the original response.
    EXPECT_EQ(200, resource->response().httpStatusCode());

    // Validate that these headers are not updated.
    EXPECT_EQ("keep-alive value", resource->response().httpHeaderField("keep-alive"));
    EXPECT_EQ("expires value", resource->response().httpHeaderField("expires"));
    EXPECT_EQ("last-modified value", resource->response().httpHeaderField("last-modified"));
    EXPECT_EQ("proxy-authenticate value", resource->response().httpHeaderField("proxy-authenticate"));
    EXPECT_EQ("proxy-authenticate value", resource->response().httpHeaderField("proxy-authenticate"));
    EXPECT_EQ("proxy-connection value", resource->response().httpHeaderField("proxy-connection"));
    EXPECT_EQ("updated", resource->response().httpHeaderField("x-custom"));

    memoryCache()->remove(resource);

    resource->removeClient(client);
    EXPECT_FALSE(resource->hasClientsOrObservers());
    EXPECT_FALSE(client->called());
    EXPECT_EQ(0u, client->data().size());
}

TEST(RawResourceTest, RedirectDuringRevalidation)
{
    Resource* resource = RawResource::create(ResourceRequest("https://example.com/1"), Resource::Raw);
    ResourceResponse response;
    response.setURL(KURL(ParsedURLString, "https://example.com/1"));
    response.setHTTPStatusCode(200);
    resource->responseReceived(response, nullptr);
    const char data[5] = "abcd";
    resource->appendData(data, 4);
    resource->finish();
    memoryCache()->add(resource);

    EXPECT_FALSE(resource->isCacheValidator());
    EXPECT_EQ("https://example.com/1", resource->resourceRequest().url().getString());
    EXPECT_EQ("https://example.com/1", resource->lastResourceRequest().url().getString());

    // Simulate a revalidation.
    resource->setRevalidatingRequest(ResourceRequest("https://example.com/1"));
    EXPECT_TRUE(resource->isCacheValidator());
    EXPECT_EQ("https://example.com/1", resource->resourceRequest().url().getString());
    EXPECT_EQ("https://example.com/1", resource->lastResourceRequest().url().getString());

    Persistent<DummyClient> client = new DummyClient;
    resource->addClient(client);

    // The revalidating request is redirected.
    ResourceResponse redirectResponse;
    redirectResponse.setURL(KURL(ParsedURLString, "https://example.com/1"));
    redirectResponse.setHTTPHeaderField("location", "https://example.com/2");
    redirectResponse.setHTTPStatusCode(308);
    ResourceRequest redirectedRevalidatingRequest("https://example.com/2");
    resource->willFollowRedirect(redirectedRevalidatingRequest, redirectResponse);
    EXPECT_FALSE(resource->isCacheValidator());
    EXPECT_EQ("https://example.com/1", resource->resourceRequest().url().getString());
    EXPECT_EQ("https://example.com/2", resource->lastResourceRequest().url().getString());

    // The final response is received.
    ResourceResponse revalidatingResponse;
    revalidatingResponse.setURL(KURL(ParsedURLString, "https://example.com/2"));
    revalidatingResponse.setHTTPStatusCode(200);
    resource->responseReceived(revalidatingResponse, nullptr);
    const char data2[4] = "xyz";
    resource->appendData(data2, 3);
    resource->finish();
    EXPECT_FALSE(resource->isCacheValidator());
    EXPECT_EQ("https://example.com/1", resource->resourceRequest().url().getString());
    EXPECT_EQ("https://example.com/2", resource->lastResourceRequest().url().getString());
    EXPECT_FALSE(resource->isCacheValidator());
    EXPECT_EQ(200, resource->response().httpStatusCode());
    EXPECT_EQ(3u, resource->resourceBuffer()->size());
    EXPECT_EQ(memoryCache()->resourceForURL(KURL(ParsedURLString, "https://example.com/1")), resource);

    EXPECT_TRUE(client->called());
    EXPECT_EQ(1, client->numberOfRedirectsReceived());
    EXPECT_EQ("xyz", String(client->data().data(), client->data().size()));

    // Test the case where a client is added after revalidation is completed.
    Persistent<DummyClient> client2 = new DummyClient;
    resource->addClient(client2);

    // Because RawResourceClient is added asynchronously,
    // |runPendingTasks()| is called to make |client2| to be notified.
    testing::runPendingTasks();

    EXPECT_TRUE(client2->called());
    EXPECT_EQ(1, client2->numberOfRedirectsReceived());
    EXPECT_EQ("xyz", String(client2->data().data(), client2->data().size()));

    memoryCache()->remove(resource);

    resource->removeClient(client);
    resource->removeClient(client2);
    EXPECT_FALSE(resource->hasClientsOrObservers());
}

TEST(RawResourceTest, AddClientDuringCallback)
{
    Resource* raw = RawResource::create(ResourceRequest("data:text/html,"), Resource::Raw);

    // Create a non-null response.
    ResourceResponse response = raw->response();
    response.setURL(KURL(ParsedURLString, "http://600.613/"));
    raw->setResponse(response);
    raw->finish();
    EXPECT_FALSE(raw->response().isNull());

    Persistent<DummyClient> dummyClient = new DummyClient();
    Persistent<AddingClient> addingClient = new AddingClient(dummyClient.get(), raw);
    raw->addClient(addingClient);
    testing::runPendingTasks();
    raw->removeClient(addingClient);
    EXPECT_FALSE(dummyClient->called());
    EXPECT_FALSE(raw->hasClientsOrObservers());
}

// This client removes another client when notified.
class RemovingClient : public GarbageCollectedFinalized<RemovingClient>, public RawResourceClient {
public:
    RemovingClient(DummyClient* client)
        : m_dummyClient(client) {}

    ~RemovingClient() override {}

    // ResourceClient implementation.
    void notifyFinished(Resource* resource) override
    {
        resource->removeClient(m_dummyClient);
        resource->removeClient(this);
    }
    String debugName() const override { return "RemovingClient"; }
    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_dummyClient);
    }

private:
    Member<DummyClient> m_dummyClient;
};

TEST(RawResourceTest, RemoveClientDuringCallback)
{
    Resource* raw = RawResource::create(ResourceRequest("data:text/html,"), Resource::Raw);

    // Create a non-null response.
    ResourceResponse response = raw->response();
    response.setURL(KURL(ParsedURLString, "http://600.613/"));
    raw->setResponse(response);
    raw->finish();
    EXPECT_FALSE(raw->response().isNull());

    Persistent<DummyClient> dummyClient = new DummyClient();
    Persistent<RemovingClient> removingClient = new RemovingClient(dummyClient.get());
    raw->addClient(dummyClient);
    raw->addClient(removingClient);
    testing::runPendingTasks();
    EXPECT_FALSE(raw->hasClientsOrObservers());
}

TEST(RawResourceTest, CanReuseDevToolsEmulateNetworkConditionsClientIdHeader)
{
    ResourceRequest request("data:text/html,");
    request.setHTTPHeaderField(HTTPNames::X_DevTools_Emulate_Network_Conditions_Client_Id, "Foo");
    Resource* raw = RawResource::create(request, Resource::Raw);
    EXPECT_TRUE(raw->canReuse(ResourceRequest("data:text/html,")));
}

} // namespace blink
