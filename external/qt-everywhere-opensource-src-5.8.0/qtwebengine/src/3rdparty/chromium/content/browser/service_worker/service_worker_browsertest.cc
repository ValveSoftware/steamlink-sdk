// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/cache_storage/cache_storage_cache.h"
#include "content/browser/cache_storage/cache_storage_cache_handle.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/referrer.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/security_style.h"
#include "content/public/common/ssl_status.h"
#include "content/public/common/web_preferences.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/test/test_content_browser_client.h"
#include "net/log/net_log.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_test_job.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_data_snapshot.h"
#include "storage/browser/blob/blob_reader.h"
#include "storage/browser/blob/blob_storage_context.h"

namespace content {

namespace {

struct FetchResult {
  ServiceWorkerStatusCode status;
  ServiceWorkerFetchEventResult result;
  ServiceWorkerResponse response;
  std::unique_ptr<storage::BlobDataHandle> blob_data_handle;
};

void RunAndQuit(const base::Closure& closure,
                const base::Closure& quit,
                base::SingleThreadTaskRunner* original_message_loop) {
  closure.Run();
  original_message_loop->PostTask(FROM_HERE, quit);
}

void RunOnIOThreadWithDelay(const base::Closure& closure,
                            base::TimeDelta delay) {
  base::RunLoop run_loop;
  BrowserThread::PostDelayedTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&RunAndQuit, closure, run_loop.QuitClosure(),
                 base::RetainedRef(base::ThreadTaskRunnerHandle::Get())),
      delay);
  run_loop.Run();
}

void RunOnIOThread(const base::Closure& closure) {
  RunOnIOThreadWithDelay(closure, base::TimeDelta());
}

void RunOnIOThread(
    const base::Callback<void(const base::Closure& continuation)>& closure) {
  base::RunLoop run_loop;
  base::Closure quit_on_original_thread =
      base::Bind(base::IgnoreResult(&base::SingleThreadTaskRunner::PostTask),
                 base::ThreadTaskRunnerHandle::Get().get(),
                 FROM_HERE,
                 run_loop.QuitClosure());
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(closure, quit_on_original_thread));
  run_loop.Run();
}

void ReceivePrepareResult(bool* is_prepared) {
  *is_prepared = true;
}

base::Closure CreatePrepareReceiver(bool* is_prepared) {
  return base::Bind(&ReceivePrepareResult, is_prepared);
}

void ReceiveFindRegistrationStatus(
    BrowserThread::ID run_quit_thread,
    const base::Closure& quit,
    ServiceWorkerStatusCode* out_status,
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  *out_status = status;
  if (!quit.is_null())
    BrowserThread::PostTask(run_quit_thread, FROM_HERE, quit);
}

ServiceWorkerStorage::FindRegistrationCallback CreateFindRegistrationReceiver(
    BrowserThread::ID run_quit_thread,
    const base::Closure& quit,
    ServiceWorkerStatusCode* status) {
  return base::Bind(&ReceiveFindRegistrationStatus, run_quit_thread, quit,
                    status);
}

void ReadResponseBody(std::string* body,
                      storage::BlobDataHandle* blob_data_handle) {
  ASSERT_TRUE(blob_data_handle);
  std::unique_ptr<storage::BlobDataSnapshot> data =
      blob_data_handle->CreateSnapshot();
  ASSERT_EQ(1U, data->items().size());
  *body = std::string(data->items()[0]->bytes(), data->items()[0]->length());
}

void ExpectResultAndRun(bool expected,
                        const base::Closure& continuation,
                        bool actual) {
  EXPECT_EQ(expected, actual);
  continuation.Run();
}

class WorkerActivatedObserver
    : public ServiceWorkerContextObserver,
      public base::RefCountedThreadSafe<WorkerActivatedObserver> {
 public:
  explicit WorkerActivatedObserver(ServiceWorkerContextWrapper* context)
      : context_(context) {}
  void Init() {
    RunOnIOThread(base::Bind(&WorkerActivatedObserver::InitOnIOThread, this));
  }
  // ServiceWorkerContextObserver overrides.
  void OnVersionStateChanged(int64_t version_id,
                             ServiceWorkerVersion::Status) override {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    const ServiceWorkerVersion* version = context_->GetLiveVersion(version_id);
    if (version->status() == ServiceWorkerVersion::ACTIVATED) {
      context_->RemoveObserver(this);
      BrowserThread::PostTask(BrowserThread::UI,
                              FROM_HERE,
                              base::Bind(&WorkerActivatedObserver::Quit, this));
    }
  }
  void Wait() { run_loop_.Run(); }

 private:
  friend class base::RefCountedThreadSafe<WorkerActivatedObserver>;
  ~WorkerActivatedObserver() override {}
  void InitOnIOThread() { context_->AddObserver(this); }
  void Quit() { run_loop_.Quit(); }

  base::RunLoop run_loop_;
  ServiceWorkerContextWrapper* context_;
  DISALLOW_COPY_AND_ASSIGN(WorkerActivatedObserver);
};

std::unique_ptr<net::test_server::HttpResponse>
VerifyServiceWorkerHeaderInRequest(
    const net::test_server::HttpRequest& request) {
  EXPECT_EQ(request.relative_url, "/service_worker/generated_sw.js");
  auto it = request.headers.find("Service-Worker");
  EXPECT_TRUE(it != request.headers.end());
  EXPECT_EQ("script", it->second);

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse());
  http_response->set_content_type("text/javascript");
  return std::move(http_response);
}

std::unique_ptr<net::test_server::HttpResponse> VerifySaveDataHeaderInRequest(
    const net::test_server::HttpRequest& request) {
  auto it = request.headers.find("Save-Data");
  EXPECT_NE(request.headers.end(), it);
  EXPECT_EQ("on", it->second);

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse());
  http_response->set_content_type("text/javascript");
  return std::move(http_response);
}

std::unique_ptr<net::test_server::HttpResponse>
VerifySaveDataHeaderNotInRequest(const net::test_server::HttpRequest& request) {
  auto it = request.headers.find("Save-Data");
  EXPECT_EQ(request.headers.end(), it);
  return base::WrapUnique(new net::test_server::BasicHttpResponse());
}

std::unique_ptr<net::test_server::HttpResponse>
VerifySaveDataNotInAccessControlRequestHeader(
    const net::test_server::HttpRequest& request) {
  // Save-Data should be present.
  auto it = request.headers.find("Save-Data");
  EXPECT_NE(request.headers.end(), it);
  EXPECT_EQ("on", it->second);

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse());
  if (request.method == net::test_server::METHOD_OPTIONS) {
    // Access-Control-Request-Headers should contain 'X-Custom-Header' and not
    // contain 'Save-Data'.
    auto acrh_iter = request.headers.find("Access-Control-Request-Headers");
    EXPECT_NE(request.headers.end(), acrh_iter);
    EXPECT_NE(std::string::npos, acrh_iter->second.find("x-custom-header"));
    EXPECT_EQ(std::string::npos, acrh_iter->second.find("save-data"));
    http_response->AddCustomHeader("Access-Control-Allow-Headers",
                                   acrh_iter->second);
    http_response->AddCustomHeader("Access-Control-Allow-Methods", "GET");
    http_response->AddCustomHeader("Access-Control-Allow-Origin", "*");
  } else {
    http_response->AddCustomHeader("Access-Control-Allow-Origin", "*");
    http_response->set_content("PASS");
  }
  return std::move(http_response);
}

// The ImportsBustMemcache test requires that the imported script
// would naturally be cached in blink's memcache, but the embedded
// test server doesn't produce headers that allow the blink's memcache
// to do that. This interceptor injects headers that give the import
// an experiration far in the future.
class LongLivedResourceInterceptor : public net::URLRequestInterceptor {
 public:
  explicit LongLivedResourceInterceptor(const std::string& body)
      : body_(body) {}
  ~LongLivedResourceInterceptor() override {}

  // net::URLRequestInterceptor implementation
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    const char kHeaders[] =
        "HTTP/1.1 200 OK\n"
        "Content-Type: text/javascript\n"
        "Expires: Thu, 1 Jan 2100 20:00:00 GMT\n"
        "\n";
    std::string headers(kHeaders, arraysize(kHeaders));
    return new net::URLRequestTestJob(
        request, network_delegate, headers, body_, true);
  }

 private:
  std::string body_;
  DISALLOW_COPY_AND_ASSIGN(LongLivedResourceInterceptor);
};

void CreateLongLivedResourceInterceptors(
    const GURL& worker_url, const GURL& import_url) {
  ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
  std::unique_ptr<net::URLRequestInterceptor> interceptor;

  interceptor.reset(new LongLivedResourceInterceptor(
      "importScripts('long_lived_import.js');"));
  net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
      worker_url, std::move(interceptor));

  interceptor.reset(new LongLivedResourceInterceptor(
      "// the imported script does nothing"));
  net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
      import_url, std::move(interceptor));
}

void CountScriptResources(
    ServiceWorkerContextWrapper* wrapper,
    const GURL& scope,
    int* num_resources) {
  *num_resources = -1;

  std::vector<ServiceWorkerRegistrationInfo> infos =
      wrapper->GetAllLiveRegistrationInfo();
  if (infos.empty())
    return;

  int version_id;
  size_t index = infos.size() - 1;
  if (infos[index].installing_version.version_id !=
      kInvalidServiceWorkerVersionId)
    version_id = infos[index].installing_version.version_id;
  else if (infos[index].waiting_version.version_id !=
           kInvalidServiceWorkerVersionId)
    version_id = infos[1].waiting_version.version_id;
  else if (infos[index].active_version.version_id !=
           kInvalidServiceWorkerVersionId)
    version_id = infos[index].active_version.version_id;
  else
    return;

  ServiceWorkerVersion* version = wrapper->GetLiveVersion(version_id);
  *num_resources = static_cast<int>(version->script_cache_map()->size());
}

}  // namespace

class ServiceWorkerBrowserTest : public ContentBrowserTest {
 protected:
  using self = ServiceWorkerBrowserTest;

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    StoragePartition* partition = BrowserContext::GetDefaultStoragePartition(
        shell()->web_contents()->GetBrowserContext());
    wrapper_ = static_cast<ServiceWorkerContextWrapper*>(
        partition->GetServiceWorkerContext());

    // Navigate to the page to set up a renderer page (where we can embed
    // a worker).
    NavigateToURLBlockUntilNavigationsComplete(
        shell(),
        embedded_test_server()->GetURL("/service_worker/empty.html"), 1);

    RunOnIOThread(base::Bind(&self::SetUpOnIOThread, this));
  }

  void TearDownOnMainThread() override {
    RunOnIOThread(base::Bind(&self::TearDownOnIOThread, this));
    wrapper_ = NULL;
  }

  virtual void SetUpOnIOThread() {}
  virtual void TearDownOnIOThread() {}

  ServiceWorkerContextWrapper* wrapper() { return wrapper_.get(); }
  ServiceWorkerContext* public_context() { return wrapper(); }

  void AssociateRendererProcessToPattern(const GURL& pattern) {
    wrapper_->process_manager()->AddProcessReferenceToPattern(
        pattern, shell()->web_contents()->GetRenderProcessHost()->GetID());
  }

 private:
  scoped_refptr<ServiceWorkerContextWrapper> wrapper_;
};

class ConsoleListener : public EmbeddedWorkerInstance::Listener {
 public:
  void OnReportConsoleMessage(int source_identifier,
                              int message_level,
                              const base::string16& message,
                              int line_number,
                              const GURL& source_url) override {
    messages_.push_back(message);
    if (!quit_.is_null() && messages_.size() == expected_message_count_) {
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, quit_);
      quit_.Reset();
    }
  }

  void WaitForConsoleMessages(size_t expected_message_count) {
    if (messages_.size() >= expected_message_count)
      return;

    expected_message_count_ = expected_message_count;
    base::RunLoop console_run_loop;
    quit_ = console_run_loop.QuitClosure();
    console_run_loop.Run();

    ASSERT_EQ(messages_.size(), expected_message_count);
  }

  bool OnMessageReceived(const IPC::Message& message) override { return false; }
  const std::vector<base::string16>& messages() const { return messages_; }

 private:
  std::vector<base::string16> messages_;
  size_t expected_message_count_;
  base::Closure quit_;
};

class ServiceWorkerVersionBrowserTest : public ServiceWorkerBrowserTest {
 public:
  using self = ServiceWorkerVersionBrowserTest;

  ~ServiceWorkerVersionBrowserTest() override {}

  void TearDownOnIOThread() override {
    registration_ = NULL;
    version_ = NULL;
  }

  void InstallTestHelper(const std::string& worker_url,
                         ServiceWorkerStatusCode expected_status) {
    RunOnIOThread(base::Bind(&self::SetUpRegistrationOnIOThread, this,
                             worker_url));

    // Dispatch install on a worker.
    ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
    base::RunLoop install_run_loop;
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(&self::InstallOnIOThread, this,
                                       install_run_loop.QuitClosure(),
                                       &status));
    install_run_loop.Run();
    ASSERT_EQ(expected_status, status);

    // Stop the worker.
    status = SERVICE_WORKER_ERROR_FAILED;
    base::RunLoop stop_run_loop;
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(&self::StopOnIOThread, this,
                                       stop_run_loop.QuitClosure(),
                                       &status));
    stop_run_loop.Run();
    ASSERT_EQ(SERVICE_WORKER_OK, status);
  }

  void ActivateTestHelper(
      const std::string& worker_url,
      ServiceWorkerStatusCode expected_status) {
    RunOnIOThread(
        base::Bind(&self::SetUpRegistrationOnIOThread, this, worker_url));
    ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
    base::RunLoop run_loop;
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(
            &self::ActivateOnIOThread, this, run_loop.QuitClosure(), &status));
    run_loop.Run();
    ASSERT_EQ(expected_status, status);
  }

  void FetchOnRegisteredWorker(
      ServiceWorkerFetchEventResult* result,
      ServiceWorkerResponse* response,
      std::unique_ptr<storage::BlobDataHandle>* blob_data_handle) {
    blob_context_ = ChromeBlobStorageContext::GetFor(
        shell()->web_contents()->GetBrowserContext());
    bool prepare_result = false;
    FetchResult fetch_result;
    fetch_result.status = SERVICE_WORKER_ERROR_FAILED;
    base::RunLoop fetch_run_loop;
    BrowserThread::PostTask(BrowserThread::IO,
                            FROM_HERE,
                            base::Bind(&self::FetchOnIOThread,
                                       this,
                                       fetch_run_loop.QuitClosure(),
                                       &prepare_result,
                                       &fetch_result));
    fetch_run_loop.Run();
    ASSERT_TRUE(prepare_result);
    *result = fetch_result.result;
    *response = fetch_result.response;
    *blob_data_handle = std::move(fetch_result.blob_data_handle);
    ASSERT_EQ(SERVICE_WORKER_OK, fetch_result.status);
  }

  void FetchTestHelper(
      const std::string& worker_url,
      ServiceWorkerFetchEventResult* result,
      ServiceWorkerResponse* response,
      std::unique_ptr<storage::BlobDataHandle>* blob_data_handle) {
    RunOnIOThread(
        base::Bind(&self::SetUpRegistrationOnIOThread, this, worker_url));
    FetchOnRegisteredWorker(result, response, blob_data_handle);
  }

  void SetUpRegistrationOnIOThread(const std::string& worker_url) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    const GURL pattern = embedded_test_server()->GetURL("/service_worker/");
    registration_ = new ServiceWorkerRegistration(
        pattern,
        wrapper()->context()->storage()->NewRegistrationId(),
        wrapper()->context()->AsWeakPtr());
    // Set the update check time to avoid triggering updates in the middle of
    // tests.
    registration_->set_last_update_check(base::Time::Now());

    version_ = new ServiceWorkerVersion(
        registration_.get(),
        embedded_test_server()->GetURL(worker_url),
        wrapper()->context()->storage()->NewVersionId(),
        wrapper()->context()->AsWeakPtr());

    // Make the registration findable via storage functions.
    wrapper()->context()->storage()->NotifyInstallingRegistration(
        registration_.get());

    AssociateRendererProcessToPattern(pattern);
  }

  void TimeoutWorkerOnIOThread() {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    version_->SimulatePingTimeoutForTesting();
  }

  void AddControlleeOnIOThread() {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    std::unique_ptr<ServiceWorkerProviderHost> host(
        new ServiceWorkerProviderHost(
            33 /* dummy render process id */,
            MSG_ROUTING_NONE /* render_frame_id */, 1 /* dummy provider_id */,
            SERVICE_WORKER_PROVIDER_FOR_WINDOW,
            ServiceWorkerProviderHost::FrameSecurityLevel::SECURE,
            wrapper()->context()->AsWeakPtr(), NULL));
    host->SetDocumentUrl(
        embedded_test_server()->GetURL("/service_worker/host"));
    host->AssociateRegistration(registration_.get(),
                                false /* notify_controllerchange */);
    wrapper()->context()->AddProviderHost(std::move(host));
  }

  void AddWaitingWorkerOnIOThread(const std::string& worker_url) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    scoped_refptr<ServiceWorkerVersion> waiting_version(
        new ServiceWorkerVersion(
            registration_.get(), embedded_test_server()->GetURL(worker_url),
            wrapper()->context()->storage()->NewVersionId(),
            wrapper()->context()->AsWeakPtr()));
    waiting_version->SetStatus(ServiceWorkerVersion::INSTALLED);
    registration_->SetWaitingVersion(waiting_version.get());
    registration_->ActivateWaitingVersionWhenReady();
  }

  void StartWorker(ServiceWorkerStatusCode expected_status) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
    base::RunLoop start_run_loop;
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(&self::StartOnIOThread, this,
                                       start_run_loop.QuitClosure(),
                                       &status));
    start_run_loop.Run();
    ASSERT_EQ(expected_status, status);
  }

  void StopWorker(ServiceWorkerStatusCode expected_status) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
    base::RunLoop stop_run_loop;
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(&self::StopOnIOThread, this,
                                       stop_run_loop.QuitClosure(),
                                       &status));
    stop_run_loop.Run();
    ASSERT_EQ(expected_status, status);
  }

  void StoreRegistration(int64_t version_id,
                         ServiceWorkerStatusCode expected_status) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
    base::RunLoop store_run_loop;
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&self::StoreOnIOThread, this, store_run_loop.QuitClosure(),
                   &status, version_id));
    store_run_loop.Run();
    ASSERT_EQ(expected_status, status);

    RunOnIOThread(base::Bind(&self::NotifyDoneInstallingRegistrationOnIOThread,
                             this, status));
  }

  void FindRegistrationForId(int64_t id,
                             const GURL& origin,
                             ServiceWorkerStatusCode expected_status) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
    base::RunLoop run_loop;
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&self::FindRegistrationForIdOnIOThread, this,
                   run_loop.QuitClosure(), &status, id, origin));
    run_loop.Run();
    ASSERT_EQ(expected_status, status);
  }

  void FindRegistrationForIdOnIOThread(const base::Closure& done,
                                       ServiceWorkerStatusCode* result,
                                       int64_t id,
                                       const GURL& origin) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    wrapper()->context()->storage()->FindRegistrationForId(
        id, origin,
        CreateFindRegistrationReceiver(BrowserThread::UI, done, result));
  }

  void NotifyDoneInstallingRegistrationOnIOThread(
      ServiceWorkerStatusCode status) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    wrapper()->context()->storage()->NotifyDoneInstallingRegistration(
        registration_.get(), version_.get(), status);
  }

  void RemoveLiveRegistrationOnIOThread(int64_t id) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    wrapper()->context()->RemoveLiveRegistration(id);
  }

  void StartOnIOThread(const base::Closure& done,
                       ServiceWorkerStatusCode* result) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                          CreateReceiver(BrowserThread::UI, done, result));
  }

  void InstallOnIOThread(const base::Closure& done,
                         ServiceWorkerStatusCode* result) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    version_->SetStatus(ServiceWorkerVersion::INSTALLING);
    version_->RunAfterStartWorker(
        ServiceWorkerMetrics::EventType::INSTALL,
        base::Bind(&self::DispatchInstallEventOnIOThread, this, done, result),
        CreateReceiver(BrowserThread::UI, done, result));
  }

  void DispatchInstallEventOnIOThread(const base::Closure& done,
                                      ServiceWorkerStatusCode* result) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    int request_id =
        version_->StartRequest(ServiceWorkerMetrics::EventType::INSTALL,
                               CreateReceiver(BrowserThread::UI, done, result));
    version_
        ->RegisterRequestCallback<ServiceWorkerHostMsg_InstallEventFinished>(
            request_id, base::Bind(&self::ReceiveInstallEventOnIOThread, this,
                                   done, result));
    version_->DispatchEvent({request_id},
                            ServiceWorkerMsg_InstallEvent(request_id));
  }

  void ReceiveInstallEventOnIOThread(const base::Closure& done,
                                     ServiceWorkerStatusCode* out_result,
                                     int request_id,
                                     blink::WebServiceWorkerEventResult result,
                                     bool has_fetch_handler) {
    version_->FinishRequest(
        request_id, result == blink::WebServiceWorkerEventResultCompleted);
    version_->set_has_fetch_handler(has_fetch_handler);

    ServiceWorkerStatusCode status = SERVICE_WORKER_OK;
    if (result == blink::WebServiceWorkerEventResultRejected)
      status = SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED;

    *out_result = status;
    if (!done.is_null())
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, done);
  }

  void StoreOnIOThread(const base::Closure& done,
                       ServiceWorkerStatusCode* result,
                       int64_t version_id) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    ServiceWorkerVersion* version =
        wrapper()->context()->GetLiveVersion(version_id);
    wrapper()->context()->storage()->StoreRegistration(
        registration_.get(), version,
        CreateReceiver(BrowserThread::UI, done, result));
  }

  void ActivateOnIOThread(const base::Closure& done,
                          ServiceWorkerStatusCode* result) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    version_->SetStatus(ServiceWorkerVersion::ACTIVATING);
    registration_->SetActiveVersion(version_.get());
    version_->RunAfterStartWorker(
        ServiceWorkerMetrics::EventType::ACTIVATE,
        base::Bind(&self::DispatchActivateEventOnIOThread, this, done, result),
        CreateReceiver(BrowserThread::UI, done, result));
  }

  void DispatchActivateEventOnIOThread(const base::Closure& done,
                                       ServiceWorkerStatusCode* result) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    int request_id =
        version_->StartRequest(ServiceWorkerMetrics::EventType::INSTALL,
                               CreateReceiver(BrowserThread::UI, done, result));
    version_->DispatchSimpleEvent<ServiceWorkerHostMsg_ActivateEventFinished>(
        request_id, ServiceWorkerMsg_ActivateEvent(request_id));
  }

  void FetchOnIOThread(const base::Closure& done,
                       bool* prepare_result,
                       FetchResult* result) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    std::unique_ptr<ServiceWorkerFetchRequest> request(
        new ServiceWorkerFetchRequest(
            embedded_test_server()->GetURL("/service_worker/empty.html"), "GET",
            ServiceWorkerHeaderMap(), Referrer(), false));
    version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
    fetch_dispatcher_.reset(new ServiceWorkerFetchDispatcher(
        std::move(request), version_.get(), RESOURCE_TYPE_MAIN_FRAME,
        net::BoundNetLog(), CreatePrepareReceiver(prepare_result),
        CreateResponseReceiver(done, blob_context_.get(), result)));
    fetch_dispatcher_->Run();
  }

  // Contrary to the style guide, the output parameter of this function comes
  // before input parameters so Bind can be used on it to create a FetchCallback
  // to pass to DispatchFetchEvent.
  void ReceiveFetchResultOnIOThread(
      const base::Closure& quit,
      ChromeBlobStorageContext* blob_context,
      FetchResult* out_result,
      ServiceWorkerStatusCode actual_status,
      ServiceWorkerFetchEventResult actual_result,
      const ServiceWorkerResponse& actual_response,
      const scoped_refptr<ServiceWorkerVersion>& worker) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    ASSERT_TRUE(fetch_dispatcher_);
    fetch_dispatcher_.reset();
    out_result->status = actual_status;
    out_result->result = actual_result;
    out_result->response = actual_response;
    if (!actual_response.blob_uuid.empty()) {
      out_result->blob_data_handle =
          blob_context->context()->GetBlobDataFromUUID(
              actual_response.blob_uuid);
    }
    if (!quit.is_null())
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, quit);
  }

  ServiceWorkerFetchDispatcher::FetchCallback CreateResponseReceiver(
      const base::Closure& quit,
      ChromeBlobStorageContext* blob_context,
      FetchResult* result) {
    return base::Bind(&self::ReceiveFetchResultOnIOThread, this, quit,
                      base::RetainedRef(blob_context), result);
  }

  void StopOnIOThread(const base::Closure& done,
                      ServiceWorkerStatusCode* result) {
    ASSERT_TRUE(version_.get());
    version_->StopWorker(CreateReceiver(BrowserThread::UI, done, result));
  }

 protected:
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  scoped_refptr<ChromeBlobStorageContext> blob_context_;
  std::unique_ptr<ServiceWorkerFetchDispatcher> fetch_dispatcher_;
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, StartAndStop) {
  RunOnIOThread(base::Bind(&self::SetUpRegistrationOnIOThread, this,
                           "/service_worker/worker.js"));

  // Start a worker.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  base::RunLoop start_run_loop;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&self::StartOnIOThread, this,
                                     start_run_loop.QuitClosure(),
                                     &status));
  start_run_loop.Run();
  ASSERT_EQ(SERVICE_WORKER_OK, status);

  // Stop the worker.
  status = SERVICE_WORKER_ERROR_FAILED;
  base::RunLoop stop_run_loop;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&self::StopOnIOThread, this,
                                     stop_run_loop.QuitClosure(),
                                     &status));
  stop_run_loop.Run();
  ASSERT_EQ(SERVICE_WORKER_OK, status);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, StartNotFound) {
  RunOnIOThread(base::Bind(&self::SetUpRegistrationOnIOThread, this,
                           "/service_worker/nonexistent.js"));

  // Start a worker for nonexistent URL.
  StartWorker(SERVICE_WORKER_ERROR_NETWORK);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, ReadResourceFailure) {
  // Create a registration.
  RunOnIOThread(base::Bind(&self::SetUpRegistrationOnIOThread, this,
                           "/service_worker/worker.js"));
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);

  // Add a non-existent resource to the version.
  std::vector<ServiceWorkerDatabase::ResourceRecord> records;
  records.push_back(
      ServiceWorkerDatabase::ResourceRecord(30, version_->script_url(), 100));
  version_->script_cache_map()->SetResources(records);

  // Store the registration.
  StoreRegistration(version_->version_id(), SERVICE_WORKER_OK);

  // Start the worker. We'll fail to read the resource.
  StartWorker(SERVICE_WORKER_ERROR_DISK_CACHE);
  EXPECT_EQ(ServiceWorkerVersion::REDUNDANT, version_->status());

  // The registration should be deleted from storage since the broken worker was
  // the stored one.
  RunOnIOThread(base::Bind(&self::RemoveLiveRegistrationOnIOThread, this,
                           registration_->id()));
  FindRegistrationForId(registration_->id(),
                        registration_->pattern().GetOrigin(),
                        SERVICE_WORKER_ERROR_NOT_FOUND);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       ReadResourceFailure_WaitingWorker) {
  // Create a registration and active version.
  RunOnIOThread(base::Bind(&self::SetUpRegistrationOnIOThread, this,
                           "/service_worker/worker.js"));
  base::RunLoop activate_run_loop;
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&self::ActivateOnIOThread, this,
                                     activate_run_loop.QuitClosure(), &status));
  activate_run_loop.Run();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  ASSERT_TRUE(registration_->active_version());

  // Give the version a controllee.
  RunOnIOThread(base::Bind(&self::AddControlleeOnIOThread, this));

  // Add a non-existent resource to the version.
  std::vector<ServiceWorkerDatabase::ResourceRecord> records;
  records.push_back(
      ServiceWorkerDatabase::ResourceRecord(30, version_->script_url(), 100));
  version_->script_cache_map()->SetResources(records);

  // Make a waiting version and store it.
  RunOnIOThread(base::Bind(&self::AddWaitingWorkerOnIOThread, this,
                           "/service_worker/worker.js"));
  registration_->waiting_version()->script_cache_map()->SetResources(records);
  StoreRegistration(registration_->waiting_version()->version_id(),
                    SERVICE_WORKER_OK);

  // Start the broken worker. We'll fail to read from disk and the worker should
  // be doomed.
  StopWorker(SERVICE_WORKER_OK);  // in case it's already running
  StartWorker(SERVICE_WORKER_ERROR_DISK_CACHE);
  EXPECT_EQ(ServiceWorkerVersion::REDUNDANT, version_->status());

  // The registration should still be in storage since the waiting worker was
  // the stored one.
  RunOnIOThread(base::Bind(&self::RemoveLiveRegistrationOnIOThread, this,
                           registration_->id()));
  FindRegistrationForId(registration_->id(),
                        registration_->pattern().GetOrigin(),
                        SERVICE_WORKER_OK);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, Install) {
  InstallTestHelper("/service_worker/worker.js", SERVICE_WORKER_OK);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       InstallWithWaitUntil_Fulfilled) {
  InstallTestHelper("/service_worker/worker_install_fulfilled.js",
                    SERVICE_WORKER_OK);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       InstallWithFetchHandler) {
  InstallTestHelper("/service_worker/fetch_event.js", SERVICE_WORKER_OK);
  EXPECT_TRUE(version_->has_fetch_handler());
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       InstallWithoutFetchHandler) {
  InstallTestHelper("/service_worker/worker.js", SERVICE_WORKER_OK);
  EXPECT_FALSE(version_->has_fetch_handler());
}

// Check that ServiceWorker script requests set a "Service-Worker: script"
// header.
IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       ServiceWorkerScriptHeader) {
  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&VerifyServiceWorkerHeaderInRequest));
  InstallTestHelper("/service_worker/generated_sw.js", SERVICE_WORKER_OK);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       Activate_NoEventListener) {
  ActivateTestHelper("/service_worker/worker.js", SERVICE_WORKER_OK);
  ASSERT_EQ(ServiceWorkerVersion::ACTIVATING, version_->status());
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, Activate_Rejected) {
  ActivateTestHelper("/service_worker/worker_activate_rejected.js",
                     SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       InstallWithWaitUntil_Rejected) {
  InstallTestHelper("/service_worker/worker_install_rejected.js",
                    SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       InstallWithWaitUntil_RejectConsoleMessage) {
  RunOnIOThread(base::Bind(&self::SetUpRegistrationOnIOThread, this,
                           "/service_worker/worker_install_rejected.js"));

  ConsoleListener console_listener;
  version_->embedded_worker()->AddListener(&console_listener);

  // Dispatch install on a worker.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  base::RunLoop install_run_loop;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&self::InstallOnIOThread, this,
                                     install_run_loop.QuitClosure(), &status));
  install_run_loop.Run();
  ASSERT_EQ(SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED, status);

  const base::string16 expected =
      base::ASCIIToUTF16("Rejecting oninstall event");
  console_listener.WaitForConsoleMessages(1);
  ASSERT_NE(base::string16::npos,
            console_listener.messages()[0].find(expected));
  version_->embedded_worker()->RemoveListener(&console_listener);
}

class WaitForLoaded : public EmbeddedWorkerInstance::Listener {
 public:
  explicit WaitForLoaded(const base::Closure& quit) : quit_(quit) {}

  void OnThreadStarted() override {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, quit_);
  }
  bool OnMessageReceived(const IPC::Message& message) override { return false; }

 private:
  base::Closure quit_;
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, TimeoutStartingWorker) {
  RunOnIOThread(base::Bind(&self::SetUpRegistrationOnIOThread, this,
                           "/service_worker/while_true_worker.js"));

  // Start a worker, waiting until the script is loaded.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  base::RunLoop start_run_loop;
  base::RunLoop load_run_loop;
  WaitForLoaded wait_for_load(load_run_loop.QuitClosure());
  version_->embedded_worker()->AddListener(&wait_for_load);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&self::StartOnIOThread, this,
                                     start_run_loop.QuitClosure(), &status));
  load_run_loop.Run();
  version_->embedded_worker()->RemoveListener(&wait_for_load);

  // The script has loaded but start has not completed yet.
  ASSERT_EQ(SERVICE_WORKER_ERROR_FAILED, status);
  EXPECT_EQ(EmbeddedWorkerStatus::STARTING, version_->running_status());

  // Simulate execution timeout. Use a delay to prevent killing the worker
  // before it's started execution.
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  RunOnIOThreadWithDelay(base::Bind(&self::TimeoutWorkerOnIOThread, this),
                         base::TimeDelta::FromMilliseconds(100));
  start_run_loop.Run();

  EXPECT_EQ(SERVICE_WORKER_ERROR_TIMEOUT, status);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, TimeoutWorkerInEvent) {
  RunOnIOThread(base::Bind(&self::SetUpRegistrationOnIOThread, this,
                           "/service_worker/while_true_in_install_worker.js"));

  // Start a worker.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  base::RunLoop start_run_loop;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&self::StartOnIOThread, this,
                                     start_run_loop.QuitClosure(), &status));
  start_run_loop.Run();
  ASSERT_EQ(SERVICE_WORKER_OK, status);

  // Dispatch an event.
  base::RunLoop install_run_loop;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&self::InstallOnIOThread, this,
                                     install_run_loop.QuitClosure(), &status));

  // Simulate execution timeout. Use a delay to prevent killing the worker
  // before it's started execution.
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  RunOnIOThreadWithDelay(base::Bind(&self::TimeoutWorkerOnIOThread, this),
                         base::TimeDelta::FromMilliseconds(100));
  install_run_loop.Run();

  // Terminating a worker, even one in an infinite loop, is treated as if
  // waitUntil was rejected in the renderer code.
  EXPECT_EQ(SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED, status);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, FetchEvent_Response) {
  ServiceWorkerFetchEventResult result;
  ServiceWorkerResponse response;
  std::unique_ptr<storage::BlobDataHandle> blob_data_handle;
  FetchTestHelper("/service_worker/fetch_event.js",
                  &result, &response, &blob_data_handle);
  ASSERT_EQ(SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE, result);
  EXPECT_EQ(301, response.status_code);
  EXPECT_EQ("Moved Permanently", response.status_text);
  ServiceWorkerHeaderMap expected_headers;
  expected_headers["content-language"] = "fi";
  expected_headers["content-type"] = "text/html; charset=UTF-8";
  EXPECT_EQ(expected_headers, response.headers);

  std::string body;
  RunOnIOThread(
      base::Bind(&ReadResponseBody,
                 &body, base::Owned(blob_data_handle.release())));
  EXPECT_EQ("This resource is gone. Gone, gone, gone.", body);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       FetchEvent_ResponseViaCache) {
  ServiceWorkerFetchEventResult result;
  ServiceWorkerResponse response1;
  ServiceWorkerResponse response2;
  std::unique_ptr<storage::BlobDataHandle> blob_data_handle;
  const base::Time start_time(base::Time::Now());

  RunOnIOThread(
      base::Bind(&self::SetUpRegistrationOnIOThread, this,
                 "/service_worker/fetch_event_response_via_cache.js"));

  FetchOnRegisteredWorker(&result, &response1, &blob_data_handle);
  ASSERT_EQ(SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE, result);
  EXPECT_EQ(200, response1.status_code);
  EXPECT_EQ("OK", response1.status_text);
  EXPECT_TRUE(response1.response_time >= start_time);
  EXPECT_FALSE(response1.is_in_cache_storage);
  EXPECT_EQ(std::string(), response2.cache_storage_cache_name);

  FetchOnRegisteredWorker(&result, &response2, &blob_data_handle);
  ASSERT_EQ(SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE, result);
  EXPECT_EQ(200, response2.status_code);
  EXPECT_EQ("OK", response2.status_text);
  EXPECT_EQ(response1.response_time, response2.response_time);
  EXPECT_TRUE(response2.is_in_cache_storage);
  EXPECT_EQ("cache_name", response2.cache_storage_cache_name);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       FetchEvent_respondWithRejection) {
  ServiceWorkerFetchEventResult result;
  ServiceWorkerResponse response;
  std::unique_ptr<storage::BlobDataHandle> blob_data_handle;

  RunOnIOThread(base::Bind(&self::SetUpRegistrationOnIOThread, this,
                           "/service_worker/fetch_event_rejected.js"));

  ConsoleListener console_listener;
  version_->embedded_worker()->AddListener(&console_listener);

  FetchOnRegisteredWorker(&result, &response, &blob_data_handle);
  const base::string16 expected1 = base::ASCIIToUTF16(
      "resulted in a network error response: the promise was rejected.");
  const base::string16 expected2 =
      base::ASCIIToUTF16("Uncaught (in promise) Rejecting respondWith promise");
  console_listener.WaitForConsoleMessages(2);
  ASSERT_NE(base::string16::npos,
            console_listener.messages()[0].find(expected1));
  ASSERT_EQ(0u, console_listener.messages()[1].find(expected2));
  version_->embedded_worker()->RemoveListener(&console_listener);

  ASSERT_EQ(SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE, result);
  EXPECT_EQ(0, response.status_code);

  ASSERT_FALSE(blob_data_handle);
}

class MockContentBrowserClient : public TestContentBrowserClient {
 public:
  MockContentBrowserClient()
      : TestContentBrowserClient(), data_saver_enabled_(false) {}

  ~MockContentBrowserClient() override {}

  void set_data_saver_enabled(bool enabled) { data_saver_enabled_ = enabled; }

  // ContentBrowserClient overrides:
  bool IsDataSaverEnabled(BrowserContext* context) override {
    return data_saver_enabled_;
  }

  void OverrideWebkitPrefs(RenderViewHost* render_view_host,
                           WebPreferences* prefs) override {
    prefs->data_saver_enabled = data_saver_enabled_;
  }

 private:
  bool data_saver_enabled_;
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, FetchWithSaveData) {
  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&VerifySaveDataHeaderInRequest));
  MockContentBrowserClient content_browser_client;
  content_browser_client.set_data_saver_enabled(true);
  ContentBrowserClient* old_client =
      SetBrowserClientForTesting(&content_browser_client);
  InstallTestHelper("/service_worker/fetch_in_install.js", SERVICE_WORKER_OK);
  SetBrowserClientForTesting(old_client);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       RequestWorkerScriptWithSaveData) {
  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&VerifySaveDataHeaderInRequest));
  MockContentBrowserClient content_browser_client;
  content_browser_client.set_data_saver_enabled(true);
  ContentBrowserClient* old_client =
      SetBrowserClientForTesting(&content_browser_client);
  InstallTestHelper("/service_worker/generated_sw.js", SERVICE_WORKER_OK);
  SetBrowserClientForTesting(old_client);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, FetchWithoutSaveData) {
  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&VerifySaveDataHeaderNotInRequest));
  MockContentBrowserClient content_browser_client;
  ContentBrowserClient* old_client =
      SetBrowserClientForTesting(&content_browser_client);
  InstallTestHelper("/service_worker/fetch_in_install.js", SERVICE_WORKER_OK);
  SetBrowserClientForTesting(old_client);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest, FetchPageWithSaveData) {
  const char kPageUrl[] = "/service_worker/handle_fetch.html";
  const char kWorkerUrl[] = "/service_worker/add_save_data_to_title.js";
  MockContentBrowserClient content_browser_client;
  content_browser_client.set_data_saver_enabled(true);
  ContentBrowserClient* old_client =
      SetBrowserClientForTesting(&content_browser_client);
  shell()->web_contents()->GetRenderViewHost()->OnWebkitPreferencesChanged();
  scoped_refptr<WorkerActivatedObserver> observer =
      new WorkerActivatedObserver(wrapper());
  observer->Init();
  public_context()->RegisterServiceWorker(
      embedded_test_server()->GetURL(kPageUrl),
      embedded_test_server()->GetURL(kWorkerUrl),
      base::Bind(&ExpectResultAndRun, true, base::Bind(&base::DoNothing)));
  observer->Wait();

  const base::string16 title1 = base::ASCIIToUTF16("save-data=on");
  TitleWatcher title_watcher1(shell()->web_contents(), title1);
  NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl));
  EXPECT_EQ(title1, title_watcher1.WaitAndGetTitle());

  SetBrowserClientForTesting(old_client);
  shell()->Close();

  base::RunLoop run_loop;
  public_context()->UnregisterServiceWorker(
      embedded_test_server()->GetURL(kPageUrl),
      base::Bind(&ExpectResultAndRun, true, run_loop.QuitClosure()));
  run_loop.Run();
}

// Tests that when data saver is enabled and a cross-origin fetch by a webpage
// is intercepted by a serviceworker, and the serviceworker does a fetch, the
// preflight request does not have save-data in Access-Control-Request-Headers.
IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest, CrossOriginFetchWithSaveData) {
  const char kPageUrl[] = "/service_worker/fetch_cross_origin.html";
  const char kWorkerUrl[] = "/service_worker/fetch_event_pass_through.js";
  net::EmbeddedTestServer cross_origin_server;
  cross_origin_server.ServeFilesFromSourceDirectory("content/test/data");
  cross_origin_server.RegisterRequestHandler(
      base::Bind(&VerifySaveDataNotInAccessControlRequestHeader));
  ASSERT_TRUE(cross_origin_server.Start());

  MockContentBrowserClient content_browser_client;
  content_browser_client.set_data_saver_enabled(true);
  ContentBrowserClient* old_client =
      SetBrowserClientForTesting(&content_browser_client);
  shell()->web_contents()->GetRenderViewHost()->OnWebkitPreferencesChanged();
  scoped_refptr<WorkerActivatedObserver> observer =
      new WorkerActivatedObserver(wrapper());
  observer->Init();
  public_context()->RegisterServiceWorker(
      embedded_test_server()->GetURL(kPageUrl),
      embedded_test_server()->GetURL(kWorkerUrl),
      base::Bind(&ExpectResultAndRun, true, base::Bind(&base::DoNothing)));
  observer->Wait();

  const base::string16 title = base::ASCIIToUTF16("PASS");
  TitleWatcher title_watcher(shell()->web_contents(), title);
  NavigateToURL(shell(),
                embedded_test_server()->GetURL(base::StringPrintf(
                    "%s?%s", kPageUrl,
                    cross_origin_server.GetURL("/cross_origin_request.html")
                        .spec()
                        .c_str())));
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());

  SetBrowserClientForTesting(old_client);
  shell()->Close();

  base::RunLoop run_loop;
  public_context()->UnregisterServiceWorker(
      embedded_test_server()->GetURL(kPageUrl),
      base::Bind(&ExpectResultAndRun, true, run_loop.QuitClosure()));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest,
                       FetchPageWithSaveDataPassThroughOnFetch) {
  const char kPageUrl[] = "/service_worker/pass_through_fetch.html";
  const char kWorkerUrl[] = "/service_worker/fetch_event_pass_through.js";
  MockContentBrowserClient content_browser_client;
  content_browser_client.set_data_saver_enabled(true);
  ContentBrowserClient* old_client =
      SetBrowserClientForTesting(&content_browser_client);
  shell()->web_contents()->GetRenderViewHost()->OnWebkitPreferencesChanged();
  scoped_refptr<WorkerActivatedObserver> observer =
      new WorkerActivatedObserver(wrapper());
  observer->Init();
  public_context()->RegisterServiceWorker(
      embedded_test_server()->GetURL(kPageUrl),
      embedded_test_server()->GetURL(kWorkerUrl),
      base::Bind(&ExpectResultAndRun, true, base::Bind(&base::DoNothing)));
  observer->Wait();

  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&VerifySaveDataHeaderInRequest));

  NavigateToURLBlockUntilNavigationsComplete(
      shell(), embedded_test_server()->GetURL(kPageUrl), 1);

  SetBrowserClientForTesting(old_client);
  shell()->Close();

  base::RunLoop run_loop;
  public_context()->UnregisterServiceWorker(
      embedded_test_server()->GetURL(kPageUrl),
      base::Bind(&ExpectResultAndRun, true, run_loop.QuitClosure()));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest, Reload) {
  const char kPageUrl[] = "/service_worker/reload.html";
  const char kWorkerUrl[] = "/service_worker/fetch_event_reload.js";
  scoped_refptr<WorkerActivatedObserver> observer =
      new WorkerActivatedObserver(wrapper());
  observer->Init();
  public_context()->RegisterServiceWorker(
      embedded_test_server()->GetURL(kPageUrl),
      embedded_test_server()->GetURL(kWorkerUrl),
      base::Bind(&ExpectResultAndRun, true, base::Bind(&base::DoNothing)));
  observer->Wait();

  const base::string16 title1 = base::ASCIIToUTF16("reload=false");
  TitleWatcher title_watcher1(shell()->web_contents(), title1);
  NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl));
  EXPECT_EQ(title1, title_watcher1.WaitAndGetTitle());

  const base::string16 title2 = base::ASCIIToUTF16("reload=true");
  TitleWatcher title_watcher2(shell()->web_contents(), title2);
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  EXPECT_EQ(title2, title_watcher2.WaitAndGetTitle());

  shell()->Close();

  base::RunLoop run_loop;
  public_context()->UnregisterServiceWorker(
      embedded_test_server()->GetURL(kPageUrl),
      base::Bind(&ExpectResultAndRun, true, run_loop.QuitClosure()));
  run_loop.Run();
}

// Flaky on Win/Mac: http://crbug.com/533631
#if defined(OS_WIN) || defined(OS_MACOSX)
#define MAYBE_ResponseFromHTTPSServiceWorkerIsMarkedAsSecure DISABLED_ResponseFromHTTPSServiceWorkerIsMarkedAsSecure
#else
#define MAYBE_ResponseFromHTTPSServiceWorkerIsMarkedAsSecure ResponseFromHTTPSServiceWorkerIsMarkedAsSecure
#endif
IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest,
                       MAYBE_ResponseFromHTTPSServiceWorkerIsMarkedAsSecure) {
  const char kPageUrl[] = "/service_worker/fetch_event_blob.html";
  const char kWorkerUrl[] = "/service_worker/fetch_event_blob.js";
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(https_server.Start());

  scoped_refptr<WorkerActivatedObserver> observer =
      new WorkerActivatedObserver(wrapper());
  observer->Init();
  public_context()->RegisterServiceWorker(
      https_server.GetURL(kPageUrl),
      https_server.GetURL(kWorkerUrl),
      base::Bind(&ExpectResultAndRun, true, base::Bind(&base::DoNothing)));
  observer->Wait();

  const base::string16 title = base::ASCIIToUTF16("Title");
  TitleWatcher title_watcher(shell()->web_contents(), title);
  NavigateToURL(shell(), https_server.GetURL(kPageUrl));
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  EXPECT_FALSE(shell()->web_contents()->DisplayedInsecureContent());
  NavigationEntry* entry =
      shell()->web_contents()->GetController().GetVisibleEntry();
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATED, entry->GetSSL().security_style);

  shell()->Close();

  base::RunLoop run_loop;
  public_context()->UnregisterServiceWorker(
      https_server.GetURL(kPageUrl),
      base::Bind(&ExpectResultAndRun, true, run_loop.QuitClosure()));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest,
                       ResponseFromHTTPServiceWorkerIsNotMarkedAsSecure) {
  const char kPageUrl[] = "/service_worker/fetch_event_blob.html";
  const char kWorkerUrl[] = "/service_worker/fetch_event_blob.js";
  scoped_refptr<WorkerActivatedObserver> observer =
      new WorkerActivatedObserver(wrapper());
  observer->Init();
  public_context()->RegisterServiceWorker(
      embedded_test_server()->GetURL(kPageUrl),
      embedded_test_server()->GetURL(kWorkerUrl),
      base::Bind(&ExpectResultAndRun, true, base::Bind(&base::DoNothing)));
  observer->Wait();

  const base::string16 title = base::ASCIIToUTF16("Title");
  TitleWatcher title_watcher(shell()->web_contents(), title);
  NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl));
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  EXPECT_FALSE(shell()->web_contents()->DisplayedInsecureContent());
  NavigationEntry* entry =
      shell()->web_contents()->GetController().GetVisibleEntry();
  EXPECT_EQ(SECURITY_STYLE_UNAUTHENTICATED, entry->GetSSL().security_style);

  shell()->Close();

  base::RunLoop run_loop;
  public_context()->UnregisterServiceWorker(
      embedded_test_server()->GetURL(kPageUrl),
      base::Bind(&ExpectResultAndRun, true, run_loop.QuitClosure()));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest, ImportsBustMemcache) {
  const char kScopeUrl[] = "/service_worker/imports_bust_memcache_scope/";
  const char kPageUrl[] = "/service_worker/imports_bust_memcache.html";
  const char kScriptUrl[] = "/service_worker/worker_with_one_import.js";
  const char kImportUrl[] = "/service_worker/long_lived_import.js";
  const base::string16 kOKTitle(base::ASCIIToUTF16("OK"));
  const base::string16 kFailTitle(base::ASCIIToUTF16("FAIL"));

  RunOnIOThread(
      base::Bind(&CreateLongLivedResourceInterceptors,
                 embedded_test_server()->GetURL(kScriptUrl),
                 embedded_test_server()->GetURL(kImportUrl)));

  TitleWatcher title_watcher(shell()->web_contents(), kOKTitle);
  title_watcher.AlsoWaitForTitle(kFailTitle);
  NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl));
  base::string16 title = title_watcher.WaitAndGetTitle();
  EXPECT_EQ(kOKTitle, title);

  // Verify the number of resources in the implicit script cache is correct.
  const int kExpectedNumResources = 2;
  int num_resources = 0;
  RunOnIOThread(
      base::Bind(&CountScriptResources,
                 base::Unretained(wrapper()),
                 embedded_test_server()->GetURL(kScopeUrl),
                 &num_resources));
  EXPECT_EQ(kExpectedNumResources, num_resources);
}

class ServiceWorkerBlackBoxBrowserTest : public ServiceWorkerBrowserTest {
 public:
  using self = ServiceWorkerBlackBoxBrowserTest;

  void FindRegistrationOnIO(const GURL& document_url,
                            ServiceWorkerStatusCode* status,
                            const base::Closure& continuation) {
    wrapper()->FindReadyRegistrationForDocument(
        document_url,
        base::Bind(&ServiceWorkerBlackBoxBrowserTest::FindRegistrationOnIO2,
                   this, status, continuation));
  }

  void FindRegistrationOnIO2(
      ServiceWorkerStatusCode* out_status,
      const base::Closure& continuation,
      ServiceWorkerStatusCode status,
      const scoped_refptr<ServiceWorkerRegistration>& registration) {
    *out_status = status;
    if (!registration.get())
      EXPECT_NE(SERVICE_WORKER_OK, status);
    continuation.Run();
  }
};

static int CountRenderProcessHosts() {
  int result = 0;
  for (RenderProcessHost::iterator iter(RenderProcessHost::AllHostsIterator());
       !iter.IsAtEnd();
       iter.Advance()) {
    result++;
  }
  return result;
}

// Flaky timeouts on CrOS: http://crbug.com/387045
#if defined(OS_CHROMEOS)
#define MAYBE_Registration DISABLED_Registration
#else
#define MAYBE_Registration Registration
#endif
IN_PROC_BROWSER_TEST_F(ServiceWorkerBlackBoxBrowserTest, MAYBE_Registration) {
  // Close the only window to be sure we're not re-using its RenderProcessHost.
  shell()->Close();
  EXPECT_EQ(0, CountRenderProcessHosts());

  const char kWorkerUrl[] = "/service_worker/fetch_event.js";
  const char kScope[] = "/service_worker/";

  // Unregistering nothing should return false.
  {
    base::RunLoop run_loop;
    public_context()->UnregisterServiceWorker(
        embedded_test_server()->GetURL("/"),
        base::Bind(&ExpectResultAndRun, false, run_loop.QuitClosure()));
    run_loop.Run();
  }

  // If we use a worker URL that doesn't exist, registration fails.
  {
    base::RunLoop run_loop;
    public_context()->RegisterServiceWorker(
        embedded_test_server()->GetURL(kScope),
        embedded_test_server()->GetURL("/does/not/exist"),
        base::Bind(&ExpectResultAndRun, false, run_loop.QuitClosure()));
    run_loop.Run();
  }
  EXPECT_EQ(0, CountRenderProcessHosts());

  // Register returns when the promise would be resolved.
  {
    base::RunLoop run_loop;
    public_context()->RegisterServiceWorker(
        embedded_test_server()->GetURL(kScope),
        embedded_test_server()->GetURL(kWorkerUrl),
        base::Bind(&ExpectResultAndRun, true, run_loop.QuitClosure()));
    run_loop.Run();
  }
  EXPECT_EQ(1, CountRenderProcessHosts());

  // Registering again should succeed, although the algo still
  // might not be complete.
  {
    base::RunLoop run_loop;
    public_context()->RegisterServiceWorker(
        embedded_test_server()->GetURL(kScope),
        embedded_test_server()->GetURL(kWorkerUrl),
        base::Bind(&ExpectResultAndRun, true, run_loop.QuitClosure()));
    run_loop.Run();
  }

  // The registration algo might not be far enough along to have
  // stored the registration data, so it may not be findable
  // at this point.

  // Unregistering something should return true.
  {
    base::RunLoop run_loop;
    public_context()->UnregisterServiceWorker(
        embedded_test_server()->GetURL(kScope),
        base::Bind(&ExpectResultAndRun, true, run_loop.QuitClosure()));
    run_loop.Run();
  }
  EXPECT_GE(1, CountRenderProcessHosts()) << "Unregistering doesn't stop the "
                                             "workers eagerly, so their RPHs "
                                             "can still be running.";

  // Should not be able to find it.
  {
    ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
    RunOnIOThread(
        base::Bind(&ServiceWorkerBlackBoxBrowserTest::FindRegistrationOnIO,
                   this,
                   embedded_test_server()->GetURL("/service_worker/empty.html"),
                   &status));
    EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND, status);
  }
}

#if defined(ANDROID)
#define MAYBE_CrossSiteTransfer DISABLED_CrossSiteTransfer
#else
#define MAYBE_CrossSiteTransfer CrossSiteTransfer
#endif
IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest, MAYBE_CrossSiteTransfer) {
  // The first page registers a service worker.
  const char kRegisterPageUrl[] = "/service_worker/cross_site_xfer.html";
  const base::string16 kOKTitle1(base::ASCIIToUTF16("OK_1"));
  const base::string16 kFailTitle1(base::ASCIIToUTF16("FAIL_1"));
  content::TitleWatcher title_watcher1(shell()->web_contents(), kOKTitle1);
  title_watcher1.AlsoWaitForTitle(kFailTitle1);

  NavigateToURL(shell(), embedded_test_server()->GetURL(kRegisterPageUrl));
  ASSERT_EQ(kOKTitle1, title_watcher1.WaitAndGetTitle());

  // Force process swapping behavior.
  ShellContentBrowserClient::SetSwapProcessesForRedirect(true);

  // The second pages loads via the serviceworker including a subresource.
  const char kConfirmPageUrl[] =
      "/service_worker/cross_site_xfer_scope/"
      "cross_site_xfer_confirm_via_serviceworker.html";
  const base::string16 kOKTitle2(base::ASCIIToUTF16("OK_2"));
  const base::string16 kFailTitle2(base::ASCIIToUTF16("FAIL_2"));
  content::TitleWatcher title_watcher2(shell()->web_contents(), kOKTitle2);
  title_watcher2.AlsoWaitForTitle(kFailTitle2);

  NavigateToURL(shell(), embedded_test_server()->GetURL(kConfirmPageUrl));
  EXPECT_EQ(kOKTitle2, title_watcher2.WaitAndGetTitle());
}

class ServiceWorkerVersionBrowserV8CacheTest
    : public ServiceWorkerVersionBrowserTest,
      public ServiceWorkerVersion::Listener {
 public:
  using self = ServiceWorkerVersionBrowserV8CacheTest;
  ~ServiceWorkerVersionBrowserV8CacheTest() override {
    if (version_)
      version_->RemoveListener(this);
  }
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kV8CacheOptions, "code");
  }
  void SetUpRegistrationAndListenerOnIOThread(const std::string& worker_url) {
    SetUpRegistrationOnIOThread(worker_url);
    version_->AddListener(this);
  }

 protected:
  // ServiceWorkerVersion::Listener overrides
  void OnCachedMetadataUpdated(ServiceWorkerVersion* version) override {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            cache_updated_closure_);
  }

  base::Closure cache_updated_closure_;
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserV8CacheTest, Restart) {
  RunOnIOThread(base::Bind(&self::SetUpRegistrationAndListenerOnIOThread, this,
                           "/service_worker/worker.js"));

  base::RunLoop cached_metadata_run_loop;
  cache_updated_closure_ = cached_metadata_run_loop.QuitClosure();

  // Start a worker.
  StartWorker(SERVICE_WORKER_OK);

  // Wait for the matadata is stored. This run loop should finish when
  // OnCachedMetadataUpdated() is called.
  cached_metadata_run_loop.Run();

  // Activate the worker.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  base::RunLoop activate_run_loop;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&self::ActivateOnIOThread, this,
                                     activate_run_loop.QuitClosure(), &status));
  activate_run_loop.Run();
  ASSERT_EQ(SERVICE_WORKER_OK, status);
  // Stop the worker.
  StopWorker(SERVICE_WORKER_OK);
  // Restart the worker.
  StartWorker(SERVICE_WORKER_OK);
  // Stop the worker.
  StopWorker(SERVICE_WORKER_OK);
}

class CacheStorageSideDataSizeChecker
    : public base::RefCountedThreadSafe<CacheStorageSideDataSizeChecker> {
 public:
  static int GetSize(CacheStorageContextImpl* cache_storage_context,
                     storage::FileSystemContext* file_system_context,
                     const GURL& origin,
                     const std::string& cache_name,
                     const GURL& url) {
    scoped_refptr<CacheStorageSideDataSizeChecker> checker(
        new CacheStorageSideDataSizeChecker(cache_storage_context,
                                            file_system_context, origin,
                                            cache_name, url));
    return checker->GetSizeImpl();
  }

 private:
  using self = CacheStorageSideDataSizeChecker;
  friend class base::RefCountedThreadSafe<self>;

  CacheStorageSideDataSizeChecker(
      CacheStorageContextImpl* cache_storage_context,
      storage::FileSystemContext* file_system_context,
      const GURL& origin,
      const std::string& cache_name,
      const GURL& url)
      : cache_storage_context_(cache_storage_context),
        file_system_context_(file_system_context),
        origin_(origin),
        cache_name_(cache_name),
        url_(url) {}

  ~CacheStorageSideDataSizeChecker() {}

  int GetSizeImpl() {
    int result = 0;
    RunOnIOThread(base::Bind(&self::OpenCacheOnIOThread, this, &result));
    return result;
  }

  void OpenCacheOnIOThread(int* result, const base::Closure& continuation) {
    cache_storage_context_->cache_manager()->OpenCache(
        origin_, cache_name_, base::Bind(&self::OnCacheStorageOpenCallback,
                                         this, result, continuation));
  }

  void OnCacheStorageOpenCallback(
      int* result,
      const base::Closure& continuation,
      std::unique_ptr<CacheStorageCacheHandle> cache_handle,
      CacheStorageError error) {
    ASSERT_EQ(CACHE_STORAGE_OK, error);
    std::unique_ptr<ServiceWorkerFetchRequest> scoped_request(
        new ServiceWorkerFetchRequest());
    scoped_request->url = url_;
    CacheStorageCache* cache = cache_handle->value();
    cache->Match(
        std::move(scoped_request),
        base::Bind(&self::OnCacheStorageCacheMatchCallback, this, result,
                   continuation, base::Passed(std::move(cache_handle))));
  }

  void OnCacheStorageCacheMatchCallback(
      int* result,
      const base::Closure& continuation,
      std::unique_ptr<CacheStorageCacheHandle> cache_handle,
      CacheStorageError error,
      std::unique_ptr<ServiceWorkerResponse> response,
      std::unique_ptr<storage::BlobDataHandle> blob_data_handle) {
    ASSERT_EQ(CACHE_STORAGE_OK, error);
    blob_data_handle_ = std::move(blob_data_handle);
    blob_reader_ = blob_data_handle_->CreateReader(
        file_system_context_,
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE).get());
    const storage::BlobReader::Status status = blob_reader_->CalculateSize(
        base::Bind(&self::OnBlobReaderCalculateSizeCallback, this, result,
                   continuation));

    ASSERT_NE(storage::BlobReader::Status::NET_ERROR, status);
    if (status == storage::BlobReader::Status::DONE)
      OnBlobReaderCalculateSizeCallback(result, continuation, net::OK);
  }

  void OnBlobReaderCalculateSizeCallback(int* result,
                                         const base::Closure& continuation,
                                         int size_result) {
    ASSERT_EQ(net::OK, size_result);
    if (!blob_reader_->has_side_data()) {
      continuation.Run();
      return;
    }
    const storage::BlobReader::Status status = blob_reader_->ReadSideData(
        base::Bind(&self::OnBlobReaderReadSideDataCallback, this, result,
                   continuation));
    ASSERT_NE(storage::BlobReader::Status::NET_ERROR, status);
    if (status == storage::BlobReader::Status::DONE) {
      OnBlobReaderReadSideDataCallback(result, continuation,
                                       storage::BlobReader::Status::DONE);
    }
  }

  void OnBlobReaderReadSideDataCallback(int* result,
                                        const base::Closure& continuation,
                                        storage::BlobReader::Status status) {
    ASSERT_NE(storage::BlobReader::Status::NET_ERROR, status);
    *result = blob_reader_->side_data()->size();
    continuation.Run();
  }

  CacheStorageContextImpl* cache_storage_context_;
  storage::FileSystemContext* file_system_context_;
  const GURL origin_;
  const std::string cache_name_;
  const GURL url_;
  std::unique_ptr<storage::BlobDataHandle> blob_data_handle_;
  std::unique_ptr<storage::BlobReader> blob_reader_;
  DISALLOW_COPY_AND_ASSIGN(CacheStorageSideDataSizeChecker);
};

class ServiceWorkerV8CacheStrategiesTest : public ServiceWorkerBrowserTest {
 public:
  ServiceWorkerV8CacheStrategiesTest() {}
  ~ServiceWorkerV8CacheStrategiesTest() override {}

 protected:
  void CheckStrategyIsNone() {
    RegisterAndActivateServiceWorker();

    NavigateToTestPage();
    EXPECT_EQ(0, GetSideDataSize());

    NavigateToTestPage();
    EXPECT_EQ(0, GetSideDataSize());

    NavigateToTestPage();
    EXPECT_EQ(0, GetSideDataSize());
  }

  void CheckStrategyIsNormal() {
    RegisterAndActivateServiceWorker();

    NavigateToTestPage();
    // fetch_event_response_via_cache.js returns |cloned_response| for the first
    // load. So the V8 code cache should not be stored to the CacheStorage.
    EXPECT_EQ(0, GetSideDataSize());

    NavigateToTestPage();
    // V8ScriptRunner::setCacheTimeStamp() stores 12 byte data (tag +
    // timestamp).
    EXPECT_EQ(kV8CacheTimeStampDataSize, GetSideDataSize());

    NavigateToTestPage();
    // The V8 code cache must be stored to the CacheStorage which must be bigger
    // than 12 byte.
    EXPECT_GT(GetSideDataSize(), kV8CacheTimeStampDataSize);
  }

  void CheckStrategyIsAggressive() {
    RegisterAndActivateServiceWorker();

    NavigateToTestPage();
    // fetch_event_response_via_cache.js returns |cloned_response| for the first
    // load. So the V8 code cache should not be stored to the CacheStorage.
    EXPECT_EQ(0, GetSideDataSize());

    NavigateToTestPage();
    // The V8 code cache must be stored to the CacheStorage which must be bigger
    // than 12 byte.
    EXPECT_GT(GetSideDataSize(), kV8CacheTimeStampDataSize);

    NavigateToTestPage();
    EXPECT_GT(GetSideDataSize(), kV8CacheTimeStampDataSize);
  }

 private:
  static const std::string kPageUrl;
  static const std::string kWorkerUrl;
  static const std::string kScriptUrl;
  static const int kV8CacheTimeStampDataSize;

  void RegisterAndActivateServiceWorker() {
    scoped_refptr<WorkerActivatedObserver> observer =
        new WorkerActivatedObserver(wrapper());
    observer->Init();
    public_context()->RegisterServiceWorker(
        embedded_test_server()->GetURL(kPageUrl),
        embedded_test_server()->GetURL(kWorkerUrl),
        base::Bind(&ExpectResultAndRun, true, base::Bind(&base::DoNothing)));
    observer->Wait();
  }

  void NavigateToTestPage() {
    const base::string16 title =
        base::ASCIIToUTF16("Title was changed by the script.");
    TitleWatcher title_watcher(shell()->web_contents(), title);
    NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl));
    EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  }

  int GetSideDataSize() {
    StoragePartition* partition = BrowserContext::GetDefaultStoragePartition(
        shell()->web_contents()->GetBrowserContext());
    return CacheStorageSideDataSizeChecker::GetSize(
        static_cast<CacheStorageContextImpl*>(
            partition->GetCacheStorageContext()),
        partition->GetFileSystemContext(), embedded_test_server()->base_url(),
        std::string("cache_name"), embedded_test_server()->GetURL(kScriptUrl));
  }

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerV8CacheStrategiesTest);
};

const std::string ServiceWorkerV8CacheStrategiesTest::kPageUrl =
    "/service_worker/v8_cache_test.html";
const std::string ServiceWorkerV8CacheStrategiesTest::kWorkerUrl =
    "/service_worker/fetch_event_response_via_cache.js";
const std::string ServiceWorkerV8CacheStrategiesTest::kScriptUrl =
    "/service_worker/v8_cache_test.js";
// V8ScriptRunner::setCacheTimeStamp() stores 12 byte data (tag + timestamp).
const int ServiceWorkerV8CacheStrategiesTest::kV8CacheTimeStampDataSize =
    sizeof(unsigned) + sizeof(double);

IN_PROC_BROWSER_TEST_F(ServiceWorkerV8CacheStrategiesTest,
                       V8CacheOnCacheStorage) {
  // The strategy is "aggressive" on default.
  CheckStrategyIsAggressive();
}

class ServiceWorkerV8CacheStrategiesNoneTest
    : public ServiceWorkerV8CacheStrategiesTest {
 public:
  ServiceWorkerV8CacheStrategiesNoneTest() {}
  ~ServiceWorkerV8CacheStrategiesNoneTest() override {}
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kV8CacheStrategiesForCacheStorage,
                                    "none");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerV8CacheStrategiesNoneTest);
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerV8CacheStrategiesNoneTest,
                       V8CacheOnCacheStorage) {
  CheckStrategyIsNone();
}

class ServiceWorkerV8CacheStrategiesNormalTest
    : public ServiceWorkerV8CacheStrategiesTest {
 public:
  ServiceWorkerV8CacheStrategiesNormalTest() {}
  ~ServiceWorkerV8CacheStrategiesNormalTest() override {}
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kV8CacheStrategiesForCacheStorage,
                                    "normal");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerV8CacheStrategiesNormalTest);
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerV8CacheStrategiesNormalTest,
                       V8CacheOnCacheStorage) {
  CheckStrategyIsNormal();
}

class ServiceWorkerV8CacheStrategiesAggressiveTest
    : public ServiceWorkerV8CacheStrategiesTest {
 public:
  ServiceWorkerV8CacheStrategiesAggressiveTest() {}
  ~ServiceWorkerV8CacheStrategiesAggressiveTest() override {}
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kV8CacheStrategiesForCacheStorage,
                                    "aggressive");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerV8CacheStrategiesAggressiveTest);
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerV8CacheStrategiesAggressiveTest,
                       V8CacheOnCacheStorage) {
  CheckStrategyIsAggressive();
}

}  // namespace content
