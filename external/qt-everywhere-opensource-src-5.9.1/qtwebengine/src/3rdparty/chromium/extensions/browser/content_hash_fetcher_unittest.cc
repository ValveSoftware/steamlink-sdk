// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/version.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/content_hash_fetcher.h"
#include "extensions/browser/content_verifier_delegate.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/file_util.h"
#include "net/url_request/test_url_request_interceptor.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/zip.h"

namespace extensions {

// Used to hold the result of a callback from the ContentHashFetcher.
struct ContentHashFetcherResult {
  std::string extension_id;
  bool success;
  bool force;
  std::set<base::FilePath> mismatch_paths;
};

// Allows waiting for the callback from a ContentHashFetcher, returning the
// data that was passed to that callback.
class ContentHashFetcherWaiter {
 public:
  ContentHashFetcherWaiter() : weak_factory_(this) {}

  ContentHashFetcher::FetchCallback GetCallback() {
    return base::Bind(&ContentHashFetcherWaiter::Callback,
                      weak_factory_.GetWeakPtr());
  }

  std::unique_ptr<ContentHashFetcherResult> WaitForCallback() {
    if (!result_) {
      base::RunLoop run_loop;
      run_loop_quit_ = run_loop.QuitClosure();
      run_loop.Run();
    }
    return std::move(result_);
  }

 private:
  // Matches signature of ContentHashFetcher::FetchCallback.
  void Callback(const std::string& extension_id,
                bool success,
                bool force,
                const std::set<base::FilePath>& mismatch_paths) {
    result_ = base::MakeUnique<ContentHashFetcherResult>();
    result_->extension_id = extension_id;
    result_->success = success;
    result_->force = force;
    result_->mismatch_paths = mismatch_paths;
    if (run_loop_quit_)
      base::ResetAndReturn(&run_loop_quit_).Run();
  }

  base::Closure run_loop_quit_;
  std::unique_ptr<ContentHashFetcherResult> result_;
  base::WeakPtrFactory<ContentHashFetcherWaiter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ContentHashFetcherWaiter);
};

// Used in setting up the behavior of our ContentHashFetcher.
class MockDelegate : public ContentVerifierDelegate {
 public:
  ContentVerifierDelegate::Mode ShouldBeVerified(
      const Extension& extension) override {
    return ContentVerifierDelegate::ENFORCE_STRICT;
  }

  ContentVerifierKey GetPublicKey() override {
    return ContentVerifierKey(kWebstoreSignaturesPublicKey,
                              kWebstoreSignaturesPublicKeySize);
  }

  GURL GetSignatureFetchUrl(const std::string& extension_id,
                            const base::Version& version) override {
    std::string url =
        base::StringPrintf("http://localhost/getsignature?id=%s&version=%s",
                           extension_id.c_str(), version.GetString().c_str());
    return GURL(url);
  }

  std::set<base::FilePath> GetBrowserImagePaths(
      const extensions::Extension* extension) override {
    ADD_FAILURE() << "Unexpected call for this test";
    return std::set<base::FilePath>();
  }

  void VerifyFailed(const std::string& extension_id,
                    ContentVerifyJob::FailureReason reason) override {
    ADD_FAILURE() << "Unexpected call for this test";
  }
};

class ContentHashFetcherTest : public ExtensionsTest {
 public:
  ContentHashFetcherTest() {}
  ~ContentHashFetcherTest() override {}

  void SetUp() override {
    ExtensionsTest::SetUp();
    // We need a real IO thread to be able to intercept the network request
    // for the missing verified_contents.json file.
    browser_threads_.reset(new content::TestBrowserThreadBundle(
        content::TestBrowserThreadBundle::REAL_IO_THREAD));
    request_context_ = new net::TestURLRequestContextGetter(
        content::BrowserThread::GetTaskRunnerForThread(
            content::BrowserThread::IO));
  }

  net::URLRequestContextGetter* request_context() {
    return request_context_.get();
  }

  // Helper to get files from our subdirectory in the general extensions test
  // data dir.
  base::FilePath GetTestPath(const base::FilePath& relative_path) {
    base::FilePath base_path;
    EXPECT_TRUE(PathService::Get(extensions::DIR_TEST_DATA, &base_path));
    base_path = base_path.AppendASCII("content_hash_fetcher");
    return base_path.Append(relative_path);
  }

  // Unzips the extension source from |extension_zip| into a temporary
  // directory and loads it, returning the resuling Extension object.
  scoped_refptr<Extension> UnzipToTempDirAndLoad(
      const base::FilePath& extension_zip) {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath destination = temp_dir_.GetPath();
    EXPECT_TRUE(zip::Unzip(extension_zip, destination));

    std::string error;
    scoped_refptr<Extension> extension = file_util::LoadExtension(
        destination, Manifest::INTERNAL, 0 /* flags */, &error);
    EXPECT_NE(nullptr, extension.get()) << " error:'" << error << "'";
    return extension;
  }

  // Registers interception of requests for |url| to respond with the contents
  // of the file at |response_path|.
  void RegisterInterception(const GURL& url,
                            const base::FilePath& response_path) {
    interceptor_ = base::MakeUnique<net::TestURLRequestInterceptor>(
        url.scheme(), url.host(),
        content::BrowserThread::GetTaskRunnerForThread(
            content::BrowserThread::IO),
        content::BrowserThread::GetBlockingPool());
    interceptor_->SetResponse(url, response_path);
  }

 protected:
  std::unique_ptr<content::TestBrowserThreadBundle> browser_threads_;
  std::unique_ptr<net::TestURLRequestInterceptor> interceptor_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  base::ScopedTempDir temp_dir_;
};

// This tests our ability to successfully fetch, parse, and validate a missing
// verified_contents.json file for an extension.
TEST_F(ContentHashFetcherTest, MissingVerifiedContents) {
  // We unzip the extension source to a temp directory to simulate it being
  // installed there, because the ContentHashFetcher will create the _metadata/
  // directory within the extension install dir and write the fetched
  // verified_contents.json file there.
  base::FilePath test_dir_base = GetTestPath(
      base::FilePath(FILE_PATH_LITERAL("missing_verified_contents")));
  scoped_refptr<Extension> extension =
      UnzipToTempDirAndLoad(test_dir_base.AppendASCII("source.zip"));

  // Make sure there isn't already a verified_contents.json file there.
  EXPECT_FALSE(
      base::PathExists(file_util::GetVerifiedContentsPath(extension->path())));

  MockDelegate delegate;
  ContentHashFetcherWaiter waiter;
  GURL fetch_url =
      delegate.GetSignatureFetchUrl(extension->id(), *extension->version());

  RegisterInterception(fetch_url,
                       test_dir_base.AppendASCII("verified_contents.json"));

  ContentHashFetcher fetcher(request_context(), &delegate,
                             waiter.GetCallback());
  fetcher.DoFetch(extension.get(), true /* force */);

  // Make sure the fetch was successful.
  std::unique_ptr<ContentHashFetcherResult> result = waiter.WaitForCallback();
  ASSERT_TRUE(result.get());
  EXPECT_TRUE(result->success);
  EXPECT_TRUE(result->force);
  EXPECT_TRUE(result->mismatch_paths.empty());

  // Make sure the verified_contents.json file was written into the extension's
  // install dir.
  EXPECT_TRUE(
      base::PathExists(file_util::GetVerifiedContentsPath(extension->path())));
}

// Similar to MissingVerifiedContents, but tests the case where the extension
// actually has corruption.
TEST_F(ContentHashFetcherTest, MissingVerifiedContentsAndCorrupt) {
  base::FilePath test_dir_base =
      GetTestPath(base::FilePath()).AppendASCII("missing_verified_contents");
  scoped_refptr<Extension> extension =
      UnzipToTempDirAndLoad(test_dir_base.AppendASCII("source.zip"));

  // Tamper with a file in the extension.
  base::FilePath script_path = extension->path().AppendASCII("script.js");
  std::string addition = "//hello world";
  ASSERT_TRUE(
      base::AppendToFile(script_path, addition.c_str(), addition.size()));
  MockDelegate delegate;
  ContentHashFetcherWaiter waiter;
  GURL fetch_url =
      delegate.GetSignatureFetchUrl(extension->id(), *extension->version());

  RegisterInterception(fetch_url,
                       test_dir_base.AppendASCII("verified_contents.json"));

  ContentHashFetcher fetcher(request_context(), &delegate,
                             waiter.GetCallback());
  fetcher.DoFetch(extension.get(), true /* force */);

  // Make sure the fetch was *not* successful.
  std::unique_ptr<ContentHashFetcherResult> result = waiter.WaitForCallback();
  ASSERT_NE(nullptr, result.get());
  EXPECT_TRUE(result->success);
  EXPECT_TRUE(result->force);
  EXPECT_TRUE(
      base::ContainsKey(result->mismatch_paths, script_path.BaseName()));

  // Make sure the verified_contents.json file was written into the extension's
  // install dir.
  EXPECT_TRUE(
      base::PathExists(file_util::GetVerifiedContentsPath(extension->path())));
}

}  // namespace extensions
