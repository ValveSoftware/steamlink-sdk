// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "content/child/request_extra_data.h"
#include "content/child/request_info.h"
#include "content/child/resource_dispatcher.h"
#include "content/common/resource_messages.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/child/request_peer.h"
#include "content/public/common/resource_response.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/child/resource_loader_bridge.h"
#include "webkit/common/appcache/appcache_interfaces.h"

using webkit_glue::ResourceLoaderBridge;

namespace content {

static const char kTestPageUrl[] = "http://www.google.com/";
static const char kTestPageHeaders[] =
  "HTTP/1.1 200 OK\nContent-Type:text/html\n\n";
static const char kTestPageMimeType[] = "text/html";
static const char kTestPageCharset[] = "";
static const char kTestPageContents[] =
  "<html><head><title>Google</title></head><body><h1>Google</h1></body></html>";
static const char kTestRedirectHeaders[] =
  "HTTP/1.1 302 Found\nLocation:http://www.google.com/\n\n";

// Listens for request response data and stores it so that it can be compared
// to the reference data.
class TestRequestPeer : public RequestPeer {
 public:
  TestRequestPeer(ResourceLoaderBridge* bridge)
      :  follow_redirects_(true),
         defer_on_redirect_(false),
         seen_redirects_(0),
         cancel_on_receive_response_(false),
         received_response_(false),
         total_encoded_data_length_(0),
         total_downloaded_data_length_(0),
         complete_(false),
         bridge_(bridge) {
  }

  virtual void OnUploadProgress(uint64 position, uint64 size) OVERRIDE {
  }

  virtual bool OnReceivedRedirect(const GURL& new_url,
                                  const GURL& new_first_party_for_cookies,
                                  const ResourceResponseInfo& info) OVERRIDE {
    ++seen_redirects_;
    if (defer_on_redirect_)
      bridge_->SetDefersLoading(true);
    return follow_redirects_;
  }

  virtual void OnReceivedResponse(const ResourceResponseInfo& info) OVERRIDE {
    EXPECT_FALSE(received_response_);
    received_response_ = true;
    if (cancel_on_receive_response_)
      bridge_->Cancel();
  }

  virtual void OnDownloadedData(int len, int encoded_data_length) OVERRIDE {
    total_downloaded_data_length_ += len;
    total_encoded_data_length_ += encoded_data_length;
  }

  virtual void OnReceivedData(const char* data,
                              int data_length,
                              int encoded_data_length) OVERRIDE {
    EXPECT_TRUE(received_response_);
    EXPECT_FALSE(complete_);
    data_.append(data, data_length);
    total_encoded_data_length_ += encoded_data_length;
  }

  virtual void OnCompletedRequest(
      int error_code,
      bool was_ignored_by_handler,
      bool stale_copy_in_cache,
      const std::string& security_info,
      const base::TimeTicks& completion_time,
      int64 total_transfer_size) OVERRIDE {
    EXPECT_TRUE(received_response_);
    EXPECT_FALSE(complete_);
    complete_ = true;
  }

  void set_follow_redirects(bool follow_redirects) {
    follow_redirects_ = follow_redirects;
  }

  void set_defer_on_redirect(bool defer_on_redirect) {
    defer_on_redirect_ = defer_on_redirect;
  }

  void set_cancel_on_receive_response(bool cancel_on_receive_response) {
    cancel_on_receive_response_ = cancel_on_receive_response;
  }

  int seen_redirects() const { return seen_redirects_; }

  bool received_response() const { return received_response_; }

  const std::string& data() const {
    return data_;
  }
  int total_encoded_data_length() const {
    return total_encoded_data_length_;
  }
  int total_downloaded_data_length() const {
    return total_downloaded_data_length_;
  }

  bool complete() const { return complete_; }

 private:
  // True if should follow redirects, false if should cancel them.
  bool follow_redirects_;
  // True if the request should be deferred on redirects.
  bool defer_on_redirect_;
  // Number of total redirects seen.
  int seen_redirects_;

  bool cancel_on_receive_response_;
  bool received_response_;

  // Data received.   If downloading to file, remains empty.
  std::string data_;
  // Total encoded data length, regardless of whether downloading to a file or
  // not.
  int total_encoded_data_length_;
  // Total length when downloading to a file.
  int total_downloaded_data_length_;

  bool complete_;

  ResourceLoaderBridge* bridge_;

  DISALLOW_COPY_AND_ASSIGN(TestRequestPeer);
};

// Sets up the message sender override for the unit test.
class ResourceDispatcherTest : public testing::Test, public IPC::Sender {
 public:
  ResourceDispatcherTest() : dispatcher_(this) {}

  virtual ~ResourceDispatcherTest() {
    STLDeleteContainerPairSecondPointers(shared_memory_map_.begin(),
                                         shared_memory_map_.end());
  }

  // Emulates IPC send operations (IPC::Sender) by adding
  // pending messages to the queue.
  virtual bool Send(IPC::Message* msg) OVERRIDE {
    message_queue_.push_back(IPC::Message(*msg));
    delete msg;
    return true;
  }

  size_t queued_messages() const { return message_queue_.size(); }

  // Returns the ID of the consumed request.  Can't make assumptions about the
  // ID, because numbering is based on a global.
  int ConsumeRequestResource() {
    if (message_queue_.empty()) {
      ADD_FAILURE() << "Missing resource request message";
      return -1;
    }

    ResourceHostMsg_RequestResource::Param params;
    if (ResourceHostMsg_RequestResource::ID != message_queue_[0].type() ||
        !ResourceHostMsg_RequestResource::Read(&message_queue_[0], &params)) {
      ADD_FAILURE() << "Expected ResourceHostMsg_RequestResource message";
      return -1;
    }
    ResourceHostMsg_Request request = params.c;
    EXPECT_EQ(kTestPageUrl, request.url.spec());
    message_queue_.erase(message_queue_.begin());
    return params.b;
  }

  void ConsumeFollowRedirect(int expected_request_id) {
    ASSERT_FALSE(message_queue_.empty());
    Tuple1<int> args;
    ASSERT_EQ(ResourceHostMsg_FollowRedirect::ID, message_queue_[0].type());
    ASSERT_TRUE(ResourceHostMsg_FollowRedirect::Read(
        &message_queue_[0], &args));
    EXPECT_EQ(expected_request_id, args.a);
    message_queue_.erase(message_queue_.begin());
  }

  void ConsumeDataReceived_ACK(int expected_request_id) {
    ASSERT_FALSE(message_queue_.empty());
    Tuple1<int> args;
    ASSERT_EQ(ResourceHostMsg_DataReceived_ACK::ID, message_queue_[0].type());
    ASSERT_TRUE(ResourceHostMsg_DataReceived_ACK::Read(
        &message_queue_[0], &args));
    EXPECT_EQ(expected_request_id, args.a);
    message_queue_.erase(message_queue_.begin());
  }

  void ConsumeDataDownloaded_ACK(int expected_request_id) {
    ASSERT_FALSE(message_queue_.empty());
    Tuple1<int> args;
    ASSERT_EQ(ResourceHostMsg_DataDownloaded_ACK::ID, message_queue_[0].type());
    ASSERT_TRUE(ResourceHostMsg_DataDownloaded_ACK::Read(
        &message_queue_[0], &args));
    EXPECT_EQ(expected_request_id, args.a);
    message_queue_.erase(message_queue_.begin());
  }

  void ConsumeReleaseDownloadedFile(int expected_request_id) {
    ASSERT_FALSE(message_queue_.empty());
    Tuple1<int> args;
    ASSERT_EQ(ResourceHostMsg_ReleaseDownloadedFile::ID,
              message_queue_[0].type());
    ASSERT_TRUE(ResourceHostMsg_ReleaseDownloadedFile::Read(
        &message_queue_[0], &args));
    EXPECT_EQ(expected_request_id, args.a);
    message_queue_.erase(message_queue_.begin());
  }

  void ConsumeCancelRequest(int expected_request_id) {
    ASSERT_FALSE(message_queue_.empty());
    Tuple1<int> args;
    ASSERT_EQ(ResourceHostMsg_CancelRequest::ID, message_queue_[0].type());
    ASSERT_TRUE(ResourceHostMsg_CancelRequest::Read(
        &message_queue_[0], &args));
    EXPECT_EQ(expected_request_id, args.a);
    message_queue_.erase(message_queue_.begin());
  }

  void NotifyReceivedRedirect(int request_id) {
    ResourceResponseHead head;
    std::string raw_headers(kTestRedirectHeaders);
    std::replace(raw_headers.begin(), raw_headers.end(), '\n', '\0');
    head.headers = new net::HttpResponseHeaders(raw_headers);
    head.error_code = net::OK;
    EXPECT_EQ(true, dispatcher_.OnMessageReceived(
        ResourceMsg_ReceivedRedirect(request_id, GURL(kTestPageUrl),
                                     GURL(kTestPageUrl), head)));
  }

  void NotifyReceivedResponse(int request_id) {
    ResourceResponseHead head;
    std::string raw_headers(kTestPageHeaders);
    std::replace(raw_headers.begin(), raw_headers.end(), '\n', '\0');
    head.headers = new net::HttpResponseHeaders(raw_headers);
    head.mime_type = kTestPageMimeType;
    head.charset = kTestPageCharset;
    head.error_code = net::OK;
    EXPECT_EQ(true,
              dispatcher_.OnMessageReceived(
                  ResourceMsg_ReceivedResponse(request_id, head)));
  }

  void NotifySetDataBuffer(int request_id, size_t buffer_size) {
    base::SharedMemory* shared_memory = new base::SharedMemory();
    ASSERT_FALSE(shared_memory_map_[request_id]);
    shared_memory_map_[request_id] = shared_memory;
    EXPECT_TRUE(shared_memory->CreateAndMapAnonymous(buffer_size));

    base::SharedMemoryHandle duplicate_handle;
    EXPECT_TRUE(shared_memory->ShareToProcess(
        base::Process::Current().handle(), &duplicate_handle));
    EXPECT_TRUE(dispatcher_.OnMessageReceived(
        ResourceMsg_SetDataBuffer(request_id, duplicate_handle,
                                  shared_memory->requested_size(), 0)));
  }

  void NotifyDataReceived(int request_id, std::string data) {
    ASSERT_LE(data.length(), shared_memory_map_[request_id]->requested_size());
    memcpy(shared_memory_map_[request_id]->memory(), data.c_str(),
           data.length());

    EXPECT_TRUE(dispatcher_.OnMessageReceived(
        ResourceMsg_DataReceived(request_id, 0, data.length(), data.length())));
  }

  void NotifyDataDownloaded(int request_id, int decoded_length,
                            int encoded_length) {
    EXPECT_TRUE(dispatcher_.OnMessageReceived(
        ResourceMsg_DataDownloaded(request_id, decoded_length,
                                   encoded_length)));
  }

  void NotifyRequestComplete(int request_id, size_t total_size) {
    ResourceMsg_RequestCompleteData request_complete_data;
    request_complete_data.error_code = net::OK;
    request_complete_data.was_ignored_by_handler = false;
    request_complete_data.exists_in_cache = false;
    request_complete_data.encoded_data_length = total_size;
    EXPECT_TRUE(dispatcher_.OnMessageReceived(
        ResourceMsg_RequestComplete(request_id, request_complete_data)));
  }

  ResourceLoaderBridge* CreateBridge() {
    return CreateBridgeInternal(false);
  }

  ResourceLoaderBridge* CreateBridgeForDownloadToFile() {
    return CreateBridgeInternal(true);
  }

  ResourceDispatcher* dispatcher() { return &dispatcher_; }

 private:
  ResourceLoaderBridge* CreateBridgeInternal(bool download_to_file) {
    RequestInfo request_info;
    request_info.method = "GET";
    request_info.url = GURL(kTestPageUrl);
    request_info.first_party_for_cookies = GURL(kTestPageUrl);
    request_info.referrer = GURL();
    request_info.headers = std::string();
    request_info.load_flags = 0;
    request_info.requestor_pid = 0;
    request_info.request_type = ResourceType::SUB_RESOURCE;
    request_info.appcache_host_id = appcache::kAppCacheNoHostId;
    request_info.routing_id = 0;
    request_info.download_to_file = download_to_file;
    RequestExtraData extra_data;
    request_info.extra_data = &extra_data;

    return dispatcher_.CreateBridge(request_info);
  }

  // Map of request IDs to shared memory.
  std::map<int, base::SharedMemory*> shared_memory_map_;

  std::vector<IPC::Message> message_queue_;
  ResourceDispatcher dispatcher_;
  base::MessageLoop message_loop_;
};

// Does a simple request and tests that the correct data is received.  Simulates
// two reads.
TEST_F(ResourceDispatcherTest, RoundTrip) {
  // Number of bytes received in the first read.
  const size_t kFirstReceiveSize = 2;
  ASSERT_LT(kFirstReceiveSize, strlen(kTestPageContents));

  scoped_ptr<ResourceLoaderBridge> bridge(CreateBridge());
  TestRequestPeer peer(bridge.get());

  EXPECT_TRUE(bridge->Start(&peer));
  int id = ConsumeRequestResource();
  EXPECT_EQ(0u, queued_messages());

  NotifyReceivedResponse(id);
  EXPECT_EQ(0u, queued_messages());
  EXPECT_TRUE(peer.received_response());

  NotifySetDataBuffer(id, strlen(kTestPageContents));
  NotifyDataReceived(id, std::string(kTestPageContents, kFirstReceiveSize));
  ConsumeDataReceived_ACK(id);
  EXPECT_EQ(0u, queued_messages());

  NotifyDataReceived(id, kTestPageContents + kFirstReceiveSize);
  ConsumeDataReceived_ACK(id);
  EXPECT_EQ(0u, queued_messages());

  NotifyRequestComplete(id, strlen(kTestPageContents));
  EXPECT_EQ(kTestPageContents, peer.data());
  EXPECT_TRUE(peer.complete());
  EXPECT_EQ(0u, queued_messages());
}

// Tests that the request IDs are straight when there are two interleaving
// requests.
TEST_F(ResourceDispatcherTest, MultipleRequests) {
  const char kTestPageContents2[] = "Not kTestPageContents";

  scoped_ptr<ResourceLoaderBridge> bridge1(CreateBridge());
  TestRequestPeer peer1(bridge1.get());
  scoped_ptr<ResourceLoaderBridge> bridge2(CreateBridge());
  TestRequestPeer peer2(bridge2.get());

  EXPECT_TRUE(bridge1->Start(&peer1));
  int id1 = ConsumeRequestResource();
  EXPECT_TRUE(bridge2->Start(&peer2));
  int id2 = ConsumeRequestResource();
  EXPECT_EQ(0u, queued_messages());

  NotifyReceivedResponse(id1);
  EXPECT_TRUE(peer1.received_response());
  EXPECT_FALSE(peer2.received_response());
  NotifyReceivedResponse(id2);
  EXPECT_TRUE(peer2.received_response());
  EXPECT_EQ(0u, queued_messages());

  NotifySetDataBuffer(id2, strlen(kTestPageContents2));
  NotifyDataReceived(id2, kTestPageContents2);
  ConsumeDataReceived_ACK(id2);
  NotifySetDataBuffer(id1, strlen(kTestPageContents));
  NotifyDataReceived(id1, kTestPageContents);
  ConsumeDataReceived_ACK(id1);
  EXPECT_EQ(0u, queued_messages());

  NotifyRequestComplete(id1, strlen(kTestPageContents));
  EXPECT_EQ(kTestPageContents, peer1.data());
  EXPECT_TRUE(peer1.complete());
  EXPECT_FALSE(peer2.complete());

  NotifyRequestComplete(id2, strlen(kTestPageContents2));
  EXPECT_EQ(kTestPageContents2, peer2.data());
  EXPECT_TRUE(peer2.complete());

  EXPECT_EQ(0u, queued_messages());
}

// Tests that the cancel method prevents other messages from being received.
TEST_F(ResourceDispatcherTest, Cancel) {
  scoped_ptr<ResourceLoaderBridge> bridge(CreateBridge());
  TestRequestPeer peer(bridge.get());

  EXPECT_TRUE(bridge->Start(&peer));
  int id = ConsumeRequestResource();
  EXPECT_EQ(0u, queued_messages());

  // Cancel the request.
  bridge->Cancel();
  ConsumeCancelRequest(id);

  // Any future messages related to the request should be ignored.
  NotifyReceivedResponse(id);
  NotifySetDataBuffer(id, strlen(kTestPageContents));
  NotifyDataReceived(id, kTestPageContents);
  NotifyRequestComplete(id, strlen(kTestPageContents));

  EXPECT_EQ(0u, queued_messages());
  EXPECT_EQ("", peer.data());
  EXPECT_FALSE(peer.received_response());
  EXPECT_FALSE(peer.complete());
}

// Tests that calling cancel during a callback works as expected.
TEST_F(ResourceDispatcherTest, CancelDuringCallback) {
  scoped_ptr<ResourceLoaderBridge> bridge(CreateBridge());
  TestRequestPeer peer(bridge.get());
  peer.set_cancel_on_receive_response(true);

  EXPECT_TRUE(bridge->Start(&peer));
  int id = ConsumeRequestResource();
  EXPECT_EQ(0u, queued_messages());

  NotifyReceivedResponse(id);
  EXPECT_TRUE(peer.received_response());
  // Request should have been cancelled.
  ConsumeCancelRequest(id);

  // Any future messages related to the request should be ignored.
  NotifySetDataBuffer(id, strlen(kTestPageContents));
  NotifyDataReceived(id, kTestPageContents);
  NotifyRequestComplete(id, strlen(kTestPageContents));

  EXPECT_EQ(0u, queued_messages());
  EXPECT_EQ("", peer.data());
  EXPECT_FALSE(peer.complete());
}

// Checks that redirects work as expected.
TEST_F(ResourceDispatcherTest, Redirect) {
  scoped_ptr<ResourceLoaderBridge> bridge(CreateBridge());
  TestRequestPeer peer(bridge.get());

  EXPECT_TRUE(bridge->Start(&peer));
  int id = ConsumeRequestResource();

  NotifyReceivedRedirect(id);
  ConsumeFollowRedirect(id);
  EXPECT_EQ(1, peer.seen_redirects());

  NotifyReceivedRedirect(id);
  ConsumeFollowRedirect(id);
  EXPECT_EQ(2, peer.seen_redirects());

  NotifyReceivedResponse(id);
  EXPECT_TRUE(peer.received_response());

  NotifySetDataBuffer(id, strlen(kTestPageContents));
  NotifyDataReceived(id, kTestPageContents);
  ConsumeDataReceived_ACK(id);

  NotifyRequestComplete(id, strlen(kTestPageContents));
  EXPECT_EQ(kTestPageContents, peer.data());
  EXPECT_TRUE(peer.complete());
  EXPECT_EQ(0u, queued_messages());
  EXPECT_EQ(2, peer.seen_redirects());
}

// Tests that that cancelling during a redirect method prevents other messages
// from being received.
TEST_F(ResourceDispatcherTest, CancelDuringRedirect) {
  scoped_ptr<ResourceLoaderBridge> bridge(CreateBridge());
  TestRequestPeer peer(bridge.get());
  peer.set_follow_redirects(false);

  EXPECT_TRUE(bridge->Start(&peer));
  int id = ConsumeRequestResource();
  EXPECT_EQ(0u, queued_messages());

  // Redirect the request, which triggers a cancellation.
  NotifyReceivedRedirect(id);
  ConsumeCancelRequest(id);
  EXPECT_EQ(1, peer.seen_redirects());
  EXPECT_EQ(0u, queued_messages());

  // Any future messages related to the request should be ignored.  In practice,
  // only the NotifyRequestComplete should be received after this point.
  NotifyReceivedRedirect(id);
  NotifyReceivedResponse(id);
  NotifySetDataBuffer(id, strlen(kTestPageContents));
  NotifyDataReceived(id, kTestPageContents);
  NotifyRequestComplete(id, strlen(kTestPageContents));

  EXPECT_EQ(0u, queued_messages());
  EXPECT_EQ("", peer.data());
  EXPECT_FALSE(peer.complete());
  EXPECT_EQ(1, peer.seen_redirects());
}

// Checks that deferring a request delays messages until it's resumed.
TEST_F(ResourceDispatcherTest, Defer) {
  scoped_ptr<ResourceLoaderBridge> bridge(CreateBridge());
  TestRequestPeer peer(bridge.get());

  EXPECT_TRUE(bridge->Start(&peer));
  int id = ConsumeRequestResource();
  EXPECT_EQ(0u, queued_messages());

  bridge->SetDefersLoading(true);
  NotifyReceivedResponse(id);
  NotifySetDataBuffer(id, strlen(kTestPageContents));
  NotifyDataReceived(id, kTestPageContents);
  NotifyRequestComplete(id, strlen(kTestPageContents));

  // None of the messages should have been processed yet, so no queued messages
  // to the browser process, and no data received by the peer.
  EXPECT_EQ(0u, queued_messages());
  EXPECT_EQ("", peer.data());
  EXPECT_FALSE(peer.complete());
  EXPECT_EQ(0, peer.seen_redirects());

  // Resuming the request should asynchronously unleash the deferred messages.
  bridge->SetDefersLoading(false);
  base::RunLoop().RunUntilIdle();

  ConsumeDataReceived_ACK(id);
  EXPECT_EQ(0u, queued_messages());
  EXPECT_TRUE(peer.received_response());
  EXPECT_EQ(kTestPageContents, peer.data());
  EXPECT_TRUE(peer.complete());
}

// Checks that deferring a request during a redirect delays messages until it's
// resumed.
TEST_F(ResourceDispatcherTest, DeferOnRedirect) {
  scoped_ptr<ResourceLoaderBridge> bridge(CreateBridge());
  TestRequestPeer peer(bridge.get());
  peer.set_defer_on_redirect(true);

  EXPECT_TRUE(bridge->Start(&peer));
  int id = ConsumeRequestResource();
  EXPECT_EQ(0u, queued_messages());

  // The request should be deferred during the redirect, including the message
  // to follow the redirect.
  NotifyReceivedRedirect(id);
  NotifyReceivedResponse(id);
  NotifySetDataBuffer(id, strlen(kTestPageContents));
  NotifyDataReceived(id, kTestPageContents);
  NotifyRequestComplete(id, strlen(kTestPageContents));

  // None of the messages should have been processed yet, so no queued messages
  // to the browser process, and no data received by the peer.
  EXPECT_EQ(0u, queued_messages());
  EXPECT_EQ("", peer.data());
  EXPECT_FALSE(peer.complete());
  EXPECT_EQ(1, peer.seen_redirects());

  // Resuming the request should asynchronously unleash the deferred messages.
  bridge->SetDefersLoading(false);
  base::RunLoop().RunUntilIdle();

  ConsumeFollowRedirect(id);
  ConsumeDataReceived_ACK(id);

  EXPECT_EQ(0u, queued_messages());
  EXPECT_TRUE(peer.received_response());
  EXPECT_EQ(kTestPageContents, peer.data());
  EXPECT_TRUE(peer.complete());
  EXPECT_EQ(1, peer.seen_redirects());
}

// Checks that a deferred request that's cancelled doesn't receive any messages.
TEST_F(ResourceDispatcherTest, CancelDeferredRequest) {
  scoped_ptr<ResourceLoaderBridge> bridge(CreateBridge());
  TestRequestPeer peer(bridge.get());

  EXPECT_TRUE(bridge->Start(&peer));
  int id = ConsumeRequestResource();
  EXPECT_EQ(0u, queued_messages());

  bridge->SetDefersLoading(true);
  NotifyReceivedRedirect(id);
  bridge->Cancel();
  ConsumeCancelRequest(id);

  NotifyRequestComplete(id, 0);
  base::RunLoop().RunUntilIdle();

  // None of the messages should have been processed.
  EXPECT_EQ(0u, queued_messages());
  EXPECT_EQ("", peer.data());
  EXPECT_FALSE(peer.complete());
  EXPECT_EQ(0, peer.seen_redirects());
}

TEST_F(ResourceDispatcherTest, DownloadToFile) {
  scoped_ptr<ResourceLoaderBridge> bridge(CreateBridgeForDownloadToFile());
  TestRequestPeer peer(bridge.get());
  const int kDownloadedIncrement = 100;
  const int kEncodedIncrement = 50;

  EXPECT_TRUE(bridge->Start(&peer));
  int id = ConsumeRequestResource();
  EXPECT_EQ(0u, queued_messages());

  NotifyReceivedResponse(id);
  EXPECT_EQ(0u, queued_messages());
  EXPECT_TRUE(peer.received_response());

  int expected_total_downloaded_length = 0;
  int expected_total_encoded_length = 0;
  for (int i = 0; i < 10; ++i) {
    NotifyDataDownloaded(id, kDownloadedIncrement, kEncodedIncrement);
    ConsumeDataDownloaded_ACK(id);
    expected_total_downloaded_length += kDownloadedIncrement;
    expected_total_encoded_length += kEncodedIncrement;
    EXPECT_EQ(expected_total_downloaded_length,
              peer.total_downloaded_data_length());
    EXPECT_EQ(expected_total_encoded_length, peer.total_encoded_data_length());
  }

  NotifyRequestComplete(id, strlen(kTestPageContents));
  EXPECT_EQ("", peer.data());
  EXPECT_TRUE(peer.complete());
  EXPECT_EQ(0u, queued_messages());

  bridge.reset();
  ConsumeReleaseDownloadedFile(id);
  EXPECT_EQ(0u, queued_messages());
  EXPECT_EQ(expected_total_downloaded_length,
            peer.total_downloaded_data_length());
  EXPECT_EQ(expected_total_encoded_length, peer.total_encoded_data_length());
}

// Make sure that when a download to file is cancelled, the file is destroyed.
TEST_F(ResourceDispatcherTest, CancelDownloadToFile) {
  scoped_ptr<ResourceLoaderBridge> bridge(CreateBridgeForDownloadToFile());
  TestRequestPeer peer(bridge.get());

  EXPECT_TRUE(bridge->Start(&peer));
  int id = ConsumeRequestResource();
  EXPECT_EQ(0u, queued_messages());

  NotifyReceivedResponse(id);
  EXPECT_EQ(0u, queued_messages());
  EXPECT_TRUE(peer.received_response());

  // Cancelling the request deletes the file.
  bridge->Cancel();
  ConsumeCancelRequest(id);
  ConsumeReleaseDownloadedFile(id);

  // Deleting the bridge shouldn't send another message to delete the file.
  bridge.reset();
  EXPECT_EQ(0u, queued_messages());
}

TEST_F(ResourceDispatcherTest, Cookies) {
  // FIXME
}

TEST_F(ResourceDispatcherTest, SerializedPostData) {
  // FIXME
}

class TimeConversionTest : public ResourceDispatcherTest,
                           public RequestPeer {
 public:
  virtual bool Send(IPC::Message* msg) OVERRIDE {
    delete msg;
    return true;
  }

  void PerformTest(const ResourceResponseHead& response_head) {
    scoped_ptr<ResourceLoaderBridge> bridge(CreateBridge());
    bridge->Start(this);

    dispatcher()->OnMessageReceived(
        ResourceMsg_ReceivedResponse(0, response_head));
  }

  // RequestPeer methods.
  virtual void OnUploadProgress(uint64 position, uint64 size) OVERRIDE {
  }

  virtual bool OnReceivedRedirect(const GURL& new_url,
                                  const GURL& new_first_party_for_cookies,
                                  const ResourceResponseInfo& info) OVERRIDE {
    return true;
  }

  virtual void OnReceivedResponse(const ResourceResponseInfo& info) OVERRIDE {
    response_info_ = info;
  }

  virtual void OnDownloadedData(int len, int encoded_data_length) OVERRIDE {
  }

  virtual void OnReceivedData(const char* data,
                              int data_length,
                              int encoded_data_length) OVERRIDE {
  }

  virtual void OnCompletedRequest(
      int error_code,
      bool was_ignored_by_handler,
      bool stale_copy_in_cache,
      const std::string& security_info,
      const base::TimeTicks& completion_time,
      int64 total_transfer_size) OVERRIDE {
  }

  const ResourceResponseInfo& response_info() const { return response_info_; }

 private:
  ResourceResponseInfo response_info_;
};

// TODO(simonjam): Enable this when 10829031 lands.
TEST_F(TimeConversionTest, DISABLED_ProperlyInitialized) {
  ResourceResponseHead response_head;
  response_head.error_code = net::OK;
  response_head.request_start = base::TimeTicks::FromInternalValue(5);
  response_head.response_start = base::TimeTicks::FromInternalValue(15);
  response_head.load_timing.request_start_time = base::Time::Now();
  response_head.load_timing.request_start =
      base::TimeTicks::FromInternalValue(10);
  response_head.load_timing.connect_timing.connect_start =
      base::TimeTicks::FromInternalValue(13);

  PerformTest(response_head);

  EXPECT_LT(base::TimeTicks(), response_info().load_timing.request_start);
  EXPECT_EQ(base::TimeTicks(),
            response_info().load_timing.connect_timing.dns_start);
  EXPECT_LE(response_head.load_timing.request_start,
            response_info().load_timing.connect_timing.connect_start);
}

TEST_F(TimeConversionTest, PartiallyInitialized) {
  ResourceResponseHead response_head;
  response_head.error_code = net::OK;
  response_head.request_start = base::TimeTicks::FromInternalValue(5);
  response_head.response_start = base::TimeTicks::FromInternalValue(15);

  PerformTest(response_head);

  EXPECT_EQ(base::TimeTicks(), response_info().load_timing.request_start);
  EXPECT_EQ(base::TimeTicks(),
            response_info().load_timing.connect_timing.dns_start);
}

TEST_F(TimeConversionTest, NotInitialized) {
  ResourceResponseHead response_head;
  response_head.error_code = net::OK;

  PerformTest(response_head);

  EXPECT_EQ(base::TimeTicks(), response_info().load_timing.request_start);
  EXPECT_EQ(base::TimeTicks(),
            response_info().load_timing.connect_timing.dns_start);
}

}  // namespace content
