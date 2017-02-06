// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/gcd_private/privet_v3_context_getter.h"

#include <stddef.h>

#include "base/location.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

using content::BrowserThread;
using net::EmbeddedTestServer;

class PrivetV3ContextGetterTest : public testing::Test,
                                  public net::URLFetcherDelegate {
 protected:
  PrivetV3ContextGetterTest()
      : thread_bundle_(content::TestBrowserThreadBundle::REAL_IO_THREAD) {}

  void SetUp() override {
    context_getter_ = new extensions::PrivetV3ContextGetter(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
  }

  void OnURLFetchComplete(const net::URLFetcher* source) override {
    done_ = true;
    status_ = source->GetStatus();

    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, quit_);
  }

  void CreateServer(EmbeddedTestServer::Type type) {
    server_.reset(new EmbeddedTestServer(type));
    ASSERT_TRUE(server_->Start());

    base::FilePath test_data_dir;
    ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
    server_->ServeFilesFromDirectory(
        test_data_dir.Append(FILE_PATH_LITERAL("chrome/test/data")));
  }

  net::URLRequestStatus::Status Run() {
    done_ = false;
    status_ = net::URLRequestStatus();

    base::RunLoop run_loop;
    quit_ = run_loop.QuitClosure();

    fetcher_ = net::URLFetcher::Create(server_->GetURL("/simple.html"),
                                       net::URLFetcher::GET, this);
    fetcher_->SetRequestContext(context_getter_.get());
    fetcher_->Start();

    run_loop.Run();

    EXPECT_TRUE(done_);
    return status_.status();
  }

  void SetFingerprint(const net::SHA256HashValue& fingerprint) {
    base::RunLoop run_loop;
    context_getter_->AddPairedHost(server_->host_port_pair().host(),
                                   fingerprint, run_loop.QuitClosure());
    run_loop.Run();
  }

  net::SHA256HashValue GetServerFingerprint() const {
    return net::X509Certificate::CalculateFingerprint256(
        server_->GetCertificate()->os_cert_handle());
  }

 private:
  bool done_ = false;
  net::URLRequestStatus status_ = net::URLRequestStatus();
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<extensions::PrivetV3ContextGetter> context_getter_;
  std::unique_ptr<EmbeddedTestServer> server_;
  std::unique_ptr<net::URLFetcher> fetcher_;

  base::Closure quit_;
};

TEST_F(PrivetV3ContextGetterTest, Http) {
  CreateServer(EmbeddedTestServer::TYPE_HTTP);
  EXPECT_EQ(net::URLRequestStatus::SUCCESS, Run());
}

TEST_F(PrivetV3ContextGetterTest, HttpsUnknownHost) {
  CreateServer(EmbeddedTestServer::TYPE_HTTPS);
  EXPECT_EQ(net::URLRequestStatus::CANCELED, Run());
}

TEST_F(PrivetV3ContextGetterTest, HttpsInvalidFingerprint) {
  CreateServer(EmbeddedTestServer::TYPE_HTTPS);
  SetFingerprint(net::SHA256HashValue());
  EXPECT_EQ(net::URLRequestStatus::CANCELED, Run());
}

TEST_F(PrivetV3ContextGetterTest, HttpsValidFingerprint) {
  CreateServer(EmbeddedTestServer::TYPE_HTTPS);
  SetFingerprint(GetServerFingerprint());
  EXPECT_EQ(net::URLRequestStatus::SUCCESS, Run());
}

TEST_F(PrivetV3ContextGetterTest, HttpsFingerprintOverride) {
  for (size_t i = 0; i < 3; ++i) {
    CreateServer(EmbeddedTestServer::TYPE_HTTPS);
    SetFingerprint(net::SHA256HashValue());
    EXPECT_EQ(net::URLRequestStatus::CANCELED, Run());

    CreateServer(EmbeddedTestServer::TYPE_HTTPS);
    SetFingerprint(GetServerFingerprint());
    EXPECT_EQ(net::URLRequestStatus::SUCCESS, Run());
  }
}

}  // namespace

}  // namespace extensions
