// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_registration.h"


#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerRegistrationTest : public testing::Test {
 public:
  ServiceWorkerRegistrationTest()
      : io_thread_(BrowserThread::IO, &message_loop_) {}

  virtual void SetUp() OVERRIDE {
    context_.reset(
        new ServiceWorkerContextCore(base::FilePath(),
                                     base::MessageLoopProxy::current(),
                                     base::MessageLoopProxy::current(),
                                     NULL,
                                     NULL,
                                     NULL));
    context_ptr_ = context_->AsWeakPtr();
  }

  virtual void TearDown() OVERRIDE {
    context_.reset();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  scoped_ptr<ServiceWorkerContextCore> context_;
  base::WeakPtr<ServiceWorkerContextCore> context_ptr_;
  base::MessageLoopForIO message_loop_;
  BrowserThreadImpl io_thread_;
};

// Make sure that activation does not leak
TEST_F(ServiceWorkerRegistrationTest, ActivatePending) {
  int64 registration_id = -1L;
  scoped_refptr<ServiceWorkerRegistration> registration =
      new ServiceWorkerRegistration(
          GURL("http://www.example.com/*"),
          GURL("http://www.example.com/service_worker.js"),
          registration_id,
          context_ptr_);

  const int64 version_1_id = 1L;
  const int64 version_2_id = 2L;
  scoped_refptr<ServiceWorkerVersion> version_1 =
      new ServiceWorkerVersion(registration, version_1_id, context_ptr_);
  version_1->SetStatus(ServiceWorkerVersion::ACTIVE);
  registration->set_active_version(version_1);

  scoped_refptr<ServiceWorkerVersion> version_2 =
      new ServiceWorkerVersion(registration, version_2_id, context_ptr_);
  registration->set_waiting_version(version_2);

  registration->ActivateWaitingVersion();
  DCHECK_EQ(version_2, registration->active_version());
  DCHECK(version_1->HasOneRef());
  version_1 = NULL;

  DCHECK(!version_2->HasOneRef());
}

}  // namespace content
