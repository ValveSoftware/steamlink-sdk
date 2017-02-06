// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_queue_store.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/background/request_queue_in_memory_store.h"
#include "components/offline_pages/background/request_queue_store_sql.h"
#include "components/offline_pages/background/save_page_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

using UpdateStatus = RequestQueueStore::UpdateStatus;

namespace {
const int64_t kRequestId = 42;
const GURL kUrl("http://example.com");
const ClientId kClientId("bookmark", "1234");

enum class LastResult {
  kNone,
  kFalse,
  kTrue,
};

bool operator==(const SavePageRequest& lhs, const SavePageRequest& rhs) {
  return lhs.request_id() == rhs.request_id() && lhs.url() == rhs.url() &&
         lhs.client_id() == rhs.client_id() &&
         lhs.creation_time() == rhs.creation_time() &&
         lhs.activation_time() == rhs.activation_time() &&
         lhs.attempt_count() == rhs.attempt_count() &&
         lhs.last_attempt_time() == rhs.last_attempt_time();
}

}  // namespace

// Class that serves as a base for testing different implementations of the
// |RequestQueueStore|. Specific implementations extend the templatized version
// of this class and provide appropriate store factory.
class RequestQueueStoreTestBase : public testing::Test {
 public:
  RequestQueueStoreTestBase();

  // Test overrides.
  void TearDown() override;

  void PumpLoop();
  void ClearResults();

  // Callback used for get requests.
  void GetRequestsDone(bool result,
                       const std::vector<SavePageRequest>& requests);
  // Callback used for add/update request.
  void AddOrUpdateDone(UpdateStatus result);
  // Callback used for remove requests.
  void RemoveDone(bool result, int count);
  // Callback used for reset.
  void ResetDone(bool result);

  LastResult last_result() const { return last_result_; }
  UpdateStatus last_update_status() const { return last_update_status_; }
  int last_remove_count() const { return last_remove_count_; }
  const std::vector<SavePageRequest>& last_requests() const {
    return last_requests_;
  }

 protected:
  base::ScopedTempDir temp_directory_;

 private:
  LastResult last_result_;
  UpdateStatus last_update_status_;
  int last_remove_count_;
  std::vector<SavePageRequest> last_requests_;

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
};

RequestQueueStoreTestBase::RequestQueueStoreTestBase()
    : last_result_(LastResult::kNone),
      last_update_status_(UpdateStatus::FAILED),
      last_remove_count_(0),
      task_runner_(new base::TestSimpleTaskRunner),
      task_runner_handle_(task_runner_) {
  EXPECT_TRUE(temp_directory_.CreateUniqueTempDir());
}

void RequestQueueStoreTestBase::TearDown() {
  // Wait for all the pieces of the store to delete itself properly.
  PumpLoop();
}

void RequestQueueStoreTestBase::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void RequestQueueStoreTestBase::ClearResults() {
  last_result_ = LastResult::kNone;
  last_update_status_ = UpdateStatus::FAILED;
  last_remove_count_ = 0;
  last_requests_.clear();
}

void RequestQueueStoreTestBase::GetRequestsDone(
    bool result,
    const std::vector<SavePageRequest>& requests) {
  last_result_ = result ? LastResult::kTrue : LastResult::kFalse;
  last_requests_ = requests;
}

void RequestQueueStoreTestBase::AddOrUpdateDone(UpdateStatus status) {
  last_update_status_ = status;
}

void RequestQueueStoreTestBase::RemoveDone(bool result, int count) {
  last_result_ = result ? LastResult::kTrue : LastResult::kFalse;
  last_remove_count_ = count;
}

void RequestQueueStoreTestBase::ResetDone(bool result) {
  last_result_ = result ? LastResult::kTrue : LastResult::kFalse;
}

// Defines interface for the store factory.
class RequestQueueStoreFactory {
 public:
  virtual RequestQueueStore* BuildStore(const base::FilePath& path) = 0;
};

// Implements a store factory for in memory store.
class RequestQueueInMemoryStoreFactory : public RequestQueueStoreFactory {
 public:
  RequestQueueStore* BuildStore(const base::FilePath& path) override {
    RequestQueueStore* store = new RequestQueueInMemoryStore();
    return store;
  }
};

// Implements a store factory for SQLite based implementation of the store.
class RequestQueueStoreSQLFactory : public RequestQueueStoreFactory {
 public:
  RequestQueueStore* BuildStore(const base::FilePath& path) override {
    RequestQueueStore* store =
        new RequestQueueStoreSQL(base::ThreadTaskRunnerHandle::Get(), path);
    return store;
  }
};

// Defines a store test fixture templatized by the store factory.
template <typename T>
class RequestQueueStoreTest : public RequestQueueStoreTestBase {
 public:
  std::unique_ptr<RequestQueueStore> BuildStore();

 protected:
  T factory_;
};

template <typename T>
std::unique_ptr<RequestQueueStore> RequestQueueStoreTest<T>::BuildStore() {
  std::unique_ptr<RequestQueueStore> store(
      factory_.BuildStore(temp_directory_.path()));
  return store;
}

// |StoreTypes| lists all factories, based on which the tests will be created.
typedef testing::Types<RequestQueueInMemoryStoreFactory,
                       RequestQueueStoreSQLFactory>
    StoreTypes;

// This portion causes test fixtures to be defined.
// Notice that in the store we are using "this->" to refer to the methods
// defined on the |RequestQuieueStoreBaseTest| class. That's by design.
TYPED_TEST_CASE(RequestQueueStoreTest, StoreTypes);

TYPED_TEST(RequestQueueStoreTest, GetRequestsEmpty) {
  std::unique_ptr<RequestQueueStore> store(this->BuildStore());
  store->GetRequests(base::Bind(&RequestQueueStoreTestBase::GetRequestsDone,
                                base::Unretained(this)));
  ASSERT_EQ(LastResult::kNone, this->last_result());
  this->PumpLoop();
  ASSERT_EQ(LastResult::kTrue, this->last_result());
  ASSERT_TRUE(this->last_requests().empty());
}

TYPED_TEST(RequestQueueStoreTest, AddRequest) {
  std::unique_ptr<RequestQueueStore> store(this->BuildStore());
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kUrl, kClientId, creation_time);

  store->AddOrUpdateRequest(
      request, base::Bind(&RequestQueueStoreTestBase::AddOrUpdateDone,
                          base::Unretained(this)));
  ASSERT_EQ(UpdateStatus::FAILED, this->last_update_status());
  this->PumpLoop();
  ASSERT_EQ(UpdateStatus::UPDATED, this->last_update_status());

  // Verifying get reqeust results after a request was added.
  this->ClearResults();
  store->GetRequests(base::Bind(&RequestQueueStoreTestBase::GetRequestsDone,
                                base::Unretained(this)));
  ASSERT_EQ(LastResult::kNone, this->last_result());
  this->PumpLoop();
  ASSERT_EQ(LastResult::kTrue, this->last_result());
  ASSERT_EQ(1ul, this->last_requests().size());
  ASSERT_TRUE(request == this->last_requests()[0]);
}

TYPED_TEST(RequestQueueStoreTest, UpdateRequest) {
  std::unique_ptr<RequestQueueStore> store(this->BuildStore());
  base::Time creation_time = base::Time::Now();
  SavePageRequest original_request(kRequestId, kUrl, kClientId, creation_time);
  store->AddOrUpdateRequest(
      original_request, base::Bind(&RequestQueueStoreTestBase::AddOrUpdateDone,
                                   base::Unretained(this)));
  this->PumpLoop();
  this->ClearResults();

  base::Time new_creation_time =
      creation_time + base::TimeDelta::FromMinutes(1);
  base::Time activation_time = creation_time + base::TimeDelta::FromHours(6);
  SavePageRequest updated_request(kRequestId, kUrl, kClientId,
                                  new_creation_time, activation_time);
  store->AddOrUpdateRequest(
      updated_request, base::Bind(&RequestQueueStoreTestBase::AddOrUpdateDone,
                                  base::Unretained(this)));
  ASSERT_EQ(UpdateStatus::FAILED, this->last_update_status());
  this->PumpLoop();
  ASSERT_EQ(UpdateStatus::UPDATED, this->last_update_status());

  // Verifying get reqeust results after a request was updated.
  this->ClearResults();
  store->GetRequests(base::Bind(&RequestQueueStoreTestBase::GetRequestsDone,
                                base::Unretained(this)));
  ASSERT_EQ(LastResult::kNone, this->last_result());
  this->PumpLoop();
  ASSERT_EQ(LastResult::kTrue, this->last_result());
  ASSERT_EQ(1ul, this->last_requests().size());
  ASSERT_TRUE(updated_request == this->last_requests()[0]);
}

TYPED_TEST(RequestQueueStoreTest, RemoveRequest) {
  std::unique_ptr<RequestQueueStore> store(this->BuildStore());
  base::Time creation_time = base::Time::Now();
  SavePageRequest original_request(kRequestId, kUrl, kClientId, creation_time);
  store->AddOrUpdateRequest(
      original_request, base::Bind(&RequestQueueStoreTestBase::AddOrUpdateDone,
                                   base::Unretained(this)));
  this->PumpLoop();
  this->ClearResults();

  std::vector<int64_t> request_ids{kRequestId};
  store->RemoveRequests(request_ids,
                        base::Bind(&RequestQueueStoreTestBase::RemoveDone,
                                   base::Unretained(this)));
  ASSERT_EQ(LastResult::kNone, this->last_result());
  ASSERT_EQ(0, this->last_remove_count());
  this->PumpLoop();
  ASSERT_EQ(LastResult::kTrue, this->last_result());
  ASSERT_EQ(1, this->last_remove_count());
  this->ClearResults();

  store->GetRequests(base::Bind(&RequestQueueStoreTestBase::GetRequestsDone,
                                base::Unretained(this)));
  this->PumpLoop();
  ASSERT_EQ(LastResult::kTrue, this->last_result());
  ASSERT_TRUE(this->last_requests().empty());
  this->ClearResults();

  // Removing a request that is missing fails.
  store->RemoveRequests(request_ids,
                        base::Bind(&RequestQueueStoreTestBase::RemoveDone,
                                   base::Unretained(this)));
  ASSERT_EQ(LastResult::kNone, this->last_result());
  ASSERT_EQ(0, this->last_remove_count());
  this->PumpLoop();
  ASSERT_EQ(LastResult::kTrue, this->last_result());
  ASSERT_EQ(0, this->last_remove_count());
}

TYPED_TEST(RequestQueueStoreTest, ResetStore) {
  std::unique_ptr<RequestQueueStore> store(this->BuildStore());
  base::Time creation_time = base::Time::Now();
  SavePageRequest original_request(kRequestId, kUrl, kClientId, creation_time);
  store->AddOrUpdateRequest(
      original_request, base::Bind(&RequestQueueStoreTestBase::AddOrUpdateDone,
                                   base::Unretained(this)));
  this->PumpLoop();
  this->ClearResults();

  store->Reset(base::Bind(&RequestQueueStoreTestBase::ResetDone,
                          base::Unretained(this)));
  ASSERT_EQ(LastResult::kNone, this->last_result());
  this->PumpLoop();
  ASSERT_EQ(LastResult::kTrue, this->last_result());
  this->ClearResults();

  store->GetRequests(base::Bind(&RequestQueueStoreTestBase::GetRequestsDone,
                                base::Unretained(this)));
  this->PumpLoop();
  ASSERT_EQ(LastResult::kTrue, this->last_result());
  ASSERT_TRUE(this->last_requests().empty());
}

class RequestQueueStoreSQLTest
    : public RequestQueueStoreTest<RequestQueueStoreSQLFactory> {};

// Makes sure that persistent DB is actually persisting requests across store
// restarts.
TEST_F(RequestQueueStoreSQLTest, SaveCloseReopenRead) {
  std::unique_ptr<RequestQueueStore> store(BuildStore());
  base::Time creation_time = base::Time::Now();
  SavePageRequest original_request(kRequestId, kUrl, kClientId, creation_time);
  store->AddOrUpdateRequest(
      original_request, base::Bind(&RequestQueueStoreTestBase::AddOrUpdateDone,
                                   base::Unretained(this)));
  PumpLoop();
  ClearResults();

  // Resets the store, using the same temp directory. The contents should be
  // intact. First reset is done separately to release DB lock.
  store.reset();
  store = BuildStore();
  store->GetRequests(base::Bind(&RequestQueueStoreTestBase::GetRequestsDone,
                                base::Unretained(this)));
  ASSERT_EQ(LastResult::kNone, this->last_result());
  this->PumpLoop();
  ASSERT_EQ(LastResult::kTrue, this->last_result());
  ASSERT_EQ(1ul, this->last_requests().size());
  ASSERT_TRUE(original_request == this->last_requests()[0]);
}

}  // offline_pages
