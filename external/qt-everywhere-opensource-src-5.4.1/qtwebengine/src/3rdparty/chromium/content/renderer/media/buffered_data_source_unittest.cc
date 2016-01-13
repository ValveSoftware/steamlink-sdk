// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "content/public/common/url_constants.h"
#include "content/renderer/media/buffered_data_source.h"
#include "content/renderer/media/test_response_generator.h"
#include "content/test/mock_webframeclient.h"
#include "content/test/mock_weburlloader.h"
#include "media/base/media_log.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

using ::testing::_;
using ::testing::Assign;
using ::testing::Invoke;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::StrictMock;

using blink::WebLocalFrame;
using blink::WebString;
using blink::WebURLLoader;
using blink::WebURLResponse;
using blink::WebView;

namespace content {

class MockBufferedDataSourceHost : public BufferedDataSourceHost {
 public:
  MockBufferedDataSourceHost() {}
  virtual ~MockBufferedDataSourceHost() {}

  MOCK_METHOD1(SetTotalBytes, void(int64 total_bytes));
  MOCK_METHOD2(AddBufferedByteRange, void(int64 start, int64 end));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBufferedDataSourceHost);
};

// Overrides CreateResourceLoader() to permit injecting a MockWebURLLoader.
// Also keeps track of whether said MockWebURLLoader is actively loading.
class MockBufferedDataSource : public BufferedDataSource {
 public:
  MockBufferedDataSource(
      const GURL& url,
      const scoped_refptr<base::MessageLoopProxy>& message_loop,
      WebLocalFrame* frame,
      BufferedDataSourceHost* host)
      : BufferedDataSource(url,
                           BufferedResourceLoader::kUnspecified,
                           message_loop,
                           frame,
                           new media::MediaLog(),
                           host,
                           base::Bind(&MockBufferedDataSource::set_downloading,
                                      base::Unretained(this))),
        downloading_(false),
        loading_(false) {}
  virtual ~MockBufferedDataSource() {}

  MOCK_METHOD2(CreateResourceLoader, BufferedResourceLoader*(int64, int64));
  BufferedResourceLoader* CreateMockResourceLoader(int64 first_byte_position,
                                                   int64 last_byte_position) {
    CHECK(!loading_) << "Previous resource load wasn't cancelled";

    BufferedResourceLoader* loader =
        BufferedDataSource::CreateResourceLoader(first_byte_position,
                                                 last_byte_position);

    // Keep track of active loading state via loadAsynchronously() and cancel().
    NiceMock<MockWebURLLoader>* url_loader = new NiceMock<MockWebURLLoader>();
    ON_CALL(*url_loader, loadAsynchronously(_, _))
        .WillByDefault(Assign(&loading_, true));
    ON_CALL(*url_loader, cancel())
        .WillByDefault(Assign(&loading_, false));

    // |test_loader_| will be used when Start() is called.
    loader->test_loader_ = scoped_ptr<WebURLLoader>(url_loader);
    return loader;
  }

  bool loading() { return loading_; }
  void set_loading(bool loading) { loading_ = loading; }
  bool downloading() { return downloading_; }
  void set_downloading(bool downloading) { downloading_ = downloading; }

 private:
  // Whether the resource is downloading or deferred.
  bool downloading_;

  // Whether the resource load has starting loading but yet to been cancelled.
  bool loading_;

  DISALLOW_COPY_AND_ASSIGN(MockBufferedDataSource);
};

static const int64 kFileSize = 5000000;
static const int64 kFarReadPosition = 4000000;
static const int kDataSize = 1024;

static const char kHttpUrl[] = "http://localhost/foo.webm";
static const char kFileUrl[] = "file:///tmp/bar.webm";

class BufferedDataSourceTest : public testing::Test {
 public:
  BufferedDataSourceTest()
      : view_(WebView::create(NULL)),
        frame_(WebLocalFrame::create(&client_)),
        preload_(AUTO) {
    view_->setMainFrame(frame_);
  }

  virtual ~BufferedDataSourceTest() {
    view_->close();
    frame_->close();
  }

  MOCK_METHOD1(OnInitialize, void(bool));

  void Initialize(const char* url, bool expected) {
    GURL gurl(url);
    data_source_.reset(
        new MockBufferedDataSource(gurl,
                                   message_loop_.message_loop_proxy(),
                                   view_->mainFrame()->toWebLocalFrame(),
                                   &host_));
    data_source_->SetPreload(preload_);

    response_generator_.reset(new TestResponseGenerator(gurl, kFileSize));
    ExpectCreateResourceLoader();
    EXPECT_CALL(*this, OnInitialize(expected));
    data_source_->Initialize(base::Bind(&BufferedDataSourceTest::OnInitialize,
                                        base::Unretained(this)));
    message_loop_.RunUntilIdle();

    bool is_http = gurl.SchemeIsHTTPOrHTTPS();
    EXPECT_EQ(data_source_->downloading(), is_http);
  }

  // Helper to initialize tests with a valid 200 response.
  void InitializeWith200Response() {
    Initialize(kHttpUrl, true);

    EXPECT_CALL(host_, SetTotalBytes(response_generator_->content_length()));
    Respond(response_generator_->Generate200());
  }

  // Helper to initialize tests with a valid 206 response.
  void InitializeWith206Response() {
    Initialize(kHttpUrl, true);

    EXPECT_CALL(host_, SetTotalBytes(response_generator_->content_length()));
    Respond(response_generator_->Generate206(0));
  }

  // Helper to initialize tests with a valid file:// response.
  void InitializeWithFileResponse() {
    Initialize(kFileUrl, true);

    EXPECT_CALL(host_, SetTotalBytes(kFileSize));
    EXPECT_CALL(host_, AddBufferedByteRange(0, kFileSize));
    Respond(response_generator_->GenerateFileResponse(0));
  }

  // Stops any active loaders and shuts down the data source.
  //
  // This typically happens when the page is closed and for our purposes is
  // appropriate to do when tearing down a test.
  void Stop() {
    if (data_source_->loading()) {
      loader()->didFail(url_loader(), response_generator_->GenerateError());
      message_loop_.RunUntilIdle();
    }

    data_source_->Stop(media::NewExpectedClosure());
    message_loop_.RunUntilIdle();
  }

  void ExpectCreateResourceLoader() {
    EXPECT_CALL(*data_source_, CreateResourceLoader(_, _))
        .WillOnce(Invoke(data_source_.get(),
                         &MockBufferedDataSource::CreateMockResourceLoader));
    message_loop_.RunUntilIdle();
  }

  void Respond(const WebURLResponse& response) {
    loader()->didReceiveResponse(url_loader(), response);
    message_loop_.RunUntilIdle();
  }

  void ReceiveData(int size) {
    scoped_ptr<char[]> data(new char[size]);
    memset(data.get(), 0xA5, size);  // Arbitrary non-zero value.

    loader()->didReceiveData(url_loader(), data.get(), size, size);
    message_loop_.RunUntilIdle();
  }

  void FinishLoading() {
    data_source_->set_loading(false);
    loader()->didFinishLoading(url_loader(), 0, -1);
    message_loop_.RunUntilIdle();
  }

  MOCK_METHOD1(ReadCallback, void(int size));

  void ReadAt(int64 position) {
    data_source_->Read(position, kDataSize, buffer_,
                       base::Bind(&BufferedDataSourceTest::ReadCallback,
                                  base::Unretained(this)));
    message_loop_.RunUntilIdle();
  }

  // Accessors for private variables on |data_source_|.
  BufferedResourceLoader* loader() {
    return data_source_->loader_.get();
  }
  WebURLLoader* url_loader() {
    return loader()->active_loader_->loader_.get();
  }

  Preload preload() { return data_source_->preload_; }
  void set_preload(Preload preload) { preload_ = preload; }
  BufferedResourceLoader::DeferStrategy defer_strategy() {
    return loader()->defer_strategy_;
  }
  int data_source_bitrate() { return data_source_->bitrate_; }
  int data_source_playback_rate() { return data_source_->playback_rate_; }
  int loader_bitrate() { return loader()->bitrate_; }
  int loader_playback_rate() { return loader()->playback_rate_; }
  bool is_local_source() { return data_source_->assume_fully_buffered(); }
  void set_might_be_reused_from_cache_in_future(bool value) {
    loader()->might_be_reused_from_cache_in_future_ = value;
  }

  scoped_ptr<MockBufferedDataSource> data_source_;

  scoped_ptr<TestResponseGenerator> response_generator_;
  MockWebFrameClient client_;
  WebView* view_;
  WebLocalFrame* frame_;

  StrictMock<MockBufferedDataSourceHost> host_;
  base::MessageLoop message_loop_;

 private:
  // Used for calling BufferedDataSource::Read().
  uint8 buffer_[kDataSize];

  Preload preload_;

  DISALLOW_COPY_AND_ASSIGN(BufferedDataSourceTest);
};

TEST_F(BufferedDataSourceTest, Range_Supported) {
  InitializeWith206Response();

  EXPECT_TRUE(data_source_->loading());
  EXPECT_FALSE(data_source_->IsStreaming());
  Stop();
}

TEST_F(BufferedDataSourceTest, Range_InstanceSizeUnknown) {
  Initialize(kHttpUrl, true);

  Respond(response_generator_->Generate206(
      0, TestResponseGenerator::kNoContentRangeInstanceSize));

  EXPECT_TRUE(data_source_->loading());
  EXPECT_TRUE(data_source_->IsStreaming());
  Stop();
}

TEST_F(BufferedDataSourceTest, Range_NotFound) {
  Initialize(kHttpUrl, false);
  Respond(response_generator_->Generate404());

  EXPECT_FALSE(data_source_->loading());
  Stop();
}

TEST_F(BufferedDataSourceTest, Range_NotSupported) {
  InitializeWith200Response();

  EXPECT_TRUE(data_source_->loading());
  EXPECT_TRUE(data_source_->IsStreaming());
  Stop();
}

// Special carve-out for Apache versions that choose to return a 200 for
// Range:0- ("because it's more efficient" than a 206)
TEST_F(BufferedDataSourceTest, Range_SupportedButReturned200) {
  Initialize(kHttpUrl, true);
  EXPECT_CALL(host_, SetTotalBytes(response_generator_->content_length()));
  WebURLResponse response = response_generator_->Generate200();
  response.setHTTPHeaderField(WebString::fromUTF8("Accept-Ranges"),
                              WebString::fromUTF8("bytes"));
  Respond(response);

  EXPECT_TRUE(data_source_->loading());
  EXPECT_FALSE(data_source_->IsStreaming());
  Stop();
}

TEST_F(BufferedDataSourceTest, Range_MissingContentRange) {
  Initialize(kHttpUrl, false);
  Respond(response_generator_->Generate206(
      0, TestResponseGenerator::kNoContentRange));

  EXPECT_FALSE(data_source_->loading());
  Stop();
}

TEST_F(BufferedDataSourceTest, Range_MissingContentLength) {
  Initialize(kHttpUrl, true);

  // It'll manage without a Content-Length response.
  EXPECT_CALL(host_, SetTotalBytes(response_generator_->content_length()));
  Respond(response_generator_->Generate206(
      0, TestResponseGenerator::kNoContentLength));

  EXPECT_TRUE(data_source_->loading());
  EXPECT_FALSE(data_source_->IsStreaming());
  Stop();
}

TEST_F(BufferedDataSourceTest, Range_WrongContentRange) {
  Initialize(kHttpUrl, false);

  // Now it's done and will fail.
  Respond(response_generator_->Generate206(1337));

  EXPECT_FALSE(data_source_->loading());
  Stop();
}

// Test the case where the initial response from the server indicates that
// Range requests are supported, but a later request prove otherwise.
TEST_F(BufferedDataSourceTest, Range_ServerLied) {
  InitializeWith206Response();

  // Read causing a new request to be made -- we'll expect it to error.
  ExpectCreateResourceLoader();
  ReadAt(kFarReadPosition);

  // Return a 200 in response to a range request.
  EXPECT_CALL(*this, ReadCallback(media::DataSource::kReadError));
  Respond(response_generator_->Generate200());

  EXPECT_FALSE(data_source_->loading());
  Stop();
}

TEST_F(BufferedDataSourceTest, Http_AbortWhileReading) {
  InitializeWith206Response();

  // Make sure there's a pending read -- we'll expect it to error.
  ReadAt(0);

  // Abort!!!
  EXPECT_CALL(*this, ReadCallback(media::DataSource::kReadError));
  data_source_->Abort();
  message_loop_.RunUntilIdle();

  EXPECT_FALSE(data_source_->loading());
  Stop();
}

TEST_F(BufferedDataSourceTest, File_AbortWhileReading) {
  InitializeWithFileResponse();

  // Make sure there's a pending read -- we'll expect it to error.
  ReadAt(0);

  // Abort!!!
  EXPECT_CALL(*this, ReadCallback(media::DataSource::kReadError));
  data_source_->Abort();
  message_loop_.RunUntilIdle();

  EXPECT_FALSE(data_source_->loading());
  Stop();
}

TEST_F(BufferedDataSourceTest, Http_Retry) {
  InitializeWith206Response();

  // Read to advance our position.
  EXPECT_CALL(*this, ReadCallback(kDataSize));
  EXPECT_CALL(host_, AddBufferedByteRange(0, kDataSize - 1));
  ReadAt(0);
  ReceiveData(kDataSize);

  // Issue a pending read but terminate the connection to force a retry.
  ReadAt(kDataSize);
  ExpectCreateResourceLoader();
  FinishLoading();
  Respond(response_generator_->Generate206(kDataSize));

  // Complete the read.
  EXPECT_CALL(*this, ReadCallback(kDataSize));
  EXPECT_CALL(host_, AddBufferedByteRange(kDataSize, (kDataSize * 2) - 1));
  ReceiveData(kDataSize);

  EXPECT_TRUE(data_source_->loading());
  Stop();
}

TEST_F(BufferedDataSourceTest, File_Retry) {
  InitializeWithFileResponse();

  // Read to advance our position.
  EXPECT_CALL(*this, ReadCallback(kDataSize));
  ReadAt(0);
  ReceiveData(kDataSize);

  // Issue a pending read but terminate the connection to force a retry.
  ReadAt(kDataSize);
  ExpectCreateResourceLoader();
  FinishLoading();
  Respond(response_generator_->GenerateFileResponse(kDataSize));

  // Complete the read.
  EXPECT_CALL(*this, ReadCallback(kDataSize));
  ReceiveData(kDataSize);

  EXPECT_TRUE(data_source_->loading());
  Stop();
}

TEST_F(BufferedDataSourceTest, Http_TooManyRetries) {
  InitializeWith206Response();

  // Make sure there's a pending read -- we'll expect it to error.
  ReadAt(0);

  // It'll try three times.
  ExpectCreateResourceLoader();
  FinishLoading();
  Respond(response_generator_->Generate206(0));

  ExpectCreateResourceLoader();
  FinishLoading();
  Respond(response_generator_->Generate206(0));

  ExpectCreateResourceLoader();
  FinishLoading();
  Respond(response_generator_->Generate206(0));

  // It'll error after this.
  EXPECT_CALL(*this, ReadCallback(media::DataSource::kReadError));
  FinishLoading();

  EXPECT_FALSE(data_source_->loading());
  Stop();
}

TEST_F(BufferedDataSourceTest, File_TooManyRetries) {
  InitializeWithFileResponse();

  // Make sure there's a pending read -- we'll expect it to error.
  ReadAt(0);

  // It'll try three times.
  ExpectCreateResourceLoader();
  FinishLoading();
  Respond(response_generator_->GenerateFileResponse(0));

  ExpectCreateResourceLoader();
  FinishLoading();
  Respond(response_generator_->GenerateFileResponse(0));

  ExpectCreateResourceLoader();
  FinishLoading();
  Respond(response_generator_->GenerateFileResponse(0));

  // It'll error after this.
  EXPECT_CALL(*this, ReadCallback(media::DataSource::kReadError));
  FinishLoading();

  EXPECT_FALSE(data_source_->loading());
  Stop();
}

TEST_F(BufferedDataSourceTest, File_InstanceSizeUnknown) {
  Initialize(kFileUrl, false);
  EXPECT_FALSE(data_source_->downloading());

  Respond(response_generator_->GenerateFileResponse(-1));

  EXPECT_FALSE(data_source_->loading());
  Stop();
}

TEST_F(BufferedDataSourceTest, File_Successful) {
  InitializeWithFileResponse();

  EXPECT_TRUE(data_source_->loading());
  EXPECT_FALSE(data_source_->IsStreaming());
  Stop();
}

static void SetTrue(bool* value) {
  *value = true;
}

// This test makes sure that Stop() does not require a task to run on
// |message_loop_| before it calls its callback. This prevents accidental
// introduction of a pipeline teardown deadlock. The pipeline owner blocks
// the render message loop while waiting for Stop() to complete. Since this
// object runs on the render message loop, Stop() will not complete if it
// requires a task to run on the the message loop that is being blocked.
TEST_F(BufferedDataSourceTest, StopDoesNotUseMessageLoopForCallback) {
  InitializeWith206Response();

  // Stop() the data source, using a callback that lets us verify that it was
  // called before Stop() returns. This is to make sure that the callback does
  // not require |message_loop_| to execute tasks before being called.
  bool stop_done_called = false;
  EXPECT_TRUE(data_source_->loading());
  data_source_->Stop(base::Bind(&SetTrue, &stop_done_called));

  // Verify that the callback was called inside the Stop() call.
  EXPECT_TRUE(stop_done_called);
  message_loop_.RunUntilIdle();
}

TEST_F(BufferedDataSourceTest, StopDuringRead) {
  InitializeWith206Response();

  uint8 buffer[256];
  data_source_->Read(0, arraysize(buffer), buffer, base::Bind(
      &BufferedDataSourceTest::ReadCallback, base::Unretained(this)));

  // The outstanding read should fail before the stop callback runs.
  {
    InSequence s;
    EXPECT_CALL(*this, ReadCallback(media::DataSource::kReadError));
    data_source_->Stop(media::NewExpectedClosure());
  }
  message_loop_.RunUntilIdle();
}

TEST_F(BufferedDataSourceTest, DefaultValues) {
  InitializeWith206Response();

  // Ensure we have sane values for default loading scenario.
  EXPECT_EQ(AUTO, preload());
  EXPECT_EQ(BufferedResourceLoader::kCapacityDefer, defer_strategy());

  EXPECT_EQ(0, data_source_bitrate());
  EXPECT_EQ(0.0f, data_source_playback_rate());
  EXPECT_EQ(0, loader_bitrate());
  EXPECT_EQ(0.0f, loader_playback_rate());

  EXPECT_TRUE(data_source_->loading());
  Stop();
}

TEST_F(BufferedDataSourceTest, SetBitrate) {
  InitializeWith206Response();

  data_source_->SetBitrate(1234);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(1234, data_source_bitrate());
  EXPECT_EQ(1234, loader_bitrate());

  // Read so far ahead to cause the loader to get recreated.
  BufferedResourceLoader* old_loader = loader();
  ExpectCreateResourceLoader();
  ReadAt(kFarReadPosition);
  Respond(response_generator_->Generate206(kFarReadPosition));

  // Verify loader changed but still has same bitrate.
  EXPECT_NE(old_loader, loader());
  EXPECT_EQ(1234, loader_bitrate());

  EXPECT_TRUE(data_source_->loading());
  EXPECT_CALL(*this, ReadCallback(media::DataSource::kReadError));
  Stop();
}

TEST_F(BufferedDataSourceTest, MediaPlaybackRateChanged) {
  InitializeWith206Response();

  data_source_->MediaPlaybackRateChanged(2.0f);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(2.0f, data_source_playback_rate());
  EXPECT_EQ(2.0f, loader_playback_rate());

  // Read so far ahead to cause the loader to get recreated.
  BufferedResourceLoader* old_loader = loader();
  ExpectCreateResourceLoader();
  ReadAt(kFarReadPosition);
  Respond(response_generator_->Generate206(kFarReadPosition));

  // Verify loader changed but still has same playback rate.
  EXPECT_NE(old_loader, loader());

  EXPECT_TRUE(data_source_->loading());
  EXPECT_CALL(*this, ReadCallback(media::DataSource::kReadError));
  Stop();
}

TEST_F(BufferedDataSourceTest, Http_Read) {
  InitializeWith206Response();

  ReadAt(0);

  // Receive first half of the read.
  EXPECT_CALL(host_, AddBufferedByteRange(0, (kDataSize / 2) - 1));
  ReceiveData(kDataSize / 2);

  // Receive last half of the read.
  EXPECT_CALL(*this, ReadCallback(kDataSize));
  EXPECT_CALL(host_, AddBufferedByteRange(0, kDataSize - 1));
  ReceiveData(kDataSize / 2);

  EXPECT_TRUE(data_source_->downloading());
  Stop();
}

TEST_F(BufferedDataSourceTest, Http_Read_Seek) {
  InitializeWith206Response();

  // Read a bit from the beginning.
  ReadAt(0);
  EXPECT_CALL(*this, ReadCallback(kDataSize));
  EXPECT_CALL(host_, AddBufferedByteRange(0, kDataSize - 1));
  ReceiveData(kDataSize);

  // Simulate a seek by reading a bit beyond kDataSize.
  ReadAt(kDataSize * 2);

  // We receive data leading up to but not including our read.
  EXPECT_CALL(host_, AddBufferedByteRange(0, kDataSize * 2 - 1));
  ReceiveData(kDataSize);

  // We now receive the rest of the data for our read.
  EXPECT_CALL(*this, ReadCallback(kDataSize));
  EXPECT_CALL(host_, AddBufferedByteRange(0, kDataSize * 3 - 1));
  ReceiveData(kDataSize);

  EXPECT_TRUE(data_source_->downloading());
  Stop();
}

TEST_F(BufferedDataSourceTest, File_Read) {
  InitializeWithFileResponse();

  ReadAt(0);

  // Receive first half of the read but no buffering update.
  ReceiveData(kDataSize / 2);

  // Receive last half of the read but no buffering update.
  EXPECT_CALL(*this, ReadCallback(kDataSize));
  ReceiveData(kDataSize / 2);

  Stop();
}

TEST_F(BufferedDataSourceTest, Http_FinishLoading) {
  InitializeWith206Response();

  EXPECT_TRUE(data_source_->downloading());
  FinishLoading();
  EXPECT_FALSE(data_source_->downloading());

  Stop();
}

TEST_F(BufferedDataSourceTest, File_FinishLoading) {
  InitializeWithFileResponse();

  EXPECT_FALSE(data_source_->downloading());
  FinishLoading();
  EXPECT_FALSE(data_source_->downloading());

  Stop();
}

TEST_F(BufferedDataSourceTest, LocalResource_DeferStrategy) {
  InitializeWithFileResponse();

  EXPECT_EQ(AUTO, preload());
  EXPECT_TRUE(is_local_source());
  EXPECT_EQ(BufferedResourceLoader::kCapacityDefer, defer_strategy());

  data_source_->MediaIsPlaying();
  EXPECT_EQ(BufferedResourceLoader::kCapacityDefer, defer_strategy());

  data_source_->MediaIsPaused();
  EXPECT_EQ(BufferedResourceLoader::kCapacityDefer, defer_strategy());

  Stop();
}

TEST_F(BufferedDataSourceTest, LocalResource_PreloadMetadata_DeferStrategy) {
  set_preload(METADATA);
  InitializeWithFileResponse();

  EXPECT_EQ(METADATA, preload());
  EXPECT_TRUE(is_local_source());
  EXPECT_EQ(BufferedResourceLoader::kReadThenDefer, defer_strategy());

  data_source_->MediaIsPlaying();
  EXPECT_EQ(BufferedResourceLoader::kCapacityDefer, defer_strategy());

  data_source_->MediaIsPaused();
  EXPECT_EQ(BufferedResourceLoader::kCapacityDefer, defer_strategy());

  Stop();
}

TEST_F(BufferedDataSourceTest, ExternalResource_Reponse200_DeferStrategy) {
  InitializeWith200Response();

  EXPECT_EQ(AUTO, preload());
  EXPECT_FALSE(is_local_source());
  EXPECT_FALSE(loader()->range_supported());
  EXPECT_EQ(BufferedResourceLoader::kCapacityDefer, defer_strategy());

  data_source_->MediaIsPlaying();
  EXPECT_EQ(BufferedResourceLoader::kCapacityDefer, defer_strategy());

  data_source_->MediaIsPaused();
  EXPECT_EQ(BufferedResourceLoader::kCapacityDefer, defer_strategy());

  Stop();
}

TEST_F(BufferedDataSourceTest,
       ExternalResource_Response200_PreloadMetadata_DeferStrategy) {
  set_preload(METADATA);
  InitializeWith200Response();

  EXPECT_EQ(METADATA, preload());
  EXPECT_FALSE(is_local_source());
  EXPECT_FALSE(loader()->range_supported());
  EXPECT_EQ(BufferedResourceLoader::kReadThenDefer, defer_strategy());

  data_source_->MediaIsPlaying();
  EXPECT_EQ(BufferedResourceLoader::kCapacityDefer, defer_strategy());

  data_source_->MediaIsPaused();
  EXPECT_EQ(BufferedResourceLoader::kCapacityDefer, defer_strategy());

  Stop();
}

TEST_F(BufferedDataSourceTest, ExternalResource_Reponse206_DeferStrategy) {
  InitializeWith206Response();

  EXPECT_EQ(AUTO, preload());
  EXPECT_FALSE(is_local_source());
  EXPECT_TRUE(loader()->range_supported());
  EXPECT_EQ(BufferedResourceLoader::kCapacityDefer, defer_strategy());

  data_source_->MediaIsPlaying();
  EXPECT_EQ(BufferedResourceLoader::kCapacityDefer, defer_strategy());
  set_might_be_reused_from_cache_in_future(true);
  data_source_->MediaIsPaused();
  EXPECT_EQ(BufferedResourceLoader::kNeverDefer, defer_strategy());

  data_source_->MediaIsPlaying();
  EXPECT_EQ(BufferedResourceLoader::kCapacityDefer, defer_strategy());
  set_might_be_reused_from_cache_in_future(false);
  data_source_->MediaIsPaused();
  EXPECT_EQ(BufferedResourceLoader::kCapacityDefer, defer_strategy());

  Stop();
}

TEST_F(BufferedDataSourceTest,
       ExternalResource_Response206_PreloadMetadata_DeferStrategy) {
  set_preload(METADATA);
  InitializeWith206Response();

  EXPECT_EQ(METADATA, preload());
  EXPECT_FALSE(is_local_source());
  EXPECT_TRUE(loader()->range_supported());
  EXPECT_EQ(BufferedResourceLoader::kReadThenDefer, defer_strategy());

  data_source_->MediaIsPlaying();
  EXPECT_EQ(BufferedResourceLoader::kCapacityDefer, defer_strategy());
  set_might_be_reused_from_cache_in_future(true);
  data_source_->MediaIsPaused();
  EXPECT_EQ(BufferedResourceLoader::kNeverDefer, defer_strategy());

  data_source_->MediaIsPlaying();
  EXPECT_EQ(BufferedResourceLoader::kCapacityDefer, defer_strategy());
  set_might_be_reused_from_cache_in_future(false);
  data_source_->MediaIsPaused();
  EXPECT_EQ(BufferedResourceLoader::kCapacityDefer, defer_strategy());

  Stop();
}

}  // namespace content
