// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "google_apis/gaia/fake_oauth2_token_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/merge_session_helper.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockObserver : public MergeSessionHelper::Observer {
 public:
  explicit MockObserver(MergeSessionHelper* helper) : helper_(helper) {
    helper_->AddObserver(this);
  }

  ~MockObserver() {
    helper_->RemoveObserver(this);
  }

  MOCK_METHOD2(MergeSessionCompleted,
               void(const std::string&,
                    const GoogleServiceAuthError& ));
 private:
  MergeSessionHelper* helper_;

  DISALLOW_COPY_AND_ASSIGN(MockObserver);
};

// Counts number of InstrumentedMergeSessionHelper created.
// We can EXPECT_* to be zero at the end of our unit tests
// to make sure everything is properly deleted.

int total = 0;

class InstrumentedMergeSessionHelper : public MergeSessionHelper {
 public:
  InstrumentedMergeSessionHelper(
      OAuth2TokenService* token_service,
      net::URLRequestContextGetter* request_context) :
    MergeSessionHelper(token_service, request_context, NULL) {
    total++;
  }

  virtual ~InstrumentedMergeSessionHelper() {
    total--;
  }

  MOCK_METHOD0(StartFetching, void());
  MOCK_METHOD0(StartLogOutUrlFetch, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(InstrumentedMergeSessionHelper);
};

class MergeSessionHelperTest : public testing::Test {
 public:
   MergeSessionHelperTest()
       : no_error_(GoogleServiceAuthError::NONE),
         error_(GoogleServiceAuthError::SERVICE_ERROR),
         canceled_(GoogleServiceAuthError::REQUEST_CANCELED),
         request_context_getter_(new net::TestURLRequestContextGetter(
             base::MessageLoopProxy::current())) {}

  OAuth2TokenService* token_service() { return &token_service_; }
  net::URLRequestContextGetter* request_context() {
    return request_context_getter_;
  }

  void SimulateUbertokenFailure(UbertokenConsumer* consumer,
                                const GoogleServiceAuthError& error) {
    consumer->OnUbertokenFailure(error);
  }

  void SimulateMergeSessionSuccess(GaiaAuthConsumer* consumer,
                                   const std::string& data) {
    consumer->OnMergeSessionSuccess(data);
  }

  void SimulateMergeSessionFailure(GaiaAuthConsumer* consumer,
                                   const GoogleServiceAuthError& error) {
    consumer->OnMergeSessionFailure(error);
  }

  void SimulateLogoutSuccess(net::URLFetcherDelegate* consumer) {
    consumer->OnURLFetchComplete(NULL);
  }

  const GoogleServiceAuthError& no_error() { return no_error_; }
  const GoogleServiceAuthError& error() { return error_; }
  const GoogleServiceAuthError& canceled() { return canceled_; }

 private:
  base::MessageLoop message_loop_;
  net::TestURLFetcherFactory factory_;
  FakeOAuth2TokenService token_service_;
  GoogleServiceAuthError no_error_;
  GoogleServiceAuthError error_;
  GoogleServiceAuthError canceled_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
};

} // namespace

using ::testing::_;

TEST_F(MergeSessionHelperTest, Success) {
  InstrumentedMergeSessionHelper helper(token_service(), request_context());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetching());
  EXPECT_CALL(observer, MergeSessionCompleted("acc1@gmail.com", no_error()));

  helper.LogIn("acc1@gmail.com");
  SimulateMergeSessionSuccess(&helper, "token");
}

TEST_F(MergeSessionHelperTest, FailedMergeSession) {
  InstrumentedMergeSessionHelper helper(token_service(), request_context());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetching());
  EXPECT_CALL(observer, MergeSessionCompleted("acc1@gmail.com", error()));

  helper.LogIn("acc1@gmail.com");
  SimulateMergeSessionFailure(&helper, error());
}

TEST_F(MergeSessionHelperTest, FailedUbertoken) {
  InstrumentedMergeSessionHelper helper(token_service(), request_context());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetching());
  EXPECT_CALL(observer, MergeSessionCompleted("acc1@gmail.com", error()));

  helper.LogIn("acc1@gmail.com");
  SimulateUbertokenFailure(&helper, error());
}

TEST_F(MergeSessionHelperTest, ContinueAfterSuccess) {
  InstrumentedMergeSessionHelper helper(token_service(), request_context());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetching()).Times(2);
  EXPECT_CALL(observer, MergeSessionCompleted("acc1@gmail.com", no_error()));
  EXPECT_CALL(observer, MergeSessionCompleted("acc2@gmail.com", no_error()));

  helper.LogIn("acc1@gmail.com");
  helper.LogIn("acc2@gmail.com");
  SimulateMergeSessionSuccess(&helper, "token1");
  SimulateMergeSessionSuccess(&helper, "token2");
}

TEST_F(MergeSessionHelperTest, ContinueAfterFailure1) {
  InstrumentedMergeSessionHelper helper(token_service(), request_context());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetching()).Times(2);
  EXPECT_CALL(observer, MergeSessionCompleted("acc1@gmail.com", error()));
  EXPECT_CALL(observer, MergeSessionCompleted("acc2@gmail.com", no_error()));

  helper.LogIn("acc1@gmail.com");
  helper.LogIn("acc2@gmail.com");
  SimulateMergeSessionFailure(&helper, error());
  SimulateMergeSessionSuccess(&helper, "token2");
}

TEST_F(MergeSessionHelperTest, ContinueAfterFailure2) {
  InstrumentedMergeSessionHelper helper(token_service(), request_context());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetching()).Times(2);
  EXPECT_CALL(observer, MergeSessionCompleted("acc1@gmail.com", error()));
  EXPECT_CALL(observer, MergeSessionCompleted("acc2@gmail.com", no_error()));

  helper.LogIn("acc1@gmail.com");
  helper.LogIn("acc2@gmail.com");
  SimulateUbertokenFailure(&helper, error());
  SimulateMergeSessionSuccess(&helper, "token2");
}

TEST_F(MergeSessionHelperTest, AllRequestsInMultipleGoes) {
  InstrumentedMergeSessionHelper helper(token_service(), request_context());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetching()).Times(4);
  EXPECT_CALL(observer, MergeSessionCompleted(_, no_error())).Times(4);

  helper.LogIn("acc1@gmail.com");
  helper.LogIn("acc2@gmail.com");

  SimulateMergeSessionSuccess(&helper, "token1");

  helper.LogIn("acc3@gmail.com");

  SimulateMergeSessionSuccess(&helper, "token2");
  SimulateMergeSessionSuccess(&helper, "token3");

  helper.LogIn("acc4@gmail.com");

  SimulateMergeSessionSuccess(&helper, "token4");
}

TEST_F(MergeSessionHelperTest, LogOut) {
  InstrumentedMergeSessionHelper helper(token_service(), request_context());
  MockObserver observer(&helper);

  std::vector<std::string> current_accounts;
  current_accounts.push_back("acc1@gmail.com");
  current_accounts.push_back("acc2@gmail.com");
  current_accounts.push_back("acc3@gmail.com");

  EXPECT_CALL(helper, StartLogOutUrlFetch());
  EXPECT_CALL(helper, StartFetching()).Times(2);
  EXPECT_CALL(observer, MergeSessionCompleted("acc1@gmail.com", no_error()));
  EXPECT_CALL(observer, MergeSessionCompleted("acc3@gmail.com", no_error()));

  helper.LogOut("acc2@gmail.com", current_accounts);
  SimulateLogoutSuccess(&helper);
  SimulateMergeSessionSuccess(&helper, "token1");
  SimulateMergeSessionSuccess(&helper, "token3");
}

TEST_F(MergeSessionHelperTest, PendingSigninThenSignout) {
  InstrumentedMergeSessionHelper helper(token_service(), request_context());
  MockObserver observer(&helper);

  std::vector<std::string> current_accounts;
  current_accounts.push_back("acc2@gmail.com");
  current_accounts.push_back("acc3@gmail.com");

  // From the first Signin.
  EXPECT_CALL(observer, MergeSessionCompleted("acc1@gmail.com", no_error()));

  // From the sign out and then re-sign in.
  EXPECT_CALL(helper, StartLogOutUrlFetch());
  EXPECT_CALL(observer, MergeSessionCompleted("acc3@gmail.com", no_error()));

  // Total sign in 2 times, not enforcing ordered sequences.
  EXPECT_CALL(helper, StartFetching()).Times(2);

  helper.LogIn("acc1@gmail.com");
  helper.LogOut("acc2@gmail.com", current_accounts);

  SimulateMergeSessionSuccess(&helper, "token1");
  SimulateLogoutSuccess(&helper);
  SimulateMergeSessionSuccess(&helper, "token3");
}

TEST_F(MergeSessionHelperTest, CancelSignIn) {
  InstrumentedMergeSessionHelper helper(token_service(), request_context());
  MockObserver observer(&helper);

  std::vector<std::string> current_accounts;

  EXPECT_CALL(helper, StartFetching());
  EXPECT_CALL(observer, MergeSessionCompleted("acc2@gmail.com", canceled()));
  EXPECT_CALL(observer, MergeSessionCompleted("acc1@gmail.com", no_error()));
  EXPECT_CALL(helper, StartLogOutUrlFetch());

  helper.LogIn("acc1@gmail.com");
  helper.LogIn("acc2@gmail.com");
  helper.LogOut("acc2@gmail.com", current_accounts);

  SimulateMergeSessionSuccess(&helper, "token1");
  SimulateLogoutSuccess(&helper);
}

TEST_F(MergeSessionHelperTest, DoubleSignout) {
  InstrumentedMergeSessionHelper helper(token_service(), request_context());
  MockObserver observer(&helper);

  std::vector<std::string> current_accounts1;
  current_accounts1.push_back("acc1@gmail.com");
  current_accounts1.push_back("acc2@gmail.com");
  current_accounts1.push_back("acc3@gmail.com");

  std::vector<std::string> current_accounts2;
  current_accounts2.push_back("acc1@gmail.com");
  current_accounts2.push_back("acc3@gmail.com");

  EXPECT_CALL(helper, StartFetching()).Times(2);
  EXPECT_CALL(observer, MergeSessionCompleted("acc3@gmail.com", canceled()));
  EXPECT_CALL(observer,
      MergeSessionCompleted("acc1@gmail.com", no_error())).Times(2);
  EXPECT_CALL(helper, StartLogOutUrlFetch());

  helper.LogIn("acc1@gmail.com");
  helper.LogOut("acc2@gmail.com", current_accounts1);
  helper.LogOut("acc3@gmail.com", current_accounts2);

  SimulateMergeSessionSuccess(&helper, "token1");
  SimulateLogoutSuccess(&helper);
  SimulateMergeSessionSuccess(&helper, "token1");
}
