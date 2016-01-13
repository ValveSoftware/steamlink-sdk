// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "content/browser/fileapi/chrome_blob_storage_context.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "webkit/browser/blob/blob_data_handle.h"
#include "webkit/browser/blob/blob_storage_context.h"
#include "webkit/common/blob/blob_data.h"

namespace content {

namespace {

struct FetchResult {
  ServiceWorkerStatusCode status;
  ServiceWorkerFetchEventResult result;
  ServiceWorkerResponse response;
  scoped_ptr<webkit_blob::BlobDataHandle> blob_data_handle;
};

void RunAndQuit(const base::Closure& closure,
                const base::Closure& quit,
                base::MessageLoopProxy* original_message_loop) {
  closure.Run();
  original_message_loop->PostTask(FROM_HERE, quit);
}

void RunOnIOThread(const base::Closure& closure) {
  base::RunLoop run_loop;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&RunAndQuit, closure, run_loop.QuitClosure(),
                 base::MessageLoopProxy::current()));
  run_loop.Run();
}

void RunOnIOThread(
    const base::Callback<void(const base::Closure& continuation)>& closure) {
  base::RunLoop run_loop;
  base::Closure quit_on_original_thread =
      base::Bind(base::IgnoreResult(&base::MessageLoopProxy::PostTask),
                 base::MessageLoopProxy::current().get(),
                 FROM_HERE,
                 run_loop.QuitClosure());
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(closure, quit_on_original_thread));
  run_loop.Run();
}

// Contrary to the style guide, the output parameter of this function comes
// before input parameters so Bind can be used on it to create a FetchCallback
// to pass to DispatchFetchEvent.
void ReceiveFetchResult(BrowserThread::ID run_quit_thread,
                        const base::Closure& quit,
                        ChromeBlobStorageContext* blob_context,
                        FetchResult* out_result,
                        ServiceWorkerStatusCode actual_status,
                        ServiceWorkerFetchEventResult actual_result,
                        const ServiceWorkerResponse& actual_response) {
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

ServiceWorkerVersion::FetchCallback CreateResponseReceiver(
    BrowserThread::ID run_quit_thread,
    const base::Closure& quit,
    ChromeBlobStorageContext* blob_context,
    FetchResult* result) {
  return base::Bind(&ReceiveFetchResult, run_quit_thread, quit,
                    make_scoped_refptr<ChromeBlobStorageContext>(blob_context),
                    result);
}

void ReadResponseBody(std::string* body,
                      webkit_blob::BlobDataHandle* blob_data_handle) {
  ASSERT_TRUE(blob_data_handle);
  ASSERT_EQ(1U, blob_data_handle->data()->items().size());
  *body = std::string(blob_data_handle->data()->items()[0].bytes(),
                      blob_data_handle->data()->items()[0].length());
}

}  // namespace

class ServiceWorkerBrowserTest : public ContentBrowserTest {
 protected:
  typedef ServiceWorkerBrowserTest self;

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kEnableServiceWorker);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
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

  virtual void TearDownOnMainThread() OVERRIDE {
    RunOnIOThread(base::Bind(&self::TearDownOnIOThread, this));
    wrapper_ = NULL;
  }

  virtual void SetUpOnIOThread() {}
  virtual void TearDownOnIOThread() {}

  ServiceWorkerContextWrapper* wrapper() { return wrapper_.get(); }
  ServiceWorkerContext* public_context() { return wrapper(); }

  void AssociateRendererProcessToWorker(EmbeddedWorkerInstance* worker) {
    worker->AddProcessReference(
        shell()->web_contents()->GetRenderProcessHost()->GetID());
  }

 private:
  scoped_refptr<ServiceWorkerContextWrapper> wrapper_;
};

class EmbeddedWorkerBrowserTest : public ServiceWorkerBrowserTest,
                                  public EmbeddedWorkerInstance::Listener {
 public:
  typedef EmbeddedWorkerBrowserTest self;

  EmbeddedWorkerBrowserTest()
      : last_worker_status_(EmbeddedWorkerInstance::STOPPED) {}
  virtual ~EmbeddedWorkerBrowserTest() {}

  virtual void TearDownOnIOThread() OVERRIDE {
    if (worker_) {
      worker_->RemoveListener(this);
      worker_.reset();
    }
  }

  void StartOnIOThread() {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    worker_ = wrapper()->context()->embedded_worker_registry()->CreateWorker();
    EXPECT_EQ(EmbeddedWorkerInstance::STOPPED, worker_->status());
    worker_->AddListener(this);

    AssociateRendererProcessToWorker(worker_.get());

    const int64 service_worker_version_id = 33L;
    const GURL scope = embedded_test_server()->GetURL("/*");
    const GURL script_url = embedded_test_server()->GetURL(
        "/service_worker/worker.js");
    std::vector<int> processes;
    processes.push_back(
        shell()->web_contents()->GetRenderProcessHost()->GetID());
    worker_->Start(
        service_worker_version_id,
        scope,
        script_url,
        processes,
        base::Bind(&EmbeddedWorkerBrowserTest::StartOnIOThread2, this));
  }
  void StartOnIOThread2(ServiceWorkerStatusCode status) {
    last_worker_status_ = worker_->status();
    EXPECT_EQ(SERVICE_WORKER_OK, status);
    EXPECT_EQ(EmbeddedWorkerInstance::STARTING, last_worker_status_);

    if (status != SERVICE_WORKER_OK && !done_closure_.is_null())
      done_closure_.Run();
  }

  void StopOnIOThread() {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    EXPECT_EQ(EmbeddedWorkerInstance::RUNNING, worker_->status());

    ServiceWorkerStatusCode status = worker_->Stop();

    last_worker_status_ = worker_->status();
    EXPECT_EQ(SERVICE_WORKER_OK, status);
    EXPECT_EQ(EmbeddedWorkerInstance::STOPPING, last_worker_status_);

    if (status != SERVICE_WORKER_OK && !done_closure_.is_null())
      done_closure_.Run();
  }

 protected:
  // EmbeddedWorkerInstance::Observer overrides:
  virtual void OnStarted() OVERRIDE {
    ASSERT_TRUE(worker_ != NULL);
    ASSERT_FALSE(done_closure_.is_null());
    last_worker_status_ = worker_->status();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, done_closure_);
  }
  virtual void OnStopped() OVERRIDE {
    ASSERT_TRUE(worker_ != NULL);
    ASSERT_FALSE(done_closure_.is_null());
    last_worker_status_ = worker_->status();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, done_closure_);
  }
  virtual void OnReportException(const base::string16& error_message,
                                 int line_number,
                                 int column_number,
                                 const GURL& source_url) OVERRIDE {}
  virtual void OnReportConsoleMessage(int source_identifier,
                                      int message_level,
                                      const base::string16& message,
                                      int line_number,
                                      const GURL& source_url) OVERRIDE {}
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    return false;
  }

  scoped_ptr<EmbeddedWorkerInstance> worker_;
  EmbeddedWorkerInstance::Status last_worker_status_;

  // Called by EmbeddedWorkerInstance::Observer overrides so that
  // test code can wait for the worker status notifications.
  base::Closure done_closure_;
};

class ServiceWorkerVersionBrowserTest : public ServiceWorkerBrowserTest {
 public:
  typedef ServiceWorkerVersionBrowserTest self;

  virtual ~ServiceWorkerVersionBrowserTest() {}

  virtual void TearDownOnIOThread() OVERRIDE {
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
    version_->SetStatus(ServiceWorkerVersion::INSTALLED);
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
      scoped_ptr<webkit_blob::BlobDataHandle>* blob_data_handle) {
    blob_context_ = ChromeBlobStorageContext::GetFor(
        shell()->web_contents()->GetBrowserContext());
    FetchResult fetch_result;
    fetch_result.status = SERVICE_WORKER_ERROR_FAILED;
    base::RunLoop fetch_run_loop;
    BrowserThread::PostTask(BrowserThread::IO,
                            FROM_HERE,
                            base::Bind(&self::FetchOnIOThread,
                                       this,
                                       fetch_run_loop.QuitClosure(),
                                       &fetch_result));
    fetch_run_loop.Run();
    *result = fetch_result.result;
    *response = fetch_result.response;
    *blob_data_handle = fetch_result.blob_data_handle.Pass();
    ASSERT_EQ(SERVICE_WORKER_OK, fetch_result.status);
  }

  void FetchTestHelper(
      const std::string& worker_url,
      ServiceWorkerFetchEventResult* result,
      ServiceWorkerResponse* response,
      scoped_ptr<webkit_blob::BlobDataHandle>* blob_data_handle) {
    RunOnIOThread(
        base::Bind(&self::SetUpRegistrationOnIOThread, this, worker_url));
    FetchOnRegisteredWorker(result, response, blob_data_handle);
  }

  void SetUpRegistrationOnIOThread(const std::string& worker_url) {
    registration_ = new ServiceWorkerRegistration(
        embedded_test_server()->GetURL("/*"),
        embedded_test_server()->GetURL(worker_url),
        wrapper()->context()->storage()->NewRegistrationId(),
        wrapper()->context()->AsWeakPtr());
    version_ = new ServiceWorkerVersion(
        registration_,
        wrapper()->context()->storage()->NewVersionId(),
        wrapper()->context()->AsWeakPtr());
    AssociateRendererProcessToWorker(version_->embedded_worker());
  }

  void StartOnIOThread(const base::Closure& done,
                       ServiceWorkerStatusCode* result) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    version_->StartWorker(CreateReceiver(BrowserThread::UI, done, result));
  }

  void InstallOnIOThread(const base::Closure& done,
                         ServiceWorkerStatusCode* result) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    version_->DispatchInstallEvent(
        -1, CreateReceiver(BrowserThread::UI, done, result));
  }

  void ActivateOnIOThread(const base::Closure& done,
                          ServiceWorkerStatusCode* result) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    version_->SetStatus(ServiceWorkerVersion::INSTALLED);
    version_->DispatchActivateEvent(
        CreateReceiver(BrowserThread::UI, done, result));
  }

  void FetchOnIOThread(const base::Closure& done, FetchResult* result) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    ServiceWorkerFetchRequest request(
        embedded_test_server()->GetURL("/service_worker/empty.html"),
        "GET",
        std::map<std::string, std::string>());
    version_->SetStatus(ServiceWorkerVersion::ACTIVE);
    version_->DispatchFetchEvent(
        request, CreateResponseReceiver(BrowserThread::UI, done,
                                        blob_context_, result));
  }

  void StopOnIOThread(const base::Closure& done,
                      ServiceWorkerStatusCode* result) {
    ASSERT_TRUE(version_);
    version_->StopWorker(CreateReceiver(BrowserThread::UI, done, result));
  }

  void SyncEventOnIOThread(const base::Closure& done,
                           ServiceWorkerStatusCode* result) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    version_->SetStatus(ServiceWorkerVersion::ACTIVE);
    version_->DispatchSyncEvent(
        CreateReceiver(BrowserThread::UI, done, result));
  }

 protected:
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  scoped_refptr<ChromeBlobStorageContext> blob_context_;
};

IN_PROC_BROWSER_TEST_F(EmbeddedWorkerBrowserTest, StartAndStop) {
  // Start a worker and wait until OnStarted() is called.
  base::RunLoop start_run_loop;
  done_closure_ = start_run_loop.QuitClosure();
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&self::StartOnIOThread, this));
  start_run_loop.Run();

  ASSERT_EQ(EmbeddedWorkerInstance::RUNNING, last_worker_status_);

  // Stop a worker and wait until OnStopped() is called.
  base::RunLoop stop_run_loop;
  done_closure_ = stop_run_loop.QuitClosure();
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&self::StopOnIOThread, this));
  stop_run_loop.Run();

  ASSERT_EQ(EmbeddedWorkerInstance::STOPPED, last_worker_status_);
}

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
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  base::RunLoop start_run_loop;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&self::StartOnIOThread, this,
                                     start_run_loop.QuitClosure(),
                                     &status));
  start_run_loop.Run();
  ASSERT_EQ(SERVICE_WORKER_ERROR_START_WORKER_FAILED, status);
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
                       Activate_NoEventListener) {
  ActivateTestHelper("/service_worker/worker.js", SERVICE_WORKER_OK);
  ASSERT_EQ(ServiceWorkerVersion::ACTIVE, version_->status());
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, Activate_Rejected) {
  ActivateTestHelper("/service_worker/worker_activate_rejected.js",
                     SERVICE_WORKER_ERROR_ACTIVATE_WORKER_FAILED);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       InstallWithWaitUntil_Rejected) {
  InstallTestHelper("/service_worker/worker_install_rejected.js",
                    SERVICE_WORKER_ERROR_INSTALL_WORKER_FAILED);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, FetchEvent_Response) {
  ServiceWorkerFetchEventResult result;
  ServiceWorkerResponse response;
  scoped_ptr<webkit_blob::BlobDataHandle> blob_data_handle;
  FetchTestHelper("/service_worker/fetch_event.js",
                  &result, &response, &blob_data_handle);
  ASSERT_EQ(SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE, result);
  EXPECT_EQ(301, response.status_code);
  EXPECT_EQ("Moved Permanently", response.status_text);
  std::map<std::string, std::string> expected_headers;
  expected_headers["Content-Language"] = "fi";
  expected_headers["Content-Type"] = "text/html; charset=UTF-8";
  EXPECT_EQ(expected_headers, response.headers);

  std::string body;
  RunOnIOThread(
      base::Bind(&ReadResponseBody,
                 &body, base::Owned(blob_data_handle.release())));
  EXPECT_EQ("This resource is gone. Gone, gone, gone.", body);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest,
                       SyncAbortedWithoutFlag) {
  RunOnIOThread(base::Bind(
      &self::SetUpRegistrationOnIOThread, this, "/service_worker/sync.js"));

  // Run the sync event.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  base::RunLoop sync_run_loop;
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&self::SyncEventOnIOThread,
                                     this,
                                     sync_run_loop.QuitClosure(),
                                     &status));
  sync_run_loop.Run();
  ASSERT_EQ(SERVICE_WORKER_ERROR_ABORT, status);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerVersionBrowserTest, SyncEventHandled) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(switches::kEnableServiceWorkerSync);

  RunOnIOThread(base::Bind(
      &self::SetUpRegistrationOnIOThread, this, "/service_worker/sync.js"));
  ServiceWorkerFetchEventResult result;
  ServiceWorkerResponse response;
  scoped_ptr<webkit_blob::BlobDataHandle> blob_data_handle;
  // Should 404 before sync event.
  FetchOnRegisteredWorker(&result, &response, &blob_data_handle);
  EXPECT_EQ(404, response.status_code);

  // Run the sync event.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  base::RunLoop sync_run_loop;
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&self::SyncEventOnIOThread,
                                     this,
                                     sync_run_loop.QuitClosure(),
                                     &status));
  sync_run_loop.Run();
  ASSERT_EQ(SERVICE_WORKER_OK, status);

  // Should 200 after sync event.
  FetchOnRegisteredWorker(&result, &response, &blob_data_handle);
  EXPECT_EQ(200, response.status_code);
}

class ServiceWorkerBlackBoxBrowserTest : public ServiceWorkerBrowserTest {
 public:
  typedef ServiceWorkerBlackBoxBrowserTest self;

  static void ExpectResultAndRun(bool expected,
                                 const base::Closure& continuation,
                                 bool actual) {
    EXPECT_EQ(expected, actual);
    continuation.Run();
  }

  void FindRegistrationOnIO(const GURL& document_url,
                            ServiceWorkerStatusCode* status,
                            GURL* script_url,
                            const base::Closure& continuation) {
    wrapper()->context()->storage()->FindRegistrationForDocument(
        document_url,
        base::Bind(&ServiceWorkerBlackBoxBrowserTest::FindRegistrationOnIO2,
                   this,
                   status,
                   script_url,
                   continuation));
  }

  void FindRegistrationOnIO2(
      ServiceWorkerStatusCode* out_status,
      GURL* script_url,
      const base::Closure& continuation,
      ServiceWorkerStatusCode status,
      const scoped_refptr<ServiceWorkerRegistration>& registration) {
    *out_status = status;
    if (registration) {
      *script_url = registration->script_url();
    } else {
      EXPECT_NE(SERVICE_WORKER_OK, status);
    }
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

// Crashes on Android: http://crbug.com/387045
#if defined(OS_ANDROID)
#define MAYBE_Registration DISABLED_Registration
#else
#define MAYBE_Registration Registration
#endif
IN_PROC_BROWSER_TEST_F(ServiceWorkerBlackBoxBrowserTest, MAYBE_Registration) {
  // Close the only window to be sure we're not re-using its RenderProcessHost.
  shell()->Close();
  EXPECT_EQ(0, CountRenderProcessHosts());

  const std::string kWorkerUrl = "/service_worker/fetch_event.js";

  // Unregistering nothing should return true.
  {
    base::RunLoop run_loop;
    public_context()->UnregisterServiceWorker(
        embedded_test_server()->GetURL("/*"),
        base::Bind(&ServiceWorkerBlackBoxBrowserTest::ExpectResultAndRun,
                   true,
                   run_loop.QuitClosure()));
    run_loop.Run();
  }

  // If we use a worker URL that doesn't exist, registration fails.
  {
    base::RunLoop run_loop;
    public_context()->RegisterServiceWorker(
        embedded_test_server()->GetURL("/*"),
        embedded_test_server()->GetURL("/does/not/exist"),
        base::Bind(&ServiceWorkerBlackBoxBrowserTest::ExpectResultAndRun,
                   false,
                   run_loop.QuitClosure()));
    run_loop.Run();
  }
  EXPECT_EQ(0, CountRenderProcessHosts());

  // Register returns when the promise would be resolved.
  {
    base::RunLoop run_loop;
    public_context()->RegisterServiceWorker(
        embedded_test_server()->GetURL("/*"),
        embedded_test_server()->GetURL(kWorkerUrl),
        base::Bind(&ServiceWorkerBlackBoxBrowserTest::ExpectResultAndRun,
                   true,
                   run_loop.QuitClosure()));
    run_loop.Run();
  }
  EXPECT_EQ(1, CountRenderProcessHosts());

  // Registering again should succeed, although the algo still
  // might not be complete.
  {
    base::RunLoop run_loop;
    public_context()->RegisterServiceWorker(
        embedded_test_server()->GetURL("/*"),
        embedded_test_server()->GetURL(kWorkerUrl),
        base::Bind(&ServiceWorkerBlackBoxBrowserTest::ExpectResultAndRun,
                   true,
                   run_loop.QuitClosure()));
    run_loop.Run();
  }

  // The registration algo might not be far enough along to have
  // stored the registration data, so it may not be findable
  // at this point.

  // Unregistering something should return true.
  {
    base::RunLoop run_loop;
    public_context()->UnregisterServiceWorker(
        embedded_test_server()->GetURL("/*"),
        base::Bind(&ServiceWorkerBlackBoxBrowserTest::ExpectResultAndRun,
                   true,
                   run_loop.QuitClosure()));
    run_loop.Run();
  }
  EXPECT_GE(1, CountRenderProcessHosts()) << "Unregistering doesn't stop the "
                                             "workers eagerly, so their RPHs "
                                             "can still be running.";

  // Should not be able to find it.
  {
    ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
    GURL script_url;
    RunOnIOThread(
        base::Bind(&ServiceWorkerBlackBoxBrowserTest::FindRegistrationOnIO,
                   this,
                   embedded_test_server()->GetURL("/service_worker/empty.html"),
                   &status,
                   &script_url));
    EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND, status);
  }
}

}  // namespace content
