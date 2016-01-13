// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::IOBuffer;
using net::WrappedIOBuffer;

namespace content {

namespace {

typedef ServiceWorkerDatabase::RegistrationData RegistrationData;
typedef ServiceWorkerDatabase::ResourceRecord ResourceRecord;

void StatusCallback(bool* was_called,
                    ServiceWorkerStatusCode* result,
                    ServiceWorkerStatusCode status) {
  *was_called = true;
  *result = status;
}

ServiceWorkerStorage::StatusCallback MakeStatusCallback(
    bool* was_called,
    ServiceWorkerStatusCode* result) {
  return base::Bind(&StatusCallback, was_called, result);
}

void FindCallback(
    bool* was_called,
    ServiceWorkerStatusCode* result,
    scoped_refptr<ServiceWorkerRegistration>* found,
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  *was_called = true;
  *result = status;
  *found = registration;
}

ServiceWorkerStorage::FindRegistrationCallback MakeFindCallback(
    bool* was_called,
    ServiceWorkerStatusCode* result,
    scoped_refptr<ServiceWorkerRegistration>* found) {
  return base::Bind(&FindCallback, was_called, result, found);
}

void GetAllCallback(
    bool* was_called,
    std::vector<ServiceWorkerRegistrationInfo>* all_out,
    const std::vector<ServiceWorkerRegistrationInfo>& all) {
  *was_called = true;
  *all_out = all;
}

ServiceWorkerStorage::GetAllRegistrationInfosCallback MakeGetAllCallback(
    bool* was_called,
    std::vector<ServiceWorkerRegistrationInfo>* all) {
  return base::Bind(&GetAllCallback, was_called, all);
}

void OnIOComplete(int* rv_out, int rv) {
  *rv_out = rv;
}

void WriteBasicResponse(ServiceWorkerStorage* storage, int64 id) {
  scoped_ptr<ServiceWorkerResponseWriter> writer =
      storage->CreateResponseWriter(id);

  const char kHttpHeaders[] =
      "HTTP/1.0 200 HONKYDORY\0Content-Length: 6\0\0";
  const char kHttpBody[] = "Hello\0";
  scoped_refptr<IOBuffer> body(new WrappedIOBuffer(kHttpBody));
  std::string raw_headers(kHttpHeaders, arraysize(kHttpHeaders));
  scoped_ptr<net::HttpResponseInfo> info(new net::HttpResponseInfo);
  info->request_time = base::Time::Now();
  info->response_time = base::Time::Now();
  info->was_cached = false;
  info->headers = new net::HttpResponseHeaders(raw_headers);
  scoped_refptr<HttpResponseInfoIOBuffer> info_buffer =
      new HttpResponseInfoIOBuffer(info.release());

  int rv = -1234;
  writer->WriteInfo(info_buffer, base::Bind(&OnIOComplete, &rv));
  base::RunLoop().RunUntilIdle();
  EXPECT_LT(0, rv);

  rv = -1234;
  writer->WriteData(body, arraysize(kHttpBody),
                    base::Bind(&OnIOComplete, &rv));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(static_cast<int>(arraysize(kHttpBody)), rv);
}

bool VerifyBasicResponse(ServiceWorkerStorage* storage, int64 id,
                         bool expected_positive_result) {
  const char kExpectedHttpBody[] = "Hello\0";
  scoped_ptr<ServiceWorkerResponseReader> reader =
      storage->CreateResponseReader(id);
  scoped_refptr<HttpResponseInfoIOBuffer> info_buffer =
      new HttpResponseInfoIOBuffer();
  int rv = -1234;
  reader->ReadInfo(info_buffer, base::Bind(&OnIOComplete, &rv));
  base::RunLoop().RunUntilIdle();
  if (expected_positive_result)
    EXPECT_LT(0, rv);
  if (rv <= 0)
    return false;

  const int kBigEnough = 512;
  scoped_refptr<net::IOBuffer> buffer = new IOBuffer(kBigEnough);
  rv = -1234;
  reader->ReadData(buffer, kBigEnough, base::Bind(&OnIOComplete, &rv));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(static_cast<int>(arraysize(kExpectedHttpBody)), rv);
  if (rv <= 0)
    return false;

  bool status_match =
      std::string("HONKYDORY") ==
          info_buffer->http_info->headers->GetStatusText();
  bool data_match =
      std::string(kExpectedHttpBody) == std::string(buffer->data());

  EXPECT_TRUE(status_match);
  EXPECT_TRUE(data_match);
  return status_match && data_match;
}

}  // namespace

class ServiceWorkerStorageTest : public testing::Test {
 public:
  ServiceWorkerStorageTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {
  }

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
  }

  ServiceWorkerStorage* storage() { return context_->storage(); }

  // A static class method for friendliness.
  static void VerifyPurgeableListStatusCallback(
      ServiceWorkerDatabase* database,
      std::set<int64> *purgeable_ids,
      bool* was_called,
      ServiceWorkerStatusCode* result,
      ServiceWorkerStatusCode status) {
    *was_called = true;
    *result = status;
    EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
              database->GetPurgeableResourceIds(purgeable_ids));
  }

 protected:
  ServiceWorkerStatusCode StoreRegistration(
      scoped_refptr<ServiceWorkerRegistration> registration,
      scoped_refptr<ServiceWorkerVersion> version) {
    bool was_called = false;
    ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_FAILED;
    storage()->StoreRegistration(
        registration, version, MakeStatusCallback(&was_called, &result));
    EXPECT_FALSE(was_called);  // always async
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
    return result;
  }

  ServiceWorkerStatusCode DeleteRegistration(
      int64 registration_id,
      const GURL& origin) {
    bool was_called = false;
    ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_FAILED;
    storage()->DeleteRegistration(
        registration_id, origin, MakeStatusCallback(&was_called, &result));
    EXPECT_FALSE(was_called);  // always async
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
    return result;
  }

  void GetAllRegistrations(
      std::vector<ServiceWorkerRegistrationInfo>* registrations) {
    bool was_called = false;
    storage()->GetAllRegistrations(
        MakeGetAllCallback(&was_called, registrations));
    EXPECT_FALSE(was_called);  // always async
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
  }

  ServiceWorkerStatusCode UpdateToActiveState(
      scoped_refptr<ServiceWorkerRegistration> registration) {
    bool was_called = false;
    ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_FAILED;
    storage()->UpdateToActiveState(
        registration, MakeStatusCallback(&was_called, &result));
    EXPECT_FALSE(was_called);  // always async
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
    return result;
  }

  ServiceWorkerStatusCode FindRegistrationForDocument(
      const GURL& document_url,
      scoped_refptr<ServiceWorkerRegistration>* registration) {
    bool was_called = false;
    ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_FAILED;
    storage()->FindRegistrationForDocument(
        document_url, MakeFindCallback(&was_called, &result, registration));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
    return result;
  }

  ServiceWorkerStatusCode FindRegistrationForPattern(
      const GURL& scope,
      scoped_refptr<ServiceWorkerRegistration>* registration) {
    bool was_called = false;
    ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_FAILED;
    storage()->FindRegistrationForPattern(
        scope, MakeFindCallback(&was_called, &result, registration));
    EXPECT_FALSE(was_called);  // always async
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
    return result;
  }

  ServiceWorkerStatusCode FindRegistrationForId(
      int64 registration_id,
      const GURL& origin,
      scoped_refptr<ServiceWorkerRegistration>* registration) {
    bool was_called = false;
    ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_FAILED;
    storage()->FindRegistrationForId(
        registration_id, origin,
        MakeFindCallback(&was_called, &result, registration));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
    return result;
  }

  scoped_ptr<ServiceWorkerContextCore> context_;
  base::WeakPtr<ServiceWorkerContextCore> context_ptr_;
  TestBrowserThreadBundle browser_thread_bundle_;
};

TEST_F(ServiceWorkerStorageTest, StoreFindUpdateDeleteRegistration) {
  const GURL kScope("http://www.test.not/scope/*");
  const GURL kScript("http://www.test.not/script.js");
  const GURL kDocumentUrl("http://www.test.not/scope/document.html");
  const int64 kRegistrationId = 0;
  const int64 kVersionId = 0;

  scoped_refptr<ServiceWorkerRegistration> found_registration;

  // We shouldn't find anything without having stored anything.
  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForDocument(kDocumentUrl, &found_registration));
  EXPECT_FALSE(found_registration);

  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForPattern(kScope, &found_registration));
  EXPECT_FALSE(found_registration);

  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForId(
                kRegistrationId, kScope.GetOrigin(), &found_registration));
  EXPECT_FALSE(found_registration);

  // Store something.
  scoped_refptr<ServiceWorkerRegistration> live_registration =
      new ServiceWorkerRegistration(
          kScope, kScript, kRegistrationId, context_ptr_);
  scoped_refptr<ServiceWorkerVersion> live_version =
      new ServiceWorkerVersion(
          live_registration, kVersionId, context_ptr_);
  live_version->SetStatus(ServiceWorkerVersion::INSTALLED);
  live_registration->set_waiting_version(live_version);
  EXPECT_EQ(SERVICE_WORKER_OK,
            StoreRegistration(live_registration, live_version));

  // Now we should find it and get the live ptr back immediately.
  EXPECT_EQ(SERVICE_WORKER_OK,
            FindRegistrationForDocument(kDocumentUrl, &found_registration));
  EXPECT_EQ(live_registration, found_registration);
  found_registration = NULL;

  // But FindRegistrationForPattern is always async.
  EXPECT_EQ(SERVICE_WORKER_OK,
            FindRegistrationForPattern(kScope, &found_registration));
  EXPECT_EQ(live_registration, found_registration);
  found_registration = NULL;

  // Can be found by id too.
  EXPECT_EQ(SERVICE_WORKER_OK,
            FindRegistrationForId(
                kRegistrationId, kScope.GetOrigin(), &found_registration));
  ASSERT_TRUE(found_registration);
  EXPECT_EQ(kRegistrationId, found_registration->id());
  EXPECT_EQ(live_registration, found_registration);
  found_registration = NULL;

  // Drop the live registration, but keep the version live.
  live_registration = NULL;

  // Now FindRegistrationForDocument should be async.
  EXPECT_EQ(SERVICE_WORKER_OK,
            FindRegistrationForDocument(kDocumentUrl, &found_registration));
  ASSERT_TRUE(found_registration);
  EXPECT_EQ(kRegistrationId, found_registration->id());
  EXPECT_TRUE(found_registration->HasOneRef());
  EXPECT_EQ(live_version, found_registration->waiting_version());
  found_registration = NULL;

  // Drop the live version too.
  live_version = NULL;

  // And FindRegistrationForPattern is always async.
  EXPECT_EQ(SERVICE_WORKER_OK,
            FindRegistrationForPattern(kScope, &found_registration));
  ASSERT_TRUE(found_registration);
  EXPECT_EQ(kRegistrationId, found_registration->id());
  EXPECT_TRUE(found_registration->HasOneRef());
  EXPECT_FALSE(found_registration->active_version());
  ASSERT_TRUE(found_registration->waiting_version());
  EXPECT_EQ(ServiceWorkerVersion::INSTALLED,
            found_registration->waiting_version()->status());

  // Update to active.
  scoped_refptr<ServiceWorkerVersion> temp_version =
      found_registration->waiting_version();
  found_registration->set_waiting_version(NULL);
  temp_version->SetStatus(ServiceWorkerVersion::ACTIVE);
  found_registration->set_active_version(temp_version);
  temp_version = NULL;
  EXPECT_EQ(SERVICE_WORKER_OK, UpdateToActiveState(found_registration));
  found_registration = NULL;

  // Trying to update a unstored registration to active should fail.
  scoped_refptr<ServiceWorkerRegistration> unstored_registration =
      new ServiceWorkerRegistration(
          kScope, kScript, kRegistrationId + 1, context_ptr_);
  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            UpdateToActiveState(unstored_registration));
  unstored_registration = NULL;

  // The Find methods should return a registration with an active version.
  EXPECT_EQ(SERVICE_WORKER_OK,
            FindRegistrationForDocument(kDocumentUrl, &found_registration));
  ASSERT_TRUE(found_registration);
  EXPECT_EQ(kRegistrationId, found_registration->id());
  EXPECT_TRUE(found_registration->HasOneRef());
  EXPECT_FALSE(found_registration->waiting_version());
  ASSERT_TRUE(found_registration->active_version());
  EXPECT_EQ(ServiceWorkerVersion::ACTIVE,
            found_registration->active_version()->status());

  // Delete from storage but with a instance still live.
  EXPECT_TRUE(context_->GetLiveVersion(kRegistrationId));
  EXPECT_EQ(SERVICE_WORKER_OK,
            DeleteRegistration(kRegistrationId, kScope.GetOrigin()));
  EXPECT_TRUE(context_->GetLiveVersion(kRegistrationId));

  // Should no longer be found.
  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForId(
                kRegistrationId, kScope.GetOrigin(), &found_registration));
  EXPECT_FALSE(found_registration);

  // Deleting an unstored registration should succeed.
  EXPECT_EQ(SERVICE_WORKER_OK,
            DeleteRegistration(kRegistrationId + 1, kScope.GetOrigin()));
}

TEST_F(ServiceWorkerStorageTest, InstallingRegistrationsAreFindable) {
  const GURL kScope("http://www.test.not/scope/*");
  const GURL kScript("http://www.test.not/script.js");
  const GURL kDocumentUrl("http://www.test.not/scope/document.html");
  const int64 kRegistrationId = 0;
  const int64 kVersionId = 0;

  scoped_refptr<ServiceWorkerRegistration> found_registration;

  // Create an unstored registration.
  scoped_refptr<ServiceWorkerRegistration> live_registration =
      new ServiceWorkerRegistration(
          kScope, kScript, kRegistrationId, context_ptr_);
  scoped_refptr<ServiceWorkerVersion> live_version =
      new ServiceWorkerVersion(
          live_registration, kVersionId, context_ptr_);
  live_version->SetStatus(ServiceWorkerVersion::INSTALLING);
  live_registration->set_waiting_version(live_version);

  // Should not be findable, including by GetAllRegistrations.
  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForId(
                kRegistrationId, kScope.GetOrigin(), &found_registration));
  EXPECT_FALSE(found_registration);

  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForDocument(kDocumentUrl, &found_registration));
  EXPECT_FALSE(found_registration);

  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForPattern(kScope, &found_registration));
  EXPECT_FALSE(found_registration);

  std::vector<ServiceWorkerRegistrationInfo> all_registrations;
  GetAllRegistrations(&all_registrations);
  EXPECT_TRUE(all_registrations.empty());

  // Notify storage of it being installed.
  storage()->NotifyInstallingRegistration(live_registration);

  // Now should be findable.
  EXPECT_EQ(SERVICE_WORKER_OK,
            FindRegistrationForId(
                kRegistrationId, kScope.GetOrigin(), &found_registration));
  EXPECT_EQ(live_registration, found_registration);
  found_registration = NULL;

  EXPECT_EQ(SERVICE_WORKER_OK,
            FindRegistrationForDocument(kDocumentUrl, &found_registration));
  EXPECT_EQ(live_registration, found_registration);
  found_registration = NULL;

  EXPECT_EQ(SERVICE_WORKER_OK,
            FindRegistrationForPattern(kScope, &found_registration));
  EXPECT_EQ(live_registration, found_registration);
  found_registration = NULL;

  GetAllRegistrations(&all_registrations);
  EXPECT_EQ(1u, all_registrations.size());
  all_registrations.clear();

  // Notify storage of installation no longer happening.
  storage()->NotifyDoneInstallingRegistration(
      live_registration, NULL, SERVICE_WORKER_OK);

  // Once again, should not be findable.
  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForId(
                kRegistrationId, kScope.GetOrigin(), &found_registration));
  EXPECT_FALSE(found_registration);

  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForDocument(kDocumentUrl, &found_registration));
  EXPECT_FALSE(found_registration);

  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForPattern(kScope, &found_registration));
  EXPECT_FALSE(found_registration);

  GetAllRegistrations(&all_registrations);
  EXPECT_TRUE(all_registrations.empty());
}

TEST_F(ServiceWorkerStorageTest, ResourceIdsAreStoredAndPurged) {
  storage()->LazyInitialize(base::Bind(&base::DoNothing));
  base::RunLoop().RunUntilIdle();
  const GURL kScope("http://www.test.not/scope/*");
  const GURL kScript("http://www.test.not/script.js");
  const GURL kImport("http://www.test.not/import.js");
  const GURL kDocumentUrl("http://www.test.not/scope/document.html");
  const int64 kRegistrationId = storage()->NewRegistrationId();
  const int64 kVersionId = storage()->NewVersionId();
  const int64 kResourceId1 = storage()->NewResourceId();
  const int64 kResourceId2 = storage()->NewResourceId();

  // Cons up a new registration+version with two script resources.
  RegistrationData data;
  data.registration_id = kRegistrationId;
  data.scope = kScope;
  data.script = kScript;
  data.version_id = kVersionId;
  data.is_active = false;
  std::vector<ResourceRecord> resources;
  resources.push_back(ResourceRecord(kResourceId1, kScript));
  resources.push_back(ResourceRecord(kResourceId2, kImport));
  scoped_refptr<ServiceWorkerRegistration> registration =
      storage()->GetOrCreateRegistration(data, resources);
  registration->waiting_version()->SetStatus(ServiceWorkerVersion::NEW);

  // Add the resources ids to the uncommitted list.
  std::set<int64> resource_ids;
  resource_ids.insert(kResourceId1);
  resource_ids.insert(kResourceId2);
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            storage()->database_->WriteUncommittedResourceIds(resource_ids));

  // And dump something in the disk cache for them.
  WriteBasicResponse(storage(), kResourceId1);
  WriteBasicResponse(storage(), kResourceId2);
  EXPECT_TRUE(VerifyBasicResponse(storage(), kResourceId1, true));
  EXPECT_TRUE(VerifyBasicResponse(storage(), kResourceId2, true));

  // Storing the registration/version should take the resources ids out
  // of the uncommitted list.
  EXPECT_EQ(SERVICE_WORKER_OK,
            StoreRegistration(registration, registration->waiting_version()));
  std::set<int64> verify_ids;
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            storage()->database_->GetUncommittedResourceIds(&verify_ids));
  EXPECT_TRUE(verify_ids.empty());

  // Deleting it should result in the resources being added to the
  // purgeable list and then doomed in the disk cache and removed from
  // that list.
  bool was_called = false;
  ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_FAILED;
  verify_ids.clear();
  storage()->DeleteRegistration(
      registration->id(), kScope.GetOrigin(),
      base::Bind(&VerifyPurgeableListStatusCallback,
                 base::Unretained(storage()->database_.get()),
                 &verify_ids, &was_called, &result));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(was_called);
  EXPECT_EQ(SERVICE_WORKER_OK, result);
  EXPECT_EQ(2u, verify_ids.size());
  verify_ids.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            storage()->database_->GetPurgeableResourceIds(&verify_ids));
  EXPECT_TRUE(verify_ids.empty());

  EXPECT_FALSE(VerifyBasicResponse(storage(), kResourceId1, false));
  EXPECT_FALSE(VerifyBasicResponse(storage(), kResourceId2, false));
}

TEST_F(ServiceWorkerStorageTest, FindRegistration_LongestScopeMatch) {
  const GURL kDocumentUrl("http://www.example.com/scope/foo");
  scoped_refptr<ServiceWorkerRegistration> found_registration;

  // Registration for "/scope/*".
  const GURL kScope1("http://www.example.com/scope/*");
  const GURL kScript1("http://www.example.com/script1.js");
  const int64 kRegistrationId1 = 1;
  const int64 kVersionId1 = 1;
  scoped_refptr<ServiceWorkerRegistration> live_registration1 =
      new ServiceWorkerRegistration(
          kScope1, kScript1, kRegistrationId1, context_ptr_);
  scoped_refptr<ServiceWorkerVersion> live_version1 =
      new ServiceWorkerVersion(
          live_registration1, kVersionId1, context_ptr_);
  live_version1->SetStatus(ServiceWorkerVersion::INSTALLED);
  live_registration1->set_waiting_version(live_version1);

  // Registration for "/scope/foo*".
  const GURL kScope2("http://www.example.com/scope/foo*");
  const GURL kScript2("http://www.example.com/script2.js");
  const int64 kRegistrationId2 = 2;
  const int64 kVersionId2 = 2;
  scoped_refptr<ServiceWorkerRegistration> live_registration2 =
      new ServiceWorkerRegistration(
          kScope2, kScript2, kRegistrationId2, context_ptr_);
  scoped_refptr<ServiceWorkerVersion> live_version2 =
      new ServiceWorkerVersion(
          live_registration2, kVersionId2, context_ptr_);
  live_version2->SetStatus(ServiceWorkerVersion::INSTALLED);
  live_registration2->set_waiting_version(live_version2);

  // Registration for "/scope/foo".
  const GURL kScope3("http://www.example.com/scope/foo");
  const GURL kScript3("http://www.example.com/script3.js");
  const int64 kRegistrationId3 = 3;
  const int64 kVersionId3 = 3;
  scoped_refptr<ServiceWorkerRegistration> live_registration3 =
      new ServiceWorkerRegistration(
          kScope3, kScript3, kRegistrationId3, context_ptr_);
  scoped_refptr<ServiceWorkerVersion> live_version3 =
      new ServiceWorkerVersion(
          live_registration3, kVersionId3, context_ptr_);
  live_version3->SetStatus(ServiceWorkerVersion::INSTALLED);
  live_registration3->set_waiting_version(live_version3);

  // Notify storage of they being installed.
  storage()->NotifyInstallingRegistration(live_registration1);
  storage()->NotifyInstallingRegistration(live_registration2);
  storage()->NotifyInstallingRegistration(live_registration3);

  // Find a registration among installing ones.
  EXPECT_EQ(SERVICE_WORKER_OK,
            FindRegistrationForDocument(kDocumentUrl, &found_registration));
  EXPECT_EQ(live_registration2, found_registration);
  found_registration = NULL;

  // Store registrations.
  EXPECT_EQ(SERVICE_WORKER_OK,
            StoreRegistration(live_registration1, live_version1));
  EXPECT_EQ(SERVICE_WORKER_OK,
            StoreRegistration(live_registration2, live_version2));
  EXPECT_EQ(SERVICE_WORKER_OK,
            StoreRegistration(live_registration3, live_version3));

  // Notify storage of installations no longer happening.
  storage()->NotifyDoneInstallingRegistration(
      live_registration1, NULL, SERVICE_WORKER_OK);
  storage()->NotifyDoneInstallingRegistration(
      live_registration2, NULL, SERVICE_WORKER_OK);
  storage()->NotifyDoneInstallingRegistration(
      live_registration3, NULL, SERVICE_WORKER_OK);

  // Find a registration among installed ones.
  EXPECT_EQ(SERVICE_WORKER_OK,
            FindRegistrationForDocument(kDocumentUrl, &found_registration));
  EXPECT_EQ(live_registration2, found_registration);
}

}  // namespace content
