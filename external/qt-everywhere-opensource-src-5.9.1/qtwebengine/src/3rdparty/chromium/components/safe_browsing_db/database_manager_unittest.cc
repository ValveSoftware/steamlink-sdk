// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing_db/database_manager.h"

#include <stddef.h>

#include <set>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/safe_browsing_db/test_database_manager.h"
#include "components/safe_browsing_db/v4_protocol_manager_util.h"
#include "components/safe_browsing_db/v4_test_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "crypto/sha2.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace safe_browsing {

namespace {

class TestClient : public SafeBrowsingDatabaseManager::Client {
 public:
  TestClient() : callback_invoked_(false) {}
  ~TestClient() override {}

  void OnCheckApiBlacklistUrlResult(const GURL& url,
                                    const ThreatMetadata& metadata) override {
    blocked_permissions_ = metadata.api_permissions;
    callback_invoked_ = true;
  }

  const std::set<std::string>& GetBlockedPermissions() {
    return blocked_permissions_;
  }

  bool callback_invoked() {return callback_invoked_;}

 private:
  std::set<std::string> blocked_permissions_;
  bool callback_invoked_;
  DISALLOW_COPY_AND_ASSIGN(TestClient);
};

}  // namespace

class SafeBrowsingDatabaseManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    db_manager_ = new TestSafeBrowsingDatabaseManager();
    db_manager_->StartOnIOThread(NULL, GetTestV4ProtocolConfig());
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    db_manager_->StopOnIOThread(false);
  }

  std::string GetStockV4GetHashResponse() {
    ListIdentifier list_id = GetChromeUrlApiId();
    FullHash full_hash = crypto::SHA256HashString("example.com/");

    FindFullHashesResponse response;
    response.mutable_negative_cache_duration()->set_seconds(600);
    ThreatMatch* m = response.add_matches();
    m->set_platform_type(list_id.platform_type());
    m->set_threat_entry_type(list_id.threat_entry_type());
    m->set_threat_type(list_id.threat_type());
    m->mutable_threat()->set_hash(full_hash);
    m->mutable_cache_duration()->set_seconds(300);

    ThreatEntryMetadata::MetadataEntry* e =
        m->mutable_threat_entry_metadata()->add_entries();
    e->set_key("permission");
    e->set_value("GEOLOCATION");

    std::string res_data;
    response.SerializeToString(&res_data);
    return res_data;
  }

  scoped_refptr<SafeBrowsingDatabaseManager> db_manager_;

 private:
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
};

TEST_F(SafeBrowsingDatabaseManagerTest, CheckApiBlacklistUrlWrongScheme) {
  EXPECT_TRUE(
      db_manager_->CheckApiBlacklistUrl(GURL("file://example.txt"), nullptr));
}

TEST_F(SafeBrowsingDatabaseManagerTest, CancelApiCheck) {
  net::TestURLFetcherFactory factory;
  TestClient client;
  const GURL url("https://www.example.com/more");

  EXPECT_FALSE(db_manager_->CheckApiBlacklistUrl(url, &client));
  EXPECT_TRUE(db_manager_->CancelApiCheck(&client));

  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  DCHECK(fetcher);
  fetcher->set_status(net::URLRequestStatus());
  fetcher->set_response_code(200);
  fetcher->SetResponseString(GetStockV4GetHashResponse());
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(client.callback_invoked());
  EXPECT_EQ(0ul, client.GetBlockedPermissions().size());
}

TEST_F(SafeBrowsingDatabaseManagerTest, GetApiCheckResponse) {
  net::TestURLFetcherFactory factory;
  TestClient client;
  const GURL url("https://www.example.com/more");

  EXPECT_FALSE(db_manager_->CheckApiBlacklistUrl(url, &client));

  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  DCHECK(fetcher);
  fetcher->set_status(net::URLRequestStatus());
  fetcher->set_response_code(200);
  fetcher->SetResponseString(GetStockV4GetHashResponse());
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(client.callback_invoked());
  ASSERT_EQ(1ul, client.GetBlockedPermissions().size());
  EXPECT_EQ("GEOLOCATION", *(client.GetBlockedPermissions().begin()));
}

}  // namespace safe_browsing
