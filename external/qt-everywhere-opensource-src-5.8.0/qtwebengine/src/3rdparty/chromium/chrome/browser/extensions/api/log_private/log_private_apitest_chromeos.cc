// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_debug_daemon_client.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/extension_builder.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

using net::test_server::BasicHttpResponse;
using net::test_server::HttpResponse;
using net::test_server::HttpRequest;

namespace {

class TestDebugDaemonClient : public chromeos::FakeDebugDaemonClient {
 public:
  explicit TestDebugDaemonClient(const base::FilePath& test_file)
      : test_file_(test_file) {}

  ~TestDebugDaemonClient() override {}

  void DumpDebugLogs(bool is_compressed,
                     base::File file,
                     scoped_refptr<base::TaskRunner> task_runner,
                     const GetDebugLogsCallback& callback) override {
    base::File* file_param = new base::File(std::move(file));
    task_runner->PostTaskAndReply(
        FROM_HERE,
        base::Bind(
            &GenerateTestLogDumpFile, test_file_, base::Owned(file_param)),
        base::Bind(callback, true));
  }

  static void GenerateTestLogDumpFile(const base::FilePath& test_tar_file,
                                      base::File* file) {
    std::string test_file_content;
    EXPECT_TRUE(base::ReadFileToString(test_tar_file, &test_file_content))
        << "Cannot read content of file " << test_tar_file.value();
    const int data_size = static_cast<int>(test_file_content.size());
    EXPECT_EQ(data_size, file->Write(0, test_file_content.data(), data_size));
    EXPECT_TRUE(file->SetLength(data_size));
    file->Close();
  }

 private:
  base::FilePath test_file_;
};

}  // namespace

namespace extensions {

class LogPrivateApiTest : public ExtensionApiTest {
 public:
  LogPrivateApiTest() {}

  ~LogPrivateApiTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    base::FilePath tar_file_path =
        test_data_dir_.Append("log_private/dump_logs/system_logs.tar");
    chromeos::DBusThreadManager::GetSetterForTesting()->SetDebugDaemonClient(
        std::unique_ptr<chromeos::DebugDaemonClient>(
            new TestDebugDaemonClient(tar_file_path)));
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  }

  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    std::unique_ptr<BasicHttpResponse> response(new BasicHttpResponse);
    response->set_code(net::HTTP_OK);
    response->set_content(
        "<html><head><title>LogPrivateTest</title>"
        "</head><body>Hello!</body></html>");
    return std::move(response);
  }
};

IN_PROC_BROWSER_TEST_F(LogPrivateApiTest, DumpLogsAndCaptureEvents) {
  // Setup dummy HTTP server.
  host_resolver()->AddRule("www.test.com", "127.0.0.1");
  ASSERT_TRUE(StartEmbeddedTestServer());
  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&LogPrivateApiTest::HandleRequest, base::Unretained(this)));

  ASSERT_TRUE(RunExtensionTest("log_private/dump_logs"));
}

}  // namespace extensions
