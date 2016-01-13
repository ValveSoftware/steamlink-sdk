// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "content/browser/dom_storage/dom_storage_area.h"
#include "content/browser/dom_storage/dom_storage_context_impl.h"
#include "content/browser/dom_storage/dom_storage_namespace.h"
#include "content/browser/dom_storage/dom_storage_task_runner.h"
#include "content/public/browser/local_storage_usage_info.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/session_storage_usage_info.h"
#include "content/public/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace content {

class DOMStorageContextImplTest : public testing::Test {
 public:
  DOMStorageContextImplTest()
    : kOrigin(GURL("http://dom_storage/")),
      kKey(ASCIIToUTF16("key")),
      kValue(ASCIIToUTF16("value")),
      kDontIncludeFileInfo(false),
      kDoIncludeFileInfo(true) {
  }

  const GURL kOrigin;
  const base::string16 kKey;
  const base::string16 kValue;
  const bool kDontIncludeFileInfo;
  const bool kDoIncludeFileInfo;

  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    storage_policy_ = new MockSpecialStoragePolicy;
    task_runner_ =
        new MockDOMStorageTaskRunner(base::MessageLoopProxy::current().get());
    context_ = new DOMStorageContextImpl(temp_dir_.path(),
                                         base::FilePath(),
                                         storage_policy_.get(),
                                         task_runner_.get());
  }

  virtual void TearDown() {
    base::MessageLoop::current()->RunUntilIdle();
  }

  void VerifySingleOriginRemains(const GURL& origin) {
    // Use a new instance to examine the contexts of temp_dir_.
    scoped_refptr<DOMStorageContextImpl> context =
        new DOMStorageContextImpl(temp_dir_.path(), base::FilePath(),
                                  NULL, NULL);
    std::vector<LocalStorageUsageInfo> infos;
    context->GetLocalStorageUsage(&infos, kDontIncludeFileInfo);
    ASSERT_EQ(1u, infos.size());
    EXPECT_EQ(origin, infos[0].origin);
  }

 protected:
  base::MessageLoop message_loop_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<MockSpecialStoragePolicy> storage_policy_;
  scoped_refptr<MockDOMStorageTaskRunner> task_runner_;
  scoped_refptr<DOMStorageContextImpl> context_;
  DISALLOW_COPY_AND_ASSIGN(DOMStorageContextImplTest);
};

TEST_F(DOMStorageContextImplTest, Basics) {
  // This test doesn't do much, checks that the constructor
  // initializes members properly and that invoking methods
  // on a newly created object w/o any data on disk do no harm.
  EXPECT_EQ(temp_dir_.path(), context_->localstorage_directory());
  EXPECT_EQ(base::FilePath(), context_->sessionstorage_directory());
  EXPECT_EQ(storage_policy_.get(), context_->special_storage_policy_.get());
  context_->DeleteLocalStorage(GURL("http://chromium.org/"));
  const int kFirstSessionStorageNamespaceId = 1;
  EXPECT_TRUE(context_->GetStorageNamespace(kLocalStorageNamespaceId));
  EXPECT_FALSE(context_->GetStorageNamespace(kFirstSessionStorageNamespaceId));
  EXPECT_EQ(kFirstSessionStorageNamespaceId, context_->AllocateSessionId());
  std::vector<LocalStorageUsageInfo> infos;
  context_->GetLocalStorageUsage(&infos, kDontIncludeFileInfo);
  EXPECT_TRUE(infos.empty());
  context_->Shutdown();
}

TEST_F(DOMStorageContextImplTest, UsageInfo) {
  // Should be empty initially
  std::vector<LocalStorageUsageInfo> infos;
  context_->GetLocalStorageUsage(&infos, kDontIncludeFileInfo);
  EXPECT_TRUE(infos.empty());
  context_->GetLocalStorageUsage(&infos, kDoIncludeFileInfo);
  EXPECT_TRUE(infos.empty());

  // Put some data into local storage and shutdown the context
  // to ensure data is written to disk.
  base::NullableString16 old_value;
  EXPECT_TRUE(context_->GetStorageNamespace(kLocalStorageNamespaceId)->
      OpenStorageArea(kOrigin)->SetItem(kKey, kValue, &old_value));
  context_->Shutdown();
  context_ = NULL;
  base::MessageLoop::current()->RunUntilIdle();

  // Create a new context that points to the same directory, see that
  // it knows about the origin that we stored data for.
  context_ = new DOMStorageContextImpl(temp_dir_.path(), base::FilePath(),
                                       NULL, NULL);
  context_->GetLocalStorageUsage(&infos, kDontIncludeFileInfo);
  EXPECT_EQ(1u, infos.size());
  EXPECT_EQ(kOrigin, infos[0].origin);
  EXPECT_EQ(0u, infos[0].data_size);
  EXPECT_EQ(base::Time(), infos[0].last_modified);
  infos.clear();
  context_->GetLocalStorageUsage(&infos, kDoIncludeFileInfo);
  EXPECT_EQ(1u, infos.size());
  EXPECT_EQ(kOrigin, infos[0].origin);
  EXPECT_NE(0u, infos[0].data_size);
  EXPECT_NE(base::Time(), infos[0].last_modified);
}

TEST_F(DOMStorageContextImplTest, SessionOnly) {
  const GURL kSessionOnlyOrigin("http://www.sessiononly.com/");
  storage_policy_->AddSessionOnly(kSessionOnlyOrigin);

  // Store data for a normal and a session-only origin and then
  // invoke Shutdown() which should delete data for session-only
  // origins.
  base::NullableString16 old_value;
  EXPECT_TRUE(context_->GetStorageNamespace(kLocalStorageNamespaceId)->
      OpenStorageArea(kOrigin)->SetItem(kKey, kValue, &old_value));
  EXPECT_TRUE(context_->GetStorageNamespace(kLocalStorageNamespaceId)->
      OpenStorageArea(kSessionOnlyOrigin)->SetItem(kKey, kValue, &old_value));
  context_->Shutdown();
  context_ = NULL;
  base::MessageLoop::current()->RunUntilIdle();

  // Verify that the session-only origin data is gone.
  VerifySingleOriginRemains(kOrigin);
}

TEST_F(DOMStorageContextImplTest, SetForceKeepSessionState) {
  const GURL kSessionOnlyOrigin("http://www.sessiononly.com/");
  storage_policy_->AddSessionOnly(kSessionOnlyOrigin);

  // Store data for a session-only origin, setup to save session data, then
  // shutdown.
  base::NullableString16 old_value;
  EXPECT_TRUE(context_->GetStorageNamespace(kLocalStorageNamespaceId)->
      OpenStorageArea(kSessionOnlyOrigin)->SetItem(kKey, kValue, &old_value));
  context_->SetForceKeepSessionState();  // Should override clear behavior.
  context_->Shutdown();
  context_ = NULL;
  base::MessageLoop::current()->RunUntilIdle();

  VerifySingleOriginRemains(kSessionOnlyOrigin);
}

TEST_F(DOMStorageContextImplTest, PersistentIds) {
  const int kFirstSessionStorageNamespaceId = 1;
  const std::string kPersistentId = "persistent";
  context_->CreateSessionNamespace(kFirstSessionStorageNamespaceId,
                                   kPersistentId);
  DOMStorageNamespace* dom_namespace =
      context_->GetStorageNamespace(kFirstSessionStorageNamespaceId);
  ASSERT_TRUE(dom_namespace);
  EXPECT_EQ(kPersistentId, dom_namespace->persistent_namespace_id());
  // Verify that the areas inherit the persistent ID.
  DOMStorageArea* area = dom_namespace->OpenStorageArea(kOrigin);
  EXPECT_EQ(kPersistentId, area->persistent_namespace_id_);

  // Verify that the persistent IDs are handled correctly when cloning.
  const int kClonedSessionStorageNamespaceId = 2;
  const std::string kClonedPersistentId = "cloned";
  context_->CloneSessionNamespace(kFirstSessionStorageNamespaceId,
                                  kClonedSessionStorageNamespaceId,
                                  kClonedPersistentId);
  DOMStorageNamespace* cloned_dom_namespace =
      context_->GetStorageNamespace(kClonedSessionStorageNamespaceId);
  ASSERT_TRUE(dom_namespace);
  EXPECT_EQ(kClonedPersistentId,
            cloned_dom_namespace->persistent_namespace_id());
  // Verify that the areas inherit the persistent ID.
  DOMStorageArea* cloned_area = cloned_dom_namespace->OpenStorageArea(kOrigin);
  EXPECT_EQ(kClonedPersistentId, cloned_area->persistent_namespace_id_);
}

TEST_F(DOMStorageContextImplTest, DeleteSessionStorage) {
  // Create a DOMStorageContextImpl which will save sessionStorage on disk.
  context_ = new DOMStorageContextImpl(temp_dir_.path(),
                                       temp_dir_.path(),
                                       storage_policy_.get(),
                                       task_runner_.get());
  context_->SetSaveSessionStorageOnDisk();
  ASSERT_EQ(temp_dir_.path(), context_->sessionstorage_directory());

  // Write data.
  const int kSessionStorageNamespaceId = 1;
  const std::string kPersistentId = "persistent";
  context_->CreateSessionNamespace(kSessionStorageNamespaceId,
                                   kPersistentId);
  DOMStorageNamespace* dom_namespace =
      context_->GetStorageNamespace(kSessionStorageNamespaceId);
  DOMStorageArea* area = dom_namespace->OpenStorageArea(kOrigin);
  const base::string16 kKey(ASCIIToUTF16("foo"));
  const base::string16 kValue(ASCIIToUTF16("bar"));
  base::NullableString16 old_nullable_value;
  area->SetItem(kKey, kValue, &old_nullable_value);
  dom_namespace->CloseStorageArea(area);

  // Destroy and recreate the DOMStorageContextImpl.
  context_->Shutdown();
  context_ = NULL;
  base::MessageLoop::current()->RunUntilIdle();
  context_ = new DOMStorageContextImpl(
      temp_dir_.path(), temp_dir_.path(),
      storage_policy_.get(), task_runner_.get());
  context_->SetSaveSessionStorageOnDisk();

  // Read the data back.
  context_->CreateSessionNamespace(kSessionStorageNamespaceId,
                                   kPersistentId);
  dom_namespace = context_->GetStorageNamespace(kSessionStorageNamespaceId);
  area = dom_namespace->OpenStorageArea(kOrigin);
  base::NullableString16 read_value;
  read_value = area->GetItem(kKey);
  EXPECT_EQ(kValue, read_value.string());
  dom_namespace->CloseStorageArea(area);

  SessionStorageUsageInfo info;
  info.origin = kOrigin;
  info.persistent_namespace_id = kPersistentId;
  context_->DeleteSessionStorage(info);

  // Destroy and recreate again.
  context_->Shutdown();
  context_ = NULL;
  base::MessageLoop::current()->RunUntilIdle();
  context_ = new DOMStorageContextImpl(
      temp_dir_.path(), temp_dir_.path(),
      storage_policy_.get(), task_runner_.get());
  context_->SetSaveSessionStorageOnDisk();

  // Now there should be no data.
  context_->CreateSessionNamespace(kSessionStorageNamespaceId,
                                   kPersistentId);
  dom_namespace = context_->GetStorageNamespace(kSessionStorageNamespaceId);
  area = dom_namespace->OpenStorageArea(kOrigin);
  read_value = area->GetItem(kKey);
  EXPECT_TRUE(read_value.is_null());
  dom_namespace->CloseStorageArea(area);
  context_->Shutdown();
  context_ = NULL;
  base::MessageLoop::current()->RunUntilIdle();
}

TEST_F(DOMStorageContextImplTest, SessionStorageAlias) {
  const int kFirstSessionStorageNamespaceId = 1;
  const std::string kPersistentId = "persistent";
  context_->CreateSessionNamespace(kFirstSessionStorageNamespaceId,
                                   kPersistentId);
  DOMStorageNamespace* dom_namespace1 =
      context_->GetStorageNamespace(kFirstSessionStorageNamespaceId);
  ASSERT_TRUE(dom_namespace1);
  DOMStorageArea* area1 = dom_namespace1->OpenStorageArea(kOrigin);
  base::NullableString16 old_value;
  area1->SetItem(kKey, kValue, &old_value);
  EXPECT_TRUE(old_value.is_null());
  base::NullableString16 read_value = area1->GetItem(kKey);
  EXPECT_TRUE(!read_value.is_null() && read_value.string() == kValue);

  // Create an alias.
  const int kAliasSessionStorageNamespaceId = 2;
  context_->CreateAliasSessionNamespace(kFirstSessionStorageNamespaceId,
                                        kAliasSessionStorageNamespaceId,
                                        kPersistentId);
  DOMStorageNamespace* dom_namespace2 =
      context_->GetStorageNamespace(kAliasSessionStorageNamespaceId);
  ASSERT_TRUE(dom_namespace2);
  ASSERT_TRUE(dom_namespace2->alias_master_namespace() == dom_namespace1);

  // Verify that read values are identical.
  DOMStorageArea* area2 = dom_namespace2->OpenStorageArea(kOrigin);
  read_value = area2->GetItem(kKey);
  EXPECT_TRUE(!read_value.is_null() && read_value.string() == kValue);

  // Verify that writes are reflected in both namespaces.
  const base::string16 kValue2(ASCIIToUTF16("value2"));
  area2->SetItem(kKey, kValue2, &old_value);
  read_value = area1->GetItem(kKey);
  EXPECT_TRUE(!read_value.is_null() && read_value.string() == kValue2);
  dom_namespace1->CloseStorageArea(area1);
  dom_namespace2->CloseStorageArea(area2);

  // When creating an alias of an alias, ensure that the master relationship
  // is collapsed.
  const int kAlias2SessionStorageNamespaceId = 3;
  context_->CreateAliasSessionNamespace(kAliasSessionStorageNamespaceId,
                                        kAlias2SessionStorageNamespaceId,
                                        kPersistentId);
  DOMStorageNamespace* dom_namespace3 =
      context_->GetStorageNamespace(kAlias2SessionStorageNamespaceId);
  ASSERT_TRUE(dom_namespace3);
  ASSERT_TRUE(dom_namespace3->alias_master_namespace() == dom_namespace1);
}

TEST_F(DOMStorageContextImplTest, SessionStorageMerge) {
  // Create a target namespace that we will merge into.
  const int kTargetSessionStorageNamespaceId = 1;
  const std::string kTargetPersistentId = "persistent";
  context_->CreateSessionNamespace(kTargetSessionStorageNamespaceId,
                                   kTargetPersistentId);
  DOMStorageNamespace* target_ns =
      context_->GetStorageNamespace(kTargetSessionStorageNamespaceId);
  ASSERT_TRUE(target_ns);
  DOMStorageArea* target_ns_area = target_ns->OpenStorageArea(kOrigin);
  base::NullableString16 old_value;
  const base::string16 kKey2(ASCIIToUTF16("key2"));
  const base::string16 kKey2Value(ASCIIToUTF16("key2value"));
  target_ns_area->SetItem(kKey, kValue, &old_value);
  target_ns_area->SetItem(kKey2, kKey2Value, &old_value);

  // Create a source namespace & its alias.
  const int kSourceSessionStorageNamespaceId = 2;
  const int kAliasSessionStorageNamespaceId = 3;
  const std::string kSourcePersistentId = "persistent_source";
  context_->CreateSessionNamespace(kSourceSessionStorageNamespaceId,
                                   kSourcePersistentId);
  context_->CreateAliasSessionNamespace(kSourceSessionStorageNamespaceId,
                                        kAliasSessionStorageNamespaceId,
                                        kSourcePersistentId);
  DOMStorageNamespace* alias_ns =
      context_->GetStorageNamespace(kAliasSessionStorageNamespaceId);
  ASSERT_TRUE(alias_ns);

  // Create a transaction log that can't be merged.
  const int kPid1 = 10;
  ASSERT_FALSE(alias_ns->IsLoggingRenderer(kPid1));
  alias_ns->AddTransactionLogProcessId(kPid1);
  ASSERT_TRUE(alias_ns->IsLoggingRenderer(kPid1));
  const base::string16 kValue2(ASCIIToUTF16("value2"));
  DOMStorageNamespace::TransactionRecord txn;
  txn.origin = kOrigin;
  txn.key = kKey;
  txn.value = base::NullableString16(kValue2, false);
  txn.transaction_type = DOMStorageNamespace::TRANSACTION_READ;
  alias_ns->AddTransaction(kPid1, txn);
  ASSERT_TRUE(alias_ns->Merge(false, kPid1, target_ns, NULL) ==
              SessionStorageNamespace::MERGE_RESULT_NOT_MERGEABLE);

  // Create a transaction log that can be merged.
  const int kPid2 = 20;
  alias_ns->AddTransactionLogProcessId(kPid2);
  txn.transaction_type = DOMStorageNamespace::TRANSACTION_WRITE;
  alias_ns->AddTransaction(kPid2, txn);
  ASSERT_TRUE(alias_ns->Merge(true, kPid2, target_ns, NULL) ==
              SessionStorageNamespace::MERGE_RESULT_MERGEABLE);

  // Verify that the merge was successful.
  ASSERT_TRUE(alias_ns->alias_master_namespace() == target_ns);
  base::NullableString16 read_value = target_ns_area->GetItem(kKey);
  EXPECT_TRUE(!read_value.is_null() && read_value.string() == kValue2);
  DOMStorageArea* alias_ns_area = alias_ns->OpenStorageArea(kOrigin);
  read_value = alias_ns_area->GetItem(kKey2);
  EXPECT_TRUE(!read_value.is_null() && read_value.string() == kKey2Value);
}

}  // namespace content
