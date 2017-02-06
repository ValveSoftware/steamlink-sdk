// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/crx_downloader.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"
#include "net/url_request/test_url_request_interceptor.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ContentsEqual;

namespace update_client {

namespace {

// Intercepts HTTP GET requests sent to "localhost".
typedef net::LocalHostTestURLRequestInterceptor GetInterceptor;

const char kTestFileName[] = "jebgalgnebhfojomionfpkfelancnnkf.crx";

const char hash_jebg[] =
    "6fc4b93fd11134de1300c2c0bb88c12b644a4ec0fd7c9b12cb7cc067667bde87";

base::FilePath MakeTestFilePath(const char* file) {
  base::FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  return path.AppendASCII("components/test/data/update_client")
      .AppendASCII(file);
}

}  // namespace

class CrxDownloaderTest : public testing::Test {
 public:
  CrxDownloaderTest();
  ~CrxDownloaderTest() override;

  // Overrides from testing::Test.
  void SetUp() override;
  void TearDown() override;

  void Quit();
  void RunThreads();
  void RunThreadsUntilIdle();

  void DownloadComplete(int crx_context, const CrxDownloader::Result& result);

  void DownloadProgress(int crx_context, const CrxDownloader::Result& result);

 protected:
  std::unique_ptr<CrxDownloader> crx_downloader_;

  std::unique_ptr<GetInterceptor> get_interceptor_;

  CrxDownloader::DownloadCallback callback_;
  CrxDownloader::ProgressCallback progress_callback_;

  int crx_context_;

  int num_download_complete_calls_;
  CrxDownloader::Result download_complete_result_;

  // These members are updated by DownloadProgress.
  int num_progress_calls_;
  CrxDownloader::Result download_progress_result_;

  // A magic value for the context to be used in the tests.
  static const int kExpectedContext = 0xaabb;

 private:
  base::MessageLoopForIO loop_;
  scoped_refptr<net::TestURLRequestContextGetter> context_;
  base::Closure quit_closure_;
};

const int CrxDownloaderTest::kExpectedContext;

CrxDownloaderTest::CrxDownloaderTest()
    : callback_(base::Bind(&CrxDownloaderTest::DownloadComplete,
                           base::Unretained(this),
                           kExpectedContext)),
      progress_callback_(base::Bind(&CrxDownloaderTest::DownloadProgress,
                                    base::Unretained(this),
                                    kExpectedContext)),
      crx_context_(0),
      num_download_complete_calls_(0),
      num_progress_calls_(0),
      context_(new net::TestURLRequestContextGetter(
          base::ThreadTaskRunnerHandle::Get())) {
}

CrxDownloaderTest::~CrxDownloaderTest() {
  context_ = NULL;

  // The GetInterceptor requires the message loop to run to destruct correctly.
  get_interceptor_.reset();
  RunThreadsUntilIdle();
}

void CrxDownloaderTest::SetUp() {
  num_download_complete_calls_ = 0;
  download_complete_result_ = CrxDownloader::Result();
  num_progress_calls_ = 0;
  download_progress_result_ = CrxDownloader::Result();

  // Do not use the background downloader in these tests.
  crx_downloader_.reset(
      CrxDownloader::Create(false, context_.get(),
                            base::ThreadTaskRunnerHandle::Get())
          .release());
  crx_downloader_->set_progress_callback(progress_callback_);

  get_interceptor_.reset(
      new GetInterceptor(base::ThreadTaskRunnerHandle::Get(),
                         base::ThreadTaskRunnerHandle::Get()));
}

void CrxDownloaderTest::TearDown() {
  crx_downloader_.reset();
}

void CrxDownloaderTest::Quit() {
  if (!quit_closure_.is_null())
    quit_closure_.Run();
}

void CrxDownloaderTest::DownloadComplete(int crx_context,
                                         const CrxDownloader::Result& result) {
  ++num_download_complete_calls_;
  crx_context_ = crx_context;
  download_complete_result_ = result;
  Quit();
}

void CrxDownloaderTest::DownloadProgress(int crx_context,
                                         const CrxDownloader::Result& result) {
  ++num_progress_calls_;
  download_progress_result_ = result;
}

void CrxDownloaderTest::RunThreads() {
  base::RunLoop runloop;
  quit_closure_ = runloop.QuitClosure();
  runloop.Run();

  // Since some tests need to drain currently enqueued tasks such as network
  // intercepts on the IO thread, run the threads until they are
  // idle. The component updater service won't loop again until the loop count
  // is set and the service is started.
  RunThreadsUntilIdle();
}

void CrxDownloaderTest::RunThreadsUntilIdle() {
  base::RunLoop().RunUntilIdle();
}

// Tests that starting a download without a url results in an error.
TEST_F(CrxDownloaderTest, NoUrl) {
  std::vector<GURL> urls;
  crx_downloader_->StartDownload(urls, std::string("abcd"), callback_);
  RunThreadsUntilIdle();

  EXPECT_EQ(1, num_download_complete_calls_);
  EXPECT_EQ(kExpectedContext, crx_context_);
  EXPECT_EQ(CrxDownloader::Error::NO_URL, download_complete_result_.error);
  EXPECT_TRUE(download_complete_result_.response.empty());
  EXPECT_EQ(-1, download_complete_result_.downloaded_bytes);
  EXPECT_EQ(-1, download_complete_result_.total_bytes);
  EXPECT_EQ(0, num_progress_calls_);
}

// Tests that starting a download without providing a hash results in an error.
TEST_F(CrxDownloaderTest, NoHash) {
  std::vector<GURL> urls(1, GURL("http://somehost/somefile"));

  crx_downloader_->StartDownload(urls, std::string(), callback_);
  RunThreadsUntilIdle();

  EXPECT_EQ(1, num_download_complete_calls_);
  EXPECT_EQ(kExpectedContext, crx_context_);
  EXPECT_EQ(CrxDownloader::Error::NO_HASH, download_complete_result_.error);
  EXPECT_TRUE(download_complete_result_.response.empty());
  EXPECT_EQ(-1, download_complete_result_.downloaded_bytes);
  EXPECT_EQ(-1, download_complete_result_.total_bytes);
  EXPECT_EQ(0, num_progress_calls_);
}

// Tests that downloading from one url is successful.
TEST_F(CrxDownloaderTest, OneUrl) {
  const GURL expected_crx_url =
      GURL("http://localhost/download/jebgalgnebhfojomionfpkfelancnnkf.crx");

  const base::FilePath test_file(MakeTestFilePath(kTestFileName));
  get_interceptor_->SetResponse(expected_crx_url, test_file);

  crx_downloader_->StartDownloadFromUrl(expected_crx_url,
                                        std::string(hash_jebg), callback_);
  RunThreads();

  EXPECT_EQ(1, get_interceptor_->GetHitCount());

  EXPECT_EQ(1, num_download_complete_calls_);
  EXPECT_EQ(kExpectedContext, crx_context_);
  EXPECT_EQ(0, download_complete_result_.error);
  EXPECT_EQ(1843, download_complete_result_.downloaded_bytes);
  EXPECT_EQ(1843, download_complete_result_.total_bytes);
  EXPECT_TRUE(ContentsEqual(download_complete_result_.response, test_file));

  EXPECT_TRUE(base::DeleteFile(download_complete_result_.response, false));

  EXPECT_LE(1, num_progress_calls_);
  EXPECT_EQ(1843, download_progress_result_.downloaded_bytes);
  EXPECT_EQ(1843, download_progress_result_.total_bytes);
}

// Tests that downloading from one url fails if the actual hash of the file
// does not match the expected hash.
TEST_F(CrxDownloaderTest, OneUrlBadHash) {
  const GURL expected_crx_url =
      GURL("http://localhost/download/jebgalgnebhfojomionfpkfelancnnkf.crx");

  const base::FilePath test_file(MakeTestFilePath(kTestFileName));
  get_interceptor_->SetResponse(expected_crx_url, test_file);

  crx_downloader_->StartDownloadFromUrl(
      expected_crx_url,
      std::string(
          "813c59747e139a608b3b5fc49633affc6db574373f309f156ea6d27229c0b3f9"),
      callback_);
  RunThreads();

  EXPECT_EQ(1, get_interceptor_->GetHitCount());

  EXPECT_EQ(1, num_download_complete_calls_);
  EXPECT_EQ(kExpectedContext, crx_context_);
  EXPECT_EQ(CrxDownloader::Error::BAD_HASH, download_complete_result_.error);
  EXPECT_EQ(1843, download_complete_result_.downloaded_bytes);
  EXPECT_EQ(1843, download_complete_result_.total_bytes);
  EXPECT_TRUE(download_complete_result_.response.empty());

  EXPECT_LE(1, num_progress_calls_);
  EXPECT_EQ(1843, download_progress_result_.downloaded_bytes);
  EXPECT_EQ(1843, download_progress_result_.total_bytes);
}

// Tests that specifying two urls has no side effects. Expect a successful
// download, and only one download request be made.
// This test is flaky on Android. crbug.com/329883
#if defined(OS_ANDROID)
#define MAYBE_TwoUrls DISABLED_TwoUrls
#else
#define MAYBE_TwoUrls TwoUrls
#endif
TEST_F(CrxDownloaderTest, MAYBE_TwoUrls) {
  const GURL expected_crx_url =
      GURL("http://localhost/download/jebgalgnebhfojomionfpkfelancnnkf.crx");

  const base::FilePath test_file(MakeTestFilePath(kTestFileName));
  get_interceptor_->SetResponse(expected_crx_url, test_file);

  std::vector<GURL> urls;
  urls.push_back(expected_crx_url);
  urls.push_back(expected_crx_url);

  crx_downloader_->StartDownload(urls, std::string(hash_jebg), callback_);
  RunThreads();

  EXPECT_EQ(1, get_interceptor_->GetHitCount());

  EXPECT_EQ(1, num_download_complete_calls_);
  EXPECT_EQ(kExpectedContext, crx_context_);
  EXPECT_EQ(0, download_complete_result_.error);
  EXPECT_EQ(1843, download_complete_result_.downloaded_bytes);
  EXPECT_EQ(1843, download_complete_result_.total_bytes);
  EXPECT_TRUE(ContentsEqual(download_complete_result_.response, test_file));

  EXPECT_TRUE(base::DeleteFile(download_complete_result_.response, false));

  EXPECT_LE(1, num_progress_calls_);
  EXPECT_EQ(1843, download_progress_result_.downloaded_bytes);
  EXPECT_EQ(1843, download_progress_result_.total_bytes);
}

// Tests that an invalid host results in a download error.
TEST_F(CrxDownloaderTest, OneUrl_InvalidHost) {
  const GURL expected_crx_url =
      GURL("http://localhost/download/jebgalgnebhfojomionfpkfelancnnkf.crx");

  const base::FilePath test_file(MakeTestFilePath(kTestFileName));
  get_interceptor_->SetResponse(expected_crx_url, test_file);

  crx_downloader_->StartDownloadFromUrl(
      GURL("http://no.such.host"
           "/download/jebgalgnebhfojomionfpkfelancnnkf.crx"),
      std::string(hash_jebg), callback_);
  RunThreads();

  EXPECT_EQ(0, get_interceptor_->GetHitCount());

  EXPECT_EQ(1, num_download_complete_calls_);
  EXPECT_EQ(kExpectedContext, crx_context_);
  EXPECT_NE(0, download_complete_result_.error);
  EXPECT_TRUE(download_complete_result_.response.empty());
}

// Tests that an invalid path results in a download error.
TEST_F(CrxDownloaderTest, OneUrl_InvalidPath) {
  const GURL expected_crx_url =
      GURL("http://localhost/download/jebgalgnebhfojomionfpkfelancnnkf.crx");

  const base::FilePath test_file(MakeTestFilePath(kTestFileName));
  get_interceptor_->SetResponse(expected_crx_url, test_file);

  crx_downloader_->StartDownloadFromUrl(GURL("http://localhost/no/such/file"),
                                        std::string(hash_jebg), callback_);
  RunThreads();

  EXPECT_EQ(0, get_interceptor_->GetHitCount());

  EXPECT_EQ(1, num_download_complete_calls_);
  EXPECT_EQ(kExpectedContext, crx_context_);
  EXPECT_NE(0, download_complete_result_.error);
  EXPECT_TRUE(download_complete_result_.response.empty());
}

// Tests that the fallback to a valid url is successful.
// This test is flaky on Android. crbug.com/329883
#if defined(OS_ANDROID)
#define MAYBE_TwoUrls_FirstInvalid DISABLED_TwoUrls_FirstInvalid
#else
#define MAYBE_TwoUrls_FirstInvalid TwoUrls_FirstInvalid
#endif
TEST_F(CrxDownloaderTest, MAYBE_TwoUrls_FirstInvalid) {
  const GURL expected_crx_url =
      GURL("http://localhost/download/jebgalgnebhfojomionfpkfelancnnkf.crx");

  const base::FilePath test_file(MakeTestFilePath(kTestFileName));
  get_interceptor_->SetResponse(expected_crx_url, test_file);

  std::vector<GURL> urls;
  urls.push_back(GURL("http://localhost/no/such/file"));
  urls.push_back(expected_crx_url);

  crx_downloader_->StartDownload(urls, std::string(hash_jebg), callback_);
  RunThreads();

  EXPECT_EQ(1, get_interceptor_->GetHitCount());

  EXPECT_EQ(1, num_download_complete_calls_);
  EXPECT_EQ(kExpectedContext, crx_context_);
  EXPECT_EQ(0, download_complete_result_.error);
  EXPECT_EQ(1843, download_complete_result_.downloaded_bytes);
  EXPECT_EQ(1843, download_complete_result_.total_bytes);
  EXPECT_TRUE(ContentsEqual(download_complete_result_.response, test_file));

  EXPECT_TRUE(base::DeleteFile(download_complete_result_.response, false));

  EXPECT_LE(1, num_progress_calls_);
  EXPECT_EQ(1843, download_progress_result_.downloaded_bytes);
  EXPECT_EQ(1843, download_progress_result_.total_bytes);
}

// Tests that the download succeeds if the first url is correct and the
// second bad url does not have a side-effect.
TEST_F(CrxDownloaderTest, TwoUrls_SecondInvalid) {
  const GURL expected_crx_url =
      GURL("http://localhost/download/jebgalgnebhfojomionfpkfelancnnkf.crx");

  const base::FilePath test_file(MakeTestFilePath(kTestFileName));
  get_interceptor_->SetResponse(expected_crx_url, test_file);

  std::vector<GURL> urls;
  urls.push_back(expected_crx_url);
  urls.push_back(GURL("http://localhost/no/such/file"));

  crx_downloader_->StartDownload(urls, std::string(hash_jebg), callback_);
  RunThreads();

  EXPECT_EQ(1, get_interceptor_->GetHitCount());

  EXPECT_EQ(1, num_download_complete_calls_);
  EXPECT_EQ(kExpectedContext, crx_context_);
  EXPECT_EQ(0, download_complete_result_.error);
  EXPECT_EQ(1843, download_complete_result_.downloaded_bytes);
  EXPECT_EQ(1843, download_complete_result_.total_bytes);
  EXPECT_TRUE(ContentsEqual(download_complete_result_.response, test_file));

  EXPECT_TRUE(base::DeleteFile(download_complete_result_.response, false));

  EXPECT_LE(1, num_progress_calls_);
  EXPECT_EQ(1843, download_progress_result_.downloaded_bytes);
  EXPECT_EQ(1843, download_progress_result_.total_bytes);
}

// Tests that the download fails if both urls are bad.
TEST_F(CrxDownloaderTest, TwoUrls_BothInvalid) {
  const GURL expected_crx_url =
      GURL("http://localhost/download/jebgalgnebhfojomionfpkfelancnnkf.crx");

  const base::FilePath test_file(MakeTestFilePath(kTestFileName));
  get_interceptor_->SetResponse(expected_crx_url, test_file);

  std::vector<GURL> urls;
  urls.push_back(GURL("http://localhost/no/such/file"));
  urls.push_back(GURL(
      "http://no.such.host/"
      "/download/jebgalgnebhfojomionfpkfelancnnkf.crx"));

  crx_downloader_->StartDownload(urls, std::string(hash_jebg), callback_);
  RunThreads();

  EXPECT_EQ(0, get_interceptor_->GetHitCount());

  EXPECT_EQ(1, num_download_complete_calls_);
  EXPECT_EQ(kExpectedContext, crx_context_);
  EXPECT_NE(0, download_complete_result_.error);
  EXPECT_TRUE(download_complete_result_.response.empty());
}

}  // namespace update_client
