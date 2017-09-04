// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_fetcher_impl.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "components/password_manager/core/browser/stub_credentials_filter.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using autofill::PasswordForm;
using base::ASCIIToUTF16;
using base::StringPiece;
using testing::_;
using testing::IsEmpty;
using testing::Pointee;
using testing::UnorderedElementsAre;

namespace password_manager {

namespace {

class MockConsumer : public FormFetcher::Consumer {
 public:
  MOCK_METHOD2(ProcessMatches,
               void(const std::vector<const PasswordForm*>& non_federated,
                    size_t filtered_count));
};

class NameFilter : public StubCredentialsFilter {
 public:
  // This class filters out all credentials which have |name| as
  // |username_value|.
  explicit NameFilter(StringPiece name) : name_(ASCIIToUTF16(name)) {}

  ~NameFilter() override = default;

  std::vector<std::unique_ptr<PasswordForm>> FilterResults(
      std::vector<std::unique_ptr<PasswordForm>> results) const override {
    results.erase(
        std::remove_if(results.begin(), results.end(),
                       [this](const std::unique_ptr<PasswordForm>& form) {
                         return !ShouldSave(*form);
                       }),
        results.end());
    return results;
  }

  bool ShouldSave(const PasswordForm& form) const override {
    return form.username_value != name_;
  }

 private:
  const base::string16 name_;  // |username_value| to filter

  DISALLOW_COPY_AND_ASSIGN(NameFilter);
};

class FakePasswordManagerClient : public StubPasswordManagerClient {
 public:
  FakePasswordManagerClient() = default;
  ~FakePasswordManagerClient() override = default;

  void set_filter(std::unique_ptr<CredentialsFilter> filter) {
    filter_ = std::move(filter);
  }

 private:
  const CredentialsFilter* GetStoreResultFilter() const override {
    return filter_ ? filter_.get()
                   : StubPasswordManagerClient::GetStoreResultFilter();
  }

  std::unique_ptr<CredentialsFilter> filter_;

  DISALLOW_COPY_AND_ASSIGN(FakePasswordManagerClient);
};

// Creates a dummy non-federated form with some basic arbitrary values.
PasswordForm CreateNonFederated() {
  PasswordForm form;
  form.origin = GURL("https://example.in");
  form.signon_realm = form.origin.spec();
  form.action = GURL("https://login.example.org");
  form.username_value = ASCIIToUTF16("user");
  form.password_value = ASCIIToUTF16("password");
  return form;
}

// Creates a dummy federated form with some basic arbitrary values.
PasswordForm CreateFederated() {
  PasswordForm form = CreateNonFederated();
  form.password_value.clear();
  form.federation_origin = url::Origin(GURL("https://accounts.google.com/"));
  return form;
}

}  // namespace

class FormFetcherImplTest : public testing::Test {
 public:
  FormFetcherImplTest() : form_fetcher_(&client_) {}

  ~FormFetcherImplTest() override = default;

 protected:
  FakePasswordManagerClient client_;
  FormFetcherImpl form_fetcher_;
  testing::NiceMock<MockConsumer> consumer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FormFetcherImplTest);
};

// Check that no PasswordStore results are handled correctly.
TEST_F(FormFetcherImplTest, NoStoreResults) {
  EXPECT_CALL(consumer_, ProcessMatches(_, _)).Times(0);
  form_fetcher_.set_state(FormFetcher::State::WAITING);
  form_fetcher_.AddConsumer(&consumer_);
  EXPECT_EQ(FormFetcher::State::WAITING, form_fetcher_.GetState());
}

// Check that empty PasswordStore results are handled correctly.
TEST_F(FormFetcherImplTest, Empty) {
  form_fetcher_.AddConsumer(&consumer_);
  form_fetcher_.set_state(FormFetcher::State::WAITING);
  EXPECT_CALL(consumer_, ProcessMatches(IsEmpty(), 0u));
  form_fetcher_.SetResults(std::vector<std::unique_ptr<PasswordForm>>());
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_.GetState());
  EXPECT_THAT(form_fetcher_.GetFederatedMatches(), IsEmpty());
}

// Check that non-federated PasswordStore results are handled correctly.
TEST_F(FormFetcherImplTest, NonFederated) {
  PasswordForm non_federated = CreateNonFederated();
  form_fetcher_.AddConsumer(&consumer_);
  form_fetcher_.set_state(FormFetcher::State::WAITING);
  std::vector<std::unique_ptr<PasswordForm>> results;
  results.push_back(base::MakeUnique<PasswordForm>(non_federated));
  EXPECT_CALL(consumer_,
              ProcessMatches(UnorderedElementsAre(Pointee(non_federated)), 0u));
  form_fetcher_.SetResults(std::move(results));
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_.GetState());
  EXPECT_THAT(form_fetcher_.GetFederatedMatches(), IsEmpty());
}

// Check that federated PasswordStore results are handled correctly.
TEST_F(FormFetcherImplTest, Federated) {
  PasswordForm federated = CreateFederated();
  form_fetcher_.AddConsumer(&consumer_);
  form_fetcher_.set_state(FormFetcher::State::WAITING);
  std::vector<std::unique_ptr<PasswordForm>> results;
  results.push_back(base::MakeUnique<PasswordForm>(federated));
  EXPECT_CALL(consumer_, ProcessMatches(IsEmpty(), 0u));
  form_fetcher_.SetResults(std::move(results));
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_.GetState());
  EXPECT_THAT(form_fetcher_.GetFederatedMatches(),
              UnorderedElementsAre(Pointee(federated)));
}

// Check that mixed PasswordStore results are handled correctly.
TEST_F(FormFetcherImplTest, Mixed) {
  PasswordForm federated1 = CreateFederated();
  federated1.username_value = ASCIIToUTF16("user");
  PasswordForm federated2 = CreateFederated();
  federated2.username_value = ASCIIToUTF16("user_B");
  PasswordForm non_federated1 = CreateNonFederated();
  non_federated1.username_value = ASCIIToUTF16("user");
  PasswordForm non_federated2 = CreateNonFederated();
  non_federated2.username_value = ASCIIToUTF16("user_C");
  PasswordForm non_federated3 = CreateNonFederated();
  non_federated3.username_value = ASCIIToUTF16("user_D");

  form_fetcher_.AddConsumer(&consumer_);
  form_fetcher_.set_state(FormFetcher::State::WAITING);
  std::vector<std::unique_ptr<PasswordForm>> results;
  results.push_back(base::MakeUnique<PasswordForm>(federated1));
  results.push_back(base::MakeUnique<PasswordForm>(federated2));
  results.push_back(base::MakeUnique<PasswordForm>(non_federated1));
  results.push_back(base::MakeUnique<PasswordForm>(non_federated2));
  results.push_back(base::MakeUnique<PasswordForm>(non_federated3));
  EXPECT_CALL(consumer_,
              ProcessMatches(UnorderedElementsAre(Pointee(non_federated1),
                                                  Pointee(non_federated2),
                                                  Pointee(non_federated3)),
                             0u));
  form_fetcher_.SetResults(std::move(results));
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_.GetState());
  EXPECT_THAT(form_fetcher_.GetFederatedMatches(),
              UnorderedElementsAre(Pointee(federated1), Pointee(federated2)));
}

// Check that PasswordStore results are filtered correctly.
TEST_F(FormFetcherImplTest, Filtered) {
  PasswordForm federated = CreateFederated();
  federated.username_value = ASCIIToUTF16("user");
  PasswordForm non_federated1 = CreateNonFederated();
  non_federated1.username_value = ASCIIToUTF16("user");
  PasswordForm non_federated2 = CreateNonFederated();
  non_federated2.username_value = ASCIIToUTF16("user_C");

  // Set up a filter to remove all credentials with the username "user".
  client_.set_filter(base::MakeUnique<NameFilter>("user"));

  form_fetcher_.AddConsumer(&consumer_);
  form_fetcher_.set_state(FormFetcher::State::WAITING);
  std::vector<std::unique_ptr<PasswordForm>> results;
  results.push_back(base::MakeUnique<PasswordForm>(federated));
  results.push_back(base::MakeUnique<PasswordForm>(non_federated1));
  results.push_back(base::MakeUnique<PasswordForm>(non_federated2));
  // Non-federated results should have been filtered: no "user" here.
  constexpr size_t kNumFiltered = 1u;
  EXPECT_CALL(consumer_,
              ProcessMatches(UnorderedElementsAre(Pointee(non_federated2)),
                             kNumFiltered));
  form_fetcher_.SetResults(std::move(results));
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_.GetState());
  // However, federated results should not be filtered.
  EXPECT_THAT(form_fetcher_.GetFederatedMatches(),
              UnorderedElementsAre(Pointee(federated)));
}

// Check that stats from PasswordStore are handled correctly.
TEST_F(FormFetcherImplTest, Stats) {
  form_fetcher_.AddConsumer(&consumer_);
  std::vector<std::unique_ptr<InteractionsStats>> stats;
  stats.push_back(base::MakeUnique<InteractionsStats>());
  form_fetcher_.SetStats(std::move(stats));
  EXPECT_EQ(1u, form_fetcher_.GetInteractionsStats().size());
}

}  // namespace password_manager
