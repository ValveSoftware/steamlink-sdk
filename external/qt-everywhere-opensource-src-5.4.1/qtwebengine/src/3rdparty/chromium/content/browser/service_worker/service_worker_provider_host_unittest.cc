// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_register_job.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

static const int kRenderProcessId = 33;  // Dummy process ID for testing.

class ServiceWorkerProviderHostTest : public testing::Test {
 protected:
  ServiceWorkerProviderHostTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}
  virtual ~ServiceWorkerProviderHostTest() {}

  virtual void SetUp() OVERRIDE {
    context_.reset(
        new ServiceWorkerContextCore(base::FilePath(),
                                     base::MessageLoopProxy::current(),
                                     base::MessageLoopProxy::current(),
                                     NULL,
                                     NULL,
                                     NULL));

    scope_ = GURL("http://www.example.com/*");
    script_url_ = GURL("http://www.example.com/service_worker.js");
    registration_ = new ServiceWorkerRegistration(
        scope_, script_url_, 1L, context_->AsWeakPtr());
    version_ = new ServiceWorkerVersion(
        registration_,
        1L, context_->AsWeakPtr());

    // Prepare provider hosts (for the same process).
    scoped_ptr<ServiceWorkerProviderHost> host1(new ServiceWorkerProviderHost(
        kRenderProcessId, 1 /* provider_id */,
        context_->AsWeakPtr(), NULL));
    scoped_ptr<ServiceWorkerProviderHost> host2(new ServiceWorkerProviderHost(
        kRenderProcessId, 2 /* provider_id */,
        context_->AsWeakPtr(), NULL));
    scoped_ptr<ServiceWorkerProviderHost> host3(new ServiceWorkerProviderHost(
        kRenderProcessId, 3 /* provider_id */,
        context_->AsWeakPtr(), NULL));
    provider_host1_ = host1->AsWeakPtr();
    provider_host2_ = host2->AsWeakPtr();
    provider_host3_ = host3->AsWeakPtr();
    context_->AddProviderHost(make_scoped_ptr(host1.release()));
    context_->AddProviderHost(make_scoped_ptr(host2.release()));
    context_->AddProviderHost(make_scoped_ptr(host3.release()));
  }

  virtual void TearDown() OVERRIDE {
    version_ = 0;
    registration_ = 0;
    context_.reset();
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<ServiceWorkerContextCore> context_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host1_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host2_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host3_;
  GURL scope_;
  GURL script_url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProviderHostTest);
};

TEST_F(ServiceWorkerProviderHostTest, SetActiveVersion_ProcessStatus) {
  ASSERT_FALSE(version_->HasProcessToRun());

  // Associating version_ to a provider_host's active version will internally
  // add the provider_host's process ref to the version.
  provider_host1_->SetActiveVersion(version_);
  ASSERT_TRUE(version_->HasProcessToRun());

  // Re-associating the same version and provider_host should just work too.
  provider_host1_->SetActiveVersion(version_);
  ASSERT_TRUE(version_->HasProcessToRun());

  // Resetting the provider_host's active version should remove process refs
  // from the version.
  provider_host1_->SetActiveVersion(NULL);
  ASSERT_FALSE(version_->HasProcessToRun());
}

TEST_F(ServiceWorkerProviderHostTest,
       SetActiveVersion_MultipleHostsForSameProcess) {
  ASSERT_FALSE(version_->HasProcessToRun());

  // Associating version_ to two providers as active version.
  provider_host1_->SetActiveVersion(version_);
  provider_host2_->SetActiveVersion(version_);
  ASSERT_TRUE(version_->HasProcessToRun());

  // Disassociating one provider_host shouldn't remove all process refs
  // from the version yet.
  provider_host1_->SetActiveVersion(NULL);
  ASSERT_TRUE(version_->HasProcessToRun());

  // Disassociating the other provider_host will remove all process refs.
  provider_host2_->SetActiveVersion(NULL);
  ASSERT_FALSE(version_->HasProcessToRun());
}

TEST_F(ServiceWorkerProviderHostTest, SetWaitingVersion_ProcessStatus) {
  ASSERT_FALSE(version_->HasProcessToRun());

  // Associating version_ to a provider_host's waiting version will internally
  // add the provider_host's process ref to the version.
  provider_host1_->SetWaitingVersion(version_);
  ASSERT_TRUE(version_->HasProcessToRun());

  // Re-associating the same version and provider_host should just work too.
  provider_host1_->SetWaitingVersion(version_);
  ASSERT_TRUE(version_->HasProcessToRun());

  // Resetting the provider_host's waiting version should remove process refs
  // from the version.
  provider_host1_->SetWaitingVersion(NULL);
  ASSERT_FALSE(version_->HasProcessToRun());
}

TEST_F(ServiceWorkerProviderHostTest,
       SetWaitingVersion_MultipleHostsForSameProcess) {
  ASSERT_FALSE(version_->HasProcessToRun());

  // Associating version_ to two providers as active version.
  provider_host1_->SetWaitingVersion(version_);
  provider_host2_->SetWaitingVersion(version_);
  ASSERT_TRUE(version_->HasProcessToRun());

  // Disassociating one provider_host shouldn't remove all process refs
  // from the version yet.
  provider_host1_->SetWaitingVersion(NULL);
  ASSERT_TRUE(version_->HasProcessToRun());

  // Disassociating the other provider_host will remove all process refs.
  provider_host2_->SetWaitingVersion(NULL);
  ASSERT_FALSE(version_->HasProcessToRun());
}

class ServiceWorkerProviderHostWaitingVersionTest : public testing::Test {
 protected:
  ServiceWorkerProviderHostWaitingVersionTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        next_provider_id_(1L) {}
  virtual ~ServiceWorkerProviderHostWaitingVersionTest() {}

  virtual void SetUp() OVERRIDE {
    context_.reset(
        new ServiceWorkerContextCore(base::FilePath(),
                                     base::MessageLoopProxy::current(),
                                     base::MessageLoopProxy::current(),
                                     NULL,
                                     NULL,
                                     NULL));

    // Prepare provider hosts (for the same process).
    provider_host1_ = CreateProviderHost(GURL("http://www.example.com/foo"));
    provider_host2_ = CreateProviderHost(GURL("http://www.example.com/bar"));
    provider_host3_ = CreateProviderHost(GURL("http://www.example.ca/foo"));
  }

  base::WeakPtr<ServiceWorkerProviderHost> CreateProviderHost(
      const GURL& document_url) {
    scoped_ptr<ServiceWorkerProviderHost> host(new ServiceWorkerProviderHost(
        kRenderProcessId, next_provider_id_++, context_->AsWeakPtr(), NULL));
    host->SetDocumentUrl(document_url);
    base::WeakPtr<ServiceWorkerProviderHost> provider_host = host->AsWeakPtr();
    context_->AddProviderHost(host.Pass());
    return provider_host;
  }

  virtual void TearDown() OVERRIDE {
    context_.reset();
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<ServiceWorkerContextCore> context_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host1_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host2_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host3_;

 private:
  int64 next_provider_id_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProviderHostWaitingVersionTest);
};

TEST_F(ServiceWorkerProviderHostWaitingVersionTest,
       AssociateWaitingVersionToDocuments) {
  const GURL scope1("http://www.example.com/*");
  const GURL script_url1("http://www.example.com/service_worker1.js");
  scoped_refptr<ServiceWorkerRegistration> registration1(
      new ServiceWorkerRegistration(
          scope1, script_url1, 1L, context_->AsWeakPtr()));
  scoped_refptr<ServiceWorkerVersion> version1(
      new ServiceWorkerVersion(registration1, 1L, context_->AsWeakPtr()));

  ServiceWorkerRegisterJob::AssociateWaitingVersionToDocuments(
      context_->AsWeakPtr(), version1.get());
  EXPECT_EQ(version1.get(), provider_host1_->waiting_version());
  EXPECT_EQ(version1.get(), provider_host2_->waiting_version());
  EXPECT_EQ(NULL, provider_host3_->waiting_version());

  // Version2 is associated with the same registration as version1, so the
  // waiting version of host1 and host2 should be replaced.
  scoped_refptr<ServiceWorkerVersion> version2(
      new ServiceWorkerVersion(registration1, 2L, context_->AsWeakPtr()));
  ServiceWorkerRegisterJob::AssociateWaitingVersionToDocuments(
      context_->AsWeakPtr(), version2.get());
  EXPECT_EQ(version2.get(), provider_host1_->waiting_version());
  EXPECT_EQ(version2.get(), provider_host2_->waiting_version());
  EXPECT_EQ(NULL, provider_host3_->waiting_version());

  const GURL scope3(provider_host1_->document_url());
  const GURL script_url3("http://www.example.com/service_worker3.js");
  scoped_refptr<ServiceWorkerRegistration> registration3(
      new ServiceWorkerRegistration(
          scope3, script_url3, 3L, context_->AsWeakPtr()));
  scoped_refptr<ServiceWorkerVersion> version3(
      new ServiceWorkerVersion(registration3, 3L, context_->AsWeakPtr()));

  // Although version3 can match longer than version2 for host1, it should be
  // ignored because version3 is associated with a different registration from
  // version2.
  ServiceWorkerRegisterJob::AssociateWaitingVersionToDocuments(
      context_->AsWeakPtr(), version3.get());
  EXPECT_EQ(version2.get(), provider_host1_->waiting_version());
  EXPECT_EQ(version2.get(), provider_host2_->waiting_version());
  EXPECT_EQ(NULL, provider_host3_->waiting_version());
}

TEST_F(ServiceWorkerProviderHostWaitingVersionTest,
       DisassociateWaitingVersionFromDocuments) {
  const GURL scope1("http://www.example.com/*");
  const GURL script_url1("http://www.example.com/service_worker.js");
  scoped_refptr<ServiceWorkerRegistration> registration1(
      new ServiceWorkerRegistration(
          scope1, script_url1, 1L, context_->AsWeakPtr()));
  scoped_refptr<ServiceWorkerVersion> version1(
      new ServiceWorkerVersion(registration1, 1L, context_->AsWeakPtr()));

  const GURL scope2("http://www.example.ca/*");
  const GURL script_url2("http://www.example.ca/service_worker.js");
  scoped_refptr<ServiceWorkerRegistration> registration2(
      new ServiceWorkerRegistration(
          scope2, script_url2, 2L, context_->AsWeakPtr()));
  scoped_refptr<ServiceWorkerVersion> version2(
      new ServiceWorkerVersion(registration2, 2L, context_->AsWeakPtr()));

  ServiceWorkerRegisterJob::AssociateWaitingVersionToDocuments(
      context_->AsWeakPtr(), version1.get());
  ServiceWorkerRegisterJob::AssociateWaitingVersionToDocuments(
      context_->AsWeakPtr(), version2.get());

  // Host1 and host2 are associated with version1 as a waiting version, whereas
  // host3 is associated with version2.
  EXPECT_EQ(version1.get(), provider_host1_->waiting_version());
  EXPECT_EQ(version1.get(), provider_host2_->waiting_version());
  EXPECT_EQ(version2.get(), provider_host3_->waiting_version());

  // Disassociate version1 from host1 and host2.
  ServiceWorkerRegisterJob::DisassociateWaitingVersionFromDocuments(
      context_->AsWeakPtr(), version1->version_id());
  EXPECT_EQ(NULL, provider_host1_->waiting_version());
  EXPECT_EQ(NULL, provider_host2_->waiting_version());
  EXPECT_EQ(version2.get(), provider_host3_->waiting_version());
}

}  // namespace content
