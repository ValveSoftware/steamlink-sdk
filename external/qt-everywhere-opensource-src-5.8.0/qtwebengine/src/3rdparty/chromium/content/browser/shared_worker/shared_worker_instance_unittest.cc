// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_instance.h"

#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/shared_worker/worker_storage_partition.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class SharedWorkerInstanceTest : public testing::Test {
 protected:
  SharedWorkerInstanceTest()
      : browser_context_(new TestBrowserContext()),
        partition_(new WorkerStoragePartition(
            BrowserContext::GetDefaultStoragePartition(browser_context_.get())->
                GetURLRequestContext(),
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL)),
        partition_id_(*partition_.get()) {}

  bool Matches(const SharedWorkerInstance& instance,
               const std::string& url,
               const base::StringPiece& name) {
    return instance.Matches(GURL(url),
                            base::ASCIIToUTF16(name),
                            partition_id_,
                            browser_context_->GetResourceContext());
  }

  TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<WorkerStoragePartition> partition_;
  const WorkerStoragePartitionId partition_id_;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerInstanceTest);
};

TEST_F(SharedWorkerInstanceTest, MatchesTest) {
  SharedWorkerInstance instance1(
      GURL("http://example.com/w.js"), base::string16(), base::string16(),
      blink::WebContentSecurityPolicyTypeReport, blink::WebAddressSpacePublic,
      browser_context_->GetResourceContext(), partition_id_,
      blink::WebSharedWorkerCreationContextTypeNonsecure);
  EXPECT_TRUE(Matches(instance1, "http://example.com/w.js", ""));
  EXPECT_FALSE(Matches(instance1, "http://example.com/w2.js", ""));
  EXPECT_FALSE(Matches(instance1, "http://example.net/w.js", ""));
  EXPECT_FALSE(Matches(instance1, "http://example.net/w2.js", ""));
  EXPECT_FALSE(Matches(instance1, "http://example.com/w.js", "name"));
  EXPECT_FALSE(Matches(instance1, "http://example.com/w2.js", "name"));
  EXPECT_FALSE(Matches(instance1, "http://example.net/w.js", "name"));
  EXPECT_FALSE(Matches(instance1, "http://example.net/w2.js", "name"));

  SharedWorkerInstance instance2(
      GURL("http://example.com/w.js"), base::ASCIIToUTF16("name"),
      base::string16(), blink::WebContentSecurityPolicyTypeReport,
      blink::WebAddressSpacePublic, browser_context_->GetResourceContext(),
      partition_id_, blink::WebSharedWorkerCreationContextTypeNonsecure);
  EXPECT_FALSE(Matches(instance2, "http://example.com/w.js", ""));
  EXPECT_FALSE(Matches(instance2, "http://example.com/w2.js", ""));
  EXPECT_FALSE(Matches(instance2, "http://example.net/w.js", ""));
  EXPECT_FALSE(Matches(instance2, "http://example.net/w2.js", ""));
  EXPECT_TRUE(Matches(instance2, "http://example.com/w.js", "name"));
  EXPECT_TRUE(Matches(instance2, "http://example.com/w2.js", "name"));
  EXPECT_FALSE(Matches(instance2, "http://example.net/w.js", "name"));
  EXPECT_FALSE(Matches(instance2, "http://example.net/w2.js", "name"));
  EXPECT_FALSE(Matches(instance2, "http://example.com/w.js", "name2"));
  EXPECT_FALSE(Matches(instance2, "http://example.com/w2.js", "name2"));
  EXPECT_FALSE(Matches(instance2, "http://example.net/w.js", "name2"));
  EXPECT_FALSE(Matches(instance2, "http://example.net/w2.js", "name2"));
}

TEST_F(SharedWorkerInstanceTest, AddressSpace) {
  for (int i = 0; i < static_cast<int>(blink::WebAddressSpaceLast); i++) {
    SharedWorkerInstance instance(
        GURL("http://example.com/w.js"), base::ASCIIToUTF16("name"),
        base::string16(), blink::WebContentSecurityPolicyTypeReport,
        static_cast<blink::WebAddressSpace>(i),
        browser_context_->GetResourceContext(), partition_id_,
        blink::WebSharedWorkerCreationContextTypeNonsecure);
    EXPECT_EQ(static_cast<blink::WebAddressSpace>(i),
              instance.creation_address_space());
  }
}

}  // namespace content
