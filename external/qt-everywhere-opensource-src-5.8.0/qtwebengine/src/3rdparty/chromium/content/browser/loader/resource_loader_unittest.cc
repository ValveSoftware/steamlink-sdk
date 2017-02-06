// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_loader.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/loader/redirect_to_file_resource_handler.h"
#include "content/browser/loader/resource_loader_delegate.h"
#include "content/common/ssl_status_serialization.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/resource_response.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_web_contents.h"
#include "ipc/ipc_message.h"
#include "net/base/chunked_upload_data_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/mock_file_stream.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/cert/x509_certificate.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_private_key.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/test_data_directory.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "testing/gtest/include/gtest/gtest.h"

using storage::ShareableFileReference;

namespace content {
namespace {

// Stub client certificate store that returns a preset list of certificates for
// each request and records the arguments of the most recent request for later
// inspection.
class ClientCertStoreStub : public net::ClientCertStore {
 public:
  // Creates a new ClientCertStoreStub that returns |response| on query. It
  // saves the number of requests and most recently certificate authorities list
  // in |requested_authorities| and |request_count|, respectively. The caller is
  // responsible for ensuring those pointers outlive the ClientCertStoreStub.
  //
  // TODO(ppi): Make the stub independent from the internal representation of
  // SSLCertRequestInfo. For now it seems that we can neither save the
  // scoped_refptr<> (since it is never passed to us) nor copy the entire
  // CertificateRequestInfo (since there is no copy constructor).
  ClientCertStoreStub(const net::CertificateList& response,
                      int* request_count,
                      std::vector<std::string>* requested_authorities)
      : response_(response),
        requested_authorities_(requested_authorities),
        request_count_(request_count) {
    requested_authorities_->clear();
    *request_count_ = 0;
  }

  ~ClientCertStoreStub() override {}

  // net::ClientCertStore:
  void GetClientCerts(const net::SSLCertRequestInfo& cert_request_info,
                      net::CertificateList* selected_certs,
                      const base::Closure& callback) override {
    *requested_authorities_ = cert_request_info.cert_authorities;
    ++(*request_count_);

    *selected_certs = response_;
    callback.Run();
  }

 private:
  const net::CertificateList response_;
  std::vector<std::string>* requested_authorities_;
  int* request_count_;
};

// Client certificate store which destroys its resource loader before the
// asynchronous GetClientCerts callback is called.
class LoaderDestroyingCertStore : public net::ClientCertStore {
 public:
  // Creates a client certificate store which, when looked up, posts a task to
  // reset |loader| and then call the callback. The caller is responsible for
  // ensuring the pointers remain valid until the process is complete.
  LoaderDestroyingCertStore(std::unique_ptr<ResourceLoader>* loader,
                            const base::Closure& on_loader_deleted_callback)
      : loader_(loader),
        on_loader_deleted_callback_(on_loader_deleted_callback) {}

  // net::ClientCertStore:
  void GetClientCerts(const net::SSLCertRequestInfo& cert_request_info,
                      net::CertificateList* selected_certs,
                      const base::Closure& cert_selected_callback) override {
    // Don't destroy |loader_| while it's on the stack.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&LoaderDestroyingCertStore::DoCallback,
                              base::Unretained(loader_),
                              cert_selected_callback,
                              on_loader_deleted_callback_));
  }

 private:
  // This needs to be static because |loader| owns the
  // LoaderDestroyingCertStore (ClientCertStores are actually handles, and not
  // global cert stores).
  static void DoCallback(std::unique_ptr<ResourceLoader>* loader,
                         const base::Closure& cert_selected_callback,
                         const base::Closure& on_loader_deleted_callback) {
    loader->reset();
    cert_selected_callback.Run();
    on_loader_deleted_callback.Run();
  }

  std::unique_ptr<ResourceLoader>* loader_;
  base::Closure on_loader_deleted_callback_;

  DISALLOW_COPY_AND_ASSIGN(LoaderDestroyingCertStore);
};

// A mock URLRequestJob which simulates an SSL client auth request.
class MockClientCertURLRequestJob : public net::URLRequestTestJob {
 public:
  MockClientCertURLRequestJob(net::URLRequest* request,
                              net::NetworkDelegate* network_delegate)
      : net::URLRequestTestJob(request, network_delegate),
        weak_factory_(this) {}

  static std::vector<std::string> test_authorities() {
    return std::vector<std::string>(1, "dummy");
  }

  // net::URLRequestTestJob:
  void Start() override {
    scoped_refptr<net::SSLCertRequestInfo> cert_request_info(
        new net::SSLCertRequestInfo);
    cert_request_info->cert_authorities = test_authorities();
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&MockClientCertURLRequestJob::NotifyCertificateRequested,
                   weak_factory_.GetWeakPtr(),
                   base::RetainedRef(cert_request_info)));
  }

  void ContinueWithCertificate(net::X509Certificate* cert,
                               net::SSLPrivateKey* private_key) override {
    net::URLRequestTestJob::Start();
  }

 private:
  ~MockClientCertURLRequestJob() override {}

  base::WeakPtrFactory<MockClientCertURLRequestJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockClientCertURLRequestJob);
};

class MockClientCertJobProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  // URLRequestJobFactory::ProtocolHandler implementation:
  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return new MockClientCertURLRequestJob(request, network_delegate);
  }
};

// Set up dummy values to use in test HTTPS requests.

scoped_refptr<net::X509Certificate> GetTestCert() {
  return net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                 "test_mail_google_com.pem");
}

const net::CertStatus kTestCertError = net::CERT_STATUS_DATE_INVALID;
const int kTestSecurityBits = 256;
// SSL3 TLS_DHE_RSA_WITH_AES_256_CBC_SHA
const int kTestConnectionStatus = 0x300039;

// A mock URLRequestJob which simulates an HTTPS request.
class MockHTTPSURLRequestJob : public net::URLRequestTestJob {
 public:
  MockHTTPSURLRequestJob(net::URLRequest* request,
                         net::NetworkDelegate* network_delegate,
                         const std::string& response_headers,
                         const std::string& response_data,
                         bool auto_advance)
      : net::URLRequestTestJob(request,
                               network_delegate,
                               response_headers,
                               response_data,
                               auto_advance) {}

  // net::URLRequestTestJob:
  void GetResponseInfo(net::HttpResponseInfo* info) override {
    // Get the original response info, but override the SSL info.
    net::URLRequestJob::GetResponseInfo(info);
    info->ssl_info.cert = GetTestCert();
    info->ssl_info.cert_status = kTestCertError;
    info->ssl_info.security_bits = kTestSecurityBits;
    info->ssl_info.connection_status = kTestConnectionStatus;
  }

 private:
  ~MockHTTPSURLRequestJob() override {}

  DISALLOW_COPY_AND_ASSIGN(MockHTTPSURLRequestJob);
};

const char kRedirectHeaders[] =
    "HTTP/1.1 302 Found\n"
    "Location: https://example.test\n"
    "\n";

class MockHTTPSJobURLRequestInterceptor : public net::URLRequestInterceptor {
 public:
  MockHTTPSJobURLRequestInterceptor(bool redirect) : redirect_(redirect) {}
  ~MockHTTPSJobURLRequestInterceptor() override {}

  // net::URLRequestInterceptor:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    std::string headers =
        redirect_ ? std::string(kRedirectHeaders, arraysize(kRedirectHeaders))
                  : net::URLRequestTestJob::test_headers();
    return new MockHTTPSURLRequestJob(request, network_delegate, headers,
                                      "dummy response", true);
  }

 private:
  bool redirect_;
};

// Arbitrary read buffer size.
const int kReadBufSize = 1024;

// Dummy implementation of ResourceHandler, instance of which is needed to
// initialize ResourceLoader.
class ResourceHandlerStub : public ResourceHandler {
 public:
  explicit ResourceHandlerStub(net::URLRequest* request)
      : ResourceHandler(request),
        read_buffer_(new net::IOBuffer(kReadBufSize)),
        defer_request_on_will_start_(false),
        expect_reads_(true),
        cancel_on_read_completed_(false),
        defer_eof_(false),
        received_on_will_read_(false),
        received_eof_(false),
        received_response_completed_(false),
        received_request_redirected_(false),
        total_bytes_downloaded_(0),
        observed_effective_connection_type_(
            net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {}

  // If true, defers the resource load in OnWillStart.
  void set_defer_request_on_will_start(bool defer_request_on_will_start) {
    defer_request_on_will_start_ = defer_request_on_will_start;
  }

  // If true, expect OnWillRead / OnReadCompleted pairs for handling
  // data. Otherwise, expect OnDataDownloaded.
  void set_expect_reads(bool expect_reads) { expect_reads_ = expect_reads; }

  // If true, cancel the request in OnReadCompleted by returning false.
  void set_cancel_on_read_completed(bool cancel_on_read_completed) {
    cancel_on_read_completed_ = cancel_on_read_completed;
  }

  // If true, cancel the request in OnReadCompleted by returning false.
  void set_defer_eof(bool defer_eof) { defer_eof_ = defer_eof; }

  const GURL& start_url() const { return start_url_; }
  ResourceResponse* response() const { return response_.get(); }
  ResourceResponse* redirect_response() const {
    return redirect_response_.get();
  }
  bool received_response_completed() const {
    return received_response_completed_;
  }
  bool received_request_redirected() const {
    return received_request_redirected_;
  }
  const net::URLRequestStatus& status() const { return status_; }
  int total_bytes_downloaded() const { return total_bytes_downloaded_; }

  net::NetworkQualityEstimator::EffectiveConnectionType
  observed_effective_connection_type() const {
    return observed_effective_connection_type_;
  }

  void Resume() {
    controller()->Resume();
  }

  bool OnRequestRedirected(const net::RedirectInfo& redirect_info,
                           ResourceResponse* response,
                           bool* defer) override {
    redirect_response_ = response;
    received_request_redirected_ = true;
    return true;
  }

  bool OnResponseStarted(ResourceResponse* response, bool* defer) override {
    EXPECT_FALSE(response_.get());
    response_ = response;
    observed_effective_connection_type_ =
        response->head.effective_connection_type;
    return true;
  }

  bool OnWillStart(const GURL& url, bool* defer) override {
    EXPECT_TRUE(start_url_.is_empty());
    start_url_ = url;
    if (defer_request_on_will_start_) {
      *defer = true;
      deferred_run_loop_.Quit();
    }
    return true;
  }

  bool OnBeforeNetworkStart(const GURL& url, bool* defer) override {
    return true;
  }

  bool OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                  int* buf_size,
                  int min_size) override {
    EXPECT_TRUE(expect_reads_);
    EXPECT_FALSE(received_on_will_read_);
    EXPECT_FALSE(received_eof_);
    EXPECT_FALSE(received_response_completed_);

    *buf = read_buffer_;
    *buf_size = kReadBufSize;
    received_on_will_read_ = true;
    return true;
  }

  bool OnReadCompleted(int bytes_read, bool* defer) override {
    EXPECT_TRUE(received_on_will_read_);
    EXPECT_TRUE(expect_reads_);
    EXPECT_FALSE(received_response_completed_);

    if (bytes_read == 0) {
      received_eof_ = true;
      if (defer_eof_) {
        defer_eof_ = false;
        *defer = true;
        deferred_run_loop_.Quit();
      }
    }

    // Need another OnWillRead() call before seeing an OnReadCompleted().
    received_on_will_read_ = false;

    return !cancel_on_read_completed_;
  }

  void OnResponseCompleted(const net::URLRequestStatus& status,
                           const std::string& security_info,
                           bool* defer) override {
    EXPECT_FALSE(received_response_completed_);
    if (status.is_success() && expect_reads_)
      EXPECT_TRUE(received_eof_);

    received_response_completed_ = true;
    status_ = status;
    response_completed_run_loop_.Quit();
  }

  void OnDataDownloaded(int bytes_downloaded) override {
    EXPECT_FALSE(expect_reads_);
    total_bytes_downloaded_ += bytes_downloaded;
  }

  // Waits for the the first deferred step to run, if there is one.
  void WaitForDeferredStep() {
    DCHECK(defer_request_on_will_start_ || defer_eof_);
    deferred_run_loop_.Run();
  }

  // Waits until the response has completed.
  void WaitForResponseComplete() {
    response_completed_run_loop_.Run();
    EXPECT_TRUE(received_response_completed_);
  }

 private:
  scoped_refptr<net::IOBuffer> read_buffer_;

  bool defer_request_on_will_start_;
  bool expect_reads_;
  bool cancel_on_read_completed_;
  bool defer_eof_;

  GURL start_url_;
  scoped_refptr<ResourceResponse> response_;
  scoped_refptr<ResourceResponse> redirect_response_;
  bool received_on_will_read_;
  bool received_eof_;
  bool received_response_completed_;
  bool received_request_redirected_;
  net::URLRequestStatus status_;
  int total_bytes_downloaded_;
  base::RunLoop deferred_run_loop_;
  base::RunLoop response_completed_run_loop_;
  std::unique_ptr<base::RunLoop> wait_for_progress_run_loop_;
  net::NetworkQualityEstimator::EffectiveConnectionType
      observed_effective_connection_type_;
};

// Test browser client that captures calls to SelectClientCertificates and
// records the arguments of the most recent call for later inspection.
class SelectCertificateBrowserClient : public TestContentBrowserClient {
 public:
  SelectCertificateBrowserClient() : call_count_(0) {}

  // Waits until the first call to SelectClientCertificate.
  void WaitForSelectCertificate() {
    select_certificate_run_loop_.Run();
    // Process any pending messages - just so tests can check if
    // SelectClientCertificate was called more than once.
    base::RunLoop().RunUntilIdle();
  }

  void SelectClientCertificate(
      WebContents* web_contents,
      net::SSLCertRequestInfo* cert_request_info,
      std::unique_ptr<ClientCertificateDelegate> delegate) override {
    EXPECT_FALSE(delegate_.get());

    ++call_count_;
    passed_certs_ = cert_request_info->client_certs;
    delegate_ = std::move(delegate);
    select_certificate_run_loop_.Quit();
  }

  int call_count() { return call_count_; }
  net::CertificateList passed_certs() { return passed_certs_; }

  void ContinueWithCertificate(net::X509Certificate* cert) {
    delegate_->ContinueWithCertificate(cert);
    delegate_.reset();
  }

  void CancelCertificateSelection() { delegate_.reset(); }

 private:
  net::CertificateList passed_certs_;
  int call_count_;
  std::unique_ptr<ClientCertificateDelegate> delegate_;

  base::RunLoop select_certificate_run_loop_;

  DISALLOW_COPY_AND_ASSIGN(SelectCertificateBrowserClient);
};

// Wraps a ChunkedUploadDataStream to behave as non-chunked to enable upload
// progress reporting.
class NonChunkedUploadDataStream : public net::UploadDataStream {
 public:
  explicit NonChunkedUploadDataStream(uint64_t size)
      : net::UploadDataStream(false, 0), stream_(0), size_(size) {}

  void AppendData(const char* data) {
    stream_.AppendData(data, strlen(data), false);
  }

 private:
  int InitInternal() override {
    SetSize(size_);
    stream_.Init(base::Bind(&NonChunkedUploadDataStream::OnInitCompleted,
                            base::Unretained(this)));
    return net::OK;
  }

  int ReadInternal(net::IOBuffer* buf, int buf_len) override {
    return stream_.Read(buf, buf_len,
                        base::Bind(&NonChunkedUploadDataStream::OnReadCompleted,
                                   base::Unretained(this)));
  }

  void ResetInternal() override { stream_.Reset(); }

  net::ChunkedUploadDataStream stream_;
  uint64_t size_;

  DISALLOW_COPY_AND_ASSIGN(NonChunkedUploadDataStream);
};

// Fails to create a temporary file with the given error.
void CreateTemporaryError(
    base::File::Error error,
    const CreateTemporaryFileStreamCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback, error,
                 base::Passed(std::unique_ptr<net::FileStream>()), nullptr));
}

}  // namespace

class TestNetworkQualityEstimator : public net::NetworkQualityEstimator {
 public:
  TestNetworkQualityEstimator()
      : net::NetworkQualityEstimator(nullptr,
                                     std::map<std::string, std::string>()),
        type_(net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {
  }
  ~TestNetworkQualityEstimator() override {}

  net::NetworkQualityEstimator::EffectiveConnectionType
  GetEffectiveConnectionType() const override {
    return type_;
  }

  void set_effective_connection_type(
      net::NetworkQualityEstimator::EffectiveConnectionType type) {
    type_ = type;
  }

 private:
  net::NetworkQualityEstimator::EffectiveConnectionType type_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkQualityEstimator);
};

class ResourceLoaderTest : public testing::Test,
                           public ResourceLoaderDelegate {
 protected:
  ResourceLoaderTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        test_url_request_context_(true),
        resource_context_(&test_url_request_context_),
        raw_ptr_resource_handler_(NULL),
        raw_ptr_to_request_(NULL) {
    test_url_request_context_.set_job_factory(&job_factory_);
    test_url_request_context_.set_network_quality_estimator(
        &network_quality_estimator_);
    test_url_request_context_.Init();
  }

  GURL test_url() const { return net::URLRequestTestJob::test_url_1(); }

  TestNetworkQualityEstimator* network_quality_estimator() {
    return &network_quality_estimator_;
  }

  std::string test_data() const {
    return net::URLRequestTestJob::test_data_1();
  }

  virtual std::unique_ptr<net::URLRequestJobFactory::ProtocolHandler>
  CreateProtocolHandler() {
    return net::URLRequestTestJob::CreateProtocolHandler();
  }

  virtual std::unique_ptr<ResourceHandler> WrapResourceHandler(
      std::unique_ptr<ResourceHandlerStub> leaf_handler,
      net::URLRequest* request) {
    return std::move(leaf_handler);
  }

  // Replaces loader_ with a new one for |request|.
  void SetUpResourceLoader(std::unique_ptr<net::URLRequest> request,
                           bool is_main_frame) {
    raw_ptr_to_request_ = request.get();

    ResourceType resource_type =
        is_main_frame ? RESOURCE_TYPE_MAIN_FRAME : RESOURCE_TYPE_SUB_FRAME;

    RenderFrameHost* rfh = web_contents_->GetMainFrame();
    ResourceRequestInfo::AllocateForTesting(
        request.get(), resource_type, &resource_context_,
        rfh->GetProcess()->GetID(), rfh->GetRenderViewHost()->GetRoutingID(),
        rfh->GetRoutingID(), is_main_frame, false /* parent_is_main_frame */,
        true /* allow_download */, false /* is_async */,
        false /* is_using_lofi_ */);
    std::unique_ptr<ResourceHandlerStub> resource_handler(
        new ResourceHandlerStub(request.get()));
    raw_ptr_resource_handler_ = resource_handler.get();
    loader_.reset(new ResourceLoader(
        std::move(request),
        WrapResourceHandler(std::move(resource_handler), raw_ptr_to_request_),
        CertStore::GetInstance(), this));
  }

  void SetUp() override {
    job_factory_.SetProtocolHandler("test", CreateProtocolHandler());

    browser_context_.reset(new TestBrowserContext());
    scoped_refptr<SiteInstance> site_instance =
        SiteInstance::Create(browser_context_.get());
    web_contents_.reset(
        TestWebContents::Create(browser_context_.get(), site_instance.get()));

    std::unique_ptr<net::URLRequest> request(
        resource_context_.GetRequestContext()->CreateRequest(
            test_url(), net::DEFAULT_PRIORITY, nullptr /* delegate */));
    SetUpResourceLoader(std::move(request), true);
  }

  void TearDown() override {
    // Destroy the WebContents and pump the event loop before destroying
    // |rvh_test_enabler_| and |thread_bundle_|. This lets asynchronous cleanup
    // tasks complete.
    web_contents_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void SetClientCertStore(std::unique_ptr<net::ClientCertStore> store) {
    dummy_cert_store_ = std::move(store);
  }

  // ResourceLoaderDelegate:
  ResourceDispatcherHostLoginDelegate* CreateLoginDelegate(
      ResourceLoader* loader,
      net::AuthChallengeInfo* auth_info) override {
    return NULL;
  }
  bool HandleExternalProtocol(ResourceLoader* loader,
                              const GURL& url) override {
    return false;
  }
  void DidStartRequest(ResourceLoader* loader) override {}
  void DidReceiveRedirect(ResourceLoader* loader,
                          const GURL& new_url) override {}
  void DidReceiveResponse(ResourceLoader* loader) override {}
  void DidFinishLoading(ResourceLoader* loader) override {}
  std::unique_ptr<net::ClientCertStore> CreateClientCertStore(
      ResourceLoader* loader) override {
    return std::move(dummy_cert_store_);
  }

  TestBrowserThreadBundle thread_bundle_;
  RenderViewHostTestEnabler rvh_test_enabler_;

  net::URLRequestJobFactoryImpl job_factory_;
  TestNetworkQualityEstimator network_quality_estimator_;
  net::TestURLRequestContext test_url_request_context_;
  MockResourceContext resource_context_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<TestWebContents> web_contents_;
  std::unique_ptr<net::ClientCertStore> dummy_cert_store_;

  // The ResourceLoader owns the URLRequest and the ResourceHandler.
  ResourceHandlerStub* raw_ptr_resource_handler_;
  net::URLRequest* raw_ptr_to_request_;
  std::unique_ptr<ResourceLoader> loader_;
};

class ClientCertResourceLoaderTest : public ResourceLoaderTest {
 protected:
  std::unique_ptr<net::URLRequestJobFactory::ProtocolHandler>
  CreateProtocolHandler() override {
    return base::WrapUnique(new MockClientCertJobProtocolHandler);
  }
};

// A ResourceLoaderTest that intercepts https://example.test and
// https://example-redirect.test URLs and sets SSL info on the
// responses. The latter serves a Location: header in the response.
class HTTPSSecurityInfoResourceLoaderTest : public ResourceLoaderTest {
 public:
  HTTPSSecurityInfoResourceLoaderTest()
      : ResourceLoaderTest(),
        test_https_url_("https://example.test"),
        test_https_redirect_url_("https://example-redirect.test") {}

  ~HTTPSSecurityInfoResourceLoaderTest() override {}

  const GURL& test_https_url() const { return test_https_url_; }
  const GURL& test_https_redirect_url() const {
    return test_https_redirect_url_;
  }

 protected:
  void SetUp() override {
    ResourceLoaderTest::SetUp();
    net::URLRequestFilter::GetInstance()->ClearHandlers();
    net::URLRequestFilter::GetInstance()->AddHostnameInterceptor(
        "https", "example.test",
        std::unique_ptr<net::URLRequestInterceptor>(
            new MockHTTPSJobURLRequestInterceptor(false /* redirect */)));
    net::URLRequestFilter::GetInstance()->AddHostnameInterceptor(
        "https", "example-redirect.test",
        std::unique_ptr<net::URLRequestInterceptor>(
            new MockHTTPSJobURLRequestInterceptor(true /* redirect */)));
  }

 private:
  const GURL test_https_url_;
  const GURL test_https_redirect_url_;
};

// Tests that client certificates are requested with ClientCertStore lookup.
TEST_F(ClientCertResourceLoaderTest, WithStoreLookup) {
  // Set up the test client cert store.
  int store_request_count;
  std::vector<std::string> store_requested_authorities;
  net::CertificateList dummy_certs(1, GetTestCert());
  std::unique_ptr<ClientCertStoreStub> test_store(new ClientCertStoreStub(
      dummy_certs, &store_request_count, &store_requested_authorities));
  SetClientCertStore(std::move(test_store));

  // Plug in test content browser client.
  SelectCertificateBrowserClient test_client;
  ContentBrowserClient* old_client = SetBrowserClientForTesting(&test_client);

  // Start the request and wait for it to pause.
  loader_->StartRequest();
  test_client.WaitForSelectCertificate();

  EXPECT_FALSE(raw_ptr_resource_handler_->received_response_completed());

  // Check if the test store was queried against correct |cert_authorities|.
  EXPECT_EQ(1, store_request_count);
  EXPECT_EQ(MockClientCertURLRequestJob::test_authorities(),
            store_requested_authorities);

  // Check if the retrieved certificates were passed to the content browser
  // client.
  EXPECT_EQ(1, test_client.call_count());
  EXPECT_EQ(dummy_certs, test_client.passed_certs());

  // Continue the request.
  test_client.ContinueWithCertificate(nullptr);
  raw_ptr_resource_handler_->WaitForResponseComplete();
  EXPECT_EQ(net::OK, raw_ptr_resource_handler_->status().error());

  // Restore the original content browser client.
  SetBrowserClientForTesting(old_client);
}

// Tests that client certificates are requested on a platform with NULL
// ClientCertStore.
TEST_F(ClientCertResourceLoaderTest, WithNullStore) {
  // Plug in test content browser client.
  SelectCertificateBrowserClient test_client;
  ContentBrowserClient* old_client = SetBrowserClientForTesting(&test_client);

  // Start the request and wait for it to pause.
  loader_->StartRequest();
  test_client.WaitForSelectCertificate();

  // Check if the SelectClientCertificate was called on the content browser
  // client.
  EXPECT_EQ(1, test_client.call_count());
  EXPECT_EQ(net::CertificateList(), test_client.passed_certs());

  // Continue the request.
  test_client.ContinueWithCertificate(nullptr);
  raw_ptr_resource_handler_->WaitForResponseComplete();
  EXPECT_EQ(net::OK, raw_ptr_resource_handler_->status().error());

  // Restore the original content browser client.
  SetBrowserClientForTesting(old_client);
}

// Tests that the ContentBrowserClient may cancel a certificate request.
TEST_F(ClientCertResourceLoaderTest, CancelSelection) {
  // Plug in test content browser client.
  SelectCertificateBrowserClient test_client;
  ContentBrowserClient* old_client = SetBrowserClientForTesting(&test_client);

  // Start the request and wait for it to pause.
  loader_->StartRequest();
  test_client.WaitForSelectCertificate();

  // Check if the SelectClientCertificate was called on the content browser
  // client.
  EXPECT_EQ(1, test_client.call_count());
  EXPECT_EQ(net::CertificateList(), test_client.passed_certs());

  // Cancel the request.
  test_client.CancelCertificateSelection();
  raw_ptr_resource_handler_->WaitForResponseComplete();
  EXPECT_EQ(net::ERR_SSL_CLIENT_AUTH_CERT_NEEDED,
            raw_ptr_resource_handler_->status().error());

  // Restore the original content browser client.
  SetBrowserClientForTesting(old_client);
}

// Verifies that requests without WebContents attached abort.
TEST_F(ClientCertResourceLoaderTest, NoWebContents) {
  // Destroy the WebContents before starting the request.
  web_contents_.reset();

  // Plug in test content browser client.
  SelectCertificateBrowserClient test_client;
  ContentBrowserClient* old_client = SetBrowserClientForTesting(&test_client);

  // Start the request and wait for it to complete.
  loader_->StartRequest();
  raw_ptr_resource_handler_->WaitForResponseComplete();

  // Check that SelectClientCertificate wasn't called and the request aborted.
  EXPECT_EQ(0, test_client.call_count());
  EXPECT_EQ(net::ERR_SSL_CLIENT_AUTH_CERT_NEEDED,
            raw_ptr_resource_handler_->status().error());

  // Restore the original content browser client.
  SetBrowserClientForTesting(old_client);
}

// Verifies that ClientCertStore's callback doesn't crash if called after the
// loader is destroyed.
TEST_F(ClientCertResourceLoaderTest, StoreAsyncCancel) {
  base::RunLoop loader_destroyed_run_loop;
  LoaderDestroyingCertStore* test_store =
      new LoaderDestroyingCertStore(&loader_,
                                    loader_destroyed_run_loop.QuitClosure());
  SetClientCertStore(base::WrapUnique(test_store));

  loader_->StartRequest();
  loader_destroyed_run_loop.Run();
  EXPECT_FALSE(loader_);

  // Pump the event loop to ensure nothing asynchronous crashes either.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ResourceLoaderTest, ResumeCancelledRequest) {
  raw_ptr_resource_handler_->set_defer_request_on_will_start(true);

  loader_->StartRequest();
  loader_->CancelRequest(true);
  static_cast<ResourceController*>(loader_.get())->Resume();
}

// Tests that no invariants are broken if a ResourceHandler cancels during
// OnReadCompleted.
TEST_F(ResourceLoaderTest, CancelOnReadCompleted) {
  raw_ptr_resource_handler_->set_cancel_on_read_completed(true);

  loader_->StartRequest();
  raw_ptr_resource_handler_->WaitForResponseComplete();

  EXPECT_EQ(test_url(), raw_ptr_resource_handler_->start_url());
  EXPECT_EQ(net::URLRequestStatus::CANCELED,
            raw_ptr_resource_handler_->status().status());
}

// Tests that no invariants are broken if a ResourceHandler defers EOF.
TEST_F(ResourceLoaderTest, DeferEOF) {
  raw_ptr_resource_handler_->set_defer_eof(true);

  loader_->StartRequest();
  raw_ptr_resource_handler_->WaitForDeferredStep();

  EXPECT_EQ(test_url(), raw_ptr_resource_handler_->start_url());
  EXPECT_FALSE(raw_ptr_resource_handler_->received_response_completed());

  raw_ptr_resource_handler_->Resume();
  raw_ptr_resource_handler_->WaitForResponseComplete();
  EXPECT_EQ(net::URLRequestStatus::SUCCESS,
            raw_ptr_resource_handler_->status().status());
}

class ResourceLoaderRedirectToFileTest : public ResourceLoaderTest {
 public:
  ResourceLoaderRedirectToFileTest()
      : file_stream_(NULL),
        redirect_to_file_resource_handler_(NULL) {
  }

  ~ResourceLoaderRedirectToFileTest() override {
    // Releasing the loader should result in destroying the file asynchronously.
    file_stream_ = nullptr;
    deletable_file_ = nullptr;
    loader_.reset();

    // Wait for the task to delete the file to run, and make sure the file is
    // cleaned up.
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(base::PathExists(temp_path()));
  }

  base::FilePath temp_path() const { return temp_path_; }
  ShareableFileReference* deletable_file() const {
    return deletable_file_.get();
  }
  net::testing::MockFileStream* file_stream() const { return file_stream_; }
  RedirectToFileResourceHandler* redirect_to_file_resource_handler() const {
    return redirect_to_file_resource_handler_;
  }

  std::unique_ptr<ResourceHandler> WrapResourceHandler(
      std::unique_ptr<ResourceHandlerStub> leaf_handler,
      net::URLRequest* request) override {
    leaf_handler->set_expect_reads(false);

    // Make a temporary file.
    CHECK(base::CreateTemporaryFile(&temp_path_));
    int flags = base::File::FLAG_WRITE | base::File::FLAG_TEMPORARY |
                base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_ASYNC;
    base::File file(temp_path_, flags);
    CHECK(file.IsValid());

    // Create mock file streams and a ShareableFileReference.
    std::unique_ptr<net::testing::MockFileStream> file_stream(
        new net::testing::MockFileStream(std::move(file),
                                         base::ThreadTaskRunnerHandle::Get()));
    file_stream_ = file_stream.get();
    deletable_file_ = ShareableFileReference::GetOrCreate(
        temp_path_,
        ShareableFileReference::DELETE_ON_FINAL_RELEASE,
        BrowserThread::GetMessageLoopProxyForThread(
            BrowserThread::FILE).get());

    // Inject them into the handler.
    std::unique_ptr<RedirectToFileResourceHandler> handler(
        new RedirectToFileResourceHandler(std::move(leaf_handler), request));
    redirect_to_file_resource_handler_ = handler.get();
    handler->SetCreateTemporaryFileStreamFunctionForTesting(
        base::Bind(&ResourceLoaderRedirectToFileTest::PostCallback,
                   base::Unretained(this),
                   base::Passed(&file_stream)));
    return std::move(handler);
  }

 private:
  void PostCallback(std::unique_ptr<net::FileStream> file_stream,
                    const CreateTemporaryFileStreamCallback& callback) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback, base::File::FILE_OK, base::Passed(&file_stream),
                   base::RetainedRef(deletable_file_)));
  }

  base::FilePath temp_path_;
  scoped_refptr<ShareableFileReference> deletable_file_;
  // These are owned by the ResourceLoader.
  net::testing::MockFileStream* file_stream_;
  RedirectToFileResourceHandler* redirect_to_file_resource_handler_;
};

// Tests that a RedirectToFileResourceHandler works and forwards everything
// downstream.
TEST_F(ResourceLoaderRedirectToFileTest, Basic) {
  // Run it to completion.
  loader_->StartRequest();
  raw_ptr_resource_handler_->WaitForResponseComplete();

  // Check that the handler forwarded all information to the downstream handler.
  EXPECT_EQ(temp_path(),
            raw_ptr_resource_handler_->response()->head.download_file_path);
  EXPECT_EQ(test_url(), raw_ptr_resource_handler_->start_url());
  EXPECT_EQ(net::URLRequestStatus::SUCCESS,
            raw_ptr_resource_handler_->status().status());
  EXPECT_EQ(test_data().size(), static_cast<size_t>(
      raw_ptr_resource_handler_->total_bytes_downloaded()));

  // Check that the data was written to the file.
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(temp_path(), &contents));
  EXPECT_EQ(test_data(), contents);
}

// Tests that RedirectToFileResourceHandler handles errors in creating the
// temporary file.
TEST_F(ResourceLoaderRedirectToFileTest, CreateTemporaryError) {
  // Swap out the create temporary function.
  redirect_to_file_resource_handler()->
      SetCreateTemporaryFileStreamFunctionForTesting(
          base::Bind(&CreateTemporaryError, base::File::FILE_ERROR_FAILED));

  // Run it to completion.
  loader_->StartRequest();
  raw_ptr_resource_handler_->WaitForResponseComplete();

  // To downstream, the request was canceled.
  EXPECT_EQ(net::URLRequestStatus::CANCELED,
            raw_ptr_resource_handler_->status().status());
  EXPECT_EQ(0, raw_ptr_resource_handler_->total_bytes_downloaded());
}

// Tests that RedirectToFileResourceHandler handles synchronous write errors.
TEST_F(ResourceLoaderRedirectToFileTest, WriteError) {
  file_stream()->set_forced_error(net::ERR_FAILED);

  // Run it to completion.
  loader_->StartRequest();
  raw_ptr_resource_handler_->WaitForResponseComplete();

  // To downstream, the request was canceled sometime after it started, but
  // before any data was written.
  EXPECT_EQ(temp_path(),
            raw_ptr_resource_handler_->response()->head.download_file_path);
  EXPECT_EQ(test_url(), raw_ptr_resource_handler_->start_url());
  EXPECT_EQ(net::URLRequestStatus::CANCELED,
            raw_ptr_resource_handler_->status().status());
  EXPECT_EQ(0, raw_ptr_resource_handler_->total_bytes_downloaded());
}

// Tests that RedirectToFileResourceHandler handles asynchronous write errors.
TEST_F(ResourceLoaderRedirectToFileTest, WriteErrorAsync) {
  file_stream()->set_forced_error_async(net::ERR_FAILED);

  // Run it to completion.
  loader_->StartRequest();
  raw_ptr_resource_handler_->WaitForResponseComplete();

  // To downstream, the request was canceled sometime after it started, but
  // before any data was written.
  EXPECT_EQ(temp_path(),
            raw_ptr_resource_handler_->response()->head.download_file_path);
  EXPECT_EQ(test_url(), raw_ptr_resource_handler_->start_url());
  EXPECT_EQ(net::URLRequestStatus::CANCELED,
            raw_ptr_resource_handler_->status().status());
  EXPECT_EQ(0, raw_ptr_resource_handler_->total_bytes_downloaded());
}

// Tests that RedirectToFileHandler defers completion if there are outstanding
// writes and accounts for errors which occur in that time.
TEST_F(ResourceLoaderRedirectToFileTest, DeferCompletion) {
  // Program the MockFileStream to error asynchronously, but throttle the
  // callback.
  file_stream()->set_forced_error_async(net::ERR_FAILED);
  file_stream()->ThrottleCallbacks();

  // Run it as far as it will go.
  loader_->StartRequest();
  base::RunLoop().RunUntilIdle();

  // At this point, the request should have completed.
  EXPECT_EQ(net::URLRequestStatus::SUCCESS,
            raw_ptr_to_request_->status().status());

  // However, the resource loader stack is stuck somewhere after receiving the
  // response.
  EXPECT_EQ(temp_path(),
            raw_ptr_resource_handler_->response()->head.download_file_path);
  EXPECT_EQ(test_url(), raw_ptr_resource_handler_->start_url());
  EXPECT_FALSE(raw_ptr_resource_handler_->received_response_completed());
  EXPECT_EQ(0, raw_ptr_resource_handler_->total_bytes_downloaded());

  // Now, release the floodgates.
  file_stream()->ReleaseCallbacks();
  raw_ptr_resource_handler_->WaitForResponseComplete();

  // Although the URLRequest was successful, the leaf handler sees a failure
  // because the write never completed.
  EXPECT_EQ(net::URLRequestStatus::CANCELED,
            raw_ptr_resource_handler_->status().status());
}

// Tests that a RedirectToFileResourceHandler behaves properly when the
// downstream handler defers OnWillStart.
TEST_F(ResourceLoaderRedirectToFileTest, DownstreamDeferStart) {
  // Defer OnWillStart.
  raw_ptr_resource_handler_->set_defer_request_on_will_start(true);

  // Run as far as we'll go.
  loader_->StartRequest();
  raw_ptr_resource_handler_->WaitForDeferredStep();

  // The request should have stopped at OnWillStart.
  EXPECT_EQ(test_url(), raw_ptr_resource_handler_->start_url());
  EXPECT_FALSE(raw_ptr_resource_handler_->response());
  EXPECT_FALSE(raw_ptr_resource_handler_->received_response_completed());
  EXPECT_EQ(0, raw_ptr_resource_handler_->total_bytes_downloaded());

  // Now resume the request. Now we complete.
  raw_ptr_resource_handler_->Resume();
  raw_ptr_resource_handler_->WaitForResponseComplete();

  // Check that the handler forwarded all information to the downstream handler.
  EXPECT_EQ(temp_path(),
            raw_ptr_resource_handler_->response()->head.download_file_path);
  EXPECT_EQ(test_url(), raw_ptr_resource_handler_->start_url());
  EXPECT_EQ(net::URLRequestStatus::SUCCESS,
            raw_ptr_resource_handler_->status().status());
  EXPECT_EQ(test_data().size(), static_cast<size_t>(
      raw_ptr_resource_handler_->total_bytes_downloaded()));

  // Check that the data was written to the file.
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(temp_path(), &contents));
  EXPECT_EQ(test_data(), contents);
}

// Test that an HTTPS resource has the expected security info attached
// to it.
TEST_F(HTTPSSecurityInfoResourceLoaderTest, SecurityInfoOnHTTPSResource) {
  // Start the request and wait for it to finish.
  std::unique_ptr<net::URLRequest> request(
      resource_context_.GetRequestContext()->CreateRequest(
          test_https_url(), net::DEFAULT_PRIORITY, nullptr /* delegate */));
  SetUpResourceLoader(std::move(request), true);

  // Send the request and wait until it completes.
  loader_->StartRequest();
  raw_ptr_resource_handler_->WaitForResponseComplete();
  ASSERT_EQ(net::URLRequestStatus::SUCCESS,
            raw_ptr_to_request_->status().status());

  ResourceResponse* response = raw_ptr_resource_handler_->response();
  ASSERT_TRUE(response);

  // Deserialize the security info from the response and check that it
  // is as expected.
  SSLStatus deserialized;
  ASSERT_TRUE(
      DeserializeSecurityInfo(response->head.security_info, &deserialized));

  // Expect a BROKEN security style because the cert status has errors.
  EXPECT_EQ(content::SECURITY_STYLE_AUTHENTICATION_BROKEN,
            deserialized.security_style);
  scoped_refptr<net::X509Certificate> cert;
  ASSERT_TRUE(
      CertStore::GetInstance()->RetrieveCert(deserialized.cert_id, &cert));
  EXPECT_TRUE(cert->Equals(GetTestCert().get()));

  EXPECT_EQ(kTestCertError, deserialized.cert_status);
  EXPECT_EQ(kTestConnectionStatus, deserialized.connection_status);
  EXPECT_EQ(kTestSecurityBits, deserialized.security_bits);
}

// Test that an HTTPS redirect response has the expected security info
// attached to it.
TEST_F(HTTPSSecurityInfoResourceLoaderTest,
       SecurityInfoOnHTTPSRedirectResource) {
  // Start the request and wait for it to finish.
  std::unique_ptr<net::URLRequest> request(
      resource_context_.GetRequestContext()->CreateRequest(
          test_https_redirect_url(), net::DEFAULT_PRIORITY,
          nullptr /* delegate */));
  SetUpResourceLoader(std::move(request), true);

  // Send the request and wait until it completes.
  loader_->StartRequest();
  raw_ptr_resource_handler_->WaitForResponseComplete();
  ASSERT_EQ(net::URLRequestStatus::SUCCESS,
            raw_ptr_to_request_->status().status());
  ASSERT_TRUE(raw_ptr_resource_handler_->received_request_redirected());

  ResourceResponse* redirect_response =
      raw_ptr_resource_handler_->redirect_response();
  ASSERT_TRUE(redirect_response);

  // Deserialize the security info from the redirect response and check
  // that it is as expected.
  SSLStatus deserialized;
  ASSERT_TRUE(DeserializeSecurityInfo(redirect_response->head.security_info,
                                      &deserialized));

  // Expect a BROKEN security style because the cert status has errors.
  EXPECT_EQ(content::SECURITY_STYLE_AUTHENTICATION_BROKEN,
            deserialized.security_style);
  scoped_refptr<net::X509Certificate> cert;
  ASSERT_TRUE(
      CertStore::GetInstance()->RetrieveCert(deserialized.cert_id, &cert));
  EXPECT_TRUE(cert->Equals(GetTestCert().get()));

  EXPECT_EQ(kTestCertError, deserialized.cert_status);
  EXPECT_EQ(kTestConnectionStatus, deserialized.connection_status);
  EXPECT_EQ(kTestSecurityBits, deserialized.security_bits);
}

class EffectiveConnectionTypeResourceLoaderTest : public ResourceLoaderTest {
 public:
  void VerifyEffectiveConnectionType(
      bool is_main_frame,
      net::NetworkQualityEstimator::EffectiveConnectionType set_type,
      net::NetworkQualityEstimator::EffectiveConnectionType expected_type) {
    network_quality_estimator()->set_effective_connection_type(set_type);

    // Start the request and wait for it to finish.
    std::unique_ptr<net::URLRequest> request(
        resource_context_.GetRequestContext()->CreateRequest(
            test_url(), net::DEFAULT_PRIORITY, nullptr /* delegate */));
    SetUpResourceLoader(std::move(request), is_main_frame);

    // Send the request and wait until it completes.
    loader_->StartRequest();
    raw_ptr_resource_handler_->WaitForResponseComplete();
    ASSERT_EQ(net::URLRequestStatus::SUCCESS,
              raw_ptr_to_request_->status().status());

    EXPECT_EQ(expected_type,
              raw_ptr_resource_handler_->observed_effective_connection_type());
  }
};

// Tests that the effective connection type is set on main frame requests.
TEST_F(EffectiveConnectionTypeResourceLoaderTest, Slow2G) {
  VerifyEffectiveConnectionType(
      true, net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
      net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
}

// Tests that the effective connection type is set on main frame requests.
TEST_F(EffectiveConnectionTypeResourceLoaderTest, 3G) {
  VerifyEffectiveConnectionType(
      true, net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_3G,
      net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_3G);
}

// Tests that the effective connection type is not set on non-main frame
// requests.
TEST_F(EffectiveConnectionTypeResourceLoaderTest, NotAMainFrame) {
  VerifyEffectiveConnectionType(
      false, net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_3G,
      net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
}

}  // namespace content
