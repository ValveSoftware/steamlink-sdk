// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/update_checker.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/version.h"
#include "build/build_config.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/test_configurator.h"
#include "components/update_client/url_request_post_interceptor.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using std::string;

namespace update_client {

namespace {

base::FilePath test_file(const char* file) {
  base::FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  return path.AppendASCII("components")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("update_client")
      .AppendASCII(file);
}

const char kUpdateItemId[] = "jebgalgnebhfojomionfpkfelancnnkf";

}  // namespace

class UpdateCheckerTest : public testing::Test {
 public:
  UpdateCheckerTest();
  ~UpdateCheckerTest() override;

  // Overrides from testing::Test.
  void SetUp() override;
  void TearDown() override;

  void UpdateCheckComplete(int error,
                           const UpdateResponse::Results& results,
                           int retry_after_sec);

 protected:
  void Quit();
  void RunThreads();
  void RunThreadsUntilIdle();

  std::unique_ptr<CrxUpdateItem> BuildCrxUpdateItem();

  scoped_refptr<TestConfigurator> config_;
  std::unique_ptr<TestingPrefServiceSimple> pref_;
  std::unique_ptr<PersistedData> metadata_;

  std::unique_ptr<UpdateChecker> update_checker_;

  std::unique_ptr<InterceptorFactory> interceptor_factory_;
  URLRequestPostInterceptor* post_interceptor_;  // Owned by the factory.

  int error_;
  UpdateResponse::Results results_;

 private:
  base::MessageLoopForIO loop_;
  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(UpdateCheckerTest);
};

UpdateCheckerTest::UpdateCheckerTest() : post_interceptor_(NULL), error_(0) {
}

UpdateCheckerTest::~UpdateCheckerTest() {
}

void UpdateCheckerTest::SetUp() {
  config_ = new TestConfigurator(base::ThreadTaskRunnerHandle::Get(),
                                 base::ThreadTaskRunnerHandle::Get());
  pref_.reset(new TestingPrefServiceSimple());
  PersistedData::RegisterPrefs(pref_->registry());
  metadata_.reset(new PersistedData(pref_.get()));
  interceptor_factory_.reset(
      new InterceptorFactory(base::ThreadTaskRunnerHandle::Get()));
  post_interceptor_ = interceptor_factory_->CreateInterceptor();
  EXPECT_TRUE(post_interceptor_);

  update_checker_.reset();

  error_ = 0;
  results_ = UpdateResponse::Results();
}

void UpdateCheckerTest::TearDown() {
  update_checker_.reset();

  post_interceptor_ = NULL;
  interceptor_factory_.reset();

  config_ = nullptr;

  // The PostInterceptor requires the message loop to run to destruct correctly.
  // TODO(sorin): This is fragile and should be fixed.
  RunThreadsUntilIdle();
}

void UpdateCheckerTest::RunThreads() {
  base::RunLoop runloop;
  quit_closure_ = runloop.QuitClosure();
  runloop.Run();

  // Since some tests need to drain currently enqueued tasks such as network
  // intercepts on the IO thread, run the threads until they are
  // idle. The component updater service won't loop again until the loop count
  // is set and the service is started.
  RunThreadsUntilIdle();
}

void UpdateCheckerTest::RunThreadsUntilIdle() {
  base::RunLoop().RunUntilIdle();
}

void UpdateCheckerTest::Quit() {
  if (!quit_closure_.is_null())
    quit_closure_.Run();
}

void UpdateCheckerTest::UpdateCheckComplete(
    int error,
    const UpdateResponse::Results& results,
    int retry_after_sec) {
  error_ = error;
  results_ = results;
  Quit();
}

std::unique_ptr<CrxUpdateItem> UpdateCheckerTest::BuildCrxUpdateItem() {
  CrxComponent crx_component;
  crx_component.name = "test_jebg";
  crx_component.pk_hash.assign(jebg_hash, jebg_hash + arraysize(jebg_hash));
  crx_component.installer = NULL;
  crx_component.version = base::Version("0.9");
  crx_component.fingerprint = "fp1";

  std::unique_ptr<CrxUpdateItem> crx_update_item =
      base::MakeUnique<CrxUpdateItem>();
  crx_update_item->state = CrxUpdateItem::State::kNew;
  crx_update_item->id = kUpdateItemId;
  crx_update_item->component = crx_component;

  return crx_update_item;
}

TEST_F(UpdateCheckerTest, UpdateCheckSuccess) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      new PartialMatch("updatecheck"), test_file("updatecheck_reply_1.xml")));

  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  std::unique_ptr<CrxUpdateItem> item = BuildCrxUpdateItem();
  item->component.installer_attributes["ap"] = "some_ap";
  IdToCrxUpdateItemMap items_to_check;
  items_to_check[kUpdateItemId] = std::move(item);

  update_checker_->CheckForUpdates(
      items_to_check, "extra=\"params\"", true,
      base::Bind(&UpdateCheckerTest::UpdateCheckComplete,
                 base::Unretained(this)));

  RunThreads();

  EXPECT_EQ(1, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  ASSERT_EQ(1, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();

  // Sanity check the request.
  const auto request = post_interceptor_->GetRequests()[0];
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[0].find(
                              "request protocol=\"3.0\" extra=\"params\""));
  // The request must not contain any "dlpref" in the default case.
  EXPECT_EQ(string::npos, request.find(" dlpref=\""));
  EXPECT_NE(
      string::npos,
      request.find(
          std::string("<app appid=\"") + kUpdateItemId +
          "\" version=\"0.9\" "
          "brand=\"TEST\" ap=\"some_ap\"><updatecheck/><ping rd=\"-2\" "));
  EXPECT_NE(string::npos,
            request.find("<packages><package fp=\"fp1\"/></packages></app>"));

  EXPECT_NE(string::npos, request.find("<hw physmemory="));

  // Tests that the progid is injected correctly from the configurator.
  EXPECT_NE(
      string::npos,
      request.find(" version=\"fake_prodid-30.0\" prodversion=\"30.0\" "));

  // Sanity check the arguments of the callback after parsing.
  EXPECT_EQ(0, error_);
  EXPECT_EQ(1ul, results_.list.size());
  EXPECT_STREQ(kUpdateItemId, results_.list[0].extension_id.c_str());
  EXPECT_STREQ("1.0", results_.list[0].manifest.version.c_str());

#if (OS_WIN)
  EXPECT_NE(string::npos, request.find(" domainjoined="));
#if defined(GOOGLE_CHROME_BUILD)
  // Check the Omaha updater state data in the request.
  EXPECT_NE(string::npos, request.find("<updater "));
  EXPECT_NE(string::npos, request.find(" name=\"Omaha\" "));
#endif  // GOOGLE_CHROME_BUILD
#endif  // OS_WINDOWS
}

// Tests that an invalid "ap" is not serialized.
TEST_F(UpdateCheckerTest, UpdateCheckInvalidAp) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      new PartialMatch("updatecheck"), test_file("updatecheck_reply_1.xml")));

  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  std::unique_ptr<CrxUpdateItem> item = BuildCrxUpdateItem();
  // Make "ap" too long.
  item->component.installer_attributes["ap"] = std::string(257, 'a');
  IdToCrxUpdateItemMap items_to_check;
  items_to_check[kUpdateItemId] = std::move(item);

  update_checker_->CheckForUpdates(
      items_to_check, "", true,
      base::Bind(&UpdateCheckerTest::UpdateCheckComplete,
                 base::Unretained(this)));

  RunThreads();

  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[0].find(
                              std::string("app appid=\"") + kUpdateItemId +
                              "\" version=\"0.9\" "
                              "brand=\"TEST\"><updatecheck/><ping rd=\"-2\" "));
  EXPECT_NE(string::npos,
            post_interceptor_->GetRequests()[0].find(
                "<packages><package fp=\"fp1\"/></packages></app>"));
}

TEST_F(UpdateCheckerTest, UpdateCheckSuccessNoBrand) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      new PartialMatch("updatecheck"), test_file("updatecheck_reply_1.xml")));

  config_->SetBrand("TOOLONG");   // Sets an invalid brand code.
  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  std::unique_ptr<CrxUpdateItem> item = BuildCrxUpdateItem();
  IdToCrxUpdateItemMap items_to_check;
  items_to_check[kUpdateItemId] = std::move(item);

  update_checker_->CheckForUpdates(
      items_to_check, "", true,
      base::Bind(&UpdateCheckerTest::UpdateCheckComplete,
                 base::Unretained(this)));

  RunThreads();

  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[0].find(
                              std::string("<app appid=\"") + kUpdateItemId +
                              "\" version=\"0.9\">"
                              "<updatecheck/><ping rd=\"-2\" "));
  EXPECT_NE(string::npos,
            post_interceptor_->GetRequests()[0].find(
                "<packages><package fp=\"fp1\"/></packages></app>"));
}

// Simulates a 403 server response error.
TEST_F(UpdateCheckerTest, UpdateCheckError) {
  EXPECT_TRUE(
      post_interceptor_->ExpectRequest(new PartialMatch("updatecheck"), 403));

  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  std::unique_ptr<CrxUpdateItem> item = BuildCrxUpdateItem();
  IdToCrxUpdateItemMap items_to_check;
  items_to_check[kUpdateItemId] = std::move(item);

  update_checker_->CheckForUpdates(
      items_to_check, "", true,
      base::Bind(&UpdateCheckerTest::UpdateCheckComplete,
                 base::Unretained(this)));
  RunThreads();

  EXPECT_EQ(1, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  EXPECT_EQ(1, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();

  EXPECT_EQ(403, error_);
  EXPECT_EQ(0ul, results_.list.size());
}

TEST_F(UpdateCheckerTest, UpdateCheckDownloadPreference) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      new PartialMatch("updatecheck"), test_file("updatecheck_reply_1.xml")));

  config_->SetDownloadPreference(string("cacheable"));

  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  std::unique_ptr<CrxUpdateItem> item = BuildCrxUpdateItem();
  IdToCrxUpdateItemMap items_to_check;
  items_to_check[kUpdateItemId] = std::move(item);

  update_checker_->CheckForUpdates(
      items_to_check, "extra=\"params\"", true,
      base::Bind(&UpdateCheckerTest::UpdateCheckComplete,
                 base::Unretained(this)));

  RunThreads();

  // The request must contain dlpref="cacheable".
  EXPECT_NE(string::npos,
            post_interceptor_->GetRequests()[0].find(" dlpref=\"cacheable\""));
}

// This test is checking that an update check signed with CUP fails, since there
// is currently no entity that can respond with a valid signed response.
// A proper CUP test requires network mocks, which are not available now.
TEST_F(UpdateCheckerTest, UpdateCheckCupError) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      new PartialMatch("updatecheck"), test_file("updatecheck_reply_1.xml")));

  config_->SetEnabledCupSigning(true);
  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  std::unique_ptr<CrxUpdateItem> item = BuildCrxUpdateItem();
  IdToCrxUpdateItemMap items_to_check;
  items_to_check[kUpdateItemId] = std::move(item);

  update_checker_->CheckForUpdates(
      items_to_check, "", true,
      base::Bind(&UpdateCheckerTest::UpdateCheckComplete,
                 base::Unretained(this)));

  RunThreads();

  EXPECT_EQ(1, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  ASSERT_EQ(1, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();

  // Sanity check the request.
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[0].find(
                              std::string("<app appid=\"") + kUpdateItemId +
                              "\" version=\"0.9\" "
                              "brand=\"TEST\"><updatecheck/><ping rd=\"-2\" "));
  EXPECT_NE(string::npos,
            post_interceptor_->GetRequests()[0].find(
                "<packages><package fp=\"fp1\"/></packages></app>"));

  // Expect an error since the response is not trusted.
  EXPECT_EQ(-10000, error_);
  EXPECT_EQ(0ul, results_.list.size());
}

// Tests that the UpdateCheckers will not make an update check for a
// component that requires encryption when the update check URL is unsecure.
TEST_F(UpdateCheckerTest, UpdateCheckRequiresEncryptionError) {
  config_->SetUpdateCheckUrl(GURL("http:\\foo\bar"));

  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  std::unique_ptr<CrxUpdateItem> item = BuildCrxUpdateItem();
  item->component.requires_network_encryption = true;
  IdToCrxUpdateItemMap items_to_check;
  items_to_check[kUpdateItemId] = std::move(item);

  update_checker_->CheckForUpdates(
      items_to_check, "", true,
      base::Bind(&UpdateCheckerTest::UpdateCheckComplete,
                 base::Unretained(this)));
  RunThreads();

  EXPECT_EQ(-1, error_);
  EXPECT_EQ(0u, results_.list.size());
}

// Tests that the PersistedData will get correctly update and reserialize
// the elapsed_days value.
TEST_F(UpdateCheckerTest, UpdateCheckDateLastRollCall) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      new PartialMatch("updatecheck"), test_file("updatecheck_reply_4.xml")));
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      new PartialMatch("updatecheck"), test_file("updatecheck_reply_4.xml")));

  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  std::unique_ptr<CrxUpdateItem> item = BuildCrxUpdateItem();
  IdToCrxUpdateItemMap items_to_check;
  items_to_check[kUpdateItemId] = std::move(item);

  // Do two update-checks.
  update_checker_->CheckForUpdates(
      items_to_check, "extra=\"params\"", true,
      base::Bind(&UpdateCheckerTest::UpdateCheckComplete,
                 base::Unretained(this)));
  RunThreads();
  update_checker_ = UpdateChecker::Create(config_, metadata_.get());
  update_checker_->CheckForUpdates(
      items_to_check, "extra=\"params\"", true,
      base::Bind(&UpdateCheckerTest::UpdateCheckComplete,
                 base::Unretained(this)));
  RunThreads();

  EXPECT_EQ(2, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  ASSERT_EQ(2, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[0].find(
                              "<ping rd=\"-2\" ping_freshness="));
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[1].find(
                              "<ping rd=\"3383\" ping_freshness="));
}

TEST_F(UpdateCheckerTest, UpdateCheckUpdateDisabled) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      new PartialMatch("updatecheck"), test_file("updatecheck_reply_1.xml")));

  config_->SetBrand("");
  update_checker_ = UpdateChecker::Create(config_, metadata_.get());

  std::unique_ptr<CrxUpdateItem> item = BuildCrxUpdateItem();
  CrxUpdateItem* item_ptr = item.get();

  // Tests the scenario where:
  //  * the component does not support group policies.
  //  * the component updates are disabled.
  // Expects the group policy to be ignored and the update check to not
  // include the "updatedisabled" attribute.
  EXPECT_FALSE(
      item_ptr->component.supports_group_policy_enable_component_updates);
  IdToCrxUpdateItemMap items_to_check;
  items_to_check[kUpdateItemId] = std::move(item);
  update_checker_->CheckForUpdates(
      items_to_check, "", false,
      base::Bind(&UpdateCheckerTest::UpdateCheckComplete,
                 base::Unretained(this)));
  RunThreads();
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[0].find(
                              std::string("<app appid=\"") + kUpdateItemId +
                              "\" version=\"0.9\">"
                              "<updatecheck/>"));

  // Tests the scenario where:
  //  * the component supports group policies.
  //  * the component updates are disabled.
  // Expects the update check to include the "updatedisabled" attribute.
  item_ptr->component.supports_group_policy_enable_component_updates = true;
  update_checker_ = UpdateChecker::Create(config_, metadata_.get());
  update_checker_->CheckForUpdates(
      items_to_check, "", false,
      base::Bind(&UpdateCheckerTest::UpdateCheckComplete,
                 base::Unretained(this)));
  RunThreads();
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[1].find(
                              std::string("<app appid=\"") + kUpdateItemId +
                              "\" version=\"0.9\">"
                              "<updatecheck updatedisabled=\"true\"/>"));

  // Tests the scenario where:
  //  * the component does not support group policies.
  //  * the component updates are enabled.
  // Expects the update check to not include the "updatedisabled" attribute.
  item_ptr->component.supports_group_policy_enable_component_updates = false;
  update_checker_ = UpdateChecker::Create(config_, metadata_.get());
  update_checker_->CheckForUpdates(
      items_to_check, "", true,
      base::Bind(&UpdateCheckerTest::UpdateCheckComplete,
                 base::Unretained(this)));
  RunThreads();
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[2].find(
                              std::string("<app appid=\"") + kUpdateItemId +
                              "\" version=\"0.9\">"
                              "<updatecheck/>"));

  // Tests the scenario where:
  //  * the component supports group policies.
  //  * the component updates are enabled.
  // Expects the update check to not include the "updatedisabled" attribute.
  item_ptr->component.supports_group_policy_enable_component_updates = true;
  update_checker_ = UpdateChecker::Create(config_, metadata_.get());
  update_checker_->CheckForUpdates(
      items_to_check, "", true,
      base::Bind(&UpdateCheckerTest::UpdateCheckComplete,
                 base::Unretained(this)));
  RunThreads();
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[3].find(
                              std::string("<app appid=\"") + kUpdateItemId +
                              "\" version=\"0.9\">"
                              "<updatecheck/>"));
}

}  // namespace update_client
