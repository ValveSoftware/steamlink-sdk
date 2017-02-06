// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains download browser tests that are known to be runnable
// in a pure content context.  Over time tests should be migrated here.

#include <stddef.h>
#include <stdint.h>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/byte_stream.h"
#include "content/browser/download/download_file_factory.h"
#include "content/browser/download/download_file_impl.h"
#include "content/browser/download/download_item_impl.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/download/download_resource_handler.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/download_danger_type.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/common/content_features.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_download_request_handler.h"
#include "content/public/test/test_file_error_injector.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_download_manager_delegate.h"
#include "content/shell/browser/shell_network_delegate.h"
#include "device/power_save_blocker/power_save_blocker.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "net/test/url_request/url_request_slow_download_job.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(ENABLE_PLUGINS)
#include "content/browser/plugin_service_impl.h"
#endif

using ::testing::AllOf;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Property;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::_;

namespace content {

namespace {

class MockDownloadItemObserver : public DownloadItem::Observer {
 public:
  MockDownloadItemObserver() {}
  virtual ~MockDownloadItemObserver() {}

  MOCK_METHOD1(OnDownloadUpdated, void(DownloadItem*));
  MOCK_METHOD1(OnDownloadOpened, void(DownloadItem*));
  MOCK_METHOD1(OnDownloadRemoved, void(DownloadItem*));
  MOCK_METHOD1(OnDownloadDestroyed, void(DownloadItem*));
};

class MockDownloadManagerObserver : public DownloadManager::Observer {
 public:
  MockDownloadManagerObserver(DownloadManager* manager) {
    manager_ = manager;
    manager->AddObserver(this);
  }
  virtual ~MockDownloadManagerObserver() {
    if (manager_)
      manager_->RemoveObserver(this);
  }

  MOCK_METHOD2(OnDownloadCreated, void(DownloadManager*, DownloadItem*));
  MOCK_METHOD1(ModelChanged, void(DownloadManager*));
  void ManagerGoingDown(DownloadManager* manager) {
    DCHECK_EQ(manager_, manager);
    MockManagerGoingDown(manager);

    manager_->RemoveObserver(this);
    manager_ = NULL;
  }

  MOCK_METHOD1(MockManagerGoingDown, void(DownloadManager*));
 private:
  DownloadManager* manager_;
};

class DownloadFileWithDelayFactory;

static DownloadManagerImpl* DownloadManagerForShell(Shell* shell) {
  // We're in a content_browsertest; we know that the DownloadManager
  // is a DownloadManagerImpl.
  return static_cast<DownloadManagerImpl*>(
      BrowserContext::GetDownloadManager(
          shell->web_contents()->GetBrowserContext()));
}

class DownloadFileWithDelay : public DownloadFileImpl {
 public:
  DownloadFileWithDelay(
      std::unique_ptr<DownloadSaveInfo> save_info,
      const base::FilePath& default_download_directory,
      std::unique_ptr<ByteStreamReader> stream,
      const net::BoundNetLog& bound_net_log,
      std::unique_ptr<device::PowerSaveBlocker> power_save_blocker,
      base::WeakPtr<DownloadDestinationObserver> observer,
      base::WeakPtr<DownloadFileWithDelayFactory> owner);

  ~DownloadFileWithDelay() override;

  // Wraps DownloadFileImpl::Rename* and intercepts the return callback,
  // storing it in the factory that produced this object for later
  // retrieval.
  void RenameAndUniquify(const base::FilePath& full_path,
                         const RenameCompletionCallback& callback) override;
  void RenameAndAnnotate(const base::FilePath& full_path,
                         const std::string& client_guid,
                         const GURL& source_url,
                         const GURL& referrer_url,
                         const RenameCompletionCallback& callback) override;

 private:
  static void RenameCallbackWrapper(
      const base::WeakPtr<DownloadFileWithDelayFactory>& factory,
      const RenameCompletionCallback& original_callback,
      DownloadInterruptReason reason,
      const base::FilePath& path);

  // This variable may only be read on the FILE thread, and may only be
  // indirected through (e.g. methods on DownloadFileWithDelayFactory called)
  // on the UI thread.  This is because after construction,
  // DownloadFileWithDelay lives on the file thread, but
  // DownloadFileWithDelayFactory is purely a UI thread object.
  base::WeakPtr<DownloadFileWithDelayFactory> owner_;

  DISALLOW_COPY_AND_ASSIGN(DownloadFileWithDelay);
};

// All routines on this class must be called on the UI thread.
class DownloadFileWithDelayFactory : public DownloadFileFactory {
 public:
  DownloadFileWithDelayFactory();
  ~DownloadFileWithDelayFactory() override;

  // DownloadFileFactory interface.
  DownloadFile* CreateFile(
      std::unique_ptr<DownloadSaveInfo> save_info,
      const base::FilePath& default_download_directory,
      std::unique_ptr<ByteStreamReader> stream,
      const net::BoundNetLog& bound_net_log,
      base::WeakPtr<DownloadDestinationObserver> observer) override;

  void AddRenameCallback(base::Closure callback);
  void GetAllRenameCallbacks(std::vector<base::Closure>* results);

  // Do not return until GetAllRenameCallbacks() will return a non-empty list.
  void WaitForSomeCallback();

 private:
  std::vector<base::Closure> rename_callbacks_;
  bool waiting_;
  base::WeakPtrFactory<DownloadFileWithDelayFactory> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DownloadFileWithDelayFactory);
};

DownloadFileWithDelay::DownloadFileWithDelay(
    std::unique_ptr<DownloadSaveInfo> save_info,
    const base::FilePath& default_download_directory,
    std::unique_ptr<ByteStreamReader> stream,
    const net::BoundNetLog& bound_net_log,
    std::unique_ptr<device::PowerSaveBlocker> power_save_blocker,
    base::WeakPtr<DownloadDestinationObserver> observer,
    base::WeakPtr<DownloadFileWithDelayFactory> owner)
    : DownloadFileImpl(std::move(save_info),
                       default_download_directory,
                       std::move(stream),
                       bound_net_log,
                       observer),
      owner_(owner) {}

DownloadFileWithDelay::~DownloadFileWithDelay() {}

void DownloadFileWithDelay::RenameAndUniquify(
    const base::FilePath& full_path,
    const RenameCompletionCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DownloadFileImpl::RenameAndUniquify(
      full_path, base::Bind(DownloadFileWithDelay::RenameCallbackWrapper,
                            owner_, callback));
}

void DownloadFileWithDelay::RenameAndAnnotate(
    const base::FilePath& full_path,
    const std::string& client_guid,
    const GURL& source_url,
    const GURL& referrer_url,
    const RenameCompletionCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DownloadFileImpl::RenameAndAnnotate(
      full_path,
      client_guid,
      source_url,
      referrer_url,
      base::Bind(
          DownloadFileWithDelay::RenameCallbackWrapper, owner_, callback));
}

// static
void DownloadFileWithDelay::RenameCallbackWrapper(
    const base::WeakPtr<DownloadFileWithDelayFactory>& factory,
    const RenameCompletionCallback& original_callback,
    DownloadInterruptReason reason,
    const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!factory)
    return;
  factory->AddRenameCallback(base::Bind(original_callback, reason, path));
}

DownloadFileWithDelayFactory::DownloadFileWithDelayFactory()
    : waiting_(false),
      weak_ptr_factory_(this) {}

DownloadFileWithDelayFactory::~DownloadFileWithDelayFactory() {}

DownloadFile* DownloadFileWithDelayFactory::CreateFile(
    std::unique_ptr<DownloadSaveInfo> save_info,
    const base::FilePath& default_download_directory,
    std::unique_ptr<ByteStreamReader> stream,
    const net::BoundNetLog& bound_net_log,
    base::WeakPtr<DownloadDestinationObserver> observer) {
  std::unique_ptr<device::PowerSaveBlocker> psb(new device::PowerSaveBlocker(
      device::PowerSaveBlocker::kPowerSaveBlockPreventAppSuspension,
      device::PowerSaveBlocker::kReasonOther, "Download in progress",
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE)));
  return new DownloadFileWithDelay(std::move(save_info),
                                   default_download_directory,
                                   std::move(stream),
                                   bound_net_log,
                                   std::move(psb),
                                   observer,
                                   weak_ptr_factory_.GetWeakPtr());
}

void DownloadFileWithDelayFactory::AddRenameCallback(base::Closure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  rename_callbacks_.push_back(callback);
  if (waiting_)
    base::MessageLoopForUI::current()->QuitWhenIdle();
}

void DownloadFileWithDelayFactory::GetAllRenameCallbacks(
    std::vector<base::Closure>* results) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  results->swap(rename_callbacks_);
}

void DownloadFileWithDelayFactory::WaitForSomeCallback() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (rename_callbacks_.empty()) {
    waiting_ = true;
    RunMessageLoop();
    waiting_ = false;
  }
}

class CountingDownloadFile : public DownloadFileImpl {
 public:
  CountingDownloadFile(
      std::unique_ptr<DownloadSaveInfo> save_info,
      const base::FilePath& default_downloads_directory,
      std::unique_ptr<ByteStreamReader> stream,
      const net::BoundNetLog& bound_net_log,
      std::unique_ptr<device::PowerSaveBlocker> power_save_blocker,
      base::WeakPtr<DownloadDestinationObserver> observer)
      : DownloadFileImpl(std::move(save_info),
                         default_downloads_directory,
                         std::move(stream),
                         bound_net_log,
                         observer) {}

  ~CountingDownloadFile() override {
    DCHECK_CURRENTLY_ON(BrowserThread::FILE);
    active_files_--;
  }

  void Initialize(const InitializeCallback& callback) override {
    DCHECK_CURRENTLY_ON(BrowserThread::FILE);
    active_files_++;
    return DownloadFileImpl::Initialize(callback);
  }

  static void GetNumberActiveFiles(int* result) {
    DCHECK_CURRENTLY_ON(BrowserThread::FILE);
    *result = active_files_;
  }

  // Can be called on any thread, and will block (running message loop)
  // until data is returned.
  static int GetNumberActiveFilesFromFileThread() {
    int result = -1;
    BrowserThread::PostTaskAndReply(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&CountingDownloadFile::GetNumberActiveFiles, &result),
        base::MessageLoop::current()->QuitWhenIdleClosure());
    base::MessageLoop::current()->Run();
    DCHECK_NE(-1, result);
    return result;
  }

 private:
  static int active_files_;
};

int CountingDownloadFile::active_files_ = 0;

class CountingDownloadFileFactory : public DownloadFileFactory {
 public:
  CountingDownloadFileFactory() {}
  ~CountingDownloadFileFactory() override {}

  // DownloadFileFactory interface.
  DownloadFile* CreateFile(
      std::unique_ptr<DownloadSaveInfo> save_info,
      const base::FilePath& default_downloads_directory,
      std::unique_ptr<ByteStreamReader> stream,
      const net::BoundNetLog& bound_net_log,
      base::WeakPtr<DownloadDestinationObserver> observer) override {
    std::unique_ptr<device::PowerSaveBlocker> psb(new device::PowerSaveBlocker(
        device::PowerSaveBlocker::kPowerSaveBlockPreventAppSuspension,
        device::PowerSaveBlocker::kReasonOther, "Download in progress",
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE)));
    return new CountingDownloadFile(std::move(save_info),
                                    default_downloads_directory,
                                    std::move(stream),
                                    bound_net_log,
                                    std::move(psb),
                                    observer);
  }
};

class TestShellDownloadManagerDelegate : public ShellDownloadManagerDelegate {
 public:
  TestShellDownloadManagerDelegate()
      : delay_download_open_(false) {}
  ~TestShellDownloadManagerDelegate() override {}

  bool ShouldOpenDownload(
      DownloadItem* item,
      const DownloadOpenDelayedCallback& callback) override {
    if (delay_download_open_) {
      delayed_callbacks_.push_back(callback);
      return false;
    }
    return true;
  }

  bool GenerateFileHash() override { return true; }

  void SetDelayedOpen(bool delay) {
    delay_download_open_ = delay;
  }

  void GetDelayedCallbacks(
      std::vector<DownloadOpenDelayedCallback>* callbacks) {
    callbacks->swap(delayed_callbacks_);
  }
 private:
  bool delay_download_open_;
  std::vector<DownloadOpenDelayedCallback> delayed_callbacks_;
};

// Get the next created download.
class DownloadCreateObserver : DownloadManager::Observer {
 public:
  DownloadCreateObserver(DownloadManager* manager)
      : manager_(manager), item_(NULL) {
    manager_->AddObserver(this);
  }

  ~DownloadCreateObserver() override {
    if (manager_)
      manager_->RemoveObserver(this);
    manager_ = NULL;
  }

  void ManagerGoingDown(DownloadManager* manager) override {
    DCHECK_EQ(manager_, manager);
    manager_->RemoveObserver(this);
    manager_ = NULL;
  }

  void OnDownloadCreated(DownloadManager* manager,
                         DownloadItem* download) override {
    if (!item_)
      item_ = download;

    if (!completion_closure_.is_null())
      base::ResetAndReturn(&completion_closure_).Run();
  }

  DownloadItem* WaitForFinished() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!item_) {
      base::RunLoop run_loop;
      completion_closure_ = run_loop.QuitClosure();
      run_loop.Run();
    }
    return item_;
  }

 private:
  DownloadManager* manager_;
  DownloadItem* item_;
  base::Closure completion_closure_;
};

bool IsDownloadInState(DownloadItem::DownloadState state, DownloadItem* item) {
  return item->GetState() == state;
}

// Request handler to be used with CreateRedirectHandler().
std::unique_ptr<net::test_server::HttpResponse>
HandleRequestAndSendRedirectResponse(
    const std::string& relative_url,
    const GURL& target_url,
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> response;
  if (request.relative_url == relative_url) {
    response.reset(new net::test_server::BasicHttpResponse);
    response->set_code(net::HTTP_FOUND);
    response->AddCustomHeader("Location", target_url.spec());
  }
  return std::move(response);
}

// Creates a request handler for EmbeddedTestServer that responds with a HTTP
// 302 redirect if the request URL matches |relative_url|.
net::EmbeddedTestServer::HandleRequestCallback CreateRedirectHandler(
    const std::string& relative_url,
    const GURL& target_url) {
  return base::Bind(
      &HandleRequestAndSendRedirectResponse, relative_url, target_url);
}

// Request handler to be used with CreateBasicResponseHandler().
std::unique_ptr<net::test_server::HttpResponse>
HandleRequestAndSendBasicResponse(
    const std::string& relative_url,
    const base::StringPairs& headers,
    const std::string& content_type,
    const std::string& body,
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> response;
  if (request.relative_url == relative_url) {
    response.reset(new net::test_server::BasicHttpResponse);
    for (const auto& pair : headers)
      response->AddCustomHeader(pair.first, pair.second);
    response->set_content_type(content_type);
    response->set_content(body);
  }
  return std::move(response);
}

// Creates a request handler for an EmbeddedTestServer that response with an
// HTTP 200 status code, a Content-Type header and a body.
net::EmbeddedTestServer::HandleRequestCallback CreateBasicResponseHandler(
    const std::string& relative_url,
    const base::StringPairs& headers,
    const std::string& content_type,
    const std::string& body) {
  return base::Bind(&HandleRequestAndSendBasicResponse, relative_url, headers,
                    content_type, body);
}

// Helper class to "flatten" handling of
// TestDownloadRequestHandler::OnStartHandler.
class TestRequestStartHandler {
 public:
  // Construct an OnStartHandler that can be set as the on_start_handler for
  // TestDownloadRequestHandler::Parameters.
  TestDownloadRequestHandler::OnStartHandler GetOnStartHandler() {
    EXPECT_FALSE(used_) << "GetOnStartHandler() should only be called once for "
                           "an instance of TestRequestStartHandler.";
    used_ = true;
    return base::Bind(&TestRequestStartHandler::OnStartHandler,
                      base::Unretained(this));
  }

  // Wait until the OnStartHandlers returned in a prior call to
  // GetOnStartHandler() is invoked.
  void WaitForCallback() {
    if (response_callback_.is_null())
      run_loop_.Run();
  }

  // Respond to the OnStartHandler() invocation using |headers| and |error|.
  void RespondWith(const std::string& headers, net::Error error) {
    ASSERT_FALSE(response_callback_.is_null());
    response_callback_.Run(headers, error);
  }

  // Return the headers returned from the invocation of OnStartHandler.
  const net::HttpRequestHeaders& headers() const {
    EXPECT_FALSE(response_callback_.is_null());
    return request_headers_;
  }

 private:
  void OnStartHandler(const net::HttpRequestHeaders& headers,
                      const TestDownloadRequestHandler::OnStartResponseCallback&
                          response_callback) {
    request_headers_ = headers;
    response_callback_ = response_callback;
    if (run_loop_.running())
      run_loop_.Quit();
  }

  bool used_ = false;
  base::RunLoop run_loop_;
  net::HttpRequestHeaders request_headers_;
  TestDownloadRequestHandler::OnStartResponseCallback response_callback_;
};

class DownloadContentTest : public ContentBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(downloads_directory_.CreateUniqueTempDir());

    test_delegate_.reset(new TestShellDownloadManagerDelegate());
    test_delegate_->SetDownloadBehaviorForTesting(downloads_directory_.path());
    DownloadManager* manager = DownloadManagerForShell(shell());
    manager->GetDelegate()->Shutdown();
    manager->SetDelegate(test_delegate_.get());
    test_delegate_->SetDownloadManager(manager);

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&net::URLRequestSlowDownloadJob::AddUrlHandler));
    base::FilePath mock_base(GetTestFilePath("download", ""));
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(
            &net::URLRequestMockHTTPJob::AddUrlHandlers, mock_base,
            make_scoped_refptr(content::BrowserThread::GetBlockingPool())));
  }

  TestShellDownloadManagerDelegate* GetDownloadManagerDelegate() {
    return test_delegate_.get();
  }

  const base::FilePath& GetDownloadDirectory() const {
    return downloads_directory_.path();
  }

  // Create a DownloadTestObserverTerminal that will wait for the
  // specified number of downloads to finish.
  DownloadTestObserver* CreateWaiter(
      Shell* shell, int num_downloads) {
    DownloadManager* download_manager = DownloadManagerForShell(shell);
    return new DownloadTestObserverTerminal(download_manager, num_downloads,
        DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
  }

  void WaitForInterrupt(DownloadItem* download) {
    DownloadUpdatedObserver(
        download, base::Bind(&IsDownloadInState, DownloadItem::INTERRUPTED))
        .WaitForEvent();
  }

  void WaitForInProgress(DownloadItem* download) {
    DownloadUpdatedObserver(
        download, base::Bind(&IsDownloadInState, DownloadItem::IN_PROGRESS))
        .WaitForEvent();
  }

  void WaitForCompletion(DownloadItem* download) {
    DownloadUpdatedObserver(
        download, base::Bind(&IsDownloadInState, DownloadItem::COMPLETE))
        .WaitForEvent();
  }

  void WaitForCancel(DownloadItem* download) {
    DownloadUpdatedObserver(
        download, base::Bind(&IsDownloadInState, DownloadItem::CANCELLED))
        .WaitForEvent();
  }

  // Note: Cannot be used with other alternative DownloadFileFactorys
  void SetupEnsureNoPendingDownloads() {
    DownloadManagerForShell(shell())->SetDownloadFileFactoryForTesting(
        std::unique_ptr<DownloadFileFactory>(
            new CountingDownloadFileFactory()));
  }

  bool EnsureNoPendingDownloads() {
    bool result = true;
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&EnsureNoPendingDownloadJobsOnIO, &result));
    base::MessageLoop::current()->Run();
    return result &&
           (CountingDownloadFile::GetNumberActiveFilesFromFileThread() == 0);
  }

  void NavigateToURLAndWaitForDownload(
      Shell* shell,
      const GURL& url,
      DownloadItem::DownloadState expected_terminal_state) {
    std::unique_ptr<DownloadTestObserver> observer(CreateWaiter(shell, 1));
    NavigateToURL(shell, url);
    observer->WaitForFinished();
    EXPECT_EQ(1u, observer->NumDownloadsSeenInState(expected_terminal_state));
  }

  // Checks that |path| is has |file_size| bytes, and matches the |value|
  // string.
  bool VerifyFile(const base::FilePath& path,
                  const std::string& value,
                  const int64_t file_size) {
    std::string file_contents;

    bool read = base::ReadFileToString(path, &file_contents);
    EXPECT_TRUE(read) << "Failed reading file: " << path.value() << std::endl;
    if (!read)
      return false;  // Couldn't read the file.

    // Note: we don't handle really large files (more than size_t can hold)
    // so we will fail in that case.
    size_t expected_size = static_cast<size_t>(file_size);

    // Check the size.
    EXPECT_EQ(expected_size, file_contents.size());
    if (expected_size != file_contents.size())
      return false;

    // Check the contents.
    EXPECT_EQ(value, file_contents);
    if (memcmp(file_contents.c_str(), value.c_str(), expected_size) != 0)
      return false;

    return true;
  }

  // Start a download and return the item.
  DownloadItem* StartDownloadAndReturnItem(Shell* shell, GURL url) {
    std::unique_ptr<DownloadCreateObserver> observer(
        new DownloadCreateObserver(DownloadManagerForShell(shell)));
    shell->LoadURL(url);
    return observer->WaitForFinished();
  }

  static void ReadAndVerifyFileContents(int seed,
                                        int64_t expected_size,
                                        const base::FilePath& path) {
    base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    ASSERT_TRUE(file.IsValid());
    int64_t file_length = file.GetLength();
    ASSERT_EQ(expected_size, file_length);

    const int64_t kBufferSize = 64 * 1024;
    std::vector<char> pattern;
    std::vector<char> data;
    pattern.resize(kBufferSize);
    data.resize(kBufferSize);
    for (int64_t offset = 0; offset < file_length;) {
      int bytes_read = file.Read(offset, &data.front(), kBufferSize);
      ASSERT_LT(0, bytes_read);
      ASSERT_GE(kBufferSize, bytes_read);

      TestDownloadRequestHandler::GetPatternBytes(seed, offset, bytes_read,
                                                  &pattern.front());
      ASSERT_EQ(0, memcmp(&pattern.front(), &data.front(), bytes_read))
          << "Comparing block at offset " << offset << " and length "
          << bytes_read;
      offset += bytes_read;
    }
  }

 private:
  static void EnsureNoPendingDownloadJobsOnIO(bool* result) {
    if (net::URLRequestSlowDownloadJob::NumberOutstandingRequests())
      *result = false;
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::MessageLoop::QuitWhenIdleClosure());
  }

  // Location of the downloads directory for these tests
  base::ScopedTempDir downloads_directory_;
  std::unique_ptr<TestShellDownloadManagerDelegate> test_delegate_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(DownloadContentTest, DownloadCancelled) {
  SetupEnsureNoPendingDownloads();

  // Create a download, wait until it's started, and confirm
  // we're in the expected state.
  DownloadItem* download = StartDownloadAndReturnItem(
      shell(), GURL(net::URLRequestSlowDownloadJob::kUnknownSizeUrl));
  ASSERT_EQ(DownloadItem::IN_PROGRESS, download->GetState());

  // Cancel the download and wait for download system quiesce.
  download->Cancel(true);
  scoped_refptr<DownloadTestFlushObserver> flush_observer(
      new DownloadTestFlushObserver(DownloadManagerForShell(shell())));
  flush_observer->WaitForFlush();

  // Get the important info from other threads and check it.
  EXPECT_TRUE(EnsureNoPendingDownloads());
}

// Check that downloading multiple (in this case, 2) files does not result in
// corrupted files.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, MultiDownload) {
  SetupEnsureNoPendingDownloads();

  // Create a download, wait until it's started, and confirm
  // we're in the expected state.
  DownloadItem* download1 = StartDownloadAndReturnItem(
      shell(), GURL(net::URLRequestSlowDownloadJob::kUnknownSizeUrl));
  ASSERT_EQ(DownloadItem::IN_PROGRESS, download1->GetState());

  // Start the second download and wait until it's done.
  GURL url(net::URLRequestMockHTTPJob::GetMockUrl("download-test.lib"));
  DownloadItem* download2 = StartDownloadAndReturnItem(shell(), url);
  WaitForCompletion(download2);

  ASSERT_EQ(DownloadItem::IN_PROGRESS, download1->GetState());
  ASSERT_EQ(DownloadItem::COMPLETE, download2->GetState());

  // Allow the first request to finish.
  std::unique_ptr<DownloadTestObserver> observer2(CreateWaiter(shell(), 1));
  NavigateToURL(shell(),
                GURL(net::URLRequestSlowDownloadJob::kFinishDownloadUrl));
  observer2->WaitForFinished();  // Wait for the third request.
  EXPECT_EQ(1u, observer2->NumDownloadsSeenInState(DownloadItem::COMPLETE));

  // Get the important info from other threads and check it.
  EXPECT_TRUE(EnsureNoPendingDownloads());

  // The |DownloadItem|s should now be done and have the final file names.
  // Verify that the files have the expected data and size.
  // |file1| should be full of '*'s, and |file2| should be the same as the
  // source file.
  base::FilePath file1(download1->GetTargetFilePath());
  size_t file_size1 = net::URLRequestSlowDownloadJob::kFirstDownloadSize +
                      net::URLRequestSlowDownloadJob::kSecondDownloadSize;
  std::string expected_contents(file_size1, '*');
  ASSERT_TRUE(VerifyFile(file1, expected_contents, file_size1));

  base::FilePath file2(download2->GetTargetFilePath());
  ASSERT_TRUE(base::ContentsEqual(
      file2, GetTestFilePath("download", "download-test.lib")));
}

#if defined(ENABLE_PLUGINS)
// Content served with a MIME type of application/octet-stream should be
// downloaded even when a plugin can be found that handles the file type.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, DownloadOctetStream) {
  const char kTestPluginName[] = "TestPlugin";
  const char kTestMimeType[] = "application/x-test-mime-type";
  const char kTestFileType[] = "abc";

  WebPluginInfo plugin_info;
  plugin_info.name = base::ASCIIToUTF16(kTestPluginName);
  plugin_info.mime_types.push_back(
      WebPluginMimeType(kTestMimeType, kTestFileType, ""));
  plugin_info.type = WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS;
  PluginServiceImpl::GetInstance()->RegisterInternalPlugin(plugin_info, false);

  // The following is served with a Content-Type of application/octet-stream.
  GURL url(
      net::URLRequestMockHTTPJob::GetMockUrl("octet-stream.abc"));
  NavigateToURLAndWaitForDownload(shell(), url, DownloadItem::COMPLETE);
}
#endif

// Try to cancel just before we release the download file, by delaying final
// rename callback.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, CancelAtFinalRename) {
  // Setup new factory.
  DownloadFileWithDelayFactory* file_factory =
      new DownloadFileWithDelayFactory();
  DownloadManagerImpl* download_manager(DownloadManagerForShell(shell()));
  download_manager->SetDownloadFileFactoryForTesting(
      std::unique_ptr<DownloadFileFactory>(file_factory));

  // Create a download
  NavigateToURL(shell(),
                net::URLRequestMockHTTPJob::GetMockUrl("download-test.lib"));

  // Wait until the first (intermediate file) rename and execute the callback.
  file_factory->WaitForSomeCallback();
  std::vector<base::Closure> callbacks;
  file_factory->GetAllRenameCallbacks(&callbacks);
  ASSERT_EQ(1u, callbacks.size());
  callbacks[0].Run();
  callbacks.clear();

  // Wait until the second (final) rename callback is posted.
  file_factory->WaitForSomeCallback();
  file_factory->GetAllRenameCallbacks(&callbacks);
  ASSERT_EQ(1u, callbacks.size());

  // Cancel it.
  std::vector<DownloadItem*> items;
  download_manager->GetAllDownloads(&items);
  ASSERT_EQ(1u, items.size());
  items[0]->Cancel(true);
  RunAllPendingInMessageLoop();

  // Check state.
  EXPECT_EQ(DownloadItem::CANCELLED, items[0]->GetState());

  // Run final rename callback.
  callbacks[0].Run();
  callbacks.clear();

  // Check state.
  EXPECT_EQ(DownloadItem::CANCELLED, items[0]->GetState());
}

// Try to cancel just after we release the download file, by delaying
// in ShouldOpenDownload.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, CancelAtRelease) {
  DownloadManagerImpl* download_manager(DownloadManagerForShell(shell()));

  // Mark delegate for delayed open.
  GetDownloadManagerDelegate()->SetDelayedOpen(true);

  // Setup new factory.
  DownloadFileWithDelayFactory* file_factory =
      new DownloadFileWithDelayFactory();
  download_manager->SetDownloadFileFactoryForTesting(
      std::unique_ptr<DownloadFileFactory>(file_factory));

  // Create a download
  NavigateToURL(shell(),
                net::URLRequestMockHTTPJob::GetMockUrl("download-test.lib"));

  // Wait until the first (intermediate file) rename and execute the callback.
  file_factory->WaitForSomeCallback();
  std::vector<base::Closure> callbacks;
  file_factory->GetAllRenameCallbacks(&callbacks);
  ASSERT_EQ(1u, callbacks.size());
  callbacks[0].Run();
  callbacks.clear();

  // Wait until the second (final) rename callback is posted.
  file_factory->WaitForSomeCallback();
  file_factory->GetAllRenameCallbacks(&callbacks);
  ASSERT_EQ(1u, callbacks.size());

  // Call it.
  callbacks[0].Run();
  callbacks.clear();

  // Confirm download still IN_PROGRESS (internal state COMPLETING).
  std::vector<DownloadItem*> items;
  download_manager->GetAllDownloads(&items);
  EXPECT_EQ(DownloadItem::IN_PROGRESS, items[0]->GetState());

  // Cancel the download; confirm cancel fails.
  ASSERT_EQ(1u, items.size());
  items[0]->Cancel(true);
  EXPECT_EQ(DownloadItem::IN_PROGRESS, items[0]->GetState());

  // Need to complete open test.
  std::vector<DownloadOpenDelayedCallback> delayed_callbacks;
  GetDownloadManagerDelegate()->GetDelayedCallbacks(
      &delayed_callbacks);
  ASSERT_EQ(1u, delayed_callbacks.size());
  delayed_callbacks[0].Run(true);

  // *Now* the download should be complete.
  EXPECT_EQ(DownloadItem::COMPLETE, items[0]->GetState());
}

#if defined(OS_ANDROID)
// Flaky on android: https://crbug.com/324525
#define MAYBE_ShutdownInProgress DISABLED_ShutdownInProgress
#else
#define MAYBE_ShutdownInProgress ShutdownInProgress
#endif

// Try to shutdown with a download in progress to make sure shutdown path
// works properly.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, MAYBE_ShutdownInProgress) {
  // Create a download that won't complete.
  DownloadItem* download = StartDownloadAndReturnItem(
      shell(), GURL(net::URLRequestSlowDownloadJob::kUnknownSizeUrl));

  EXPECT_EQ(DownloadItem::IN_PROGRESS, download->GetState());

  // Shutdown the download manager and make sure we get the right
  // notifications in the right order.
  StrictMock<MockDownloadItemObserver> item_observer;
  download->AddObserver(&item_observer);
  MockDownloadManagerObserver manager_observer(
      DownloadManagerForShell(shell()));
  // Don't care about ModelChanged() events.
  EXPECT_CALL(manager_observer, ModelChanged(_))
      .WillRepeatedly(Return());
  {
    InSequence notifications;

    EXPECT_CALL(manager_observer, MockManagerGoingDown(
        DownloadManagerForShell(shell())))
        .WillOnce(Return());
    EXPECT_CALL(
        item_observer,
        OnDownloadUpdated(AllOf(download, Property(&DownloadItem::GetState,
                                                   DownloadItem::CANCELLED))))
        .WillOnce(Return());
    EXPECT_CALL(item_observer, OnDownloadDestroyed(download))
        .WillOnce(Return());
  }

  // See http://crbug.com/324525.  If we have a refcount release/post task
  // race, the second post will stall the IO thread long enough so that we'll
  // lose the race and crash.  The first stall is just to give the UI thread
  // a chance to get the second stall onto the IO thread queue after the cancel
  // message created by Shutdown and before the notification callback
  // created by the IO thread in canceling the request.
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&base::PlatformThread::Sleep,
                                     base::TimeDelta::FromMilliseconds(25)));
  DownloadManagerForShell(shell())->Shutdown();
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&base::PlatformThread::Sleep,
                                     base::TimeDelta::FromMilliseconds(25)));
}

// Try to shutdown just after we release the download file, by delaying
// release.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, ShutdownAtRelease) {
  DownloadManagerImpl* download_manager(DownloadManagerForShell(shell()));

  // Mark delegate for delayed open.
  GetDownloadManagerDelegate()->SetDelayedOpen(true);

  // Setup new factory.
  DownloadFileWithDelayFactory* file_factory =
      new DownloadFileWithDelayFactory();
  download_manager->SetDownloadFileFactoryForTesting(
      std::unique_ptr<DownloadFileFactory>(file_factory));

  // Create a download
  NavigateToURL(shell(),
                net::URLRequestMockHTTPJob::GetMockUrl("download-test.lib"));

  // Wait until the first (intermediate file) rename and execute the callback.
  file_factory->WaitForSomeCallback();
  std::vector<base::Closure> callbacks;
  file_factory->GetAllRenameCallbacks(&callbacks);
  ASSERT_EQ(1u, callbacks.size());
  callbacks[0].Run();
  callbacks.clear();

  // Wait until the second (final) rename callback is posted.
  file_factory->WaitForSomeCallback();
  file_factory->GetAllRenameCallbacks(&callbacks);
  ASSERT_EQ(1u, callbacks.size());

  // Call it.
  callbacks[0].Run();
  callbacks.clear();

  // Confirm download isn't complete yet.
  std::vector<DownloadItem*> items;
  DownloadManagerForShell(shell())->GetAllDownloads(&items);
  EXPECT_EQ(DownloadItem::IN_PROGRESS, items[0]->GetState());

  // Cancel the download; confirm cancel fails anyway.
  ASSERT_EQ(1u, items.size());
  items[0]->Cancel(true);
  EXPECT_EQ(DownloadItem::IN_PROGRESS, items[0]->GetState());
  RunAllPendingInMessageLoop();
  EXPECT_EQ(DownloadItem::IN_PROGRESS, items[0]->GetState());

  MockDownloadItemObserver observer;
  items[0]->AddObserver(&observer);
  EXPECT_CALL(observer, OnDownloadDestroyed(items[0]));

  // Shutdown the download manager.  Mostly this is confirming a lack of
  // crashes.
  DownloadManagerForShell(shell())->Shutdown();
}

// Test resumption with a response that contains strong validators.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, StrongValidators) {
  TestDownloadRequestHandler request_handler;
  TestDownloadRequestHandler::Parameters parameters =
      TestDownloadRequestHandler::Parameters::WithSingleInterruption();
  const TestDownloadRequestHandler::InjectedError interruption =
      parameters.injected_errors.front();
  request_handler.StartServing(parameters);

  DownloadItem* download =
      StartDownloadAndReturnItem(shell(), request_handler.url());
  WaitForInterrupt(download);

  ASSERT_EQ(interruption.offset, download->GetReceivedBytes());
  ASSERT_EQ(parameters.size, download->GetTotalBytes());

  download->Resume();
  WaitForCompletion(download);

  ASSERT_EQ(parameters.size, download->GetReceivedBytes());
  ASSERT_EQ(parameters.size, download->GetTotalBytes());
  ASSERT_NO_FATAL_FAILURE(ReadAndVerifyFileContents(
      parameters.pattern_generator_seed, parameters.size,
      download->GetTargetFilePath()));

  // Characterization risk: The next portion of the test examines the requests
  // that were sent out while downloading our resource. These requests
  // correspond to the requests that were generated by the browser and the
  // downloads system and may change as implementation details change.
  TestDownloadRequestHandler::CompletedRequests requests;
  request_handler.GetCompletedRequestInfo(&requests);

  ASSERT_EQ(2u, requests.size());

  // The first request only transferrs bytes up until the interruption point.
  EXPECT_EQ(interruption.offset, requests[0]->transferred_byte_count);

  // The next request should only have transferred the remainder of the
  // resource.
  EXPECT_EQ(parameters.size - interruption.offset,
            requests[1]->transferred_byte_count);

  std::string value;
  ASSERT_TRUE(requests[1]->request_headers.GetHeader(
      net::HttpRequestHeaders::kIfRange, &value));
  EXPECT_EQ(parameters.etag, value);

  ASSERT_TRUE(requests[1]->request_headers.GetHeader(
      net::HttpRequestHeaders::kRange, &value));
  EXPECT_EQ(base::StringPrintf("bytes=%" PRId64 "-", interruption.offset),
            value);
}

// Resumption should only attempt to contact the final URL if the download has a
// URL chain.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, RedirectBeforeResume) {
  TestDownloadRequestHandler request_handler_1(
      GURL("http://example.com/first-url"));
  request_handler_1.StartServingStaticResponse(
      "HTTP/1.1 302 Redirect\r\n"
      "Location: http://example.com/second-url\r\n"
      "\r\n");

  TestDownloadRequestHandler request_handler_2(
      GURL("http://example.com/second-url"));
  request_handler_2.StartServingStaticResponse(
      "HTTP/1.1 302 Redirect\r\n"
      "Location: http://example.com/third-url\r\n"
      "\r\n");

  TestDownloadRequestHandler request_handler_3(
      GURL("http://example.com/third-url"));
  request_handler_3.StartServingStaticResponse(
      "HTTP/1.1 302 Redirect\r\n"
      "Location: http://example.com/download\r\n"
      "\r\n");

  TestDownloadRequestHandler resumable_request_handler(
      GURL("http://example.com/download"));
  TestDownloadRequestHandler::Parameters parameters =
      TestDownloadRequestHandler::Parameters::WithSingleInterruption();
  resumable_request_handler.StartServing(parameters);

  DownloadItem* download =
      StartDownloadAndReturnItem(shell(), request_handler_1.url());
  WaitForInterrupt(download);

  EXPECT_EQ(4u, download->GetUrlChain().size());
  EXPECT_EQ(request_handler_1.url(), download->GetOriginalUrl());
  EXPECT_EQ(resumable_request_handler.url(), download->GetURL());

  // Now that the download is interrupted, make all intermediate servers return
  // a 404. The only way a resumption request would succeed if the resumption
  // request is sent to the final server in the chain.
  const char k404Response[] = "HTTP/1.1 404 Not found\r\n\r\n";
  request_handler_1.StartServingStaticResponse(k404Response);
  request_handler_2.StartServingStaticResponse(k404Response);
  request_handler_3.StartServingStaticResponse(k404Response);

  download->Resume();
  WaitForCompletion(download);

  ASSERT_NO_FATAL_FAILURE(ReadAndVerifyFileContents(
      parameters.pattern_generator_seed, parameters.size,
      download->GetTargetFilePath()));
}

// If a resumption request results in a redirect, the response should be ignored
// and the download should be marked as interrupted again.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, RedirectWhileResume) {
  TestDownloadRequestHandler request_handler(
      GURL("http://example.com/first-url"));
  TestDownloadRequestHandler::Parameters parameters =
      TestDownloadRequestHandler::Parameters::WithSingleInterruption();
  ++parameters.pattern_generator_seed;
  request_handler.StartServing(parameters);

  // We should never send a request to the decoy. If we do, the request will
  // always succeed, which results in behavior that diverges from what we want,
  // which is for the download to return to being interrupted.
  TestDownloadRequestHandler decoy_request_handler(
      GURL("http://example.com/decoy"));
  decoy_request_handler.StartServing(TestDownloadRequestHandler::Parameters());

  DownloadItem* download =
      StartDownloadAndReturnItem(shell(), request_handler.url());
  WaitForInterrupt(download);

  // Upon resumption, the server starts responding with a redirect. This
  // response should not be accepted.
  request_handler.StartServingStaticResponse(
      "HTTP/1.1 302 Redirect\r\n"
      "Location: http://example.com/decoy\r\n"
      "\r\n");
  download->Resume();
  WaitForInterrupt(download);
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_SERVER_UNREACHABLE,
            download->GetLastReason());

  // Back to the original request handler. Resumption should now succeed, and
  // use the partial data it had prior to the first interruption.
  request_handler.StartServing(parameters);
  download->Resume();
  WaitForCompletion(download);

  ASSERT_EQ(parameters.size, download->GetReceivedBytes());
  ASSERT_EQ(parameters.size, download->GetTotalBytes());
  ASSERT_NO_FATAL_FAILURE(ReadAndVerifyFileContents(
      parameters.pattern_generator_seed, parameters.size,
      download->GetTargetFilePath()));

  // Characterization risk: The next portion of the test examines the requests
  // that were sent out while downloading our resource. These requests
  // correspond to the requests that were generated by the browser and the
  // downloads system and may change as implementation details change.
  TestDownloadRequestHandler::CompletedRequests requests;
  request_handler.GetCompletedRequestInfo(&requests);

  ASSERT_EQ(3u, requests.size());

  // None of the request should have transferred the entire resource. The
  // redirect response shows up as a response with 0 bytes transferred.
  EXPECT_GT(parameters.size, requests[0]->transferred_byte_count);
  EXPECT_EQ(0, requests[1]->transferred_byte_count);
  EXPECT_GT(parameters.size, requests[2]->transferred_byte_count);
}

// If the server response for the resumption request specifies a bad range (i.e.
// not the range that was requested or an invalid or missing Content-Range
// header), then the download should be marked as interrupted again without
// discarding the partial state.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, BadRangeHeader) {
  TestDownloadRequestHandler request_handler;
  TestDownloadRequestHandler::Parameters parameters =
      TestDownloadRequestHandler::Parameters::WithSingleInterruption();
  request_handler.StartServing(parameters);

  DownloadItem* download =
      StartDownloadAndReturnItem(shell(), request_handler.url());
  WaitForInterrupt(download);

  // Upon resumption, the server starts responding with a bad range header.
  request_handler.StartServingStaticResponse(
      "HTTP/1.1 206 Partial Content\r\n"
      "Content-Range: bytes 1000000-2000000/3000000\r\n"
      "\r\n");
  download->Resume();
  WaitForInterrupt(download);
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT,
            download->GetLastReason());

  // Or this time, the server sends a response with an invalid Content-Range
  // header.
  request_handler.StartServingStaticResponse(
      "HTTP/1.1 206 Partial Content\r\n"
      "Content-Range: ooga-booga-booga-booga\r\n"
      "\r\n");
  download->Resume();
  WaitForInterrupt(download);
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT,
            download->GetLastReason());

  // Or no Content-Range header at all.
  request_handler.StartServingStaticResponse(
      "HTTP/1.1 206 Partial Content\r\n"
      "Some-Headers: ooga-booga-booga-booga\r\n"
      "\r\n");
  download->Resume();
  WaitForInterrupt(download);
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT,
            download->GetLastReason());

  // Back to the original request handler. Resumption should now succeed, and
  // use the partial data it had prior to the first interruption.
  request_handler.StartServing(parameters);
  download->Resume();
  WaitForCompletion(download);

  ASSERT_EQ(parameters.size, download->GetReceivedBytes());
  ASSERT_EQ(parameters.size, download->GetTotalBytes());
  ASSERT_NO_FATAL_FAILURE(ReadAndVerifyFileContents(
      parameters.pattern_generator_seed, parameters.size,
      download->GetTargetFilePath()));

  // Characterization risk: The next portion of the test examines the requests
  // that were sent out while downloading our resource. These requests
  // correspond to the requests that were generated by the browser and the
  // downloads system and may change as implementation details change.
  TestDownloadRequestHandler::CompletedRequests requests;
  request_handler.GetCompletedRequestInfo(&requests);

  ASSERT_EQ(5u, requests.size());

  // None of the request should have transferred the entire resource.
  EXPECT_GT(parameters.size, requests[0]->transferred_byte_count);
  EXPECT_EQ(0, requests[1]->transferred_byte_count);
  EXPECT_EQ(0, requests[2]->transferred_byte_count);
  EXPECT_EQ(0, requests[3]->transferred_byte_count);
  EXPECT_GT(parameters.size, requests[4]->transferred_byte_count);
}

// A partial resumption results in an HTTP 200 response. I.e. the server ignored
// the range request and sent the entire resource instead. For If-Range requests
// (as opposed to If-Match), the behavior for a precondition failure is also to
// respond with a 200. So this test case covers both validation failure and
// ignoring the range request.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, RestartIfNotPartialResponse) {
  const int kOriginalPatternGeneratorSeed = 1;
  const int kNewPatternGeneratorSeed = 2;

  TestDownloadRequestHandler::Parameters parameters =
      TestDownloadRequestHandler::Parameters::WithSingleInterruption();
  parameters.pattern_generator_seed = kOriginalPatternGeneratorSeed;
  const TestDownloadRequestHandler::InjectedError interruption =
      parameters.injected_errors.front();

  TestDownloadRequestHandler request_handler;
  request_handler.StartServing(parameters);

  DownloadItem* download =
      StartDownloadAndReturnItem(shell(), request_handler.url());
  WaitForInterrupt(download);

  ASSERT_EQ(interruption.offset, download->GetReceivedBytes());
  ASSERT_EQ(parameters.size, download->GetTotalBytes());

  parameters = TestDownloadRequestHandler::Parameters();
  parameters.support_byte_ranges = false;
  parameters.pattern_generator_seed = kNewPatternGeneratorSeed;
  request_handler.StartServing(parameters);

  download->Resume();
  WaitForCompletion(download);

  ASSERT_EQ(parameters.size, download->GetReceivedBytes());
  ASSERT_EQ(parameters.size, download->GetTotalBytes());
  ASSERT_NO_FATAL_FAILURE(
      ReadAndVerifyFileContents(kNewPatternGeneratorSeed, parameters.size,
                                download->GetTargetFilePath()));

  // When the downloads system sees the full response, it should accept the
  // response without restarting. On the network, we should deterministically
  // see two requests:
  // * The original request which transfers upto our interruption point.
  // * The resumption attempt, which receives the entire entity.
  TestDownloadRequestHandler::CompletedRequests requests;
  request_handler.GetCompletedRequestInfo(&requests);

  ASSERT_EQ(2u, requests.size());

  // The first request only transfers data up to the interruption point.
  EXPECT_EQ(interruption.offset, requests[0]->transferred_byte_count);

  // The second request transfers the entire response.
  EXPECT_EQ(parameters.size, requests[1]->transferred_byte_count);

  std::string value;
  ASSERT_TRUE(requests[1]->request_headers.GetHeader(
      net::HttpRequestHeaders::kIfRange, &value));
  EXPECT_EQ(parameters.etag, value);

  ASSERT_TRUE(requests[1]->request_headers.GetHeader(
      net::HttpRequestHeaders::kRange, &value));
  EXPECT_EQ(base::StringPrintf("bytes=%" PRId64 "-", interruption.offset),
            value);
}

// Confirm we restart if we don't have a verifier.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, RestartIfNoETag) {
  const int kOriginalPatternGeneratorSeed = 1;
  const int kNewPatternGeneratorSeed = 2;

  TestDownloadRequestHandler::Parameters parameters =
      TestDownloadRequestHandler::Parameters::WithSingleInterruption();
  ASSERT_EQ(1u, parameters.injected_errors.size());
  parameters.etag.clear();
  parameters.pattern_generator_seed = kOriginalPatternGeneratorSeed;

  TestDownloadRequestHandler request_handler;
  request_handler.StartServing(parameters);
  DownloadItem* download =
      StartDownloadAndReturnItem(shell(), request_handler.url());
  WaitForInterrupt(download);

  parameters.pattern_generator_seed = kNewPatternGeneratorSeed;
  parameters.ClearInjectedErrors();
  request_handler.StartServing(parameters);

  download->Resume();
  WaitForCompletion(download);

  ASSERT_EQ(parameters.size, download->GetReceivedBytes());
  ASSERT_EQ(parameters.size, download->GetTotalBytes());
  ASSERT_NO_FATAL_FAILURE(
      ReadAndVerifyFileContents(kNewPatternGeneratorSeed, parameters.size,
                                download->GetTargetFilePath()));

  TestDownloadRequestHandler::CompletedRequests requests;
  request_handler.GetCompletedRequestInfo(&requests);

  // Neither If-Range nor Range headers should be present in the second request.
  ASSERT_EQ(2u, requests.size());
  std::string value;
  EXPECT_FALSE(requests[1]->request_headers.GetHeader(
      net::HttpRequestHeaders::kIfRange, &value));
  EXPECT_FALSE(requests[1]->request_headers.GetHeader(
      net::HttpRequestHeaders::kRange, &value));
}

// Partial file goes missing before the download is resumed. The download should
// restart.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, RestartIfNoPartialFile) {
  TestDownloadRequestHandler::Parameters parameters =
      TestDownloadRequestHandler::Parameters::WithSingleInterruption();

  TestDownloadRequestHandler request_handler;
  request_handler.StartServing(parameters);
  DownloadItem* download =
      StartDownloadAndReturnItem(shell(), request_handler.url());
  WaitForInterrupt(download);

  // Delete the intermediate file.
  ASSERT_TRUE(base::PathExists(download->GetFullPath()));
  ASSERT_TRUE(base::DeleteFile(download->GetFullPath(), false));

  parameters.ClearInjectedErrors();
  request_handler.StartServing(parameters);

  download->Resume();
  WaitForCompletion(download);

  ASSERT_EQ(parameters.size, download->GetReceivedBytes());
  ASSERT_EQ(parameters.size, download->GetTotalBytes());
  ASSERT_NO_FATAL_FAILURE(ReadAndVerifyFileContents(
      parameters.pattern_generator_seed, parameters.size,
      download->GetTargetFilePath()));
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, RecoverFromInitFileError) {
  TestDownloadRequestHandler request_handler;
  request_handler.StartServing(TestDownloadRequestHandler::Parameters());

  // Setup the error injector.
  scoped_refptr<TestFileErrorInjector> injector(
      TestFileErrorInjector::Create(DownloadManagerForShell(shell())));

  const TestFileErrorInjector::FileErrorInfo err = {
      TestFileErrorInjector::FILE_OPERATION_INITIALIZE, 0,
      DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE};
  injector->InjectError(err);

  // Start and watch for interrupt.
  DownloadItem* download(
      StartDownloadAndReturnItem(shell(), request_handler.url()));
  WaitForInterrupt(download);
  ASSERT_EQ(DownloadItem::INTERRUPTED, download->GetState());
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE,
            download->GetLastReason());
  EXPECT_EQ(0, download->GetReceivedBytes());
  EXPECT_TRUE(download->GetFullPath().empty());
  EXPECT_FALSE(download->GetTargetFilePath().empty());

  // We need to make sure that any cross-thread downloads communication has
  // quiesced before clearing and injecting the new errors, as the
  // InjectErrors() routine alters the currently in use download file
  // factory, which is a file thread object.
  RunAllPendingInMessageLoop(BrowserThread::FILE);
  RunAllPendingInMessageLoop();

  // Clear the old errors list.
  injector->ClearError();

  // Resume and watch completion.
  download->Resume();
  WaitForCompletion(download);
  EXPECT_EQ(download->GetState(), DownloadItem::COMPLETE);
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest,
                       RecoverFromIntermediateFileRenameError) {
  TestDownloadRequestHandler request_handler;
  request_handler.StartServing(TestDownloadRequestHandler::Parameters());

  // Setup the error injector.
  scoped_refptr<TestFileErrorInjector> injector(
      TestFileErrorInjector::Create(DownloadManagerForShell(shell())));

  const TestFileErrorInjector::FileErrorInfo err = {
      TestFileErrorInjector::FILE_OPERATION_RENAME_UNIQUIFY, 0,
      DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE};
  injector->InjectError(err);

  // Start and watch for interrupt.
  DownloadItem* download(
      StartDownloadAndReturnItem(shell(), request_handler.url()));
  WaitForInterrupt(download);
  ASSERT_EQ(DownloadItem::INTERRUPTED, download->GetState());
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE,
            download->GetLastReason());
  EXPECT_TRUE(download->GetFullPath().empty());
  // Target path will have been set after file name determination. GetFullPath()
  // being empty is sufficient to signal that filename determination needs to be
  // redone.
  EXPECT_FALSE(download->GetTargetFilePath().empty());

  // We need to make sure that any cross-thread downloads communication has
  // quiesced before clearing and injecting the new errors, as the
  // InjectErrors() routine alters the currently in use download file
  // factory, which is a file thread object.
  RunAllPendingInMessageLoop(BrowserThread::FILE);
  RunAllPendingInMessageLoop();

  // Clear the old errors list.
  injector->ClearError();

  download->Resume();
  WaitForCompletion(download);
  EXPECT_EQ(download->GetState(), DownloadItem::COMPLETE);
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, RecoverFromFinalRenameError) {
  TestDownloadRequestHandler request_handler;
  request_handler.StartServing(TestDownloadRequestHandler::Parameters());

  // Setup the error injector.
  scoped_refptr<TestFileErrorInjector> injector(
      TestFileErrorInjector::Create(DownloadManagerForShell(shell())));

  DownloadManagerForShell(shell())->RemoveAllDownloads();
  TestFileErrorInjector::FileErrorInfo err = {
      TestFileErrorInjector::FILE_OPERATION_RENAME_ANNOTATE, 0,
      DOWNLOAD_INTERRUPT_REASON_FILE_FAILED};
  injector->InjectError(err);

  // Start and watch for interrupt.
  DownloadItem* download(
      StartDownloadAndReturnItem(shell(), request_handler.url()));
  WaitForInterrupt(download);
  ASSERT_EQ(DownloadItem::INTERRUPTED, download->GetState());
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_FILE_FAILED, download->GetLastReason());
  EXPECT_TRUE(download->GetFullPath().empty());
  // Target path should still be intact.
  EXPECT_FALSE(download->GetTargetFilePath().empty());

  // We need to make sure that any cross-thread downloads communication has
  // quiesced before clearing and injecting the new errors, as the
  // InjectErrors() routine alters the currently in use download file
  // factory, which is a file thread object.
  RunAllPendingInMessageLoop(BrowserThread::FILE);
  RunAllPendingInMessageLoop();

  // Clear the old errors list.
  injector->ClearError();

  download->Resume();
  WaitForCompletion(download);
  EXPECT_EQ(download->GetState(), DownloadItem::COMPLETE);
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, Resume_Hash) {
  using InjectedError = TestDownloadRequestHandler::InjectedError;
  const char kExpectedHash[] =
      "\xa7\x44\x49\x86\x24\xc6\x84\x6c\x89\xdf\xd8\xec\xa0\xe0\x61\x12\xdc\x80"
      "\x13\xf2\x83\x49\xa9\x14\x52\x32\xf0\x95\x20\xca\x5b\x30";
  std::string expected_hash(kExpectedHash);
  TestDownloadRequestHandler request_handler;
  TestDownloadRequestHandler::Parameters parameters;

  // As a control, let's try GetHash() on an uninterrupted download.
  request_handler.StartServing(parameters);
  DownloadItem* uninterrupted_download(
      StartDownloadAndReturnItem(shell(), request_handler.url()));
  WaitForCompletion(uninterrupted_download);
  EXPECT_EQ(expected_hash, uninterrupted_download->GetHash());

  // Now with interruptions.
  parameters.injected_errors.push(
      InjectedError(100, net::ERR_CONNECTION_RESET));
  parameters.injected_errors.push(
      InjectedError(211, net::ERR_CONNECTION_RESET));
  parameters.injected_errors.push(
      InjectedError(337, net::ERR_CONNECTION_RESET));
  parameters.injected_errors.push(
      InjectedError(400, net::ERR_CONNECTION_RESET));
  parameters.injected_errors.push(
      InjectedError(512, net::ERR_CONNECTION_RESET));
  request_handler.StartServing(parameters);

  // Start and watch for interrupt.
  DownloadItem* download(
      StartDownloadAndReturnItem(shell(), request_handler.url()));
  WaitForInterrupt(download);

  download->Resume();
  WaitForInterrupt(download);

  download->Resume();
  WaitForInterrupt(download);

  download->Resume();
  WaitForInterrupt(download);

  download->Resume();
  WaitForInterrupt(download);

  download->Resume();
  WaitForCompletion(download);

  EXPECT_EQ(expected_hash, download->GetHash());
}

// An interrupted download should remove the intermediate file when it is
// cancelled.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, CancelInterruptedDownload) {
  TestDownloadRequestHandler request_handler;
  request_handler.StartServing(
      TestDownloadRequestHandler::Parameters::WithSingleInterruption());

  DownloadItem* download =
      StartDownloadAndReturnItem(shell(), request_handler.url());
  WaitForInterrupt(download);

  base::FilePath intermediate_path = download->GetFullPath();
  ASSERT_FALSE(intermediate_path.empty());
  ASSERT_TRUE(base::PathExists(intermediate_path));

  download->Cancel(true /* user_cancel */);
  RunAllPendingInMessageLoop(BrowserThread::FILE);
  RunAllPendingInMessageLoop();

  // The intermediate file should now be gone.
  EXPECT_FALSE(base::PathExists(intermediate_path));
  EXPECT_TRUE(download->GetFullPath().empty());
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, RemoveInterruptedDownload) {
  TestDownloadRequestHandler request_handler;
  request_handler.StartServing(
      TestDownloadRequestHandler::Parameters::WithSingleInterruption());

  DownloadItem* download =
      StartDownloadAndReturnItem(shell(), request_handler.url());
  WaitForInterrupt(download);

  base::FilePath intermediate_path = download->GetFullPath();
  ASSERT_FALSE(intermediate_path.empty());
  ASSERT_TRUE(base::PathExists(intermediate_path));

  download->Remove();
  RunAllPendingInMessageLoop(BrowserThread::FILE);
  RunAllPendingInMessageLoop();

  // The intermediate file should now be gone.
  EXPECT_FALSE(base::PathExists(intermediate_path));
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, RemoveCompletedDownload) {
  // A completed download shouldn't delete the downloaded file when it is
  // removed.
  TestDownloadRequestHandler request_handler;
  request_handler.StartServing(TestDownloadRequestHandler::Parameters());
  std::unique_ptr<DownloadTestObserver> completion_observer(
      CreateWaiter(shell(), 1));
  DownloadItem* download(
      StartDownloadAndReturnItem(shell(), request_handler.url()));
  completion_observer->WaitForFinished();

  // The target path should exist.
  base::FilePath target_path(download->GetTargetFilePath());
  EXPECT_TRUE(base::PathExists(target_path));
  download->Remove();
  RunAllPendingInMessageLoop(BrowserThread::FILE);
  RunAllPendingInMessageLoop();

  // The file should still exist.
  EXPECT_TRUE(base::PathExists(target_path));
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, RemoveResumingDownload) {
  TestDownloadRequestHandler::Parameters parameters =
      TestDownloadRequestHandler::Parameters::WithSingleInterruption();
  TestDownloadRequestHandler request_handler;
  request_handler.StartServing(parameters);

  DownloadItem* download =
      StartDownloadAndReturnItem(shell(), request_handler.url());
  WaitForInterrupt(download);

  base::FilePath intermediate_path(download->GetFullPath());
  ASSERT_FALSE(intermediate_path.empty());
  EXPECT_TRUE(base::PathExists(intermediate_path));

  // Resume and remove download. We expect only a single OnDownloadCreated()
  // call, and that's for the second download created below.
  MockDownloadManagerObserver dm_observer(DownloadManagerForShell(shell()));
  EXPECT_CALL(dm_observer, OnDownloadCreated(_,_)).Times(1);

  TestRequestStartHandler request_start_handler;
  parameters.on_start_handler = request_start_handler.GetOnStartHandler();
  request_handler.StartServing(parameters);

  download->Resume();
  request_start_handler.WaitForCallback();

  // At this point, the download resumption request has been sent out, but the
  // reponse hasn't been received yet.
  download->Remove();

  request_start_handler.RespondWith(std::string(), net::OK);

  // The intermediate file should now be gone.
  RunAllPendingInMessageLoop(BrowserThread::FILE);
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(base::PathExists(intermediate_path));

  parameters.ClearInjectedErrors();
  parameters.on_start_handler.Reset();
  request_handler.StartServing(parameters);

  // Start the second download and wait until it's done. This exercises the
  // entire downloads stack and effectively flushes all of our worker threads.
  // We are testing whether the URL request created in the previous
  // DownloadItem::Resume() call reulted in a new download or not.
  NavigateToURLAndWaitForDownload(shell(), request_handler.url(),
                                  DownloadItem::COMPLETE);
  EXPECT_TRUE(EnsureNoPendingDownloads());
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, CancelResumingDownload) {
  TestDownloadRequestHandler::Parameters parameters =
      TestDownloadRequestHandler::Parameters::WithSingleInterruption();
  TestDownloadRequestHandler request_handler;
  request_handler.StartServing(parameters);

  DownloadItem* download =
      StartDownloadAndReturnItem(shell(), request_handler.url());
  WaitForInterrupt(download);

  base::FilePath intermediate_path(download->GetFullPath());
  ASSERT_FALSE(intermediate_path.empty());
  EXPECT_TRUE(base::PathExists(intermediate_path));

  // Resume and cancel download. We expect only a single OnDownloadCreated()
  // call, and that's for the second download created below.
  MockDownloadManagerObserver dm_observer(DownloadManagerForShell(shell()));
  EXPECT_CALL(dm_observer, OnDownloadCreated(_,_)).Times(1);

  TestRequestStartHandler request_start_handler;
  parameters.on_start_handler = request_start_handler.GetOnStartHandler();
  request_handler.StartServing(parameters);

  download->Resume();
  request_start_handler.WaitForCallback();

  // At this point, the download item has initiated a network request for the
  // resumption attempt, but hasn't received a response yet.
  download->Cancel(true /* user_cancel */);

  request_start_handler.RespondWith(std::string(), net::OK);

  // The intermediate file should now be gone.
  RunAllPendingInMessageLoop(BrowserThread::IO);
  RunAllPendingInMessageLoop(BrowserThread::FILE);
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(base::PathExists(intermediate_path));

  parameters.ClearInjectedErrors();
  parameters.on_start_handler.Reset();
  request_handler.StartServing(parameters);

  // Start the second download and wait until it's done. This exercises the
  // entire downloads stack and effectively flushes all of our worker threads.
  // We are testing whether the URL request created in the previous
  // DownloadItem::Resume() call reulted in a new download or not.
  NavigateToURLAndWaitForDownload(shell(), request_handler.url(),
                                  DownloadItem::COMPLETE);
  EXPECT_TRUE(EnsureNoPendingDownloads());
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, RemoveResumedDownload) {
  TestDownloadRequestHandler::Parameters parameters =
      TestDownloadRequestHandler::Parameters::WithSingleInterruption();
  TestDownloadRequestHandler request_handler;
  request_handler.StartServing(parameters);

  DownloadItem* download =
      StartDownloadAndReturnItem(shell(), request_handler.url());
  WaitForInterrupt(download);

  base::FilePath intermediate_path(download->GetFullPath());
  base::FilePath target_path(download->GetTargetFilePath());
  ASSERT_FALSE(intermediate_path.empty());
  EXPECT_TRUE(base::PathExists(intermediate_path));
  EXPECT_FALSE(base::PathExists(target_path));

  // Resume and remove download. We don't expect OnDownloadCreated() calls.
  MockDownloadManagerObserver dm_observer(DownloadManagerForShell(shell()));
  EXPECT_CALL(dm_observer, OnDownloadCreated(_, _)).Times(0);

  download->Resume();
  WaitForInProgress(download);

  download->Remove();

  // The intermediate file should now be gone.
  RunAllPendingInMessageLoop(BrowserThread::FILE);
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(base::PathExists(intermediate_path));
  EXPECT_FALSE(base::PathExists(target_path));
  EXPECT_TRUE(EnsureNoPendingDownloads());
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, CancelResumedDownload) {
  TestDownloadRequestHandler::Parameters parameters =
      TestDownloadRequestHandler::Parameters::WithSingleInterruption();
  TestDownloadRequestHandler request_handler;
  request_handler.StartServing(parameters);

  DownloadItem* download =
      StartDownloadAndReturnItem(shell(), request_handler.url());
  WaitForInterrupt(download);

  base::FilePath intermediate_path(download->GetFullPath());
  base::FilePath target_path(download->GetTargetFilePath());
  ASSERT_FALSE(intermediate_path.empty());
  EXPECT_TRUE(base::PathExists(intermediate_path));
  EXPECT_FALSE(base::PathExists(target_path));

  // Resume and remove download. We don't expect OnDownloadCreated() calls.
  MockDownloadManagerObserver dm_observer(DownloadManagerForShell(shell()));
  EXPECT_CALL(dm_observer, OnDownloadCreated(_, _)).Times(0);

  download->Resume();
  WaitForInProgress(download);

  download->Cancel(true);

  // The intermediate file should now be gone.
  RunAllPendingInMessageLoop(BrowserThread::FILE);
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(base::PathExists(intermediate_path));
  EXPECT_FALSE(base::PathExists(target_path));
  EXPECT_TRUE(EnsureNoPendingDownloads());
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, ResumeRestoredDownload_NoFile) {
  TestDownloadRequestHandler request_handler;
  TestDownloadRequestHandler::Parameters parameters;
  request_handler.StartServing(parameters);

  base::FilePath intermediate_file_path =
      GetDownloadDirectory().AppendASCII("intermediate");
  std::vector<GURL> url_chain;

  const int kIntermediateSize = 1331;
  url_chain.push_back(request_handler.url());

  DownloadItem* download = DownloadManagerForShell(shell())->CreateDownloadItem(
      "F7FB1F59-7DE1-4845-AFDB-8A688F70F583", 1, intermediate_file_path,
      base::FilePath(), url_chain, GURL(), GURL(), GURL(), GURL(),
      "application/octet-stream", "application/octet-stream", base::Time::Now(),
      base::Time(), parameters.etag, std::string(), kIntermediateSize,
      parameters.size, std::string(), DownloadItem::INTERRUPTED,
      DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, false);

  download->Resume();
  WaitForCompletion(download);

  EXPECT_FALSE(base::PathExists(intermediate_file_path));
  ReadAndVerifyFileContents(parameters.pattern_generator_seed,
                            parameters.size,
                            download->GetTargetFilePath());

  TestDownloadRequestHandler::CompletedRequests completed_requests;
  request_handler.GetCompletedRequestInfo(&completed_requests);

  // There will be two requests. The first one is issued optimistically assuming
  // that the intermediate file exists and matches the size expectations set
  // forth in the download metadata (i.e. assuming that a 1331 byte file exists
  // at |intermediate_file_path|.
  //
  // However, once the response is received, DownloadFile will report that the
  // intermediate file doesn't exist and hence the download is marked
  // interrupted again.
  //
  // The second request reads the entire entity.
  //
  // N.b. we can't make any assumptions about how many bytes are transferred by
  // the first request since response data will be bufferred until DownloadFile
  // is done initializing.
  //
  // TODO(asanka): Ideally we'll check that the intermediate file matches
  // expectations prior to issuing the first resumption request.
  ASSERT_EQ(2u, completed_requests.size());
  EXPECT_EQ(parameters.size, completed_requests[1]->transferred_byte_count);
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, ResumeRestoredDownload_NoHash) {
  TestDownloadRequestHandler request_handler;
  TestDownloadRequestHandler::Parameters parameters;
  request_handler.StartServing(parameters);

  base::FilePath intermediate_file_path =
      GetDownloadDirectory().AppendASCII("intermediate");
  std::vector<GURL> url_chain;

  const int kIntermediateSize = 1331;
  std::vector<char> buffer(kIntermediateSize);
  request_handler.GetPatternBytes(
      parameters.pattern_generator_seed, 0, buffer.size(), buffer.data());
  ASSERT_EQ(
      kIntermediateSize,
      base::WriteFile(intermediate_file_path, buffer.data(), buffer.size()));

  url_chain.push_back(request_handler.url());

  DownloadItem* download = DownloadManagerForShell(shell())->CreateDownloadItem(
      "F7FB1F59-7DE1-4845-AFDB-8A688F70F583", 1, intermediate_file_path,
      base::FilePath(), url_chain, GURL(), GURL(), GURL(), GURL(),
      "application/octet-stream", "application/octet-stream", base::Time::Now(),
      base::Time(), parameters.etag, std::string(), kIntermediateSize,
      parameters.size, std::string(), DownloadItem::INTERRUPTED,
      DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, false);

  download->Resume();
  WaitForCompletion(download);

  EXPECT_FALSE(base::PathExists(intermediate_file_path));
  ReadAndVerifyFileContents(parameters.pattern_generator_seed,
                            parameters.size,
                            download->GetTargetFilePath());

  TestDownloadRequestHandler::CompletedRequests completed_requests;
  request_handler.GetCompletedRequestInfo(&completed_requests);

  // There's only one network request issued, and that is for the remainder of
  // the file.
  ASSERT_EQ(1u, completed_requests.size());
  EXPECT_EQ(parameters.size - kIntermediateSize,
            completed_requests[0]->transferred_byte_count);
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest,
                       ResumeRestoredDownload_EtagMismatch) {
  TestDownloadRequestHandler request_handler;
  TestDownloadRequestHandler::Parameters parameters;
  request_handler.StartServing(parameters);

  base::FilePath intermediate_file_path =
      GetDownloadDirectory().AppendASCII("intermediate");
  std::vector<GURL> url_chain;

  const int kIntermediateSize = 1331;
  std::vector<char> buffer(kIntermediateSize);
  request_handler.GetPatternBytes(
      parameters.pattern_generator_seed + 1, 0, buffer.size(), buffer.data());
  ASSERT_EQ(
      kIntermediateSize,
      base::WriteFile(intermediate_file_path, buffer.data(), buffer.size()));

  url_chain.push_back(request_handler.url());

  DownloadItem* download = DownloadManagerForShell(shell())->CreateDownloadItem(
      "F7FB1F59-7DE1-4845-AFDB-8A688F70F583", 1, intermediate_file_path,
      base::FilePath(), url_chain, GURL(), GURL(), GURL(), GURL(),
      "application/octet-stream", "application/octet-stream", base::Time::Now(),
      base::Time(), "fake-etag", std::string(), kIntermediateSize,
      parameters.size, std::string(), DownloadItem::INTERRUPTED,
      DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, false);

  download->Resume();
  WaitForCompletion(download);

  EXPECT_FALSE(base::PathExists(intermediate_file_path));
  ReadAndVerifyFileContents(parameters.pattern_generator_seed,
                            parameters.size,
                            download->GetTargetFilePath());

  TestDownloadRequestHandler::CompletedRequests completed_requests;
  request_handler.GetCompletedRequestInfo(&completed_requests);

  // There's only one network request issued. The If-Range header allows the
  // server to respond with the entire entity in one go. The existing contents
  // of the file should be discarded, and overwritten by the new contents.
  ASSERT_EQ(1u, completed_requests.size());
  EXPECT_EQ(parameters.size, completed_requests[0]->transferred_byte_count);
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest,
                       ResumeRestoredDownload_CorrectHash) {
  TestDownloadRequestHandler request_handler;
  TestDownloadRequestHandler::Parameters parameters;
  request_handler.StartServing(parameters);

  base::FilePath intermediate_file_path =
      GetDownloadDirectory().AppendASCII("intermediate");
  std::vector<GURL> url_chain;

  const int kIntermediateSize = 1331;
  std::vector<char> buffer(kIntermediateSize);
  request_handler.GetPatternBytes(
      parameters.pattern_generator_seed, 0, buffer.size(), buffer.data());
  ASSERT_EQ(
      kIntermediateSize,
      base::WriteFile(intermediate_file_path, buffer.data(), buffer.size()));
  // SHA-256 hash of the pattern bytes in buffer.
  static const uint8_t kPartialHash[] = {
      0x77, 0x14, 0xfd, 0x83, 0x06, 0x15, 0x10, 0x7a, 0x47, 0x15, 0xd3,
      0xcf, 0xdd, 0x46, 0xa2, 0x61, 0x96, 0xff, 0xc3, 0xbb, 0x49, 0x30,
      0xaf, 0x31, 0x3a, 0x64, 0x0b, 0xd5, 0xfa, 0xb1, 0xe3, 0x81};

  url_chain.push_back(request_handler.url());

  DownloadItem* download = DownloadManagerForShell(shell())->CreateDownloadItem(
      "F7FB1F59-7DE1-4845-AFDB-8A688F70F583", 1, intermediate_file_path,
      base::FilePath(), url_chain, GURL(), GURL(), GURL(), GURL(),
      "application/octet-stream", "application/octet-stream", base::Time::Now(),
      base::Time(), parameters.etag, std::string(), kIntermediateSize,
      parameters.size,
      std::string(std::begin(kPartialHash), std::end(kPartialHash)),
      DownloadItem::INTERRUPTED, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, false);

  download->Resume();
  WaitForCompletion(download);

  EXPECT_FALSE(base::PathExists(intermediate_file_path));
  ReadAndVerifyFileContents(parameters.pattern_generator_seed,
                            parameters.size,
                            download->GetTargetFilePath());

  TestDownloadRequestHandler::CompletedRequests completed_requests;
  request_handler.GetCompletedRequestInfo(&completed_requests);

  // There's only one network request issued, and that is for the remainder of
  // the file.
  ASSERT_EQ(1u, completed_requests.size());
  EXPECT_EQ(parameters.size - kIntermediateSize,
            completed_requests[0]->transferred_byte_count);

  // SHA-256 hash of the entire 102400 bytes in the target file.
  static const uint8_t kFullHash[] = {
      0xa7, 0x44, 0x49, 0x86, 0x24, 0xc6, 0x84, 0x6c, 0x89, 0xdf, 0xd8,
      0xec, 0xa0, 0xe0, 0x61, 0x12, 0xdc, 0x80, 0x13, 0xf2, 0x83, 0x49,
      0xa9, 0x14, 0x52, 0x32, 0xf0, 0x95, 0x20, 0xca, 0x5b, 0x30};
  EXPECT_EQ(std::string(std::begin(kFullHash), std::end(kFullHash)),
            download->GetHash());
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, ResumeRestoredDownload_WrongHash) {
  TestDownloadRequestHandler request_handler;
  TestDownloadRequestHandler::Parameters parameters;
  request_handler.StartServing(parameters);

  base::FilePath intermediate_file_path =
      GetDownloadDirectory().AppendASCII("intermediate");
  std::vector<GURL> url_chain;

  const int kIntermediateSize = 1331;
  std::vector<char> buffer(kIntermediateSize);
  ASSERT_EQ(
      kIntermediateSize,
      base::WriteFile(intermediate_file_path, buffer.data(), buffer.size()));
  // SHA-256 hash of the expected pattern bytes in buffer. This doesn't match
  // the current contents of the intermediate file which should all be 0.
  static const uint8_t kPartialHash[] = {
      0x77, 0x14, 0xfd, 0x83, 0x06, 0x15, 0x10, 0x7a, 0x47, 0x15, 0xd3,
      0xcf, 0xdd, 0x46, 0xa2, 0x61, 0x96, 0xff, 0xc3, 0xbb, 0x49, 0x30,
      0xaf, 0x31, 0x3a, 0x64, 0x0b, 0xd5, 0xfa, 0xb1, 0xe3, 0x81};

  url_chain.push_back(request_handler.url());

  DownloadItem* download = DownloadManagerForShell(shell())->CreateDownloadItem(
      "F7FB1F59-7DE1-4845-AFDB-8A688F70F583", 1, intermediate_file_path,
      base::FilePath(), url_chain, GURL(), GURL(), GURL(), GURL(),
      "application/octet-stream", "application/octet-stream", base::Time::Now(),
      base::Time(), parameters.etag, std::string(), kIntermediateSize,
      parameters.size,
      std::string(std::begin(kPartialHash), std::end(kPartialHash)),
      DownloadItem::INTERRUPTED, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, false);

  download->Resume();
  WaitForCompletion(download);

  EXPECT_FALSE(base::PathExists(intermediate_file_path));
  ReadAndVerifyFileContents(parameters.pattern_generator_seed,
                            parameters.size,
                            download->GetTargetFilePath());

  TestDownloadRequestHandler::CompletedRequests completed_requests;
  request_handler.GetCompletedRequestInfo(&completed_requests);

  // There will be two requests. The first one is issued optimistically assuming
  // that the intermediate file exists and matches the size expectations set
  // forth in the download metadata (i.e. assuming that a 1331 byte file exists
  // at |intermediate_file_path|.
  //
  // However, once the response is received, DownloadFile will report that the
  // intermediate file doesn't match the expected hash.
  //
  // The second request reads the entire entity.
  //
  // N.b. we can't make any assumptions about how many bytes are transferred by
  // the first request since response data will be bufferred until DownloadFile
  // is done initializing.
  //
  // TODO(asanka): Ideally we'll check that the intermediate file matches
  // expectations prior to issuing the first resumption request.
  ASSERT_EQ(2u, completed_requests.size());
  EXPECT_EQ(parameters.size, completed_requests[1]->transferred_byte_count);

  // SHA-256 hash of the entire 102400 bytes in the target file.
  static const uint8_t kFullHash[] = {
      0xa7, 0x44, 0x49, 0x86, 0x24, 0xc6, 0x84, 0x6c, 0x89, 0xdf, 0xd8,
      0xec, 0xa0, 0xe0, 0x61, 0x12, 0xdc, 0x80, 0x13, 0xf2, 0x83, 0x49,
      0xa9, 0x14, 0x52, 0x32, 0xf0, 0x95, 0x20, 0xca, 0x5b, 0x30};
  EXPECT_EQ(std::string(std::begin(kFullHash), std::end(kFullHash)),
            download->GetHash());
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, ResumeRestoredDownload_ShortFile) {
  TestDownloadRequestHandler request_handler;
  TestDownloadRequestHandler::Parameters parameters;
  request_handler.StartServing(parameters);

  base::FilePath intermediate_file_path =
      GetDownloadDirectory().AppendASCII("intermediate");
  std::vector<GURL> url_chain;

  const int kIntermediateSize = 1331;
  // Size of file is slightly shorter than the size known to DownloadItem.
  std::vector<char> buffer(kIntermediateSize - 100);
  request_handler.GetPatternBytes(
      parameters.pattern_generator_seed, 0, buffer.size(), buffer.data());
  ASSERT_EQ(
      kIntermediateSize - 100,
      base::WriteFile(intermediate_file_path, buffer.data(), buffer.size()));
  url_chain.push_back(request_handler.url());

  DownloadItem* download = DownloadManagerForShell(shell())->CreateDownloadItem(
      "F7FB1F59-7DE1-4845-AFDB-8A688F70F583", 1, intermediate_file_path,
      base::FilePath(), url_chain, GURL(), GURL(), GURL(), GURL(),
      "application/octet-stream", "application/octet-stream", base::Time::Now(),
      base::Time(), parameters.etag, std::string(), kIntermediateSize,
      parameters.size, std::string(), DownloadItem::INTERRUPTED,
      DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, false);

  download->Resume();
  WaitForCompletion(download);

  EXPECT_FALSE(base::PathExists(intermediate_file_path));
  ReadAndVerifyFileContents(parameters.pattern_generator_seed,
                            parameters.size,
                            download->GetTargetFilePath());

  TestDownloadRequestHandler::CompletedRequests completed_requests;
  request_handler.GetCompletedRequestInfo(&completed_requests);

  // There will be two requests. The first one is issued optimistically assuming
  // that the intermediate file exists and matches the size expectations set
  // forth in the download metadata (i.e. assuming that a 1331 byte file exists
  // at |intermediate_file_path|.
  //
  // However, once the response is received, DownloadFile will report that the
  // intermediate file is too short and hence the download is marked interrupted
  // again.
  //
  // The second request reads the entire entity.
  //
  // N.b. we can't make any assumptions about how many bytes are transferred by
  // the first request since response data will be bufferred until DownloadFile
  // is done initializing.
  //
  // TODO(asanka): Ideally we'll check that the intermediate file matches
  // expectations prior to issuing the first resumption request.
  ASSERT_EQ(2u, completed_requests.size());
  EXPECT_EQ(parameters.size, completed_requests[1]->transferred_byte_count);
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, ResumeRestoredDownload_LongFile) {
  // These numbers are sufficiently large that the intermediate file won't be
  // read in a single Read().
  const int kFileSize = 1024 * 1024;
  const int kIntermediateSize = kFileSize / 2 + 111;

  TestDownloadRequestHandler request_handler;
  TestDownloadRequestHandler::Parameters parameters;
  parameters.size = kFileSize;
  request_handler.StartServing(parameters);

  base::FilePath intermediate_file_path =
      GetDownloadDirectory().AppendASCII("intermediate");
  std::vector<GURL> url_chain;

  // Size of file is slightly longer than the size known to DownloadItem.
  std::vector<char> buffer(kIntermediateSize + 100);
  request_handler.GetPatternBytes(
      parameters.pattern_generator_seed, 0, buffer.size(), buffer.data());
  ASSERT_EQ(
      kIntermediateSize + 100,
      base::WriteFile(intermediate_file_path, buffer.data(), buffer.size()));
  url_chain.push_back(request_handler.url());

  DownloadItem* download = DownloadManagerForShell(shell())->CreateDownloadItem(
      "F7FB1F59-7DE1-4845-AFDB-8A688F70F583", 1, intermediate_file_path,
      base::FilePath(), url_chain, GURL(), GURL(), GURL(), GURL(),
      "application/octet-stream", "application/octet-stream", base::Time::Now(),
      base::Time(), parameters.etag, std::string(), kIntermediateSize,
      parameters.size, std::string(), DownloadItem::INTERRUPTED,
      DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, false);

  download->Resume();
  WaitForCompletion(download);

  EXPECT_FALSE(base::PathExists(intermediate_file_path));
  ReadAndVerifyFileContents(parameters.pattern_generator_seed,
                            parameters.size,
                            download->GetTargetFilePath());

  TestDownloadRequestHandler::CompletedRequests completed_requests;
  request_handler.GetCompletedRequestInfo(&completed_requests);

  // There should be only one request. The intermediate file should be truncated
  // to the expected size, and the request should be issued for the remainder.
  //
  // TODO(asanka): Ideally we'll check that the intermediate file matches
  // expectations prior to issuing the first resumption request.
  ASSERT_EQ(1u, completed_requests.size());
  EXPECT_EQ(parameters.size - kIntermediateSize,
            completed_requests[0]->transferred_byte_count);
}

// Test that the referrer header is set correctly for a download that's resumed
// partially.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, ReferrerForPartialResumption) {
  TestDownloadRequestHandler request_handler;
  TestDownloadRequestHandler::Parameters parameters =
      TestDownloadRequestHandler::Parameters::WithSingleInterruption();
  request_handler.StartServing(parameters);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL document_url = embedded_test_server()->GetURL(
      std::string("/download/download-link.html?dl=")
          .append(request_handler.url().spec()));

  DownloadItem* download = StartDownloadAndReturnItem(shell(), document_url);
  WaitForInterrupt(download);

  download->Resume();
  WaitForCompletion(download);

  ASSERT_EQ(parameters.size, download->GetReceivedBytes());
  ASSERT_EQ(parameters.size, download->GetTotalBytes());
  ASSERT_NO_FATAL_FAILURE(ReadAndVerifyFileContents(
      parameters.pattern_generator_seed, parameters.size,
      download->GetTargetFilePath()));

  TestDownloadRequestHandler::CompletedRequests requests;
  request_handler.GetCompletedRequestInfo(&requests);

  ASSERT_GE(2u, requests.size());
  EXPECT_EQ(document_url.spec(), requests.back()->referrer);
}

// Check that the cookie policy is correctly updated when downloading a file
// that redirects cross origin.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, CookiePolicy) {
  net::EmbeddedTestServer origin_one;
  net::EmbeddedTestServer origin_two;
  ASSERT_TRUE(origin_one.Start());
  ASSERT_TRUE(origin_two.Start());

  // Block third-party cookies.
  ShellNetworkDelegate::SetAcceptAllCookies(false);

  // |url| redirects to a different origin |download| which tries to set a
  // cookie.
  base::StringPairs cookie_header;
  cookie_header.push_back(
      std::make_pair(std::string("Set-Cookie"), std::string("A=B")));
  origin_one.RegisterRequestHandler(CreateBasicResponseHandler(
      "/foo", cookie_header, "application/octet-stream", "abcd"));
  origin_two.RegisterRequestHandler(
      CreateRedirectHandler("/bar", origin_one.GetURL("/foo")));

  // Download the file.
  SetupEnsureNoPendingDownloads();
  std::unique_ptr<DownloadUrlParameters> download_parameters(
      DownloadUrlParameters::CreateForWebContentsMainFrame(
          shell()->web_contents(), origin_two.GetURL("/bar")));
  std::unique_ptr<DownloadTestObserver> observer(CreateWaiter(shell(), 1));
  DownloadManagerForShell(shell())->DownloadUrl(std::move(download_parameters));
  observer->WaitForFinished();

  // Get the important info from other threads and check it.
  EXPECT_TRUE(EnsureNoPendingDownloads());

  std::vector<DownloadItem*> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  ASSERT_EQ(DownloadItem::COMPLETE, downloads[0]->GetState());

  // Check that the cookies were correctly set.
  EXPECT_EQ("A=B",
            content::GetCookies(shell()->web_contents()->GetBrowserContext(),
                                origin_one.GetURL("/")));
}

// A filename suggestion specified via a @download attribute should not be
// effective if the final download URL is in another origin from the original
// download URL.
IN_PROC_BROWSER_TEST_F(DownloadContentTest,
                       DownloadAttributeCrossOriginRedirect) {
  net::EmbeddedTestServer origin_one;
  net::EmbeddedTestServer origin_two;
  ASSERT_TRUE(origin_one.Start());
  ASSERT_TRUE(origin_two.Start());

  // The download-attribute.html page contains an anchor element whose href is
  // set to the value of the query parameter (specified as |target| in the URL
  // below). The suggested filename for the anchor is 'suggested-filename'. When
  // the page is loaded, a script simulates a click on the anchor, triggering a
  // download of the target URL.
  //
  // We construct two test servers; origin_one and origin_two. Once started, the
  // server URLs will differ by the port number. Therefore they will be in
  // different origins.
  GURL download_url = origin_one.GetURL("/ping");
  GURL referrer_url = origin_one.GetURL(
      std::string("/download-attribute.html?target=") + download_url.spec());

  // <origin_one>/download-attribute.html initiates a download of
  // <origin_one>/ping, which redirects to <origin_two>/download.
  origin_one.ServeFilesFromDirectory(GetTestFilePath("download", ""));
  origin_one.RegisterRequestHandler(
      CreateRedirectHandler("/ping", origin_two.GetURL("/download")));
  origin_two.RegisterRequestHandler(CreateBasicResponseHandler(
      "/download", base::StringPairs(), "application/octet-stream", "Hello"));

  NavigateToURLAndWaitForDownload(
      shell(), referrer_url, DownloadItem::COMPLETE);

  std::vector<DownloadItem*> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());

  EXPECT_EQ(FILE_PATH_LITERAL("download"),
            downloads[0]->GetTargetFilePath().BaseName().value());
  ASSERT_TRUE(origin_one.ShutdownAndWaitUntilComplete());
  ASSERT_TRUE(origin_two.ShutdownAndWaitUntilComplete());
}

// A filename suggestion specified via a @download attribute should be effective
// if the final download URL is in the same origin as the initial download URL.
// Test that this holds even if there are cross origin redirects in the middle
// of the redirect chain.
IN_PROC_BROWSER_TEST_F(DownloadContentTest,
                       DownloadAttributeSameOriginRedirect) {
  net::EmbeddedTestServer origin_one;
  net::EmbeddedTestServer origin_two;
  ASSERT_TRUE(origin_one.Start());
  ASSERT_TRUE(origin_two.Start());

  // The download-attribute.html page contains an anchor element whose href is
  // set to the value of the query parameter (specified as |target| in the URL
  // below). The suggested filename for the anchor is 'suggested-filename'. When
  // the page is loaded, a script simulates a click on the anchor, triggering a
  // download of the target URL.
  //
  // We construct two test servers; origin_one and origin_two. Once started, the
  // server URLs will differ by the port number. Therefore they will be in
  // different origins.
  GURL download_url = origin_one.GetURL("/ping");
  GURL referrer_url = origin_one.GetURL(
      std::string("/download-attribute.html?target=") + download_url.spec());
  origin_one.ServeFilesFromDirectory(GetTestFilePath("download", ""));

  // <origin_one>/download-attribute.html initiates a download of
  // <origin_one>/ping, which redirects to <origin_two>/pong, and then finally
  // to <origin_one>/download.
  origin_one.RegisterRequestHandler(
      CreateRedirectHandler("/ping", origin_two.GetURL("/pong")));
  origin_two.RegisterRequestHandler(
      CreateRedirectHandler("/pong", origin_one.GetURL("/download")));
  origin_one.RegisterRequestHandler(CreateBasicResponseHandler(
      "/download", base::StringPairs(), "application/octet-stream", "Hello"));

  NavigateToURLAndWaitForDownload(
      shell(), referrer_url, DownloadItem::COMPLETE);

  std::vector<DownloadItem*> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());

  EXPECT_EQ(FILE_PATH_LITERAL("suggested-filename"),
            downloads[0]->GetTargetFilePath().BaseName().value());
  ASSERT_TRUE(origin_one.ShutdownAndWaitUntilComplete());
  ASSERT_TRUE(origin_two.ShutdownAndWaitUntilComplete());
}

// A request for a non-existent resource should still result in a DownloadItem
// that's created in an interrupted state.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, DownloadAttributeServerError) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL download_url =
      embedded_test_server()->GetURL("/download/does-not-exist");
  GURL document_url = embedded_test_server()->GetURL(
      std::string("/download/download-attribute.html?target=") +
      download_url.spec());

  DownloadItem* download = StartDownloadAndReturnItem(shell(), document_url);
  WaitForInterrupt(download);

  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT,
            download->GetLastReason());
}

namespace {

void ErrorReturningRequestHandler(
    const net::HttpRequestHeaders& headers,
    const TestDownloadRequestHandler::OnStartResponseCallback& callback) {
  callback.Run(std::string(), net::ERR_INTERNET_DISCONNECTED);
}

}  // namespace

// A request that fails before it gets a response from the server should also
// result in a DownloadItem that's created in an interrupted state.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, DownloadAttributeNetworkError) {
  ASSERT_TRUE(embedded_test_server()->Start());
  TestDownloadRequestHandler request_handler;
  TestDownloadRequestHandler::Parameters parameters;

  parameters.on_start_handler = base::Bind(&ErrorReturningRequestHandler);
  request_handler.StartServing(parameters);

  GURL document_url = embedded_test_server()->GetURL(
      std::string("/download/download-attribute.html?target=") +
      request_handler.url().spec());
  DownloadItem* download = StartDownloadAndReturnItem(shell(), document_url);
  WaitForInterrupt(download);

  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED,
            download->GetLastReason());
}

// A request that fails due to it being rejected by policy should result in a
// DownloadItem that's marked as interrupted.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, DownloadAttributeInvalidURL) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL document_url = embedded_test_server()->GetURL(
      "/download/download-attribute.html?target=about:version");
  DownloadItem* download = StartDownloadAndReturnItem(shell(), document_url);
  WaitForInterrupt(download);

  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST,
            download->GetLastReason());
  EXPECT_FALSE(download->CanResume());
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, DownloadAttributeBlobURL) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL document_url =
      embedded_test_server()->GetURL("/download/download-attribute-blob.html");
  DownloadItem* download = StartDownloadAndReturnItem(shell(), document_url);
  WaitForCompletion(download);

  EXPECT_STREQ(FILE_PATH_LITERAL("suggested-filename.txt"),
               download->GetTargetFilePath().BaseName().value().c_str());
}

// The file empty.bin is served with a MIME type of application/octet-stream.
// The content body is empty. Make sure this case is handled properly and we
// don't regress on http://crbug.com/320394.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, DownloadGZipWithNoContent) {
  GURL url = net::URLRequestMockHTTPJob::GetMockUrl("empty.bin");
  NavigateToURLAndWaitForDownload(shell(), url, DownloadItem::COMPLETE);
  // That's it. This should work without crashing.
}

// Make sure that sniffed MIME types are correctly passed through to the
// download item.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, SniffedMimeType) {
  GURL url = net::URLRequestMockHTTPJob::GetMockUrl("gzip-content.gz");
  DownloadItem* item = StartDownloadAndReturnItem(shell(), url);
  WaitForCompletion(item);

  EXPECT_STREQ("application/x-gzip", item->GetMimeType().c_str());
  EXPECT_TRUE(item->GetOriginalMimeType().empty());
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, DuplicateContentDisposition) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // double-content-disposition.txt is served with two Content-Disposition
  // headers, both of which are identical.
  NavigateToURLAndWaitForDownload(
      shell(),
      embedded_test_server()->GetURL(
          "/download/double-content-disposition.txt"),
      DownloadItem::COMPLETE);

  std::vector<DownloadItem*> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());

  EXPECT_EQ(FILE_PATH_LITERAL("Jumboshrimp.txt"),
            downloads[0]->GetTargetFilePath().BaseName().value());
}

}  // namespace content
