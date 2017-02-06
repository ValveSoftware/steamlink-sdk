// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing_db/v4_update_protocol_manager.h"

#include <memory>
#include <vector>

#include "base/base64.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/safe_browsing_db/safebrowsing.pb.h"
#include "components/safe_browsing_db/util.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using base::Time;
using base::TimeDelta;

namespace {

const char kClient[] = "unittest";
const char kAppVer[] = "1.0";
const char kKeyParam[] = "test_key_param";

}  // namespace

namespace safe_browsing {

class V4UpdateProtocolManagerTest : public PlatformTest {
  void SetUp() override {
    PlatformTest::SetUp();

    SetupStoreStates();
  }

 protected:
  void ValidateGetUpdatesResults(
      const std::vector<ListUpdateResponse>& expected_lurs,
      std::unique_ptr<ParsedServerResponse> parsed_server_response) {
    // The callback should never be called if expect_callback_to_be_called_ is
    // false.
    EXPECT_TRUE(expect_callback_to_be_called_);
    ASSERT_EQ(expected_lurs.size(), parsed_server_response->size());

    for (unsigned int i = 0; i < parsed_server_response->size(); ++i) {
      const ListUpdateResponse& expected = expected_lurs[i];
      const std::unique_ptr<ListUpdateResponse>& actual =
          (*parsed_server_response)[i];
      EXPECT_EQ(expected.platform_type(), actual->platform_type());
      EXPECT_EQ(expected.response_type(), actual->response_type());
      EXPECT_EQ(expected.threat_entry_type(), actual->threat_entry_type());
      EXPECT_EQ(expected.threat_type(), actual->threat_type());
      EXPECT_EQ(expected.new_client_state(), actual->new_client_state());

      // TODO(vakh): Test more fields from the proto.
    }
  }

  std::unique_ptr<V4UpdateProtocolManager> CreateProtocolManager(
      const std::vector<ListUpdateResponse>& expected_lurs) {
    V4ProtocolConfig config;
    config.client_name = kClient;
    config.version = kAppVer;
    config.key_param = kKeyParam;
    config.disable_auto_update = false;
    return V4UpdateProtocolManager::Create(
        NULL, config,
        base::Bind(&V4UpdateProtocolManagerTest::ValidateGetUpdatesResults,
                   base::Unretained(this), expected_lurs));
  }

  void SetupStoreStates() {
    store_state_map_ = base::MakeUnique<StoreStateMap>();

    UpdateListIdentifier win_url_malware(WINDOWS_PLATFORM, URL, MALWARE_THREAT);
    store_state_map_->insert({win_url_malware, "initial_state_1"});

    UpdateListIdentifier win_url_uws(WINDOWS_PLATFORM, URL, UNWANTED_SOFTWARE);
    store_state_map_->insert({win_url_uws, "initial_state_2"});

    UpdateListIdentifier win_exe_uws(WINDOWS_PLATFORM, EXECUTABLE,
                                     UNWANTED_SOFTWARE);
    store_state_map_->insert({win_exe_uws, "initial_state_3"});
  }

  void SetupExpectedListUpdateResponse(
      std::vector<ListUpdateResponse>* expected_lurs) {
    ListUpdateResponse lur;
    lur.set_platform_type(WINDOWS_PLATFORM);
    lur.set_response_type(ListUpdateResponse::PARTIAL_UPDATE);
    lur.set_threat_entry_type(URL);
    lur.set_threat_type(MALWARE_THREAT);
    lur.set_new_client_state("new_state_1");
    expected_lurs->push_back(lur);

    lur.set_platform_type(WINDOWS_PLATFORM);
    lur.set_response_type(ListUpdateResponse::PARTIAL_UPDATE);
    lur.set_threat_entry_type(URL);
    lur.set_threat_type(UNWANTED_SOFTWARE);
    lur.set_new_client_state("new_state_2");
    expected_lurs->push_back(lur);

    lur.set_platform_type(WINDOWS_PLATFORM);
    lur.set_response_type(ListUpdateResponse::FULL_UPDATE);
    lur.set_threat_entry_type(EXECUTABLE);
    lur.set_threat_type(MALWARE_THREAT);
    lur.set_new_client_state("new_state_3");
    expected_lurs->push_back(lur);
  }

  std::string GetExpectedV4UpdateResponse(
      std::vector<ListUpdateResponse>& expected_lurs) const {
    FetchThreatListUpdatesResponse response;

    for (const auto& expected_lur : expected_lurs) {
      ListUpdateResponse* lur = response.add_list_update_responses();
      lur->set_new_client_state(expected_lur.new_client_state());
      lur->set_platform_type(expected_lur.platform_type());
      lur->set_response_type(expected_lur.response_type());
      lur->set_threat_entry_type(expected_lur.threat_entry_type());
      lur->set_threat_type(expected_lur.threat_type());
    }

    // Serialize.
    std::string res_data;
    response.SerializeToString(&res_data);

    return res_data;
  }

  bool expect_callback_to_be_called_;
  std::unique_ptr<StoreStateMap> store_state_map_;
};

// TODO(vakh): Add many more tests.
TEST_F(V4UpdateProtocolManagerTest, TestGetUpdatesErrorHandlingNetwork) {
  scoped_refptr<base::TestSimpleTaskRunner> runner(
      new base::TestSimpleTaskRunner());
  base::ThreadTaskRunnerHandle runner_handler(runner);
  net::TestURLFetcherFactory factory;
  const std::vector<ListUpdateResponse> expected_lurs;
  std::unique_ptr<V4UpdateProtocolManager> pm(
      CreateProtocolManager(expected_lurs));
  runner->ClearPendingTasks();

  // Initial state. No errors.
  EXPECT_EQ(0ul, pm->update_error_count_);
  EXPECT_EQ(1ul, pm->update_back_off_mult_);
  expect_callback_to_be_called_ = false;
  pm->store_state_map_ = std::move(store_state_map_);
  pm->IssueUpdateRequest();

  EXPECT_FALSE(pm->IsUpdateScheduled());

  runner->RunPendingTasks();

  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  DCHECK(fetcher);
  // Failed request status should result in error.
  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                            net::ERR_CONNECTION_RESET));
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  // Should have recorded one error, but back off multiplier is unchanged.
  EXPECT_EQ(1ul, pm->update_error_count_);
  EXPECT_EQ(1ul, pm->update_back_off_mult_);
  EXPECT_TRUE(pm->IsUpdateScheduled());
}

TEST_F(V4UpdateProtocolManagerTest, TestGetUpdatesErrorHandlingResponseCode) {
  scoped_refptr<base::TestSimpleTaskRunner> runner(
      new base::TestSimpleTaskRunner());
  base::ThreadTaskRunnerHandle runner_handler(runner);
  net::TestURLFetcherFactory factory;
  const std::vector<ListUpdateResponse> expected_lurs;
  std::unique_ptr<V4UpdateProtocolManager> pm(
      CreateProtocolManager(expected_lurs));
  runner->ClearPendingTasks();

  // Initial state. No errors.
  EXPECT_EQ(0ul, pm->update_error_count_);
  EXPECT_EQ(1ul, pm->update_back_off_mult_);
  expect_callback_to_be_called_ = false;
  pm->store_state_map_ = std::move(store_state_map_);
  pm->IssueUpdateRequest();

  EXPECT_FALSE(pm->IsUpdateScheduled());

  runner->RunPendingTasks();

  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  DCHECK(fetcher);
  fetcher->set_status(net::URLRequestStatus());
  // Response code of anything other than 200 should result in error.
  fetcher->set_response_code(net::HTTP_NO_CONTENT);
  fetcher->SetResponseString("");
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  // Should have recorded one error, but back off multiplier is unchanged.
  EXPECT_EQ(1ul, pm->update_error_count_);
  EXPECT_EQ(1ul, pm->update_back_off_mult_);
  EXPECT_TRUE(pm->IsUpdateScheduled());
}

TEST_F(V4UpdateProtocolManagerTest, TestGetUpdatesNoError) {
  scoped_refptr<base::TestSimpleTaskRunner> runner(
      new base::TestSimpleTaskRunner());
  base::ThreadTaskRunnerHandle runner_handler(runner);
  net::TestURLFetcherFactory factory;
  std::vector<ListUpdateResponse> expected_lurs;
  SetupExpectedListUpdateResponse(&expected_lurs);
  std::unique_ptr<V4UpdateProtocolManager> pm(
      CreateProtocolManager(expected_lurs));
  runner->ClearPendingTasks();

  // Initial state. No errors.
  EXPECT_EQ(0ul, pm->update_error_count_);
  EXPECT_EQ(1ul, pm->update_back_off_mult_);
  expect_callback_to_be_called_ = true;
  pm->store_state_map_ = std::move(store_state_map_);
  pm->IssueUpdateRequest();

  EXPECT_FALSE(pm->IsUpdateScheduled());

  runner->RunPendingTasks();

  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  DCHECK(fetcher);
  fetcher->set_status(net::URLRequestStatus());
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetExpectedV4UpdateResponse(expected_lurs));
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  // No error, back off multiplier is unchanged.
  EXPECT_EQ(0ul, pm->update_error_count_);
  EXPECT_EQ(1ul, pm->update_back_off_mult_);
  EXPECT_FALSE(pm->IsUpdateScheduled());
}

TEST_F(V4UpdateProtocolManagerTest, TestGetUpdatesWithOneBackoff) {
  scoped_refptr<base::TestSimpleTaskRunner> runner(
      new base::TestSimpleTaskRunner());
  base::ThreadTaskRunnerHandle runner_handler(runner);
  net::TestURLFetcherFactory factory;
  std::vector<ListUpdateResponse> expected_lurs;
  SetupExpectedListUpdateResponse(&expected_lurs);
  std::unique_ptr<V4UpdateProtocolManager> pm(
      CreateProtocolManager(expected_lurs));
  runner->ClearPendingTasks();

  // Initial state. No errors.
  EXPECT_EQ(0ul, pm->update_error_count_);
  EXPECT_EQ(1ul, pm->update_back_off_mult_);
  expect_callback_to_be_called_ = false;
  pm->store_state_map_ = std::move(store_state_map_);
  pm->IssueUpdateRequest();

  EXPECT_FALSE(pm->IsUpdateScheduled());

  runner->RunPendingTasks();

  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  DCHECK(fetcher);
  fetcher->set_status(net::URLRequestStatus());
  // Response code of anything other than 200 should result in error.
  fetcher->set_response_code(net::HTTP_NO_CONTENT);
  fetcher->SetResponseString("");
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  // Should have recorded one error, but back off multiplier is unchanged.
  EXPECT_EQ(1ul, pm->update_error_count_);
  EXPECT_EQ(1ul, pm->update_back_off_mult_);
  EXPECT_TRUE(pm->IsUpdateScheduled());

  // Retry, now no backoff.
  expect_callback_to_be_called_ = true;
  runner->RunPendingTasks();

  fetcher = factory.GetFetcherByID(1);
  DCHECK(fetcher);
  fetcher->set_status(net::URLRequestStatus());
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetExpectedV4UpdateResponse(expected_lurs));
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  // No error, back off multiplier is unchanged.
  EXPECT_EQ(0ul, pm->update_error_count_);
  EXPECT_EQ(1ul, pm->update_back_off_mult_);
  EXPECT_FALSE(pm->IsUpdateScheduled());
}

TEST_F(V4UpdateProtocolManagerTest, TestBase64EncodingUsesUrlEncoding) {
  // NOTE(vakh): I handpicked this value for state by generating random strings
  // and picked the one that leads to a '-' in the base64 url encoded request
  // output.
  std::string state_minus = "z-R~3ruViQH";
  StoreStateMap store_state_map_minus{
      {UpdateListIdentifier(LINUX_PLATFORM, URL, MALWARE_THREAT), state_minus}};
  std::string encoded_request_with_minus =
      V4UpdateProtocolManager::GetBase64SerializedUpdateRequestProto(
          store_state_map_minus);
  EXPECT_EQ("GhcIARACGgt6LVJ-M3J1VmlRSCICIAEoAQ==", encoded_request_with_minus);

  // NOTE(vakh): Same process for chosing this string. I am representing it
  // in base64 encoded form because the actual state value contains non-ASCII
  // characters.
  std::string base64_encoded_state_underscore = "VTFfITBf4lBM";
  std::string state_underscore;
  base::Base64Decode(base64_encoded_state_underscore, &state_underscore);
  StoreStateMap store_state_map_underscore{
      {UpdateListIdentifier(LINUX_PLATFORM, URL, MALWARE_THREAT),
       state_underscore}};
  std::string encoded_request_with_underscore =
      V4UpdateProtocolManager::GetBase64SerializedUpdateRequestProto(
          store_state_map_underscore);
  EXPECT_EQ("GhUIARACGglVMV8hMF_iUEwiAiABKAE=",
            encoded_request_with_underscore);
}

}  // namespace safe_browsing
