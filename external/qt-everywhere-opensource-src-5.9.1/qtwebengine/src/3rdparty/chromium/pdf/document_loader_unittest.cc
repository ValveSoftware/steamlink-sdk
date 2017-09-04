// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/document_loader.h"

#include <memory>
#include <string>

#include "base/logging.h"
#include "pdf/url_loader_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/range/range.h"

using ::testing::_;
using ::testing::Mock;
using ::testing::Sequence;
using ::testing::NiceMock;
using ::testing::Return;

namespace chrome_pdf {

namespace {

class TestURLLoader : public URLLoaderWrapper {
 public:
  class LoaderData {
   public:
    LoaderData() {}
    ~LoaderData() {
      // We should call callbacks to prevent memory leaks.
      // The callbacks don't do anything, because the objects that created the
      // callbacks have been destroyed.
      if (IsWaitRead())
        CallReadCallback(-1);
      if (IsWaitOpen())
        CallOpenCallback(-1);
    }

    int content_length() const { return content_length_; }
    void set_content_length(int content_length) {
      content_length_ = content_length;
    }
    bool accept_ranges_bytes() const { return accept_ranges_bytes_; }
    void set_accept_ranges_bytes(bool accept_ranges_bytes) {
      accept_ranges_bytes_ = accept_ranges_bytes;
    }
    bool content_encoded() const { return content_encoded_; }
    void set_content_encoded(bool content_encoded) {
      content_encoded_ = content_encoded;
    }
    const std::string& content_type() const { return content_type_; }
    void set_content_type(const std::string& content_type) {
      content_type_ = content_type;
    }
    const std::string& content_disposition() const {
      return content_disposition_;
    }
    void set_content_disposition(const std::string& content_disposition) {
      content_disposition_ = content_disposition;
    }
    const std::string& multipart_boundary() const {
      return multipart_boundary_;
    }
    void set_multipart_boundary(const std::string& multipart_boundary) {
      multipart_boundary_ = multipart_boundary;
    }
    const gfx::Range& byte_range() const { return byte_range_; }
    void set_byte_range(const gfx::Range& byte_range) {
      byte_range_ = byte_range;
    }
    bool is_multipart() const { return is_multipart_; }
    void set_is_multipart(bool is_multipart) { is_multipart_ = is_multipart; }
    int status_code() const { return status_code_; }
    void set_status_code(int status_code) { status_code_ = status_code; }
    bool closed() const { return closed_; }
    void set_closed(bool closed) { closed_ = closed; }
    const gfx::Range& open_byte_range() const { return open_byte_range_; }
    void set_open_byte_range(const gfx::Range& open_byte_range) {
      open_byte_range_ = open_byte_range;
    }

    bool IsWaitRead() const { return !did_read_callback_.IsOptional(); }
    bool IsWaitOpen() const { return !did_open_callback_.IsOptional(); }
    char* buffer() const { return buffer_; }
    int buffer_size() const { return buffer_size_; }

    void SetReadCallback(const pp::CompletionCallback& read_callback,
                         char* buffer,
                         int buffer_size) {
      did_read_callback_ = read_callback;
      buffer_ = buffer;
      buffer_size_ = buffer_size;
    }

    void SetOpenCallback(const pp::CompletionCallback& open_callback,
                         gfx::Range req_byte_range) {
      did_open_callback_ = open_callback;
      set_open_byte_range(req_byte_range);
    }

    void CallOpenCallback(int result) {
      DCHECK(IsWaitOpen());
      did_open_callback_.RunAndClear(result);
    }

    void CallReadCallback(int result) {
      DCHECK(IsWaitRead());
      did_read_callback_.RunAndClear(result);
    }

   private:
    pp::CompletionCallback did_open_callback_;
    pp::CompletionCallback did_read_callback_;
    char* buffer_ = nullptr;
    int buffer_size_ = 0;

    int content_length_ = -1;
    bool accept_ranges_bytes_ = false;
    bool content_encoded_ = false;
    std::string content_type_;
    std::string content_disposition_;
    std::string multipart_boundary_;
    gfx::Range byte_range_ = gfx::Range::InvalidRange();
    bool is_multipart_ = false;
    int status_code_ = 0;
    bool closed_ = true;
    gfx::Range open_byte_range_ = gfx::Range::InvalidRange();

    DISALLOW_COPY_AND_ASSIGN(LoaderData);
  };

  explicit TestURLLoader(LoaderData* data) : data_(data) {
    data_->set_closed(false);
  }

  ~TestURLLoader() override { Close(); }

  int GetContentLength() const override { return data_->content_length(); }

  bool IsAcceptRangesBytes() const override {
    return data_->accept_ranges_bytes();
  }

  bool IsContentEncoded() const override { return data_->content_encoded(); }

  std::string GetContentType() const override { return data_->content_type(); }

  std::string GetContentDisposition() const override {
    return data_->content_disposition();
  }

  int GetStatusCode() const override { return data_->status_code(); }

  bool IsMultipart() const override { return data_->is_multipart(); }

  bool GetByteRange(int* start, int* end) const override {
    *start = data_->byte_range().start();
    *end = data_->byte_range().end();
    return data_->byte_range().IsValid();
  }

  void Close() override { data_->set_closed(true); }

  void OpenRange(const std::string& url,
                 const std::string& referrer_url,
                 uint32_t position,
                 uint32_t size,
                 const pp::CompletionCallback& cc) override {
    data_->SetOpenCallback(cc, gfx::Range(position, position + size));
  }

  void ReadResponseBody(char* buffer,
                        int buffer_size,
                        const pp::CompletionCallback& cc) override {
    data_->SetReadCallback(cc, buffer, buffer_size);
  }

  bool GetDownloadProgress(int64_t* bytes_received,
                           int64_t* total_bytes_to_be_received) const override {
    return false;
  }

 private:
  LoaderData* data_;

  DISALLOW_COPY_AND_ASSIGN(TestURLLoader);
};

class TestClient : public DocumentLoader::Client {
 public:
  TestClient() { full_page_loader_data()->set_content_type("application/pdf"); }
  ~TestClient() override {}

  // DocumentLoader::Client overrides:
  pp::Instance* GetPluginInstance() override { return nullptr; }
  std::unique_ptr<URLLoaderWrapper> CreateURLLoader() override {
    return std::unique_ptr<URLLoaderWrapper>(
        new TestURLLoader(partial_loader_data()));
  }
  void OnPendingRequestComplete() override {}
  void OnNewDataAvailable() override {}
  void OnDocumentComplete() override {}
  void OnDocumentCanceled() override {}
  void CancelBrowserDownload() override {}

  std::unique_ptr<URLLoaderWrapper> CreateFullPageLoader() {
    return std::unique_ptr<URLLoaderWrapper>(
        new TestURLLoader(full_page_loader_data()));
  }

  TestURLLoader::LoaderData* full_page_loader_data() {
    return &full_page_loader_data_;
  }
  TestURLLoader::LoaderData* partial_loader_data() {
    return &partial_loader_data_;
  }

  void SetCanUsePartialLoading() {
    full_page_loader_data()->set_content_length(10 * 1024 * 1024);
    full_page_loader_data()->set_content_encoded(false);
    full_page_loader_data()->set_accept_ranges_bytes(true);
  }

  void SendAllPartialData() {
    partial_loader_data_.set_byte_range(partial_loader_data_.open_byte_range());
    partial_loader_data_.CallOpenCallback(0);
    uint32_t length = partial_loader_data_.byte_range().length();
    while (length > 0) {
      const uint32_t max_part_len = DocumentLoader::kDefaultRequestSize;
      const uint32_t part_len = std::min(length, max_part_len);
      partial_loader_data_.CallReadCallback(part_len);
      length -= part_len;
    }
    if (partial_loader_data_.IsWaitRead()) {
      partial_loader_data_.CallReadCallback(0);
    }
  }

 private:
  TestURLLoader::LoaderData full_page_loader_data_;
  TestURLLoader::LoaderData partial_loader_data_;

  DISALLOW_COPY_AND_ASSIGN(TestClient);
};

class MockClient : public TestClient {
 public:
  MockClient() {}

  MOCK_METHOD0(OnPendingRequestComplete, void());
  MOCK_METHOD0(OnNewDataAvailable, void());
  MOCK_METHOD0(OnDocumentComplete, void());
  MOCK_METHOD0(OnDocumentCanceled, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockClient);
};

}  // namespace

using DocumentLoaderTest = ::testing::Test;

TEST_F(DocumentLoaderTest, PartialLoadingEnabled) {
  TestClient client;
  client.SetCanUsePartialLoading();
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  loader.RequestData(1000000, 1);
  EXPECT_FALSE(loader.is_partial_loader_active());
  // Always send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);
  EXPECT_TRUE(loader.is_partial_loader_active());
}

TEST_F(DocumentLoaderTest, PartialLoadingDisabledOnSmallFiles) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(
      DocumentLoader::kDefaultRequestSize * 2);
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  loader.RequestData(1000000, 1);
  EXPECT_FALSE(loader.is_partial_loader_active());
  // Always send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);
  EXPECT_FALSE(loader.is_partial_loader_active());
}

TEST_F(DocumentLoaderTest, PartialLoadingDisabledIfContentEncoded) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_encoded(true);
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  loader.RequestData(1000000, 1);
  EXPECT_FALSE(loader.is_partial_loader_active());
  // Always send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);
  EXPECT_FALSE(loader.is_partial_loader_active());
}

TEST_F(DocumentLoaderTest, PartialLoadingDisabledNoAcceptRangeBytes) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_accept_ranges_bytes(false);
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  loader.RequestData(1000000, 1);
  EXPECT_FALSE(loader.is_partial_loader_active());
  // Always send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);
  EXPECT_FALSE(loader.is_partial_loader_active());
}

TEST_F(DocumentLoaderTest, PartialLoadingReallyDisabledRequestFromBegin) {
  TestClient client;
  DocumentLoader loader(&client);
  client.SetCanUsePartialLoading();
  loader.SetPartialLoadingEnabled(false);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  // We should not start partial loading if requested data is beside full page
  // loading position.
  loader.RequestData(DocumentLoader::kDefaultRequestSize, 1);
  EXPECT_FALSE(loader.is_partial_loader_active());
  // Always send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);
  EXPECT_FALSE(loader.is_partial_loader_active());
}

TEST_F(DocumentLoaderTest, PartialLoadingReallyDisabledRequestFromMiddle) {
  TestClient client;
  client.SetCanUsePartialLoading();
  DocumentLoader loader(&client);
  loader.SetPartialLoadingEnabled(false);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  loader.RequestData(1000000, 1);
  EXPECT_FALSE(loader.is_partial_loader_active());
  // Always send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);
  EXPECT_FALSE(loader.is_partial_loader_active());
}

TEST_F(DocumentLoaderTest, PartialLoadingSimple) {
  TestClient client;
  client.SetCanUsePartialLoading();

  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  // While we have no requests, we should not start partial loading.
  EXPECT_FALSE(loader.is_partial_loader_active());

  loader.RequestData(5000000, 1);

  EXPECT_FALSE(client.partial_loader_data()->IsWaitOpen());
  EXPECT_FALSE(loader.is_partial_loader_active());

  // Always send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);

  // Partial loader should request headers.
  EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
  EXPECT_TRUE(loader.is_partial_loader_active());
  // Loader should be stopped.
  EXPECT_TRUE(client.full_page_loader_data()->closed());

  EXPECT_EQ("{4980736,10485760}",
            client.partial_loader_data()->open_byte_range().ToString());
}

TEST_F(DocumentLoaderTest, PartialLoadingBackOrder) {
  TestClient client;
  client.SetCanUsePartialLoading();

  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  // While we have no requests, we should not start partial loading.
  EXPECT_FALSE(loader.is_partial_loader_active());

  loader.RequestData(client.full_page_loader_data()->content_length() - 1, 1);

  EXPECT_FALSE(client.partial_loader_data()->IsWaitOpen());
  EXPECT_FALSE(loader.is_partial_loader_active());

  // Always send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);

  // Partial loader should request headers.
  EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
  EXPECT_TRUE(loader.is_partial_loader_active());
  // Loader should be stopped.
  EXPECT_TRUE(client.full_page_loader_data()->closed());

  // Requested range should be enlarged.
  EXPECT_GT(client.partial_loader_data()->open_byte_range().length(), 1u);
  EXPECT_EQ("{9830400,10485760}",
            client.partial_loader_data()->open_byte_range().ToString());
}

TEST_F(DocumentLoaderTest, CompleteWithoutPartial) {
  TestClient client;
  client.SetCanUsePartialLoading();
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  EXPECT_FALSE(client.full_page_loader_data()->closed());
  while (client.full_page_loader_data()->IsWaitRead()) {
    client.full_page_loader_data()->CallReadCallback(1000);
  }
  EXPECT_TRUE(loader.IsDocumentComplete());
  EXPECT_TRUE(client.full_page_loader_data()->closed());
}

TEST_F(DocumentLoaderTest, ErrorDownloadFullDocument) {
  TestClient client;
  client.SetCanUsePartialLoading();
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  EXPECT_TRUE(client.full_page_loader_data()->IsWaitRead());
  EXPECT_FALSE(client.full_page_loader_data()->closed());
  client.full_page_loader_data()->CallReadCallback(-3);
  EXPECT_TRUE(client.full_page_loader_data()->closed());
  EXPECT_FALSE(loader.IsDocumentComplete());
}

TEST_F(DocumentLoaderTest, CompleteNoContentLength) {
  TestClient client;
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  EXPECT_FALSE(client.full_page_loader_data()->closed());
  for (int i = 0; i < 10; ++i) {
    EXPECT_TRUE(client.full_page_loader_data()->IsWaitRead());
    client.full_page_loader_data()->CallReadCallback(1000);
  }
  EXPECT_TRUE(client.full_page_loader_data()->IsWaitRead());
  client.full_page_loader_data()->CallReadCallback(0);
  EXPECT_EQ(10000ul, loader.GetDocumentSize());
  EXPECT_TRUE(loader.IsDocumentComplete());
  EXPECT_TRUE(client.full_page_loader_data()->closed());
}

TEST_F(DocumentLoaderTest, CompleteWithPartial) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(
      DocumentLoader::kDefaultRequestSize * 20);
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  loader.RequestData(19 * DocumentLoader::kDefaultRequestSize,
                     DocumentLoader::kDefaultRequestSize);
  EXPECT_FALSE(client.full_page_loader_data()->closed());
  EXPECT_FALSE(client.partial_loader_data()->IsWaitRead());
  EXPECT_FALSE(client.partial_loader_data()->IsWaitOpen());

  // Always send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);
  EXPECT_TRUE(client.full_page_loader_data()->closed());
  EXPECT_FALSE(client.partial_loader_data()->closed());

  client.SendAllPartialData();
  // Now we should send other document data.
  client.SendAllPartialData();
  EXPECT_TRUE(client.full_page_loader_data()->closed());
  EXPECT_TRUE(client.partial_loader_data()->closed());
}

TEST_F(DocumentLoaderTest, PartialRequestLastChunk) {
  const uint32_t kLastChunkSize = 300;
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(
      DocumentLoader::kDefaultRequestSize * 20 + kLastChunkSize);
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  loader.RequestData(20 * DocumentLoader::kDefaultRequestSize, 1);

  // Always send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);

  EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
  EXPECT_EQ(
      static_cast<int>(client.partial_loader_data()->open_byte_range().end()),
      client.full_page_loader_data()->content_length());
  client.partial_loader_data()->set_byte_range(
      client.partial_loader_data()->open_byte_range());
  client.partial_loader_data()->CallOpenCallback(0);
  uint32_t data_length = client.partial_loader_data()->byte_range().length();
  while (data_length > DocumentLoader::kDefaultRequestSize) {
    client.partial_loader_data()->CallReadCallback(
        DocumentLoader::kDefaultRequestSize);
    data_length -= DocumentLoader::kDefaultRequestSize;
  }
  EXPECT_EQ(kLastChunkSize, data_length);
  client.partial_loader_data()->CallReadCallback(kLastChunkSize);
  EXPECT_TRUE(loader.IsDataAvailable(DocumentLoader::kDefaultRequestSize * 20,
                                     kLastChunkSize));
}

TEST_F(DocumentLoaderTest, DocumentSize) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(123456789);
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  EXPECT_EQ(static_cast<int>(loader.GetDocumentSize()),
            client.full_page_loader_data()->content_length());
}

TEST_F(DocumentLoaderTest, DocumentSizeNoContentLength) {
  TestClient client;
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  EXPECT_EQ(0ul, loader.GetDocumentSize());
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);
  client.full_page_loader_data()->CallReadCallback(1000);
  client.full_page_loader_data()->CallReadCallback(500);
  client.full_page_loader_data()->CallReadCallback(0);
  EXPECT_EQ(DocumentLoader::kDefaultRequestSize + 1000ul + 500ul,
            loader.GetDocumentSize());
  EXPECT_TRUE(loader.IsDocumentComplete());
}

TEST_F(DocumentLoaderTest, ClearPendingRequests) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(
      DocumentLoader::kDefaultRequestSize * 100 + 58383);
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  loader.RequestData(17 * DocumentLoader::kDefaultRequestSize + 100, 10);
  loader.ClearPendingRequests();
  loader.RequestData(15 * DocumentLoader::kDefaultRequestSize + 200, 20);
  // pending requests are accumulating, and will be processed after initial data
  // load.
  EXPECT_FALSE(client.partial_loader_data()->IsWaitOpen());

  // Send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);

  {
    EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
    const gfx::Range range_requested(15 * DocumentLoader::kDefaultRequestSize,
                                     16 * DocumentLoader::kDefaultRequestSize);
    EXPECT_EQ(range_requested.start(),
              client.partial_loader_data()->open_byte_range().start());
    EXPECT_LE(range_requested.end(),
              client.partial_loader_data()->open_byte_range().end());
    client.partial_loader_data()->set_byte_range(
        client.partial_loader_data()->open_byte_range());
  }
  // clear requests before Open callback.
  loader.ClearPendingRequests();
  // Current request should continue loading.
  EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
  client.partial_loader_data()->CallOpenCallback(0);
  client.partial_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);
  EXPECT_FALSE(client.partial_loader_data()->closed());
  // Current request should continue loading, because no other request queued.

  loader.RequestData(18 * DocumentLoader::kDefaultRequestSize + 200, 20);
  // Requests queue is processed only on receiving data.
  client.partial_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);
  // New request within close distance from the one currently loading. Loading
  // isn't restarted.
  EXPECT_FALSE(client.partial_loader_data()->IsWaitOpen());

  loader.ClearPendingRequests();
  // request again two.
  loader.RequestData(60 * DocumentLoader::kDefaultRequestSize + 100, 10);
  loader.RequestData(35 * DocumentLoader::kDefaultRequestSize + 200, 20);
  // Requests queue is processed only on receiving data.
  client.partial_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);
  {
    // new requset not with in close distance from current loading.
    // Loading should be restarted.
    EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
    // The first requested chunk should be processed.
    const gfx::Range range_requested(35 * DocumentLoader::kDefaultRequestSize,
                                     36 * DocumentLoader::kDefaultRequestSize);
    EXPECT_EQ(range_requested.start(),
              client.partial_loader_data()->open_byte_range().start());
    EXPECT_LE(range_requested.end(),
              client.partial_loader_data()->open_byte_range().end());
    client.partial_loader_data()->set_byte_range(
        client.partial_loader_data()->open_byte_range());
  }
  EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
  client.partial_loader_data()->CallOpenCallback(0);
  // Override pending requests.
  loader.ClearPendingRequests();
  loader.RequestData(70 * DocumentLoader::kDefaultRequestSize + 100, 10);

  // Requests queue is processed only on receiving data.
  client.partial_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);
  {
    // New requset not with in close distance from current loading.
    // Loading should be restarted .
    EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
    // The first requested chunk should be processed.
    const gfx::Range range_requested(70 * DocumentLoader::kDefaultRequestSize,
                                     71 * DocumentLoader::kDefaultRequestSize);
    EXPECT_EQ(range_requested.start(),
              client.partial_loader_data()->open_byte_range().start());
    EXPECT_LE(range_requested.end(),
              client.partial_loader_data()->open_byte_range().end());
    client.partial_loader_data()->set_byte_range(
        client.partial_loader_data()->open_byte_range());
  }
  EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
}

TEST_F(DocumentLoaderTest, GetBlock) {
  std::vector<char> buffer(DocumentLoader::kDefaultRequestSize);
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(
      DocumentLoader::kDefaultRequestSize * 20 + 58383);
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  EXPECT_FALSE(loader.GetBlock(0, 1000, buffer.data()));
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);
  EXPECT_TRUE(loader.GetBlock(0, 1000, buffer.data()));
  EXPECT_FALSE(loader.GetBlock(DocumentLoader::kDefaultRequestSize, 1500,
                               buffer.data()));
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);
  EXPECT_TRUE(loader.GetBlock(DocumentLoader::kDefaultRequestSize, 1500,
                              buffer.data()));

  EXPECT_FALSE(loader.GetBlock(17 * DocumentLoader::kDefaultRequestSize, 3000,
                               buffer.data()));
  loader.RequestData(17 * DocumentLoader::kDefaultRequestSize + 100, 10);
  EXPECT_FALSE(loader.GetBlock(17 * DocumentLoader::kDefaultRequestSize, 3000,
                               buffer.data()));

  // Requests queue is processed only on receiving data.
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);

  client.SendAllPartialData();
  EXPECT_TRUE(loader.GetBlock(17 * DocumentLoader::kDefaultRequestSize, 3000,
                              buffer.data()));
}

TEST_F(DocumentLoaderTest, IsDataAvailable) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(
      DocumentLoader::kDefaultRequestSize * 20 + 58383);
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  EXPECT_FALSE(loader.IsDataAvailable(0, 1000));
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);
  EXPECT_TRUE(loader.IsDataAvailable(0, 1000));
  EXPECT_FALSE(
      loader.IsDataAvailable(DocumentLoader::kDefaultRequestSize, 1500));
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);
  EXPECT_TRUE(
      loader.IsDataAvailable(DocumentLoader::kDefaultRequestSize, 1500));

  EXPECT_FALSE(
      loader.IsDataAvailable(17 * DocumentLoader::kDefaultRequestSize, 3000));
  loader.RequestData(17 * DocumentLoader::kDefaultRequestSize + 100, 10);
  EXPECT_FALSE(
      loader.IsDataAvailable(17 * DocumentLoader::kDefaultRequestSize, 3000));

  // Requests queue is processed only on receiving data.
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);

  client.SendAllPartialData();
  EXPECT_TRUE(
      loader.IsDataAvailable(17 * DocumentLoader::kDefaultRequestSize, 3000));
}

TEST_F(DocumentLoaderTest, RequestData) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(
      DocumentLoader::kDefaultRequestSize * 100 + 58383);
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  loader.RequestData(37 * DocumentLoader::kDefaultRequestSize + 200, 10);
  loader.RequestData(25 * DocumentLoader::kDefaultRequestSize + 600, 100);
  loader.RequestData(13 * DocumentLoader::kDefaultRequestSize + 900, 500);

  // Send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);

  {
    EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
    const gfx::Range range_requested(13 * DocumentLoader::kDefaultRequestSize,
                                     14 * DocumentLoader::kDefaultRequestSize);
    EXPECT_EQ(range_requested.start(),
              client.partial_loader_data()->open_byte_range().start());
    EXPECT_LE(range_requested.end(),
              client.partial_loader_data()->open_byte_range().end());
    client.partial_loader_data()->set_byte_range(
        client.partial_loader_data()->open_byte_range());
  }
  client.partial_loader_data()->CallOpenCallback(0);
  // Override pending requests.
  loader.ClearPendingRequests();
  loader.RequestData(38 * DocumentLoader::kDefaultRequestSize + 200, 10);
  loader.RequestData(26 * DocumentLoader::kDefaultRequestSize + 600, 100);
  // Requests queue is processed only on receiving data.
  client.partial_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);
  {
    EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
    const gfx::Range range_requested(26 * DocumentLoader::kDefaultRequestSize,
                                     27 * DocumentLoader::kDefaultRequestSize);
    EXPECT_EQ(range_requested.start(),
              client.partial_loader_data()->open_byte_range().start());
    EXPECT_LE(range_requested.end(),
              client.partial_loader_data()->open_byte_range().end());
    client.partial_loader_data()->set_byte_range(
        client.partial_loader_data()->open_byte_range());
  }
  client.partial_loader_data()->CallOpenCallback(0);
  // Override pending requests.
  loader.ClearPendingRequests();
  loader.RequestData(39 * DocumentLoader::kDefaultRequestSize + 200, 10);
  // Requests queue is processed only on receiving data.
  client.partial_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);
  {
    EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
    const gfx::Range range_requested(39 * DocumentLoader::kDefaultRequestSize,
                                     40 * DocumentLoader::kDefaultRequestSize);
    EXPECT_EQ(range_requested.start(),
              client.partial_loader_data()->open_byte_range().start());
    EXPECT_LE(range_requested.end(),
              client.partial_loader_data()->open_byte_range().end());
    client.partial_loader_data()->set_byte_range(
        client.partial_loader_data()->open_byte_range());
  }
  // Fill all gaps.
  while (!loader.IsDocumentComplete()) {
    client.SendAllPartialData();
  }
  EXPECT_TRUE(client.partial_loader_data()->closed());
}

TEST_F(DocumentLoaderTest, DoNotLoadAvailablePartialData) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(
      DocumentLoader::kDefaultRequestSize * 20 + 58383);
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  // Send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);

  // Send more data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);

  loader.RequestData(2 * DocumentLoader::kDefaultRequestSize + 200, 10);

  // Send more data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);

  // Partial loading should not have started for already available data.
  EXPECT_TRUE(client.partial_loader_data()->closed());
}

TEST_F(DocumentLoaderTest, DoNotLoadDataAfterComplete) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(
      DocumentLoader::kDefaultRequestSize * 20);
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  for (int i = 0; i < 20; ++i) {
    client.full_page_loader_data()->CallReadCallback(
        DocumentLoader::kDefaultRequestSize);
  }

  EXPECT_TRUE(loader.IsDocumentComplete());

  loader.RequestData(17 * DocumentLoader::kDefaultRequestSize + 200, 10);

  EXPECT_TRUE(client.partial_loader_data()->closed());
  EXPECT_TRUE(client.full_page_loader_data()->closed());
}

TEST_F(DocumentLoaderTest, DoNotLoadPartialDataAboveDocumentSize) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(
      DocumentLoader::kDefaultRequestSize * 20);
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  loader.RequestData(20 * DocumentLoader::kDefaultRequestSize + 200, 10);

  // Send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);

  EXPECT_TRUE(client.partial_loader_data()->closed());
}

TEST_F(DocumentLoaderTest, MergePendingRequests) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(
      DocumentLoader::kDefaultRequestSize * 50 + 58383);
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  loader.RequestData(17 * DocumentLoader::kDefaultRequestSize + 200, 10);
  loader.RequestData(16 * DocumentLoader::kDefaultRequestSize + 600, 100);

  // Send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);

  const gfx::Range range_requested(16 * DocumentLoader::kDefaultRequestSize,
                                   18 * DocumentLoader::kDefaultRequestSize);
  EXPECT_EQ(range_requested.start(),
            client.partial_loader_data()->open_byte_range().start());
  EXPECT_LE(range_requested.end(),
            client.partial_loader_data()->open_byte_range().end());

  EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());

  // Fill all gaps.
  while (!loader.IsDocumentComplete()) {
    client.SendAllPartialData();
  }
  EXPECT_TRUE(client.partial_loader_data()->closed());
}

TEST_F(DocumentLoaderTest, PartialStopOnStatusCodeError) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(
      DocumentLoader::kDefaultRequestSize * 20);
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  loader.RequestData(17 * DocumentLoader::kDefaultRequestSize + 200, 10);

  // Send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);

  EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
  client.partial_loader_data()->set_status_code(404);
  client.partial_loader_data()->CallOpenCallback(0);
  EXPECT_TRUE(client.partial_loader_data()->closed());
}

TEST_F(DocumentLoaderTest,
       PartialAsFullDocumentLoadingRangeRequestNoRangeField) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(
      DocumentLoader::kDefaultRequestSize * 20);
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  loader.RequestData(17 * DocumentLoader::kDefaultRequestSize + 200, 10);

  // Send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);

  EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
  client.partial_loader_data()->set_byte_range(gfx::Range::InvalidRange());
  client.partial_loader_data()->CallOpenCallback(0);
  EXPECT_FALSE(client.partial_loader_data()->closed());
  // Partial loader is used to load the whole page, like full page loader.
  EXPECT_FALSE(loader.is_partial_loader_active());
}

TEST_F(DocumentLoaderTest, PartialMultiPart) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(
      DocumentLoader::kDefaultRequestSize * 20);
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  loader.RequestData(17 * DocumentLoader::kDefaultRequestSize + 200, 10);

  // Send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);

  EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
  client.partial_loader_data()->set_is_multipart(true);
  client.partial_loader_data()->CallOpenCallback(0);
  client.partial_loader_data()->set_byte_range(
      gfx::Range(17 * DocumentLoader::kDefaultRequestSize,
                 18 * DocumentLoader::kDefaultRequestSize));
  client.partial_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);
  EXPECT_TRUE(loader.IsDataAvailable(17 * DocumentLoader::kDefaultRequestSize,
                                     DocumentLoader::kDefaultRequestSize));
}

TEST_F(DocumentLoaderTest, PartialMultiPartRangeError) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(
      DocumentLoader::kDefaultRequestSize * 20);
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  loader.RequestData(17 * DocumentLoader::kDefaultRequestSize + 200, 10);

  // Send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);

  EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
  client.partial_loader_data()->set_is_multipart(true);
  client.partial_loader_data()->CallOpenCallback(0);
  client.partial_loader_data()->set_byte_range(gfx::Range::InvalidRange());
  client.partial_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);
  EXPECT_FALSE(loader.IsDataAvailable(17 * DocumentLoader::kDefaultRequestSize,
                                      DocumentLoader::kDefaultRequestSize));
  EXPECT_TRUE(client.partial_loader_data()->closed());
}

TEST_F(DocumentLoaderTest, PartialConnectionErrorOnOpen) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(
      DocumentLoader::kDefaultRequestSize * 20);
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  loader.RequestData(17 * DocumentLoader::kDefaultRequestSize + 200, 10);

  // Send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);

  EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
  client.partial_loader_data()->CallOpenCallback(-3);
  EXPECT_TRUE(client.partial_loader_data()->closed());

  // Partial loading should not restart after any error.
  loader.RequestData(18 * DocumentLoader::kDefaultRequestSize + 200, 10);

  EXPECT_FALSE(client.partial_loader_data()->IsWaitOpen());
  EXPECT_TRUE(client.partial_loader_data()->closed());
}

TEST_F(DocumentLoaderTest, PartialConnectionErrorOnRead) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(
      DocumentLoader::kDefaultRequestSize * 20);
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  loader.RequestData(17 * DocumentLoader::kDefaultRequestSize + 200, 10);

  // Send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);

  EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
  client.partial_loader_data()->set_byte_range(
      gfx::Range(17 * DocumentLoader::kDefaultRequestSize,
                 18 * DocumentLoader::kDefaultRequestSize));
  client.partial_loader_data()->CallOpenCallback(0);
  EXPECT_TRUE(client.partial_loader_data()->IsWaitRead());
  client.partial_loader_data()->CallReadCallback(-3);
  EXPECT_TRUE(client.partial_loader_data()->closed());

  // Partial loading should not restart after any error.
  loader.RequestData(18 * DocumentLoader::kDefaultRequestSize + 200, 10);

  EXPECT_FALSE(client.partial_loader_data()->IsWaitOpen());
  EXPECT_TRUE(client.partial_loader_data()->closed());
}

TEST_F(DocumentLoaderTest, GetProgress) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(
      DocumentLoader::kDefaultRequestSize * 20);
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  EXPECT_EQ(0., loader.GetProgress());

  for (int i = 0; i < 20; ++i) {
    EXPECT_EQ(i * 100 / 20, static_cast<int>(loader.GetProgress() * 100));
    client.full_page_loader_data()->CallReadCallback(
        DocumentLoader::kDefaultRequestSize);
  }
  EXPECT_EQ(1., loader.GetProgress());
}

TEST_F(DocumentLoaderTest, GetProgressNoContentLength) {
  TestClient client;
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  EXPECT_EQ(-1., loader.GetProgress());

  for (int i = 0; i < 20; ++i) {
    EXPECT_EQ(-1., loader.GetProgress());
    client.full_page_loader_data()->CallReadCallback(
        DocumentLoader::kDefaultRequestSize);
  }
  client.full_page_loader_data()->CallReadCallback(0);
  EXPECT_EQ(1., loader.GetProgress());
}

TEST_F(DocumentLoaderTest, ClientCompleteCallbacks) {
  MockClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(
      DocumentLoader::kDefaultRequestSize * 20);
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  EXPECT_CALL(client, OnDocumentComplete()).Times(0);
  for (int i = 0; i < 19; ++i) {
    client.full_page_loader_data()->CallReadCallback(
        DocumentLoader::kDefaultRequestSize);
  }
  Mock::VerifyAndClear(&client);

  EXPECT_CALL(client, OnDocumentComplete()).Times(1);
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);
  Mock::VerifyAndClear(&client);
}

TEST_F(DocumentLoaderTest, ClientCompleteCallbacksNoContentLength) {
  MockClient client;
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  EXPECT_CALL(client, OnDocumentCanceled()).Times(0);
  EXPECT_CALL(client, OnDocumentComplete()).Times(0);
  for (int i = 0; i < 20; ++i) {
    client.full_page_loader_data()->CallReadCallback(
        DocumentLoader::kDefaultRequestSize);
  }
  Mock::VerifyAndClear(&client);

  EXPECT_CALL(client, OnDocumentCanceled()).Times(0);
  EXPECT_CALL(client, OnDocumentComplete()).Times(1);
  client.full_page_loader_data()->CallReadCallback(0);
  Mock::VerifyAndClear(&client);
}

TEST_F(DocumentLoaderTest, ClientCancelCallback) {
  MockClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(
      DocumentLoader::kDefaultRequestSize * 20);
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  EXPECT_CALL(client, OnDocumentCanceled()).Times(0);
  EXPECT_CALL(client, OnDocumentComplete()).Times(0);
  for (int i = 0; i < 10; ++i) {
    client.full_page_loader_data()->CallReadCallback(
        DocumentLoader::kDefaultRequestSize);
  }
  Mock::VerifyAndClear(&client);

  EXPECT_CALL(client, OnDocumentComplete()).Times(0);
  EXPECT_CALL(client, OnDocumentCanceled()).Times(1);
  client.full_page_loader_data()->CallReadCallback(-3);
  Mock::VerifyAndClear(&client);
}

TEST_F(DocumentLoaderTest, NewDataAvailable) {
  MockClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(
      DocumentLoader::kDefaultRequestSize * 20);
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  EXPECT_CALL(client, OnNewDataAvailable()).Times(1);
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);
  Mock::VerifyAndClear(&client);

  EXPECT_CALL(client, OnNewDataAvailable()).Times(0);
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize - 100);
  Mock::VerifyAndClear(&client);

  EXPECT_CALL(client, OnNewDataAvailable()).Times(1);
  client.full_page_loader_data()->CallReadCallback(100);
  Mock::VerifyAndClear(&client);
}

TEST_F(DocumentLoaderTest, ClientPendingRequestCompleteFullLoader) {
  MockClient client;
  client.SetCanUsePartialLoading();
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  loader.RequestData(1000, 4000);

  EXPECT_CALL(client, OnPendingRequestComplete()).Times(1);
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);
  Mock::VerifyAndClear(&client);
}

TEST_F(DocumentLoaderTest, ClientPendingRequestCompletePartialLoader) {
  MockClient client;
  client.SetCanUsePartialLoading();
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  EXPECT_CALL(client, OnPendingRequestComplete()).Times(1);
  loader.RequestData(15 * DocumentLoader::kDefaultRequestSize + 4000, 4000);

  // Always send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(
      DocumentLoader::kDefaultRequestSize);

  client.SendAllPartialData();
  Mock::VerifyAndClear(&client);
}

TEST_F(DocumentLoaderTest, ClientPendingRequestCompletePartialAndFullLoader) {
  MockClient client;
  client.SetCanUsePartialLoading();
  DocumentLoader loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  EXPECT_CALL(client, OnPendingRequestComplete()).Times(1);
  loader.RequestData(16 * DocumentLoader::kDefaultRequestSize + 4000, 4000);
  loader.RequestData(4 * DocumentLoader::kDefaultRequestSize + 4000, 4000);

  for (int i = 0; i < 5; ++i) {
    client.full_page_loader_data()->CallReadCallback(
        DocumentLoader::kDefaultRequestSize);
  }

  Mock::VerifyAndClear(&client);

  EXPECT_CALL(client, OnPendingRequestComplete()).Times(1);
  client.SendAllPartialData();
  Mock::VerifyAndClear(&client);
}

}  // namespace chrome_pdf
