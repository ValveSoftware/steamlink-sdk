// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/test_signin_client.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/signin/core/browser/webdata/token_service_table.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_database_service.h"
#include "testing/gtest/include/gtest/gtest.h"

TestSigninClient::TestSigninClient(PrefService* pref_service)
    : pref_service_(pref_service), are_signin_cookies_allowed_(true) {}

TestSigninClient::~TestSigninClient() {}

void TestSigninClient::DoFinalInit() {}

PrefService* TestSigninClient::GetPrefs() {
  return pref_service_;
}

scoped_refptr<TokenWebData> TestSigninClient::GetDatabase() {
  return database_;
}

bool TestSigninClient::CanRevokeCredentials() { return true; }

std::string TestSigninClient::GetSigninScopedDeviceId() {
  return std::string();
}

void TestSigninClient::OnSignedOut() {}

void TestSigninClient::PostSignedIn(const std::string& account_id,
                  const std::string& username,
                  const std::string& password) {
  signed_in_password_ = password;
}

net::URLRequestContextGetter* TestSigninClient::GetURLRequestContext() {
  return request_context_.get();
}

void TestSigninClient::SetURLRequestContext(
    net::URLRequestContextGetter* request_context) {
  request_context_ = request_context;
}

std::string TestSigninClient::GetProductVersion() { return ""; }

void TestSigninClient::LoadTokenDatabase() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath path = temp_dir_.path().AppendASCII("TestWebDB");
  scoped_refptr<WebDatabaseService> web_database =
      new WebDatabaseService(path, base::ThreadTaskRunnerHandle::Get(),
                             base::ThreadTaskRunnerHandle::Get());
  web_database->AddTable(base::WrapUnique(new TokenServiceTable()));
  web_database->LoadDatabase();
  database_ =
      new TokenWebData(web_database, base::ThreadTaskRunnerHandle::Get(),
                       base::ThreadTaskRunnerHandle::Get(),
                       WebDataServiceBase::ProfileErrorCallback());
  database_->Init();
}

bool TestSigninClient::ShouldMergeSigninCredentialsIntoCookieJar() {
  return true;
}

std::unique_ptr<SigninClient::CookieChangedSubscription>
TestSigninClient::AddCookieChangedCallback(
    const GURL& url,
    const std::string& name,
    const net::CookieStore::CookieChangedCallback& callback) {
  return base::WrapUnique(new SigninClient::CookieChangedSubscription);
}

bool TestSigninClient::IsFirstRun() const {
  return false;
}

base::Time TestSigninClient::GetInstallDate() {
  return base::Time::Now();
}

bool TestSigninClient::AreSigninCookiesAllowed() {
  return are_signin_cookies_allowed_;
}

void TestSigninClient::AddContentSettingsObserver(
    content_settings::Observer* observer) {
}

void TestSigninClient::RemoveContentSettingsObserver(
    content_settings::Observer* observer) {
}

void TestSigninClient::DelayNetworkCall(const base::Closure& callback) {
  callback.Run();
}

GaiaAuthFetcher* TestSigninClient::CreateGaiaAuthFetcher(
    GaiaAuthConsumer* consumer,
    const std::string& source,
    net::URLRequestContextGetter* getter) {
  return new GaiaAuthFetcher(consumer, source, getter);
}
