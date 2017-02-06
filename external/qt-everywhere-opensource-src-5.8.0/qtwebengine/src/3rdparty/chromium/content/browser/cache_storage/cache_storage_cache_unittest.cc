// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_cache.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/cache_storage/cache_storage_cache_handle.h"
#include "content/browser/fileapi/mock_url_request_delegate.h"
#include "content/browser/quota/mock_quota_manager_proxy.h"
#include "content/common/cache_storage/cache_storage_types.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/referrer.h"
#include "content/public/test/mock_special_storage_policy.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/test_completion_callback.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_data_snapshot.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_url_request_job_factory.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {
const char kTestData[] = "Hello World";
const char kOrigin[] = "http://example.com";
const char kCacheName[] = "test_cache";

// Returns a BlobProtocolHandler that uses |blob_storage_context|. Caller owns
// the memory.
std::unique_ptr<storage::BlobProtocolHandler> CreateMockBlobProtocolHandler(
    storage::BlobStorageContext* blob_storage_context) {
  // The FileSystemContext and thread task runner are not actually used but a
  // task runner is needed to avoid a DCHECK in BlobURLRequestJob ctor.
  return base::WrapUnique(new storage::BlobProtocolHandler(
      blob_storage_context, NULL, base::ThreadTaskRunnerHandle::Get().get()));
}

// A disk_cache::Backend wrapper that can delay operations.
class DelayableBackend : public disk_cache::Backend {
 public:
  DelayableBackend(std::unique_ptr<disk_cache::Backend> backend)
      : backend_(std::move(backend)), delay_open_(false) {}

  // disk_cache::Backend overrides
  net::CacheType GetCacheType() const override {
    return backend_->GetCacheType();
  }
  int32_t GetEntryCount() const override { return backend_->GetEntryCount(); }
  int OpenEntry(const std::string& key,
                disk_cache::Entry** entry,
                const CompletionCallback& callback) override {
    if (delay_open_) {
      open_entry_callback_ =
          base::Bind(&DelayableBackend::OpenEntryDelayedImpl,
                     base::Unretained(this), key, entry, callback);
      return net::ERR_IO_PENDING;
    }

    return backend_->OpenEntry(key, entry, callback);
  }
  int CreateEntry(const std::string& key,
                  disk_cache::Entry** entry,
                  const CompletionCallback& callback) override {
    return backend_->CreateEntry(key, entry, callback);
  }
  int DoomEntry(const std::string& key,
                const CompletionCallback& callback) override {
    return backend_->DoomEntry(key, callback);
  }
  int DoomAllEntries(const CompletionCallback& callback) override {
    return backend_->DoomAllEntries(callback);
  }
  int DoomEntriesBetween(base::Time initial_time,
                         base::Time end_time,
                         const CompletionCallback& callback) override {
    return backend_->DoomEntriesBetween(initial_time, end_time, callback);
  }
  int DoomEntriesSince(base::Time initial_time,
                       const CompletionCallback& callback) override {
    return backend_->DoomEntriesSince(initial_time, callback);
  }
  int CalculateSizeOfAllEntries(
      const CompletionCallback& callback) override {
    return backend_->CalculateSizeOfAllEntries(callback);
  }
  std::unique_ptr<Iterator> CreateIterator() override {
    return backend_->CreateIterator();
  }
  void GetStats(base::StringPairs* stats) override {
    return backend_->GetStats(stats);
  }
  void OnExternalCacheHit(const std::string& key) override {
    return backend_->OnExternalCacheHit(key);
  }

  // Call to continue a delayed open.
  void OpenEntryContinue() {
    EXPECT_FALSE(open_entry_callback_.is_null());
    open_entry_callback_.Run();
  }

  void set_delay_open(bool value) { delay_open_ = value; }

 private:
  void OpenEntryDelayedImpl(const std::string& key,
                            disk_cache::Entry** entry,
                            const CompletionCallback& callback) {
    int rv = backend_->OpenEntry(key, entry, callback);
    if (rv != net::ERR_IO_PENDING)
      callback.Run(rv);
  }

  std::unique_ptr<disk_cache::Backend> backend_;
  bool delay_open_;
  base::Closure open_entry_callback_;
};

void CopyBody(const storage::BlobDataHandle& blob_handle, std::string* output) {
  *output = std::string();
  std::unique_ptr<storage::BlobDataSnapshot> data =
      blob_handle.CreateSnapshot();
  const auto& items = data->items();
  for (const auto& item : items) {
    switch (item->type()) {
      case storage::DataElement::TYPE_BYTES: {
        output->append(item->bytes(), item->length());
        break;
      }
      case storage::DataElement::TYPE_DISK_CACHE_ENTRY: {
        disk_cache::Entry* entry = item->disk_cache_entry();
        int32_t body_size = entry->GetDataSize(item->disk_cache_stream_index());

        scoped_refptr<net::IOBuffer> io_buffer = new net::IOBuffer(body_size);
        net::TestCompletionCallback callback;
        int rv =
            entry->ReadData(item->disk_cache_stream_index(), 0, io_buffer.get(),
                            body_size, callback.callback());
        if (rv == net::ERR_IO_PENDING)
          rv = callback.WaitForResult();
        EXPECT_EQ(body_size, rv);
        if (rv > 0)
          output->append(io_buffer->data(), rv);
        break;
      }
      default: { ADD_FAILURE() << "invalid response blob type"; } break;
    }
  }
}

void CopySideData(const storage::BlobDataHandle& blob_handle,
                  std::string* output) {
  *output = std::string();
  std::unique_ptr<storage::BlobDataSnapshot> data =
      blob_handle.CreateSnapshot();
  const auto& items = data->items();
  ASSERT_EQ(1u, items.size());
  const auto& item = items[0];
  ASSERT_EQ(storage::DataElement::TYPE_DISK_CACHE_ENTRY, item->type());
  ASSERT_EQ(CacheStorageCache::INDEX_SIDE_DATA,
            item->disk_cache_side_stream_index());

  disk_cache::Entry* entry = item->disk_cache_entry();
  int32_t body_size = entry->GetDataSize(item->disk_cache_side_stream_index());
  scoped_refptr<net::IOBuffer> io_buffer = new net::IOBuffer(body_size);
  net::TestCompletionCallback callback;
  int rv = entry->ReadData(item->disk_cache_side_stream_index(), 0,
                           io_buffer.get(), body_size, callback.callback());
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(body_size, rv);
  if (rv > 0)
    output->append(io_buffer->data(), rv);
}

bool ResponseMetadataEqual(const ServiceWorkerResponse& expected,
                           const ServiceWorkerResponse& actual) {
  EXPECT_EQ(expected.status_code, actual.status_code);
  if (expected.status_code != actual.status_code)
    return false;
  EXPECT_EQ(expected.status_text, actual.status_text);
  if (expected.status_text != actual.status_text)
    return false;
  EXPECT_EQ(expected.url, actual.url);
  if (expected.url != actual.url)
    return false;
  EXPECT_EQ(expected.blob_size, actual.blob_size);
  if (expected.blob_size != actual.blob_size)
    return false;

  if (expected.blob_size == 0) {
    EXPECT_STREQ("", actual.blob_uuid.c_str());
    if (!actual.blob_uuid.empty())
      return false;
  } else {
    EXPECT_STRNE("", actual.blob_uuid.c_str());
    if (actual.blob_uuid.empty())
      return false;
  }

  EXPECT_EQ(expected.response_time, actual.response_time);
  if (expected.response_time != actual.response_time)
    return false;

  EXPECT_EQ(expected.cache_storage_cache_name, actual.cache_storage_cache_name);
  if (expected.cache_storage_cache_name != actual.cache_storage_cache_name)
    return false;

  EXPECT_EQ(expected.cors_exposed_header_names,
            actual.cors_exposed_header_names);
  if (expected.cors_exposed_header_names != actual.cors_exposed_header_names)
    return false;

  return true;
}

bool ResponseBodiesEqual(const std::string& expected_body,
                         const storage::BlobDataHandle& actual_body_handle) {
  std::string actual_body;
  CopyBody(actual_body_handle, &actual_body);
  return expected_body == actual_body;
}

bool ResponseSideDataEqual(const std::string& expected_side_data,
                           const storage::BlobDataHandle& actual_body_handle) {
  std::string actual_body;
  CopySideData(actual_body_handle, &actual_body);
  return expected_side_data == actual_body;
}

ServiceWorkerResponse SetCacheName(const ServiceWorkerResponse& original) {
  ServiceWorkerResponse result(original);
  result.is_in_cache_storage = true;
  result.cache_storage_cache_name = kCacheName;
  return result;
}

}  // namespace

// A CacheStorageCache that can optionally delay during backend creation.
class TestCacheStorageCache : public CacheStorageCache {
 public:
  TestCacheStorageCache(
      const GURL& origin,
      const std::string& cache_name,
      const base::FilePath& path,
      CacheStorage* cache_storage,
      const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
      const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy,
      base::WeakPtr<storage::BlobStorageContext> blob_context)
      : CacheStorageCache(origin,
                          cache_name,
                          path,
                          cache_storage,
                          request_context_getter,
                          quota_manager_proxy,
                          blob_context),
        delay_backend_creation_(false) {}

  void CreateBackend(const ErrorCallback& callback) override {
    backend_creation_callback_ = callback;
    if (delay_backend_creation_)
      return;
    ContinueCreateBackend();
  }

  void ContinueCreateBackend() {
    CacheStorageCache::CreateBackend(backend_creation_callback_);
  }

  void set_delay_backend_creation(bool delay) {
    delay_backend_creation_ = delay;
  }

  // Swap the existing backend with a delayable one. The backend must have been
  // created before calling this.
  DelayableBackend* UseDelayableBackend() {
    EXPECT_TRUE(backend_);
    DelayableBackend* delayable_backend =
        new DelayableBackend(std::move(backend_));
    backend_.reset(delayable_backend);
    return delayable_backend;
  }

 private:
  std::unique_ptr<CacheStorageCacheHandle> CreateCacheHandle() override {
    // Returns an empty handle. There is no need for CacheStorage and its
    // handles in these tests.
    return std::unique_ptr<CacheStorageCacheHandle>();
  }

  bool delay_backend_creation_;
  ErrorCallback backend_creation_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestCacheStorageCache);
};

class CacheStorageCacheTest : public testing::Test {
 public:
  CacheStorageCacheTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  void SetUp() override {
    ChromeBlobStorageContext* blob_storage_context =
        ChromeBlobStorageContext::GetFor(&browser_context_);
    // Wait for chrome_blob_storage_context to finish initializing.
    base::RunLoop().RunUntilIdle();
    blob_storage_context_ = blob_storage_context->context();

    if (!MemoryOnly())
      ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    quota_policy_ = new MockSpecialStoragePolicy;
    mock_quota_manager_ = new MockQuotaManager(
        MemoryOnly() /* is incognito */, temp_dir_.path(),
        base::ThreadTaskRunnerHandle::Get().get(),
        base::ThreadTaskRunnerHandle::Get().get(), quota_policy_.get());
    mock_quota_manager_->SetQuota(GURL(kOrigin), storage::kStorageTypeTemporary,
                                  1024 * 1024 * 100);

    quota_manager_proxy_ = new MockQuotaManagerProxy(
        mock_quota_manager_.get(), base::ThreadTaskRunnerHandle::Get().get());

    url_request_job_factory_.reset(new net::URLRequestJobFactoryImpl);
    url_request_job_factory_->SetProtocolHandler(
        "blob", CreateMockBlobProtocolHandler(blob_storage_context->context()));

    net::URLRequestContext* url_request_context =
        BrowserContext::GetDefaultStoragePartition(&browser_context_)->
            GetURLRequestContext()->GetURLRequestContext();

    url_request_context->set_job_factory(url_request_job_factory_.get());

    CreateRequests(blob_storage_context);

    cache_ = base::MakeUnique<TestCacheStorageCache>(
        GURL(kOrigin), kCacheName, temp_dir_.path(), nullptr /* CacheStorage */,
        BrowserContext::GetDefaultStoragePartition(&browser_context_)
            ->GetURLRequestContext(),
        quota_manager_proxy_, blob_storage_context->context()->AsWeakPtr());
  }

  void TearDown() override {
    quota_manager_proxy_->SimulateQuotaManagerDestroyed();
    base::RunLoop().RunUntilIdle();
  }

  void CreateRequests(ChromeBlobStorageContext* blob_storage_context) {
    ServiceWorkerHeaderMap headers;
    headers.insert(std::make_pair("a", "a"));
    headers.insert(std::make_pair("b", "b"));
    body_request_ =
        ServiceWorkerFetchRequest(GURL("http://example.com/body.html"), "GET",
                                  headers, Referrer(), false);
    body_request_with_query_ = ServiceWorkerFetchRequest(
        GURL("http://example.com/body.html?query=test"), "GET", headers,
        Referrer(), false);
    no_body_request_ =
        ServiceWorkerFetchRequest(GURL("http://example.com/no_body.html"),
                                  "GET", headers, Referrer(), false);

    std::string expected_response;
    for (int i = 0; i < 100; ++i)
      expected_blob_data_ += kTestData;

    std::unique_ptr<storage::BlobDataBuilder> blob_data(
        new storage::BlobDataBuilder("blob-id:myblob"));
    blob_data->AppendData(expected_blob_data_);

    blob_handle_ =
        blob_storage_context->context()->AddFinishedBlob(blob_data.get());

    body_response_ = ServiceWorkerResponse(
        GURL("http://example.com/body.html"), 200, "OK",
        blink::WebServiceWorkerResponseTypeDefault, headers,
        blob_handle_->uuid(), expected_blob_data_.size(), GURL(),
        blink::WebServiceWorkerResponseErrorUnknown, base::Time::Now(),
        false /* is_in_cache_storage */,
        std::string() /* cache_storage_cache_name */,
        ServiceWorkerHeaderList() /* cors_exposed_header_names */);

    body_response_with_query_ = ServiceWorkerResponse(
        GURL("http://example.com/body.html?query=test"), 200, "OK",
        blink::WebServiceWorkerResponseTypeDefault, headers,
        blob_handle_->uuid(), expected_blob_data_.size(), GURL(),
        blink::WebServiceWorkerResponseErrorUnknown, base::Time::Now(),
        false /* is_in_cache_storage */,
        std::string() /* cache_storage_cache_name */,
        {"a"} /* cors_exposed_header_names */);

    no_body_response_ = ServiceWorkerResponse(
        GURL("http://example.com/no_body.html"), 200, "OK",
        blink::WebServiceWorkerResponseTypeDefault, headers, "", 0, GURL(),
        blink::WebServiceWorkerResponseErrorUnknown, base::Time::Now(),
        false /* is_in_cache_storage */,
        std::string() /* cache_storage_cache_name */,
        ServiceWorkerHeaderList() /* cors_exposed_header_names */);
  }

  std::unique_ptr<ServiceWorkerFetchRequest> CopyFetchRequest(
      const ServiceWorkerFetchRequest& request) {
    return base::WrapUnique(new ServiceWorkerFetchRequest(
        request.url, request.method, request.headers, request.referrer,
        request.is_reload));
  }

  CacheStorageError BatchOperation(
      const std::vector<CacheStorageBatchOperation>& operations) {
    std::unique_ptr<base::RunLoop> loop(new base::RunLoop());

    cache_->BatchOperation(
        operations,
        base::Bind(&CacheStorageCacheTest::ErrorTypeCallback,
                   base::Unretained(this), base::Unretained(loop.get())));
    // TODO(jkarlin): These functions should use base::RunLoop().RunUntilIdle()
    // once the cache uses a passed in task runner instead of the CACHE thread.
    loop->Run();

    return callback_error_;
  }

  bool Put(const ServiceWorkerFetchRequest& request,
           const ServiceWorkerResponse& response) {
    CacheStorageBatchOperation operation;
    operation.operation_type = CACHE_STORAGE_CACHE_OPERATION_TYPE_PUT;
    operation.request = request;
    operation.response = response;

    CacheStorageError error =
        BatchOperation(std::vector<CacheStorageBatchOperation>(1, operation));
    return error == CACHE_STORAGE_OK;
  }

  bool Match(const ServiceWorkerFetchRequest& request) {
    std::unique_ptr<base::RunLoop> loop(new base::RunLoop());

    cache_->Match(
        CopyFetchRequest(request),
        base::Bind(&CacheStorageCacheTest::ResponseAndErrorCallback,
                   base::Unretained(this), base::Unretained(loop.get())));
    loop->Run();

    return callback_error_ == CACHE_STORAGE_OK;
  }

  bool MatchAll(
      const ServiceWorkerFetchRequest& request,
      const CacheStorageCacheQueryParams& match_params,
      std::unique_ptr<CacheStorageCache::Responses>* responses,
      std::unique_ptr<CacheStorageCache::BlobDataHandles>* body_handles) {
    base::RunLoop loop;
    cache_->MatchAll(
        CopyFetchRequest(request), match_params,
        base::Bind(&CacheStorageCacheTest::ResponsesAndErrorCallback,
                   base::Unretained(this), loop.QuitClosure(), responses,
                   body_handles));
    loop.Run();
    return callback_error_ == CACHE_STORAGE_OK;
  }

  bool MatchAll(
      std::unique_ptr<CacheStorageCache::Responses>* responses,
      std::unique_ptr<CacheStorageCache::BlobDataHandles>* body_handles) {
    return MatchAll(ServiceWorkerFetchRequest(), CacheStorageCacheQueryParams(),
                    responses, body_handles);
  }

  bool Delete(const ServiceWorkerFetchRequest& request,
              const CacheStorageCacheQueryParams& match_params =
                  CacheStorageCacheQueryParams()) {
    CacheStorageBatchOperation operation;
    operation.operation_type = CACHE_STORAGE_CACHE_OPERATION_TYPE_DELETE;
    operation.request = request;
    operation.match_params = match_params;

    CacheStorageError error =
        BatchOperation(std::vector<CacheStorageBatchOperation>(1, operation));
    return error == CACHE_STORAGE_OK;
  }

  bool Keys() {
    std::unique_ptr<base::RunLoop> loop(new base::RunLoop());

    cache_->Keys(base::Bind(&CacheStorageCacheTest::RequestsCallback,
                            base::Unretained(this),
                            base::Unretained(loop.get())));
    loop->Run();

    return callback_error_ == CACHE_STORAGE_OK;
  }

  bool Close() {
    std::unique_ptr<base::RunLoop> loop(new base::RunLoop());

    cache_->Close(base::Bind(&CacheStorageCacheTest::CloseCallback,
                             base::Unretained(this),
                             base::Unretained(loop.get())));
    loop->Run();
    return callback_closed_;
  }

  bool WriteSideData(const GURL& url,
                     base::Time expected_response_time,
                     scoped_refptr<net::IOBuffer> buffer,
                     int buf_len) {
    base::RunLoop run_loop;
    cache_->WriteSideData(
        base::Bind(&CacheStorageCacheTest::ErrorTypeCallback,
                   base::Unretained(this), base::Unretained(&run_loop)),
        url, expected_response_time, buffer, buf_len);
    run_loop.Run();

    return callback_error_ == CACHE_STORAGE_OK;
  }

  int64_t Size() {
    // Storage notification happens after an operation completes. Let the any
    // notifications complete before calling Size.
    base::RunLoop().RunUntilIdle();

    base::RunLoop run_loop;
    bool callback_called = false;
    cache_->Size(base::Bind(&CacheStorageCacheTest::SizeCallback,
                            base::Unretained(this), &run_loop,
                            &callback_called));
    run_loop.Run();
    EXPECT_TRUE(callback_called);
    return callback_size_;
  }

  int64_t GetSizeThenClose() {
    base::RunLoop run_loop;
    bool callback_called = false;
    cache_->GetSizeThenClose(base::Bind(&CacheStorageCacheTest::SizeCallback,
                                        base::Unretained(this), &run_loop,
                                        &callback_called));
    run_loop.Run();
    EXPECT_TRUE(callback_called);
    return callback_size_;
  }

  void RequestsCallback(base::RunLoop* run_loop,
                        CacheStorageError error,
                        std::unique_ptr<CacheStorageCache::Requests> requests) {
    callback_error_ = error;
    callback_strings_.clear();
    if (requests) {
      for (size_t i = 0u; i < requests->size(); ++i)
        callback_strings_.push_back(requests->at(i).url.spec());
    }
    if (run_loop)
      run_loop->Quit();
  }

  void ErrorTypeCallback(base::RunLoop* run_loop, CacheStorageError error) {
    callback_error_ = error;
    if (run_loop)
      run_loop->Quit();
  }

  void SequenceCallback(int sequence,
                        int* sequence_out,
                        base::RunLoop* run_loop,
                        CacheStorageError error) {
    *sequence_out = sequence;
    callback_error_ = error;
    if (run_loop)
      run_loop->Quit();
  }

  void ResponseAndErrorCallback(
      base::RunLoop* run_loop,
      CacheStorageError error,
      std::unique_ptr<ServiceWorkerResponse> response,
      std::unique_ptr<storage::BlobDataHandle> body_handle) {
    callback_error_ = error;
    callback_response_ = std::move(response);
    callback_response_data_.reset();
    if (error == CACHE_STORAGE_OK && !callback_response_->blob_uuid.empty())
      callback_response_data_ = std::move(body_handle);

    if (run_loop)
      run_loop->Quit();
  }

  void ResponsesAndErrorCallback(
      const base::Closure& quit_closure,
      std::unique_ptr<CacheStorageCache::Responses>* responses_out,
      std::unique_ptr<CacheStorageCache::BlobDataHandles>* body_handles_out,
      CacheStorageError error,
      std::unique_ptr<CacheStorageCache::Responses> responses,
      std::unique_ptr<CacheStorageCache::BlobDataHandles> body_handles) {
    callback_error_ = error;
    responses_out->swap(responses);
    body_handles_out->swap(body_handles);
    quit_closure.Run();
  }

  void CloseCallback(base::RunLoop* run_loop) {
    EXPECT_FALSE(callback_closed_);
    callback_closed_ = true;
    if (run_loop)
      run_loop->Quit();
  }

  void SizeCallback(base::RunLoop* run_loop,
                    bool* callback_called,
                    int64_t size) {
    *callback_called = true;
    callback_size_ = size;
    if (run_loop)
      run_loop->Quit();
  }

  bool VerifyKeys(const std::vector<std::string>& expected_keys) {
    if (expected_keys.size() != callback_strings_.size())
      return false;

    std::set<std::string> found_set;
    for (int i = 0, max = callback_strings_.size(); i < max; ++i)
      found_set.insert(callback_strings_[i]);

    for (int i = 0, max = expected_keys.size(); i < max; ++i) {
      if (found_set.find(expected_keys[i]) == found_set.end())
        return false;
    }
    return true;
  }

  bool TestResponseType(blink::WebServiceWorkerResponseType response_type) {
    body_response_.response_type = response_type;
    EXPECT_TRUE(Put(body_request_, body_response_));
    EXPECT_TRUE(Match(body_request_));
    EXPECT_TRUE(Delete(body_request_));
    return response_type == callback_response_->response_type;
  }

  void VerifyAllOpsFail() {
    EXPECT_FALSE(Put(no_body_request_, no_body_response_));
    EXPECT_FALSE(Match(no_body_request_));
    EXPECT_FALSE(Delete(body_request_));
    EXPECT_FALSE(Keys());
  }

  virtual bool MemoryOnly() { return false; }

 protected:
  base::ScopedTempDir temp_dir_;
  TestBrowserThreadBundle browser_thread_bundle_;
  TestBrowserContext browser_context_;
  std::unique_ptr<net::URLRequestJobFactoryImpl> url_request_job_factory_;
  scoped_refptr<MockSpecialStoragePolicy> quota_policy_;
  scoped_refptr<MockQuotaManager> mock_quota_manager_;
  scoped_refptr<MockQuotaManagerProxy> quota_manager_proxy_;
  storage::BlobStorageContext* blob_storage_context_;

  std::unique_ptr<TestCacheStorageCache> cache_;

  ServiceWorkerFetchRequest body_request_;
  ServiceWorkerResponse body_response_;
  ServiceWorkerFetchRequest body_request_with_query_;
  ServiceWorkerResponse body_response_with_query_;
  ServiceWorkerFetchRequest no_body_request_;
  ServiceWorkerResponse no_body_response_;
  std::unique_ptr<storage::BlobDataHandle> blob_handle_;
  std::string expected_blob_data_;

  CacheStorageError callback_error_ = CACHE_STORAGE_OK;
  std::unique_ptr<ServiceWorkerResponse> callback_response_;
  std::unique_ptr<storage::BlobDataHandle> callback_response_data_;
  std::vector<std::string> callback_strings_;
  bool callback_closed_ = false;
  int64_t callback_size_ = 0;
};

class CacheStorageCacheTestP : public CacheStorageCacheTest,
                               public testing::WithParamInterface<bool> {
  bool MemoryOnly() override { return !GetParam(); }
};

TEST_P(CacheStorageCacheTestP, PutNoBody) {
  EXPECT_TRUE(Put(no_body_request_, no_body_response_));
}

TEST_P(CacheStorageCacheTestP, PutBody) {
  EXPECT_TRUE(Put(body_request_, body_response_));
}

TEST_P(CacheStorageCacheTestP, PutBody_Multiple) {
  CacheStorageBatchOperation operation1;
  operation1.operation_type = CACHE_STORAGE_CACHE_OPERATION_TYPE_PUT;
  operation1.request = body_request_;
  operation1.request.url = GURL("http://example.com/1");
  operation1.response = body_response_;
  operation1.response.url = GURL("http://example.com/1");

  CacheStorageBatchOperation operation2;
  operation2.operation_type = CACHE_STORAGE_CACHE_OPERATION_TYPE_PUT;
  operation2.request = body_request_;
  operation2.request.url = GURL("http://example.com/2");
  operation2.response = body_response_;
  operation2.response.url = GURL("http://example.com/2");

  CacheStorageBatchOperation operation3;
  operation3.operation_type = CACHE_STORAGE_CACHE_OPERATION_TYPE_PUT;
  operation3.request = body_request_;
  operation3.request.url = GURL("http://example.com/3");
  operation3.response = body_response_;
  operation3.response.url = GURL("http://example.com/3");

  std::vector<CacheStorageBatchOperation> operations;
  operations.push_back(operation1);
  operations.push_back(operation2);
  operations.push_back(operation3);

  EXPECT_EQ(CACHE_STORAGE_OK, BatchOperation(operations));
  EXPECT_TRUE(Match(operation1.request));
  EXPECT_TRUE(Match(operation2.request));
  EXPECT_TRUE(Match(operation3.request));
}

// TODO(nhiroki): Add a test for the case where one of PUT operations fails.
// Currently there is no handy way to fail only one operation in a batch.
// This could be easily achieved after adding some security checks in the
// browser side (http://crbug.com/425505).

TEST_P(CacheStorageCacheTestP, ResponseURLDiffersFromRequestURL) {
  no_body_response_.url = GURL("http://example.com/foobar");
  EXPECT_STRNE("http://example.com/foobar",
               no_body_request_.url.spec().c_str());
  EXPECT_TRUE(Put(no_body_request_, no_body_response_));
  EXPECT_TRUE(Match(no_body_request_));
  EXPECT_STREQ("http://example.com/foobar",
               callback_response_->url.spec().c_str());
}

TEST_P(CacheStorageCacheTestP, ResponseURLEmpty) {
  no_body_response_.url = GURL();
  EXPECT_STRNE("", no_body_request_.url.spec().c_str());
  EXPECT_TRUE(Put(no_body_request_, no_body_response_));
  EXPECT_TRUE(Match(no_body_request_));
  EXPECT_STREQ("", callback_response_->url.spec().c_str());
}

TEST_F(CacheStorageCacheTest, PutBodyDropBlobRef) {
  CacheStorageBatchOperation operation;
  operation.operation_type = CACHE_STORAGE_CACHE_OPERATION_TYPE_PUT;
  operation.request = body_request_;
  operation.response = body_response_;

  std::unique_ptr<base::RunLoop> loop(new base::RunLoop());
  cache_->BatchOperation(
      std::vector<CacheStorageBatchOperation>(1, operation),
      base::Bind(&CacheStorageCacheTestP::ErrorTypeCallback,
                 base::Unretained(this), base::Unretained(loop.get())));
  // The handle should be held by the cache now so the deref here should be
  // okay.
  blob_handle_.reset();
  loop->Run();

  EXPECT_EQ(CACHE_STORAGE_OK, callback_error_);
}

TEST_P(CacheStorageCacheTestP, PutReplace) {
  EXPECT_TRUE(Put(body_request_, no_body_response_));
  EXPECT_TRUE(Match(body_request_));
  EXPECT_FALSE(callback_response_data_);

  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_TRUE(Match(body_request_));
  EXPECT_TRUE(callback_response_data_);

  EXPECT_TRUE(Put(body_request_, no_body_response_));
  EXPECT_TRUE(Match(body_request_));
  EXPECT_FALSE(callback_response_data_);
}

TEST_P(CacheStorageCacheTestP, PutReplcaceInBatch) {
  CacheStorageBatchOperation operation1;
  operation1.operation_type = CACHE_STORAGE_CACHE_OPERATION_TYPE_PUT;
  operation1.request = body_request_;
  operation1.response = no_body_response_;

  CacheStorageBatchOperation operation2;
  operation2.operation_type = CACHE_STORAGE_CACHE_OPERATION_TYPE_PUT;
  operation2.request = body_request_;
  operation2.response = body_response_;

  std::vector<CacheStorageBatchOperation> operations;
  operations.push_back(operation1);
  operations.push_back(operation2);

  EXPECT_EQ(CACHE_STORAGE_OK, BatchOperation(operations));

  // |operation2| should win.
  EXPECT_TRUE(Match(operation2.request));
  EXPECT_TRUE(callback_response_data_);
}

TEST_P(CacheStorageCacheTestP, MatchNoBody) {
  EXPECT_TRUE(Put(no_body_request_, no_body_response_));
  EXPECT_TRUE(Match(no_body_request_));
  EXPECT_TRUE(ResponseMetadataEqual(SetCacheName(no_body_response_),
                                    *callback_response_));
  EXPECT_FALSE(callback_response_data_);
}

TEST_P(CacheStorageCacheTestP, MatchBody) {
  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_TRUE(Match(body_request_));
  EXPECT_TRUE(
      ResponseMetadataEqual(SetCacheName(body_response_), *callback_response_));
  EXPECT_TRUE(
      ResponseBodiesEqual(expected_blob_data_, *callback_response_data_));
}

TEST_P(CacheStorageCacheTestP, MatchAll_Empty) {
  std::unique_ptr<CacheStorageCache::Responses> responses;
  std::unique_ptr<CacheStorageCache::BlobDataHandles> body_handles;
  EXPECT_TRUE(MatchAll(&responses, &body_handles));
  EXPECT_TRUE(responses->empty());
  EXPECT_TRUE(body_handles->empty());
}

TEST_P(CacheStorageCacheTestP, MatchAll_NoBody) {
  EXPECT_TRUE(Put(no_body_request_, no_body_response_));

  std::unique_ptr<CacheStorageCache::Responses> responses;
  std::unique_ptr<CacheStorageCache::BlobDataHandles> body_handles;
  EXPECT_TRUE(MatchAll(&responses, &body_handles));

  ASSERT_EQ(1u, responses->size());
  EXPECT_TRUE(
      ResponseMetadataEqual(SetCacheName(no_body_response_), responses->at(0)));
  EXPECT_TRUE(body_handles->empty());
}

TEST_P(CacheStorageCacheTestP, MatchAll_Body) {
  EXPECT_TRUE(Put(body_request_, body_response_));

  std::unique_ptr<CacheStorageCache::Responses> responses;
  std::unique_ptr<CacheStorageCache::BlobDataHandles> body_handles;
  EXPECT_TRUE(MatchAll(&responses, &body_handles));

  ASSERT_EQ(1u, responses->size());
  ASSERT_EQ(1u, body_handles->size());
  EXPECT_TRUE(
      ResponseMetadataEqual(SetCacheName(body_response_), responses->at(0)));
  EXPECT_TRUE(ResponseBodiesEqual(expected_blob_data_, body_handles->at(0)));
}

TEST_P(CacheStorageCacheTestP, MatchAll_TwoResponsesThenOne) {
  EXPECT_TRUE(Put(no_body_request_, no_body_response_));
  EXPECT_TRUE(Put(body_request_, body_response_));

  std::unique_ptr<CacheStorageCache::Responses> responses;
  std::unique_ptr<CacheStorageCache::BlobDataHandles> body_handles;
  EXPECT_TRUE(MatchAll(&responses, &body_handles));
  ASSERT_EQ(2u, responses->size());
  ASSERT_EQ(1u, body_handles->size());

  // Order of returned responses is not guaranteed.
  std::set<std::string> matched_set;
  for (const ServiceWorkerResponse& response : *responses) {
    if (response.url.spec() == "http://example.com/no_body.html") {
      EXPECT_TRUE(
          ResponseMetadataEqual(SetCacheName(no_body_response_), response));
      matched_set.insert(response.url.spec());
    } else if (response.url.spec() == "http://example.com/body.html") {
      EXPECT_TRUE(
          ResponseMetadataEqual(SetCacheName(body_response_), response));
      EXPECT_TRUE(
          ResponseBodiesEqual(expected_blob_data_, body_handles->at(0)));
      matched_set.insert(response.url.spec());
    }
  }
  EXPECT_EQ(2u, matched_set.size());

  responses->clear();
  body_handles->clear();

  EXPECT_TRUE(Delete(body_request_));
  EXPECT_TRUE(MatchAll(&responses, &body_handles));

  ASSERT_EQ(1u, responses->size());
  EXPECT_TRUE(
      ResponseMetadataEqual(SetCacheName(no_body_response_), responses->at(0)));
  EXPECT_TRUE(body_handles->empty());
}

TEST_P(CacheStorageCacheTestP, MatchAll_IgnoreSearch) {
  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_TRUE(Put(body_request_with_query_, body_response_with_query_));
  EXPECT_TRUE(Put(no_body_request_, no_body_response_));

  std::unique_ptr<CacheStorageCache::Responses> responses;
  std::unique_ptr<CacheStorageCache::BlobDataHandles> body_handles;
  CacheStorageCacheQueryParams match_params;
  match_params.ignore_search = true;
  EXPECT_TRUE(MatchAll(body_request_, match_params, &responses, &body_handles));

  ASSERT_EQ(2u, responses->size());
  ASSERT_EQ(2u, body_handles->size());

  // Order of returned responses is not guaranteed.
  std::set<std::string> matched_set;
  for (const ServiceWorkerResponse& response : *responses) {
    if (response.url.spec() == "http://example.com/body.html?query=test") {
      EXPECT_TRUE(ResponseMetadataEqual(SetCacheName(body_response_with_query_),
                                        response));
      matched_set.insert(response.url.spec());
    } else if (response.url.spec() == "http://example.com/body.html") {
      EXPECT_TRUE(
          ResponseMetadataEqual(SetCacheName(body_response_), response));
      matched_set.insert(response.url.spec());
    }
  }
  EXPECT_EQ(2u, matched_set.size());
}

TEST_P(CacheStorageCacheTestP, Vary) {
  body_request_.headers["vary_foo"] = "foo";
  body_response_.headers["vary"] = "vary_foo";
  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_TRUE(Match(body_request_));

  body_request_.headers["vary_foo"] = "bar";
  EXPECT_FALSE(Match(body_request_));

  body_request_.headers.erase("vary_foo");
  EXPECT_FALSE(Match(body_request_));
}

TEST_P(CacheStorageCacheTestP, EmptyVary) {
  body_response_.headers["vary"] = "";
  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_TRUE(Match(body_request_));

  body_request_.headers["zoo"] = "zoo";
  EXPECT_TRUE(Match(body_request_));
}

TEST_P(CacheStorageCacheTestP, NoVaryButDiffHeaders) {
  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_TRUE(Match(body_request_));

  body_request_.headers["zoo"] = "zoo";
  EXPECT_TRUE(Match(body_request_));
}

TEST_P(CacheStorageCacheTestP, VaryMultiple) {
  body_request_.headers["vary_foo"] = "foo";
  body_request_.headers["vary_bar"] = "bar";
  body_response_.headers["vary"] = " vary_foo    , vary_bar";
  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_TRUE(Match(body_request_));

  body_request_.headers["vary_bar"] = "foo";
  EXPECT_FALSE(Match(body_request_));

  body_request_.headers.erase("vary_bar");
  EXPECT_FALSE(Match(body_request_));
}

TEST_P(CacheStorageCacheTestP, VaryNewHeader) {
  body_request_.headers["vary_foo"] = "foo";
  body_response_.headers["vary"] = " vary_foo, vary_bar";
  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_TRUE(Match(body_request_));

  body_request_.headers["vary_bar"] = "bar";
  EXPECT_FALSE(Match(body_request_));
}

TEST_P(CacheStorageCacheTestP, VaryStar) {
  body_response_.headers["vary"] = "*";
  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_FALSE(Match(body_request_));
}

TEST_P(CacheStorageCacheTestP, EmptyKeys) {
  EXPECT_TRUE(Keys());
  EXPECT_EQ(0u, callback_strings_.size());
}

TEST_P(CacheStorageCacheTestP, TwoKeys) {
  EXPECT_TRUE(Put(no_body_request_, no_body_response_));
  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_TRUE(Keys());
  EXPECT_EQ(2u, callback_strings_.size());
  std::vector<std::string> expected_keys;
  expected_keys.push_back(no_body_request_.url.spec());
  expected_keys.push_back(body_request_.url.spec());
  EXPECT_TRUE(VerifyKeys(expected_keys));
}

TEST_P(CacheStorageCacheTestP, TwoKeysThenOne) {
  EXPECT_TRUE(Put(no_body_request_, no_body_response_));
  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_TRUE(Keys());
  EXPECT_EQ(2u, callback_strings_.size());
  std::vector<std::string> expected_keys;
  expected_keys.push_back(no_body_request_.url.spec());
  expected_keys.push_back(body_request_.url.spec());
  EXPECT_TRUE(VerifyKeys(expected_keys));

  EXPECT_TRUE(Delete(body_request_));
  EXPECT_TRUE(Keys());
  EXPECT_EQ(1u, callback_strings_.size());
  std::vector<std::string> expected_key;
  expected_key.push_back(no_body_request_.url.spec());
  EXPECT_TRUE(VerifyKeys(expected_key));
}

TEST_P(CacheStorageCacheTestP, DeleteNoBody) {
  EXPECT_TRUE(Put(no_body_request_, no_body_response_));
  EXPECT_TRUE(Match(no_body_request_));
  EXPECT_TRUE(Delete(no_body_request_));
  EXPECT_FALSE(Match(no_body_request_));
  EXPECT_FALSE(Delete(no_body_request_));
  EXPECT_TRUE(Put(no_body_request_, no_body_response_));
  EXPECT_TRUE(Match(no_body_request_));
  EXPECT_TRUE(Delete(no_body_request_));
}

TEST_P(CacheStorageCacheTestP, DeleteBody) {
  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_TRUE(Match(body_request_));
  EXPECT_TRUE(Delete(body_request_));
  EXPECT_FALSE(Match(body_request_));
  EXPECT_FALSE(Delete(body_request_));
  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_TRUE(Match(body_request_));
  EXPECT_TRUE(Delete(body_request_));
}

TEST_P(CacheStorageCacheTestP, DeleteWithIgnoreSearchTrue) {
  EXPECT_TRUE(Put(no_body_request_, no_body_response_));
  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_TRUE(Put(body_request_with_query_, body_response_with_query_));

  EXPECT_TRUE(Keys());
  EXPECT_EQ(3u, callback_strings_.size());
  std::vector<std::string> expected_keys;
  expected_keys.push_back(no_body_request_.url.spec());
  expected_keys.push_back(body_request_.url.spec());
  expected_keys.push_back(body_request_with_query_.url.spec());
  EXPECT_TRUE(VerifyKeys(expected_keys));

  // The following delete operation will remove both of body_request_ and
  // body_request_with_query_ from cache storage.
  CacheStorageCacheQueryParams match_params;
  match_params.ignore_search = true;
  EXPECT_TRUE(Delete(body_request_with_query_, match_params));

  EXPECT_TRUE(Keys());
  EXPECT_EQ(1u, callback_strings_.size());
  expected_keys.clear();
  expected_keys.push_back(no_body_request_.url.spec());
  EXPECT_TRUE(VerifyKeys(expected_keys));
}

TEST_P(CacheStorageCacheTestP, DeleteWithIgnoreSearchFalse) {
  EXPECT_TRUE(Put(no_body_request_, no_body_response_));
  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_TRUE(Put(body_request_with_query_, body_response_with_query_));

  EXPECT_TRUE(Keys());
  EXPECT_EQ(3u, callback_strings_.size());
  std::vector<std::string> expected_keys;
  expected_keys.push_back(no_body_request_.url.spec());
  expected_keys.push_back(body_request_.url.spec());
  expected_keys.push_back(body_request_with_query_.url.spec());
  EXPECT_TRUE(VerifyKeys(expected_keys));

  // Default value of ignore_search is false.
  CacheStorageCacheQueryParams match_params;
  match_params.ignore_search = false;
  EXPECT_EQ(match_params.ignore_search,
            CacheStorageCacheQueryParams().ignore_search);

  EXPECT_TRUE(Delete(body_request_with_query_, match_params));

  EXPECT_TRUE(Keys());
  EXPECT_EQ(2u, callback_strings_.size());
  expected_keys.clear();
  expected_keys.push_back(no_body_request_.url.spec());
  expected_keys.push_back(body_request_.url.spec());
  EXPECT_TRUE(VerifyKeys(expected_keys));
}

TEST_P(CacheStorageCacheTestP, QuickStressNoBody) {
  for (int i = 0; i < 100; ++i) {
    EXPECT_FALSE(Match(no_body_request_));
    EXPECT_TRUE(Put(no_body_request_, no_body_response_));
    EXPECT_TRUE(Match(no_body_request_));
    EXPECT_TRUE(Delete(no_body_request_));
  }
}

TEST_P(CacheStorageCacheTestP, QuickStressBody) {
  for (int i = 0; i < 100; ++i) {
    ASSERT_FALSE(Match(body_request_));
    ASSERT_TRUE(Put(body_request_, body_response_));
    ASSERT_TRUE(Match(body_request_));
    ASSERT_TRUE(Delete(body_request_));
  }
}

TEST_P(CacheStorageCacheTestP, PutResponseType) {
  EXPECT_TRUE(TestResponseType(blink::WebServiceWorkerResponseTypeBasic));
  EXPECT_TRUE(TestResponseType(blink::WebServiceWorkerResponseTypeCORS));
  EXPECT_TRUE(TestResponseType(blink::WebServiceWorkerResponseTypeDefault));
  EXPECT_TRUE(TestResponseType(blink::WebServiceWorkerResponseTypeError));
  EXPECT_TRUE(TestResponseType(blink::WebServiceWorkerResponseTypeOpaque));
}

TEST_P(CacheStorageCacheTestP, WriteSideData) {
  base::Time response_time(base::Time::Now());
  ServiceWorkerResponse response(body_response_);
  response.response_time = response_time;
  EXPECT_TRUE(Put(body_request_, response));

  const std::string expected_side_data1 = "SideDataSample";
  scoped_refptr<net::IOBuffer> buffer1(
      new net::StringIOBuffer(expected_side_data1));
  EXPECT_TRUE(WriteSideData(body_request_.url, response_time, buffer1,
                            expected_side_data1.length()));

  EXPECT_TRUE(Match(body_request_));
  EXPECT_TRUE(callback_response_data_);
  EXPECT_TRUE(
      ResponseBodiesEqual(expected_blob_data_, *callback_response_data_));
  EXPECT_TRUE(
      ResponseSideDataEqual(expected_side_data1, *callback_response_data_));

  const std::string expected_side_data2 = "New data";
  scoped_refptr<net::IOBuffer> buffer2(
      new net::StringIOBuffer(expected_side_data2));
  EXPECT_TRUE(WriteSideData(body_request_.url, response_time, buffer2,
                            expected_side_data2.length()));
  EXPECT_TRUE(Match(body_request_));
  EXPECT_TRUE(callback_response_data_);
  EXPECT_TRUE(
      ResponseBodiesEqual(expected_blob_data_, *callback_response_data_));
  EXPECT_TRUE(
      ResponseSideDataEqual(expected_side_data2, *callback_response_data_));

  ASSERT_TRUE(Delete(body_request_));
}

TEST_P(CacheStorageCacheTestP, WriteSideData_QuotaExceeded) {
  mock_quota_manager_->SetQuota(GURL(kOrigin), storage::kStorageTypeTemporary,
                                1024 * 1023);
  base::Time response_time(base::Time::Now());
  ServiceWorkerResponse response;
  response.response_time = response_time;
  EXPECT_TRUE(Put(no_body_request_, response));

  const size_t kSize = 1024 * 1024;
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kSize));
  memset(buffer->data(), 0, kSize);
  EXPECT_FALSE(
      WriteSideData(no_body_request_.url, response_time, buffer, kSize));
  EXPECT_EQ(CACHE_STORAGE_ERROR_QUOTA_EXCEEDED, callback_error_);
  ASSERT_TRUE(Delete(no_body_request_));
}

TEST_P(CacheStorageCacheTestP, WriteSideData_QuotaManagerModified) {
  base::Time response_time(base::Time::Now());
  ServiceWorkerResponse response;
  response.response_time = response_time;
  EXPECT_EQ(0, quota_manager_proxy_->notify_storage_modified_count());
  EXPECT_TRUE(Put(no_body_request_, response));
  // Storage notification happens after the operation returns, so continue the
  // event loop.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, quota_manager_proxy_->notify_storage_modified_count());

  const size_t kSize = 10;
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kSize));
  memset(buffer->data(), 0, kSize);
  EXPECT_TRUE(
      WriteSideData(no_body_request_.url, response_time, buffer, kSize));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, quota_manager_proxy_->notify_storage_modified_count());
  ASSERT_TRUE(Delete(no_body_request_));
}

TEST_P(CacheStorageCacheTestP, WriteSideData_DifferentTimeStamp) {
  base::Time response_time(base::Time::Now());
  ServiceWorkerResponse response;
  response.response_time = response_time;
  EXPECT_TRUE(Put(no_body_request_, response));

  const size_t kSize = 10;
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kSize));
  memset(buffer->data(), 0, kSize);
  EXPECT_FALSE(WriteSideData(no_body_request_.url,
                             response_time + base::TimeDelta::FromSeconds(1),
                             buffer, kSize));
  EXPECT_EQ(CACHE_STORAGE_ERROR_NOT_FOUND, callback_error_);
  ASSERT_TRUE(Delete(no_body_request_));
}

TEST_P(CacheStorageCacheTestP, WriteSideData_NotFound) {
  const size_t kSize = 10;
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kSize));
  memset(buffer->data(), 0, kSize);
  EXPECT_FALSE(WriteSideData(GURL("http://www.example.com/not_exist"),
                             base::Time::Now(), buffer, kSize));
  EXPECT_EQ(CACHE_STORAGE_ERROR_NOT_FOUND, callback_error_);
}

TEST_F(CacheStorageCacheTest, CaselessServiceWorkerResponseHeaders) {
  // CacheStorageCache depends on ServiceWorkerResponse having caseless
  // headers so that it can quickly lookup vary headers.
  ServiceWorkerResponse response(
      GURL("http://www.example.com"), 200, "OK",
      blink::WebServiceWorkerResponseTypeDefault, ServiceWorkerHeaderMap(), "",
      0, GURL(), blink::WebServiceWorkerResponseErrorUnknown, base::Time(),
      false /* is_in_cache_storage */,
      std::string() /* cache_storage_cache_name */,
      ServiceWorkerHeaderList() /* cors_exposed_header_names */);
  response.headers["content-type"] = "foo";
  response.headers["Content-Type"] = "bar";
  EXPECT_EQ("bar", response.headers["content-type"]);
}

TEST_F(CacheStorageCacheTest, CaselessServiceWorkerFetchRequestHeaders) {
  // CacheStorageCache depends on ServiceWorkerFetchRequest having caseless
  // headers so that it can quickly lookup vary headers.
  ServiceWorkerFetchRequest request(GURL("http://www.example.com"), "GET",
                                    ServiceWorkerHeaderMap(), Referrer(),
                                    false);
  request.headers["content-type"] = "foo";
  request.headers["Content-Type"] = "bar";
  EXPECT_EQ("bar", request.headers["content-type"]);
}

TEST_P(CacheStorageCacheTestP, QuotaManagerModified) {
  EXPECT_EQ(0, quota_manager_proxy_->notify_storage_modified_count());

  EXPECT_TRUE(Put(no_body_request_, no_body_response_));
  // Storage notification happens after the operation returns, so continue the
  // event loop.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, quota_manager_proxy_->notify_storage_modified_count());
  EXPECT_LT(0, quota_manager_proxy_->last_notified_delta());
  int64_t sum_delta = quota_manager_proxy_->last_notified_delta();

  EXPECT_TRUE(Put(body_request_, body_response_));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, quota_manager_proxy_->notify_storage_modified_count());
  EXPECT_LT(sum_delta, quota_manager_proxy_->last_notified_delta());
  sum_delta += quota_manager_proxy_->last_notified_delta();

  EXPECT_TRUE(Delete(body_request_));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, quota_manager_proxy_->notify_storage_modified_count());
  sum_delta += quota_manager_proxy_->last_notified_delta();

  EXPECT_TRUE(Delete(no_body_request_));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(4, quota_manager_proxy_->notify_storage_modified_count());
  sum_delta += quota_manager_proxy_->last_notified_delta();

  EXPECT_EQ(0, sum_delta);
}

TEST_P(CacheStorageCacheTestP, PutObeysQuotaLimits) {
  mock_quota_manager_->SetQuota(GURL(kOrigin), storage::kStorageTypeTemporary,
                                0);
  EXPECT_FALSE(Put(body_request_, body_response_));
  EXPECT_EQ(CACHE_STORAGE_ERROR_QUOTA_EXCEEDED, callback_error_);
}

TEST_P(CacheStorageCacheTestP, Size) {
  EXPECT_EQ(0, Size());
  EXPECT_TRUE(Put(no_body_request_, no_body_response_));
  EXPECT_LT(0, Size());
  int64_t no_body_size = Size();

  EXPECT_TRUE(Delete(no_body_request_));
  EXPECT_EQ(0, Size());

  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_LT(no_body_size, Size());

  EXPECT_TRUE(Delete(body_request_));
  EXPECT_EQ(0, Size());
}

TEST_P(CacheStorageCacheTestP, GetSizeThenClose) {
  EXPECT_TRUE(Put(body_request_, body_response_));
  int64_t cache_size = Size();
  EXPECT_EQ(cache_size, GetSizeThenClose());
  VerifyAllOpsFail();
}

TEST_P(CacheStorageCacheTestP, OpsFailOnClosedBackendNeverCreated) {
  cache_->set_delay_backend_creation(
      true);  // Will hang the test if a backend is created.
  EXPECT_TRUE(Close());
  VerifyAllOpsFail();
}

TEST_P(CacheStorageCacheTestP, OpsFailOnClosedBackend) {
  // Create the backend and put something in it.
  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_TRUE(Close());
  VerifyAllOpsFail();
}

TEST_P(CacheStorageCacheTestP, VerifySerialScheduling) {
  // Start two operations, the first one is delayed but the second isn't. The
  // second should wait for the first.
  EXPECT_TRUE(Keys());  // Opens the backend.
  DelayableBackend* delayable_backend = cache_->UseDelayableBackend();
  delayable_backend->set_delay_open(true);

  int sequence_out = -1;

  CacheStorageBatchOperation operation1;
  operation1.operation_type = CACHE_STORAGE_CACHE_OPERATION_TYPE_PUT;
  operation1.request = body_request_;
  operation1.response = body_response_;

  std::unique_ptr<base::RunLoop> close_loop1(new base::RunLoop());
  cache_->BatchOperation(
      std::vector<CacheStorageBatchOperation>(1, operation1),
      base::Bind(&CacheStorageCacheTest::SequenceCallback,
                 base::Unretained(this), 1, &sequence_out, close_loop1.get()));

  // Blocks on opening the cache entry.
  base::RunLoop().RunUntilIdle();

  CacheStorageBatchOperation operation2;
  operation2.operation_type = CACHE_STORAGE_CACHE_OPERATION_TYPE_PUT;
  operation2.request = body_request_;
  operation2.response = body_response_;

  delayable_backend->set_delay_open(false);
  std::unique_ptr<base::RunLoop> close_loop2(new base::RunLoop());
  cache_->BatchOperation(
      std::vector<CacheStorageBatchOperation>(1, operation2),
      base::Bind(&CacheStorageCacheTest::SequenceCallback,
                 base::Unretained(this), 2, &sequence_out, close_loop2.get()));

  // The second put operation should wait for the first to complete.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback_response_);

  delayable_backend->OpenEntryContinue();
  close_loop1->Run();
  EXPECT_EQ(1, sequence_out);
  close_loop2->Run();
  EXPECT_EQ(2, sequence_out);
}

INSTANTIATE_TEST_CASE_P(CacheStorageCacheTest,
                        CacheStorageCacheTestP,
                        ::testing::Values(false, true));

}  // namespace content
