// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains download browser tests that are known to be runnable
// in a pure content context.  Over time tests should be migrated here.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "content/browser/byte_stream.h"
#include "content/browser/download/download_file_factory.h"
#include "content/browser/download/download_file_impl.h"
#include "content/browser/download/download_item_impl.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/download/download_resource_handler.h"
#include "content/browser/plugin_service_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/power_save_blocker.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_file_error_injector.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_download_manager_delegate.h"
#include "content/shell/browser/shell_network_delegate.h"
#include "content/test/net/url_request_mock_http_job.h"
#include "content/test/net/url_request_slow_download_job.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::net::test_server::EmbeddedTestServer;
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
      scoped_ptr<DownloadSaveInfo> save_info,
      const base::FilePath& default_download_directory,
      const GURL& url,
      const GURL& referrer_url,
      bool calculate_hash,
      scoped_ptr<ByteStreamReader> stream,
      const net::BoundNetLog& bound_net_log,
      scoped_ptr<PowerSaveBlocker> power_save_blocker,
      base::WeakPtr<DownloadDestinationObserver> observer,
      base::WeakPtr<DownloadFileWithDelayFactory> owner);

  virtual ~DownloadFileWithDelay();

  // Wraps DownloadFileImpl::Rename* and intercepts the return callback,
  // storing it in the factory that produced this object for later
  // retrieval.
  virtual void RenameAndUniquify(
      const base::FilePath& full_path,
      const RenameCompletionCallback& callback) OVERRIDE;
  virtual void RenameAndAnnotate(
      const base::FilePath& full_path,
      const RenameCompletionCallback& callback) OVERRIDE;

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
  virtual ~DownloadFileWithDelayFactory();

  // DownloadFileFactory interface.
  virtual DownloadFile* CreateFile(
      scoped_ptr<DownloadSaveInfo> save_info,
      const base::FilePath& default_download_directory,
      const GURL& url,
      const GURL& referrer_url,
      bool calculate_hash,
      scoped_ptr<ByteStreamReader> stream,
      const net::BoundNetLog& bound_net_log,
      base::WeakPtr<DownloadDestinationObserver> observer) OVERRIDE;

  void AddRenameCallback(base::Closure callback);
  void GetAllRenameCallbacks(std::vector<base::Closure>* results);

  // Do not return until GetAllRenameCallbacks() will return a non-empty list.
  void WaitForSomeCallback();

 private:
  base::WeakPtrFactory<DownloadFileWithDelayFactory> weak_ptr_factory_;
  std::vector<base::Closure> rename_callbacks_;
  bool waiting_;

  DISALLOW_COPY_AND_ASSIGN(DownloadFileWithDelayFactory);
};

DownloadFileWithDelay::DownloadFileWithDelay(
    scoped_ptr<DownloadSaveInfo> save_info,
    const base::FilePath& default_download_directory,
    const GURL& url,
    const GURL& referrer_url,
    bool calculate_hash,
    scoped_ptr<ByteStreamReader> stream,
    const net::BoundNetLog& bound_net_log,
    scoped_ptr<PowerSaveBlocker> power_save_blocker,
    base::WeakPtr<DownloadDestinationObserver> observer,
    base::WeakPtr<DownloadFileWithDelayFactory> owner)
    : DownloadFileImpl(
        save_info.Pass(), default_download_directory, url, referrer_url,
        calculate_hash, stream.Pass(), bound_net_log, observer),
      owner_(owner) {}

DownloadFileWithDelay::~DownloadFileWithDelay() {}

void DownloadFileWithDelay::RenameAndUniquify(
    const base::FilePath& full_path,
    const RenameCompletionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DownloadFileImpl::RenameAndUniquify(
      full_path, base::Bind(DownloadFileWithDelay::RenameCallbackWrapper,
                            owner_, callback));
}

void DownloadFileWithDelay::RenameAndAnnotate(
    const base::FilePath& full_path, const RenameCompletionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DownloadFileImpl::RenameAndAnnotate(
      full_path, base::Bind(DownloadFileWithDelay::RenameCallbackWrapper,
                            owner_, callback));
}

// static
void DownloadFileWithDelay::RenameCallbackWrapper(
    const base::WeakPtr<DownloadFileWithDelayFactory>& factory,
    const RenameCompletionCallback& original_callback,
    DownloadInterruptReason reason,
    const base::FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!factory)
    return;
  factory->AddRenameCallback(base::Bind(original_callback, reason, path));
}

DownloadFileWithDelayFactory::DownloadFileWithDelayFactory()
    : weak_ptr_factory_(this),
      waiting_(false) {}
DownloadFileWithDelayFactory::~DownloadFileWithDelayFactory() {}

DownloadFile* DownloadFileWithDelayFactory::CreateFile(
    scoped_ptr<DownloadSaveInfo> save_info,
    const base::FilePath& default_download_directory,
    const GURL& url,
    const GURL& referrer_url,
    bool calculate_hash,
    scoped_ptr<ByteStreamReader> stream,
    const net::BoundNetLog& bound_net_log,
    base::WeakPtr<DownloadDestinationObserver> observer) {
  scoped_ptr<PowerSaveBlocker> psb(
      PowerSaveBlocker::Create(
          PowerSaveBlocker::kPowerSaveBlockPreventAppSuspension,
          "Download in progress"));
  return new DownloadFileWithDelay(
      save_info.Pass(), default_download_directory, url, referrer_url,
      calculate_hash, stream.Pass(), bound_net_log,
      psb.Pass(), observer, weak_ptr_factory_.GetWeakPtr());
}

void DownloadFileWithDelayFactory::AddRenameCallback(base::Closure callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  rename_callbacks_.push_back(callback);
  if (waiting_)
    base::MessageLoopForUI::current()->Quit();
}

void DownloadFileWithDelayFactory::GetAllRenameCallbacks(
    std::vector<base::Closure>* results) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  results->swap(rename_callbacks_);
}

void DownloadFileWithDelayFactory::WaitForSomeCallback() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (rename_callbacks_.empty()) {
    waiting_ = true;
    RunMessageLoop();
    waiting_ = false;
  }
}

class CountingDownloadFile : public DownloadFileImpl {
 public:
  CountingDownloadFile(
    scoped_ptr<DownloadSaveInfo> save_info,
    const base::FilePath& default_downloads_directory,
    const GURL& url,
    const GURL& referrer_url,
    bool calculate_hash,
    scoped_ptr<ByteStreamReader> stream,
    const net::BoundNetLog& bound_net_log,
    scoped_ptr<PowerSaveBlocker> power_save_blocker,
    base::WeakPtr<DownloadDestinationObserver> observer)
      : DownloadFileImpl(save_info.Pass(), default_downloads_directory,
                         url, referrer_url, calculate_hash,
                         stream.Pass(), bound_net_log, observer) {}

  virtual ~CountingDownloadFile() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    active_files_--;
  }

  virtual void Initialize(const InitializeCallback& callback) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    active_files_++;
    return DownloadFileImpl::Initialize(callback);
  }

  static void GetNumberActiveFiles(int* result) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    *result = active_files_;
  }

  // Can be called on any thread, and will block (running message loop)
  // until data is returned.
  static int GetNumberActiveFilesFromFileThread() {
    int result = -1;
    BrowserThread::PostTaskAndReply(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&CountingDownloadFile::GetNumberActiveFiles, &result),
        base::MessageLoop::current()->QuitClosure());
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
  virtual ~CountingDownloadFileFactory() {}

  // DownloadFileFactory interface.
  virtual DownloadFile* CreateFile(
    scoped_ptr<DownloadSaveInfo> save_info,
    const base::FilePath& default_downloads_directory,
    const GURL& url,
    const GURL& referrer_url,
    bool calculate_hash,
    scoped_ptr<ByteStreamReader> stream,
    const net::BoundNetLog& bound_net_log,
    base::WeakPtr<DownloadDestinationObserver> observer) OVERRIDE {
    scoped_ptr<PowerSaveBlocker> psb(
        PowerSaveBlocker::Create(
            PowerSaveBlocker::kPowerSaveBlockPreventAppSuspension,
            "Download in progress"));
    return new CountingDownloadFile(
        save_info.Pass(), default_downloads_directory, url, referrer_url,
        calculate_hash, stream.Pass(), bound_net_log,
        psb.Pass(), observer);
  }
};

class TestShellDownloadManagerDelegate : public ShellDownloadManagerDelegate {
 public:
  TestShellDownloadManagerDelegate()
      : delay_download_open_(false) {}
  virtual ~TestShellDownloadManagerDelegate() {}

  virtual bool ShouldOpenDownload(
      DownloadItem* item,
      const DownloadOpenDelayedCallback& callback) OVERRIDE {
    if (delay_download_open_) {
      delayed_callbacks_.push_back(callback);
      return false;
    }
    return true;
  }

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

// Record all state transitions and byte counts on the observed download.
class RecordingDownloadObserver : DownloadItem::Observer {
 public:
  struct RecordStruct {
    DownloadItem::DownloadState state;
    int bytes_received;
  };

  typedef std::vector<RecordStruct> RecordVector;

  RecordingDownloadObserver(DownloadItem* download)
      : download_(download) {
    last_state_.state = download->GetState();
    last_state_.bytes_received = download->GetReceivedBytes();
    download_->AddObserver(this);
  }

  virtual ~RecordingDownloadObserver() {
    RemoveObserver();
  }

  void CompareToExpectedRecord(const RecordStruct expected[], size_t size) {
    EXPECT_EQ(size, record_.size());
    int min = size > record_.size() ? record_.size() : size;
    for (int i = 0; i < min; ++i) {
      EXPECT_EQ(expected[i].state, record_[i].state) << "Iteration " << i;
      EXPECT_EQ(expected[i].bytes_received, record_[i].bytes_received)
          << "Iteration " << i;
    }
  }

 private:
  virtual void OnDownloadUpdated(DownloadItem* download) OVERRIDE {
    DCHECK_EQ(download_, download);
    DownloadItem::DownloadState state = download->GetState();
    int bytes = download->GetReceivedBytes();
    if (last_state_.state != state || last_state_.bytes_received > bytes) {
      last_state_.state = state;
      last_state_.bytes_received = bytes;
      record_.push_back(last_state_);
    }
  }

  virtual void OnDownloadDestroyed(DownloadItem* download) OVERRIDE {
    DCHECK_EQ(download_, download);
    RemoveObserver();
  }

  void RemoveObserver() {
    if (download_) {
      download_->RemoveObserver(this);
      download_ = NULL;
    }
  }

  DownloadItem* download_;
  RecordStruct last_state_;
  RecordVector record_;
};

// Get the next created download.
class DownloadCreateObserver : DownloadManager::Observer {
 public:
  DownloadCreateObserver(DownloadManager* manager)
      : manager_(manager),
        item_(NULL),
        waiting_(false) {
    manager_->AddObserver(this);
  }

  virtual ~DownloadCreateObserver() {
    if (manager_)
      manager_->RemoveObserver(this);
    manager_ = NULL;
  }

  virtual void ManagerGoingDown(DownloadManager* manager) OVERRIDE {
    DCHECK_EQ(manager_, manager);
    manager_->RemoveObserver(this);
    manager_ = NULL;
  }

  virtual void OnDownloadCreated(DownloadManager* manager,
                                 DownloadItem* download) OVERRIDE {
    if (!item_)
      item_ = download;

    if (waiting_)
      base::MessageLoopForUI::current()->Quit();
  }

  DownloadItem* WaitForFinished() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (!item_) {
      waiting_ = true;
      RunMessageLoop();
      waiting_ = false;
    }
    return item_;
  }

 private:
  DownloadManager* manager_;
  DownloadItem* item_;
  bool waiting_;
};


// Filter for waiting for a certain number of bytes.
bool DataReceivedFilter(int number_of_bytes, DownloadItem* download) {
  return download->GetReceivedBytes() >= number_of_bytes;
}

// Filter for download completion.
bool DownloadCompleteFilter(DownloadItem* download) {
  return download->GetState() == DownloadItem::COMPLETE;
}

// Filter for saving the size of the download when the first IN_PROGRESS
// is hit.
bool InitialSizeFilter(int* download_size, DownloadItem* download) {
  if (download->GetState() != DownloadItem::IN_PROGRESS)
    return false;

  *download_size = download->GetReceivedBytes();
  return true;
}

// Request handler to be used with CreateRedirectHandler().
scoped_ptr<net::test_server::HttpResponse> HandleRequestAndSendRedirectResponse(
    const std::string& relative_url,
    const GURL& target_url,
    const net::test_server::HttpRequest& request) {
  scoped_ptr<net::test_server::BasicHttpResponse> response;
  if (request.relative_url == relative_url) {
    response.reset(new net::test_server::BasicHttpResponse);
    response->set_code(net::HTTP_FOUND);
    response->AddCustomHeader("Location", target_url.spec());
  }
  return response.PassAs<net::test_server::HttpResponse>();
}

// Creates a request handler for EmbeddedTestServer that responds with a HTTP
// 302 redirect if the request URL matches |relative_url|.
EmbeddedTestServer::HandleRequestCallback CreateRedirectHandler(
    const std::string& relative_url,
    const GURL& target_url) {
  return base::Bind(
      &HandleRequestAndSendRedirectResponse, relative_url, target_url);
}

// Request handler to be used with CreateBasicResponseHandler().
scoped_ptr<net::test_server::HttpResponse> HandleRequestAndSendBasicResponse(
    const std::string& relative_url,
    const std::string& content_type,
    const std::string& body,
    const net::test_server::HttpRequest& request) {
  scoped_ptr<net::test_server::BasicHttpResponse> response;
  if (request.relative_url == relative_url) {
    response.reset(new net::test_server::BasicHttpResponse);
    response->set_content_type(content_type);
    response->set_content(body);
  }
  return response.PassAs<net::test_server::HttpResponse>();
}

// Creates a request handler for an EmbeddedTestServer that response with an
// HTTP 200 status code, a Content-Type header and a body.
EmbeddedTestServer::HandleRequestCallback CreateBasicResponseHandler(
    const std::string& relative_url,
    const std::string& content_type,
    const std::string& body) {
  return base::Bind(
      &HandleRequestAndSendBasicResponse, relative_url, content_type, body);
}

}  // namespace

class DownloadContentTest : public ContentBrowserTest {
 protected:
  // An initial send from a website of at least this size will not be
  // help up by buffering in the underlying downloads ByteStream data
  // transfer.  This is important because on resumption tests we wait
  // until we've gotten the data we expect before allowing the test server
  // to send its reset, to get around hard close semantics on the Windows
  // socket layer implementation.
  int GetSafeBufferChunk() const {
    return (DownloadResourceHandler::kDownloadByteStreamSize /
       ByteStreamWriter::kFractionBufferBeforeSending) + 1;
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    ASSERT_TRUE(downloads_directory_.CreateUniqueTempDir());

    test_delegate_.reset(new TestShellDownloadManagerDelegate());
    test_delegate_->SetDownloadBehaviorForTesting(downloads_directory_.path());
    DownloadManager* manager = DownloadManagerForShell(shell());
    manager->GetDelegate()->Shutdown();
    manager->SetDelegate(test_delegate_.get());
    test_delegate_->SetDownloadManager(manager);

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&URLRequestSlowDownloadJob::AddUrlHandler));
    base::FilePath mock_base(GetTestFilePath("download", ""));
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&URLRequestMockHTTPJob::AddUrlHandler, mock_base));
  }

  TestShellDownloadManagerDelegate* GetDownloadManagerDelegate() {
    return test_delegate_.get();
  }

  // Create a DownloadTestObserverTerminal that will wait for the
  // specified number of downloads to finish.
  DownloadTestObserver* CreateWaiter(
      Shell* shell, int num_downloads) {
    DownloadManager* download_manager = DownloadManagerForShell(shell);
    return new DownloadTestObserverTerminal(download_manager, num_downloads,
        DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
  }

  // Create a DownloadTestObserverInProgress that will wait for the
  // specified number of downloads to start.
  DownloadCreateObserver* CreateInProgressWaiter(
      Shell* shell, int num_downloads) {
    DownloadManager* download_manager = DownloadManagerForShell(shell);
    return new DownloadCreateObserver(download_manager);
  }

  DownloadTestObserver* CreateInterruptedWaiter(
      Shell* shell, int num_downloads) {
    DownloadManager* download_manager = DownloadManagerForShell(shell);
    return new DownloadTestObserverInterrupted(download_manager, num_downloads,
        DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
  }

  // Note: Cannot be used with other alternative DownloadFileFactorys
  void SetupEnsureNoPendingDownloads() {
    DownloadManagerForShell(shell())->SetDownloadFileFactoryForTesting(
        scoped_ptr<DownloadFileFactory>(
            new CountingDownloadFileFactory()).Pass());
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
    scoped_ptr<DownloadTestObserver> observer(CreateWaiter(shell, 1));
    NavigateToURL(shell, url);
    observer->WaitForFinished();
    EXPECT_EQ(1u, observer->NumDownloadsSeenInState(expected_terminal_state));
  }

  // Checks that |path| is has |file_size| bytes, and matches the |value|
  // string.
  bool VerifyFile(const base::FilePath& path,
                  const std::string& value,
                  const int64 file_size) {
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
  DownloadItem* StartDownloadAndReturnItem(GURL url) {
    scoped_ptr<DownloadCreateObserver> observer(
        CreateInProgressWaiter(shell(), 1));
    NavigateToURL(shell(), url);
    observer->WaitForFinished();
    std::vector<DownloadItem*> downloads;
    DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
    EXPECT_EQ(1u, downloads.size());
    if (1u != downloads.size())
      return NULL;
    return downloads[0];
  }

  // Wait for data
  void WaitForData(DownloadItem* download, int size) {
    DownloadUpdatedObserver data_observer(
        download, base::Bind(&DataReceivedFilter, size));
    data_observer.WaitForEvent();
    ASSERT_EQ(size, download->GetReceivedBytes());
    ASSERT_EQ(DownloadItem::IN_PROGRESS, download->GetState());
  }

  // Tell the test server to release a pending RST and confirm
  // that the interrupt is received properly (for download resumption
  // testing).
  void ReleaseRSTAndConfirmInterruptForResume(DownloadItem* download) {
    scoped_ptr<DownloadTestObserver> rst_observer(
        CreateInterruptedWaiter(shell(), 1));
    NavigateToURL(shell(), test_server()->GetURL("download-finish"));
    rst_observer->WaitForFinished();
    EXPECT_EQ(DownloadItem::INTERRUPTED, download->GetState());
  }

  // Confirm file status expected for the given location in a stream
  // provided by the resume test server.
  void ConfirmFileStatusForResume(
      DownloadItem* download, bool file_exists,
      int received_bytes, int total_bytes,
      const base::FilePath& expected_filename) {
    // expected_filename is only known if the file exists.
    ASSERT_EQ(file_exists, !expected_filename.empty());
    EXPECT_EQ(received_bytes, download->GetReceivedBytes());
    EXPECT_EQ(total_bytes, download->GetTotalBytes());
    EXPECT_EQ(expected_filename.value(),
              download->GetFullPath().BaseName().value());
    EXPECT_EQ(file_exists,
              (!download->GetFullPath().empty() &&
               base::PathExists(download->GetFullPath())));

    if (file_exists) {
      std::string file_contents;
      EXPECT_TRUE(base::ReadFileToString(
          download->GetFullPath(), &file_contents));

      ASSERT_EQ(static_cast<size_t>(received_bytes), file_contents.size());
      for (int i = 0; i < received_bytes; ++i) {
        EXPECT_EQ(static_cast<char>((i * 2 + 15) % 256), file_contents[i])
            << "File contents diverged at position " << i
            << " for " << expected_filename.value();

        if (static_cast<char>((i * 2 + 15) % 256) != file_contents[i])
          return;
      }
    }
  }

 private:
  static void EnsureNoPendingDownloadJobsOnIO(bool* result) {
    if (URLRequestSlowDownloadJob::NumberOutstandingRequests())
      *result = false;
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE, base::MessageLoop::QuitClosure());
  }

  // Location of the downloads directory for these tests
  base::ScopedTempDir downloads_directory_;
  scoped_ptr<TestShellDownloadManagerDelegate> test_delegate_;
};

IN_PROC_BROWSER_TEST_F(DownloadContentTest, DownloadCancelled) {
  SetupEnsureNoPendingDownloads();

  // Create a download, wait until it's started, and confirm
  // we're in the expected state.
  scoped_ptr<DownloadCreateObserver> observer(
      CreateInProgressWaiter(shell(), 1));
  NavigateToURL(shell(), GURL(URLRequestSlowDownloadJob::kUnknownSizeUrl));
  observer->WaitForFinished();

  std::vector<DownloadItem*> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, downloads[0]->GetState());

  // Cancel the download and wait for download system quiesce.
  downloads[0]->Cancel(true);
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
  scoped_ptr<DownloadCreateObserver> observer1(
      CreateInProgressWaiter(shell(), 1));
  NavigateToURL(shell(), GURL(URLRequestSlowDownloadJob::kUnknownSizeUrl));
  observer1->WaitForFinished();

  std::vector<DownloadItem*> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, downloads[0]->GetState());
  DownloadItem* download1 = downloads[0];  // The only download.

  // Start the second download and wait until it's done.
  base::FilePath file(FILE_PATH_LITERAL("download-test.lib"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));
  // Download the file and wait.
  NavigateToURLAndWaitForDownload(shell(), url, DownloadItem::COMPLETE);

  // Should now have 2 items on the manager.
  downloads.clear();
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  ASSERT_EQ(2u, downloads.size());
  // We don't know the order of the downloads.
  DownloadItem* download2 = downloads[(download1 == downloads[0]) ? 1 : 0];

  ASSERT_EQ(DownloadItem::IN_PROGRESS, download1->GetState());
  ASSERT_EQ(DownloadItem::COMPLETE, download2->GetState());

  // Allow the first request to finish.
  scoped_ptr<DownloadTestObserver> observer2(CreateWaiter(shell(), 1));
  NavigateToURL(shell(), GURL(URLRequestSlowDownloadJob::kFinishDownloadUrl));
  observer2->WaitForFinished();  // Wait for the third request.
  EXPECT_EQ(1u, observer2->NumDownloadsSeenInState(DownloadItem::COMPLETE));

  // Get the important info from other threads and check it.
  EXPECT_TRUE(EnsureNoPendingDownloads());

  // The |DownloadItem|s should now be done and have the final file names.
  // Verify that the files have the expected data and size.
  // |file1| should be full of '*'s, and |file2| should be the same as the
  // source file.
  base::FilePath file1(download1->GetTargetFilePath());
  size_t file_size1 = URLRequestSlowDownloadJob::kFirstDownloadSize +
                      URLRequestSlowDownloadJob::kSecondDownloadSize;
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
  const base::FilePath::CharType kTestFilePath[] =
      FILE_PATH_LITERAL("octet-stream.abc");
  const char kTestPluginName[] = "TestPlugin";
  const char kTestMimeType[] = "application/x-test-mime-type";
  const char kTestFileType[] = "abc";

  WebPluginInfo plugin_info;
  plugin_info.name = base::ASCIIToUTF16(kTestPluginName);
  plugin_info.mime_types.push_back(
      WebPluginMimeType(kTestMimeType, kTestFileType, ""));
  PluginServiceImpl::GetInstance()->RegisterInternalPlugin(plugin_info, false);

  // The following is served with a Content-Type of application/octet-stream.
  GURL url(URLRequestMockHTTPJob::GetMockUrl(base::FilePath(kTestFilePath)));
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
      scoped_ptr<DownloadFileFactory>(file_factory).Pass());

  // Create a download
  base::FilePath file(FILE_PATH_LITERAL("download-test.lib"));
  NavigateToURL(shell(), URLRequestMockHTTPJob::GetMockUrl(file));

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
      scoped_ptr<DownloadFileFactory>(file_factory).Pass());

  // Create a download
  base::FilePath file(FILE_PATH_LITERAL("download-test.lib"));
  NavigateToURL(shell(), URLRequestMockHTTPJob::GetMockUrl(file));

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

// Try to shutdown with a download in progress to make sure shutdown path
// works properly.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, ShutdownInProgress) {
  // Create a download that won't complete.
  scoped_ptr<DownloadCreateObserver> observer(
      CreateInProgressWaiter(shell(), 1));
  NavigateToURL(shell(), GURL(URLRequestSlowDownloadJob::kUnknownSizeUrl));
  observer->WaitForFinished();

  // Get the item.
  std::vector<DownloadItem*> items;
  DownloadManagerForShell(shell())->GetAllDownloads(&items);
  ASSERT_EQ(1u, items.size());
  EXPECT_EQ(DownloadItem::IN_PROGRESS, items[0]->GetState());

  // Shutdown the download manager and make sure we get the right
  // notifications in the right order.
  StrictMock<MockDownloadItemObserver> item_observer;
  items[0]->AddObserver(&item_observer);
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
    EXPECT_CALL(item_observer, OnDownloadUpdated(
        AllOf(items[0],
              Property(&DownloadItem::GetState, DownloadItem::CANCELLED))))
        .WillOnce(Return());
    EXPECT_CALL(item_observer, OnDownloadDestroyed(items[0]))
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
  items.clear();
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
      scoped_ptr<DownloadFileFactory>(file_factory).Pass());

  // Create a download
  base::FilePath file(FILE_PATH_LITERAL("download-test.lib"));
  NavigateToURL(shell(), URLRequestMockHTTPJob::GetMockUrl(file));

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

IN_PROC_BROWSER_TEST_F(DownloadContentTest, ResumeInterruptedDownload) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableDownloadResumption);
  ASSERT_TRUE(test_server()->Start());

  GURL url = test_server()->GetURL(
      base::StringPrintf("rangereset?size=%d&rst_boundary=%d",
                   GetSafeBufferChunk() * 3, GetSafeBufferChunk()));

  MockDownloadManagerObserver dm_observer(DownloadManagerForShell(shell()));
  EXPECT_CALL(dm_observer, OnDownloadCreated(_,_)).Times(1);

  DownloadItem* download(StartDownloadAndReturnItem(url));
  WaitForData(download, GetSafeBufferChunk());
  ::testing::Mock::VerifyAndClearExpectations(&dm_observer);

  // Confirm resumption while in progress doesn't do anything.
  download->Resume();
  ASSERT_EQ(GetSafeBufferChunk(), download->GetReceivedBytes());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, download->GetState());

  // Tell the server to send the RST and confirm the interrupt happens.
  ReleaseRSTAndConfirmInterruptForResume(download);
  ConfirmFileStatusForResume(
      download, true, GetSafeBufferChunk(), GetSafeBufferChunk() * 3,
      base::FilePath(FILE_PATH_LITERAL("rangereset.crdownload")));

  // Resume, confirming received bytes on resumption is correct.
  // Make sure no creation calls are included.
  EXPECT_CALL(dm_observer, OnDownloadCreated(_,_)).Times(0);
  int initial_size = 0;
  DownloadUpdatedObserver initial_size_observer(
      download, base::Bind(&InitialSizeFilter, &initial_size));
  download->Resume();
  initial_size_observer.WaitForEvent();
  EXPECT_EQ(GetSafeBufferChunk(), initial_size);
  ::testing::Mock::VerifyAndClearExpectations(&dm_observer);

  // and wait for expected data.
  WaitForData(download, GetSafeBufferChunk() * 2);

  // Tell the server to send the RST and confirm the interrupt happens.
  ReleaseRSTAndConfirmInterruptForResume(download);
  ConfirmFileStatusForResume(
      download, true, GetSafeBufferChunk() * 2, GetSafeBufferChunk() * 3,
      base::FilePath(FILE_PATH_LITERAL("rangereset.crdownload")));

  // Resume and wait for completion.
  DownloadUpdatedObserver completion_observer(
      download, base::Bind(DownloadCompleteFilter));
  download->Resume();
  completion_observer.WaitForEvent();

  ConfirmFileStatusForResume(
      download, true, GetSafeBufferChunk() * 3, GetSafeBufferChunk() * 3,
      base::FilePath(FILE_PATH_LITERAL("rangereset")));

  // Confirm resumption while complete doesn't do anything.
  download->Resume();
  ASSERT_EQ(GetSafeBufferChunk() * 3, download->GetReceivedBytes());
  ASSERT_EQ(DownloadItem::COMPLETE, download->GetState());
  RunAllPendingInMessageLoop();
  ASSERT_EQ(GetSafeBufferChunk() * 3, download->GetReceivedBytes());
  ASSERT_EQ(DownloadItem::COMPLETE, download->GetState());
}

// Confirm restart fallback happens if a range request is bounced.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, ResumeInterruptedDownloadNoRange) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableDownloadResumption);
  ASSERT_TRUE(test_server()->Start());

  // Auto-restart if server doesn't handle ranges.
  GURL url = test_server()->GetURL(
      base::StringPrintf(
          // First download hits an RST, rest don't, no ranges.
          "rangereset?size=%d&rst_boundary=%d&"
          "token=NoRange&rst_limit=1&bounce_range",
          GetSafeBufferChunk() * 3, GetSafeBufferChunk()));

  // Start the download and wait for first data chunk.
  DownloadItem* download(StartDownloadAndReturnItem(url));
  WaitForData(download, GetSafeBufferChunk());

  RecordingDownloadObserver recorder(download);

  ReleaseRSTAndConfirmInterruptForResume(download);
  ConfirmFileStatusForResume(
      download, true, GetSafeBufferChunk(), GetSafeBufferChunk() * 3,
      base::FilePath(FILE_PATH_LITERAL("rangereset.crdownload")));

  DownloadUpdatedObserver completion_observer(
      download, base::Bind(DownloadCompleteFilter));
  download->Resume();
  completion_observer.WaitForEvent();

  ConfirmFileStatusForResume(
      download, true, GetSafeBufferChunk() * 3, GetSafeBufferChunk() * 3,
      base::FilePath(FILE_PATH_LITERAL("rangereset")));

  static const RecordingDownloadObserver::RecordStruct expected_record[] = {
    // Result of RST
    {DownloadItem::INTERRUPTED, GetSafeBufferChunk()},
    // Starting continuation
    {DownloadItem::IN_PROGRESS, GetSafeBufferChunk()},
    // Notification of receiving whole file.
    {DownloadItem::IN_PROGRESS, 0},
    // Completion.
    {DownloadItem::COMPLETE, GetSafeBufferChunk() * 3},
  };

  recorder.CompareToExpectedRecord(expected_record, arraysize(expected_record));
}

// Confirm restart fallback happens if a precondition is failed.
IN_PROC_BROWSER_TEST_F(DownloadContentTest,
                       ResumeInterruptedDownloadBadPrecondition) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableDownloadResumption);
  ASSERT_TRUE(test_server()->Start());

  GURL url = test_server()->GetURL(base::StringPrintf(
      // First download hits an RST, rest don't, precondition fail.
      "rangereset?size=%d&rst_boundary=%d&"
      "token=BadPrecondition&rst_limit=1&fail_precondition=2",
      GetSafeBufferChunk() * 3,
      GetSafeBufferChunk()));

  // Start the download and wait for first data chunk.
  DownloadItem* download(StartDownloadAndReturnItem(url));
  WaitForData(download, GetSafeBufferChunk());

  RecordingDownloadObserver recorder(download);

  ReleaseRSTAndConfirmInterruptForResume(download);
  ConfirmFileStatusForResume(
      download, true, GetSafeBufferChunk(), GetSafeBufferChunk() * 3,
      base::FilePath(FILE_PATH_LITERAL("rangereset.crdownload")));
  EXPECT_EQ("BadPrecondition2", download->GetETag());

  DownloadUpdatedObserver completion_observer(
      download, base::Bind(DownloadCompleteFilter));
  download->Resume();
  completion_observer.WaitForEvent();

  ConfirmFileStatusForResume(
      download, true, GetSafeBufferChunk() * 3, GetSafeBufferChunk() * 3,
      base::FilePath(FILE_PATH_LITERAL("rangereset")));
  EXPECT_EQ("BadPrecondition0", download->GetETag());

  static const RecordingDownloadObserver::RecordStruct expected_record[] = {
    // Result of RST
    {DownloadItem::INTERRUPTED, GetSafeBufferChunk()},
    // Starting continuation
    {DownloadItem::IN_PROGRESS, GetSafeBufferChunk()},
    // Server precondition fail.
    {DownloadItem::INTERRUPTED, 0},
    // Notification of successful restart.
    {DownloadItem::IN_PROGRESS, 0},
    // Completion.
    {DownloadItem::COMPLETE, GetSafeBufferChunk() * 3},
  };

  recorder.CompareToExpectedRecord(expected_record, arraysize(expected_record));
}

// Confirm we don't try to resume if we don't have a verifier.
IN_PROC_BROWSER_TEST_F(DownloadContentTest,
                       ResumeInterruptedDownloadNoVerifiers) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableDownloadResumption);
  ASSERT_TRUE(test_server()->Start());

  GURL url = test_server()->GetURL(
      base::StringPrintf(
          // First download hits an RST, rest don't, no verifiers.
          "rangereset?size=%d&rst_boundary=%d&"
          "token=NoRange&rst_limit=1&no_verifiers",
          GetSafeBufferChunk() * 3, GetSafeBufferChunk()));

  // Start the download and wait for first data chunk.
  DownloadItem* download(StartDownloadAndReturnItem(url));
  WaitForData(download, GetSafeBufferChunk());

  RecordingDownloadObserver recorder(download);

  ReleaseRSTAndConfirmInterruptForResume(download);
  ConfirmFileStatusForResume(
      download, false, GetSafeBufferChunk(), GetSafeBufferChunk() * 3,
      base::FilePath());

  DownloadUpdatedObserver completion_observer(
      download, base::Bind(DownloadCompleteFilter));
  download->Resume();
  completion_observer.WaitForEvent();

  ConfirmFileStatusForResume(
      download, true, GetSafeBufferChunk() * 3, GetSafeBufferChunk() * 3,
      base::FilePath(FILE_PATH_LITERAL("rangereset")));

  static const RecordingDownloadObserver::RecordStruct expected_record[] = {
    // Result of RST
    {DownloadItem::INTERRUPTED, GetSafeBufferChunk()},
    // Restart for lack of verifiers
    {DownloadItem::IN_PROGRESS, 0},
    // Completion.
    {DownloadItem::COMPLETE, GetSafeBufferChunk() * 3},
  };

  recorder.CompareToExpectedRecord(expected_record, arraysize(expected_record));
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, ResumeWithDeletedFile) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableDownloadResumption);
  ASSERT_TRUE(test_server()->Start());

  GURL url = test_server()->GetURL(
      base::StringPrintf(
          // First download hits an RST, rest don't
          "rangereset?size=%d&rst_boundary=%d&"
          "token=NoRange&rst_limit=1",
          GetSafeBufferChunk() * 3, GetSafeBufferChunk()));

  // Start the download and wait for first data chunk.
  DownloadItem* download(StartDownloadAndReturnItem(url));
  WaitForData(download, GetSafeBufferChunk());

  RecordingDownloadObserver recorder(download);

  ReleaseRSTAndConfirmInterruptForResume(download);
  ConfirmFileStatusForResume(
      download, true, GetSafeBufferChunk(), GetSafeBufferChunk() * 3,
      base::FilePath(FILE_PATH_LITERAL("rangereset.crdownload")));

  // Delete the intermediate file.
  base::DeleteFile(download->GetFullPath(), false);

  DownloadUpdatedObserver completion_observer(
      download, base::Bind(DownloadCompleteFilter));
  download->Resume();
  completion_observer.WaitForEvent();

  ConfirmFileStatusForResume(
      download, true, GetSafeBufferChunk() * 3, GetSafeBufferChunk() * 3,
      base::FilePath(FILE_PATH_LITERAL("rangereset")));

  static const RecordingDownloadObserver::RecordStruct expected_record[] = {
    // Result of RST
    {DownloadItem::INTERRUPTED, GetSafeBufferChunk()},
    // Starting continuation
    {DownloadItem::IN_PROGRESS, GetSafeBufferChunk()},
    // Error because file isn't there.
    {DownloadItem::INTERRUPTED, 0},
    // Restart.
    {DownloadItem::IN_PROGRESS, 0},
    // Completion.
    {DownloadItem::COMPLETE, GetSafeBufferChunk() * 3},
  };

  recorder.CompareToExpectedRecord(expected_record, arraysize(expected_record));
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, ResumeWithFileInitError) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableDownloadResumption);
  base::FilePath file(FILE_PATH_LITERAL("download-test.lib"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));

  // Setup the error injector.
  scoped_refptr<TestFileErrorInjector> injector(
      TestFileErrorInjector::Create(DownloadManagerForShell(shell())));

  TestFileErrorInjector::FileErrorInfo err = {
    url.spec(),
    TestFileErrorInjector::FILE_OPERATION_INITIALIZE,
    0,
    DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE
  };
  injector->AddError(err);
  injector->InjectErrors();

  // Start and watch for interrupt.
  scoped_ptr<DownloadTestObserver> int_observer(
      CreateInterruptedWaiter(shell(), 1));
  DownloadItem* download(StartDownloadAndReturnItem(url));
  int_observer->WaitForFinished();
  ASSERT_EQ(DownloadItem::INTERRUPTED, download->GetState());
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE,
            download->GetLastReason());
  EXPECT_EQ(0, download->GetReceivedBytes());
  EXPECT_TRUE(download->GetFullPath().empty());
  EXPECT_TRUE(download->GetTargetFilePath().empty());

  // We need to make sure that any cross-thread downloads communication has
  // quiesced before clearing and injecting the new errors, as the
  // InjectErrors() routine alters the currently in use download file
  // factory, which is a file thread object.
  RunAllPendingInMessageLoop(BrowserThread::FILE);
  RunAllPendingInMessageLoop();

  // Clear the old errors list.
  injector->ClearErrors();
  injector->InjectErrors();

  // Resume and watch completion.
  DownloadUpdatedObserver completion_observer(
      download, base::Bind(DownloadCompleteFilter));
  download->Resume();
  completion_observer.WaitForEvent();
  EXPECT_EQ(download->GetState(), DownloadItem::COMPLETE);
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest,
                       ResumeWithFileIntermediateRenameError) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableDownloadResumption);
  base::FilePath file(FILE_PATH_LITERAL("download-test.lib"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));

  // Setup the error injector.
  scoped_refptr<TestFileErrorInjector> injector(
      TestFileErrorInjector::Create(DownloadManagerForShell(shell())));

  TestFileErrorInjector::FileErrorInfo err = {
    url.spec(),
    TestFileErrorInjector::FILE_OPERATION_RENAME_UNIQUIFY,
    0,
    DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE
  };
  injector->AddError(err);
  injector->InjectErrors();

  // Start and watch for interrupt.
  scoped_ptr<DownloadTestObserver> int_observer(
      CreateInterruptedWaiter(shell(), 1));
  DownloadItem* download(StartDownloadAndReturnItem(url));
  int_observer->WaitForFinished();
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
  injector->ClearErrors();
  injector->InjectErrors();

  // Resume and watch completion.
  DownloadUpdatedObserver completion_observer(
      download, base::Bind(DownloadCompleteFilter));
  download->Resume();
  completion_observer.WaitForEvent();
  EXPECT_EQ(download->GetState(), DownloadItem::COMPLETE);
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, ResumeWithFileFinalRenameError) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableDownloadResumption);
  base::FilePath file(FILE_PATH_LITERAL("download-test.lib"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));

  // Setup the error injector.
  scoped_refptr<TestFileErrorInjector> injector(
      TestFileErrorInjector::Create(DownloadManagerForShell(shell())));

  DownloadManagerForShell(shell())->RemoveAllDownloads();
  TestFileErrorInjector::FileErrorInfo err = {
    url.spec(),
    TestFileErrorInjector::FILE_OPERATION_RENAME_ANNOTATE,
    0,
    DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE
  };
  injector->AddError(err);
  injector->InjectErrors();

  // Start and watch for interrupt.
  scoped_ptr<DownloadTestObserver> int_observer(
      CreateInterruptedWaiter(shell(), 1));
  DownloadItem* download(StartDownloadAndReturnItem(url));
  int_observer->WaitForFinished();
  ASSERT_EQ(DownloadItem::INTERRUPTED, download->GetState());
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE,
            download->GetLastReason());
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
  injector->ClearErrors();
  injector->InjectErrors();

  // Resume and watch completion.
  DownloadUpdatedObserver completion_observer(
      download, base::Bind(DownloadCompleteFilter));
  download->Resume();
  completion_observer.WaitForEvent();
  EXPECT_EQ(download->GetState(), DownloadItem::COMPLETE);
}

// An interrupted download should remove the intermediate file when it is
// cancelled.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, CancelInterruptedDownload) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableDownloadResumption);
  ASSERT_TRUE(test_server()->Start());

  GURL url1 = test_server()->GetURL(
      base::StringPrintf("rangereset?size=%d&rst_boundary=%d",
                         GetSafeBufferChunk() * 3, GetSafeBufferChunk()));

  DownloadItem* download(StartDownloadAndReturnItem(url1));
  WaitForData(download, GetSafeBufferChunk());

  ReleaseRSTAndConfirmInterruptForResume(download);
  ConfirmFileStatusForResume(
      download, true, GetSafeBufferChunk(), GetSafeBufferChunk() * 3,
      base::FilePath(FILE_PATH_LITERAL("rangereset.crdownload")));

  base::FilePath intermediate_path(download->GetFullPath());
  ASSERT_FALSE(intermediate_path.empty());
  EXPECT_TRUE(base::PathExists(intermediate_path));

  download->Cancel(true /* user_cancel */);
  RunAllPendingInMessageLoop(BrowserThread::FILE);
  RunAllPendingInMessageLoop();

  // The intermediate file should now be gone.
  EXPECT_FALSE(base::PathExists(intermediate_path));
  EXPECT_TRUE(download->GetFullPath().empty());
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, RemoveDownload) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableDownloadResumption);
  ASSERT_TRUE(test_server()->Start());

  // An interrupted download should remove the intermediate file when it is
  // removed.
  {
    GURL url1 = test_server()->GetURL(
        base::StringPrintf("rangereset?size=%d&rst_boundary=%d",
                           GetSafeBufferChunk() * 3, GetSafeBufferChunk()));

    DownloadItem* download(StartDownloadAndReturnItem(url1));
    WaitForData(download, GetSafeBufferChunk());
    ReleaseRSTAndConfirmInterruptForResume(download);
    ConfirmFileStatusForResume(
        download, true, GetSafeBufferChunk(), GetSafeBufferChunk() * 3,
        base::FilePath(FILE_PATH_LITERAL("rangereset.crdownload")));

    base::FilePath intermediate_path(download->GetFullPath());
    ASSERT_FALSE(intermediate_path.empty());
    EXPECT_TRUE(base::PathExists(intermediate_path));

    download->Remove();
    RunAllPendingInMessageLoop(BrowserThread::FILE);
    RunAllPendingInMessageLoop();

    // The intermediate file should now be gone.
    EXPECT_FALSE(base::PathExists(intermediate_path));
  }

  // A completed download shouldn't delete the downloaded file when it is
  // removed.
  {
    // Start the second download and wait until it's done.
    base::FilePath file2(FILE_PATH_LITERAL("download-test.lib"));
    GURL url2(URLRequestMockHTTPJob::GetMockUrl(file2));
    scoped_ptr<DownloadTestObserver> completion_observer(
        CreateWaiter(shell(), 1));
    DownloadItem* download(StartDownloadAndReturnItem(url2));
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
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, RemoveResumingDownload) {
  SetupEnsureNoPendingDownloads();
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableDownloadResumption);
  ASSERT_TRUE(test_server()->Start());

  GURL url = test_server()->GetURL(
      base::StringPrintf("rangereset?size=%d&rst_boundary=%d",
                         GetSafeBufferChunk() * 3, GetSafeBufferChunk()));

  MockDownloadManagerObserver dm_observer(DownloadManagerForShell(shell()));
  EXPECT_CALL(dm_observer, OnDownloadCreated(_,_)).Times(1);

  DownloadItem* download(StartDownloadAndReturnItem(url));
  WaitForData(download, GetSafeBufferChunk());
  ::testing::Mock::VerifyAndClearExpectations(&dm_observer);

  // Tell the server to send the RST and confirm the interrupt happens.
  ReleaseRSTAndConfirmInterruptForResume(download);
  ConfirmFileStatusForResume(
      download, true, GetSafeBufferChunk(), GetSafeBufferChunk() * 3,
      base::FilePath(FILE_PATH_LITERAL("rangereset.crdownload")));

  base::FilePath intermediate_path(download->GetFullPath());
  ASSERT_FALSE(intermediate_path.empty());
  EXPECT_TRUE(base::PathExists(intermediate_path));

  // Resume and remove download. We expect only a single OnDownloadCreated()
  // call, and that's for the second download created below.
  EXPECT_CALL(dm_observer, OnDownloadCreated(_,_)).Times(1);
  download->Resume();
  download->Remove();

  // The intermediate file should now be gone.
  RunAllPendingInMessageLoop(BrowserThread::FILE);
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(base::PathExists(intermediate_path));

  // Start the second download and wait until it's done. The test server is
  // single threaded. The response to this download request should follow the
  // response to the previous resumption request.
  GURL url2(test_server()->GetURL("rangereset?size=100&rst_limit=0&token=x"));
  NavigateToURLAndWaitForDownload(shell(), url2, DownloadItem::COMPLETE);

  EXPECT_TRUE(EnsureNoPendingDownloads());
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, CancelResumingDownload) {
  SetupEnsureNoPendingDownloads();
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableDownloadResumption);
  ASSERT_TRUE(test_server()->Start());

  GURL url = test_server()->GetURL(
      base::StringPrintf("rangereset?size=%d&rst_boundary=%d",
                         GetSafeBufferChunk() * 3, GetSafeBufferChunk()));

  MockDownloadManagerObserver dm_observer(DownloadManagerForShell(shell()));
  EXPECT_CALL(dm_observer, OnDownloadCreated(_,_)).Times(1);

  DownloadItem* download(StartDownloadAndReturnItem(url));
  WaitForData(download, GetSafeBufferChunk());
  ::testing::Mock::VerifyAndClearExpectations(&dm_observer);

  // Tell the server to send the RST and confirm the interrupt happens.
  ReleaseRSTAndConfirmInterruptForResume(download);
  ConfirmFileStatusForResume(
      download, true, GetSafeBufferChunk(), GetSafeBufferChunk() * 3,
      base::FilePath(FILE_PATH_LITERAL("rangereset.crdownload")));

  base::FilePath intermediate_path(download->GetFullPath());
  ASSERT_FALSE(intermediate_path.empty());
  EXPECT_TRUE(base::PathExists(intermediate_path));

  // Resume and cancel download. We expect only a single OnDownloadCreated()
  // call, and that's for the second download created below.
  EXPECT_CALL(dm_observer, OnDownloadCreated(_,_)).Times(1);
  download->Resume();
  download->Cancel(true);

  // The intermediate file should now be gone.
  RunAllPendingInMessageLoop(BrowserThread::FILE);
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(base::PathExists(intermediate_path));
  EXPECT_TRUE(download->GetFullPath().empty());

  // Start the second download and wait until it's done. The test server is
  // single threaded. The response to this download request should follow the
  // response to the previous resumption request.
  GURL url2(test_server()->GetURL("rangereset?size=100&rst_limit=0&token=x"));
  NavigateToURLAndWaitForDownload(shell(), url2, DownloadItem::COMPLETE);

  EXPECT_TRUE(EnsureNoPendingDownloads());
}

// Check that the cookie policy is correctly updated when downloading a file
// that redirects cross origin.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, CookiePolicy) {
  ASSERT_TRUE(test_server()->Start());
  net::HostPortPair host_port = test_server()->host_port_pair();
  DCHECK_EQ(host_port.host(), std::string("127.0.0.1"));

  // Block third-party cookies.
  ShellNetworkDelegate::SetAcceptAllCookies(false);

  // |url| redirects to a different origin |download| which tries to set a
  // cookie.
  std::string download(base::StringPrintf(
      "http://localhost:%d/set-cookie?A=B", host_port.port()));
  GURL url(test_server()->GetURL("server-redirect?" + download));

  // Download the file.
  SetupEnsureNoPendingDownloads();
  scoped_ptr<DownloadUrlParameters> dl_params(
      DownloadUrlParameters::FromWebContents(shell()->web_contents(), url));
  scoped_ptr<DownloadTestObserver> observer(CreateWaiter(shell(), 1));
  DownloadManagerForShell(shell())->DownloadUrl(dl_params.Pass());
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
                                GURL(download)));
}

// A filename suggestion specified via a @download attribute should not be
// effective if the final download URL is in another origin from the original
// download URL.
IN_PROC_BROWSER_TEST_F(DownloadContentTest,
                       DownloadAttributeCrossOriginRedirect) {
  EmbeddedTestServer origin_one;
  EmbeddedTestServer origin_two;
  ASSERT_TRUE(origin_one.InitializeAndWaitUntilReady());
  ASSERT_TRUE(origin_two.InitializeAndWaitUntilReady());

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
      "/download", "application/octet-stream", "Hello"));

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
  EmbeddedTestServer origin_one;
  EmbeddedTestServer origin_two;
  ASSERT_TRUE(origin_one.InitializeAndWaitUntilReady());
  ASSERT_TRUE(origin_two.InitializeAndWaitUntilReady());

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
      "/download", "application/octet-stream", "Hello"));

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

// The file empty.bin is served with a MIME type of application/octet-stream.
// The content body is empty. Make sure this case is handled properly and we
// don't regress on http://crbug.com/320394.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, DownloadGZipWithNoContent) {
  EmbeddedTestServer test_server;
  ASSERT_TRUE(test_server.InitializeAndWaitUntilReady());

  GURL url = test_server.GetURL("/empty.bin");
  test_server.ServeFilesFromDirectory(GetTestFilePath("download", ""));

  NavigateToURLAndWaitForDownload(shell(), url, DownloadItem::COMPLETE);
  // That's it. This should work without crashing.
}

}  // namespace content
