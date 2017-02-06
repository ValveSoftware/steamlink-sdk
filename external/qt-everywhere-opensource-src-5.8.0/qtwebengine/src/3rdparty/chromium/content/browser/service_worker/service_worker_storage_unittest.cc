// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_storage.h"

#include <stdint.h>
#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "ipc/ipc_message.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::IOBuffer;
using net::TestCompletionCallback;
using net::WrappedIOBuffer;

namespace content {

namespace {

typedef ServiceWorkerDatabase::RegistrationData RegistrationData;
typedef ServiceWorkerDatabase::ResourceRecord ResourceRecord;

void StatusAndQuitCallback(ServiceWorkerStatusCode* result,
                           const base::Closure& quit_closure,
                           ServiceWorkerStatusCode status) {
  *result = status;
  quit_closure.Run();
}

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
    ServiceWorkerStatusCode* result,
    std::vector<scoped_refptr<ServiceWorkerRegistration>>* all_out,
    ServiceWorkerStatusCode status,
    const std::vector<scoped_refptr<ServiceWorkerRegistration>>& all) {
  *was_called = true;
  *result = status;
  *all_out = all;
}

void GetAllInfosCallback(
    bool* was_called,
    ServiceWorkerStatusCode* result,
    std::vector<ServiceWorkerRegistrationInfo>* all_out,
    ServiceWorkerStatusCode status,
    const std::vector<ServiceWorkerRegistrationInfo>& all) {
  *was_called = true;
  *result = status;
  *all_out = all;
}

ServiceWorkerStorage::GetRegistrationsCallback MakeGetRegistrationsCallback(
    bool* was_called,
    ServiceWorkerStatusCode* status,
    std::vector<scoped_refptr<ServiceWorkerRegistration>>* all) {
  return base::Bind(&GetAllCallback, was_called, status, all);
}

ServiceWorkerStorage::GetRegistrationsInfosCallback
MakeGetRegistrationsInfosCallback(
    bool* was_called,
    ServiceWorkerStatusCode* status,
    std::vector<ServiceWorkerRegistrationInfo>* all) {
  return base::Bind(&GetAllInfosCallback, was_called, status, all);
}

void GetUserDataCallback(bool* was_called,
                         std::vector<std::string>* data_out,
                         ServiceWorkerStatusCode* status_out,
                         const std::vector<std::string>& data,
                         ServiceWorkerStatusCode status) {
  *was_called = true;
  *data_out = data;
  *status_out = status;
}

void GetUserDataForAllRegistrationsCallback(
    bool* was_called,
    std::vector<std::pair<int64_t, std::string>>* data_out,
    ServiceWorkerStatusCode* status_out,
    const std::vector<std::pair<int64_t, std::string>>& data,
    ServiceWorkerStatusCode status) {
  *was_called = true;
  *data_out = data;
  *status_out = status;
}

int WriteResponse(ServiceWorkerStorage* storage,
                  int64_t id,
                  const std::string& headers,
                  IOBuffer* body,
                  int length) {
  std::unique_ptr<ServiceWorkerResponseWriter> writer =
      storage->CreateResponseWriter(id);

  std::unique_ptr<net::HttpResponseInfo> info(new net::HttpResponseInfo);
  info->request_time = base::Time::Now();
  info->response_time = base::Time::Now();
  info->was_cached = false;
  info->headers = new net::HttpResponseHeaders(headers);
  scoped_refptr<HttpResponseInfoIOBuffer> info_buffer =
      new HttpResponseInfoIOBuffer(info.release());
  int rv = 0;
  {
    TestCompletionCallback cb;
    writer->WriteInfo(info_buffer.get(), cb.callback());
    rv = cb.WaitForResult();
    if (rv < 0)
      return rv;
  }
  {
    TestCompletionCallback cb;
    writer->WriteData(body, length, cb.callback());
    rv = cb.WaitForResult();
  }
  return rv;
}

int WriteStringResponse(ServiceWorkerStorage* storage,
                        int64_t id,
                        const std::string& headers,
                        const std::string& body) {
  scoped_refptr<IOBuffer> body_buffer(new WrappedIOBuffer(body.data()));
  return WriteResponse(storage, id, headers, body_buffer.get(), body.length());
}

int WriteBasicResponse(ServiceWorkerStorage* storage, int64_t id) {
  const char kHttpHeaders[] = "HTTP/1.0 200 HONKYDORY\0Content-Length: 5\0\0";
  const char kHttpBody[] = "Hello";
  std::string headers(kHttpHeaders, arraysize(kHttpHeaders));
  return WriteStringResponse(storage, id, headers, std::string(kHttpBody));
}

int ReadResponseInfo(ServiceWorkerStorage* storage,
                     int64_t id,
                     HttpResponseInfoIOBuffer* info_buffer) {
  std::unique_ptr<ServiceWorkerResponseReader> reader =
      storage->CreateResponseReader(id);
  TestCompletionCallback cb;
  reader->ReadInfo(info_buffer, cb.callback());
  return cb.WaitForResult();
}

bool VerifyBasicResponse(ServiceWorkerStorage* storage,
                         int64_t id,
                         bool expected_positive_result) {
  const std::string kExpectedHttpBody("Hello");
  std::unique_ptr<ServiceWorkerResponseReader> reader =
      storage->CreateResponseReader(id);
  scoped_refptr<HttpResponseInfoIOBuffer> info_buffer =
      new HttpResponseInfoIOBuffer();
  int rv = ReadResponseInfo(storage, id, info_buffer.get());
  if (expected_positive_result)
    EXPECT_LT(0, rv);
  if (rv <= 0)
    return false;

  std::string received_body;
  const int kBigEnough = 512;
  scoped_refptr<net::IOBuffer> buffer = new IOBuffer(kBigEnough);
  TestCompletionCallback cb;
  reader->ReadData(buffer.get(), kBigEnough, cb.callback());
  rv = cb.WaitForResult();
  EXPECT_EQ(static_cast<int>(kExpectedHttpBody.size()), rv);
  if (rv <= 0)
    return false;
  received_body.assign(buffer->data(), rv);

  bool status_match =
      std::string("HONKYDORY") ==
          info_buffer->http_info->headers->GetStatusText();
  bool data_match = kExpectedHttpBody == received_body;

  EXPECT_TRUE(status_match);
  EXPECT_TRUE(data_match);
  return status_match && data_match;
}

int WriteResponseMetadata(ServiceWorkerStorage* storage,
                          int64_t id,
                          const std::string& metadata) {
  scoped_refptr<IOBuffer> body_buffer(new WrappedIOBuffer(metadata.data()));
  std::unique_ptr<ServiceWorkerResponseMetadataWriter> metadata_writer =
      storage->CreateResponseMetadataWriter(id);
  TestCompletionCallback cb;
  metadata_writer->WriteMetadata(body_buffer.get(), metadata.length(),
                                 cb.callback());
  return cb.WaitForResult();
}

int WriteMetadata(ServiceWorkerVersion* version,
                  const GURL& url,
                  const std::string& metadata) {
  const std::vector<char> data(metadata.begin(), metadata.end());
  EXPECT_TRUE(version);
  TestCompletionCallback cb;
  version->script_cache_map()->WriteMetadata(url, data, cb.callback());
  return cb.WaitForResult();
}

int ClearMetadata(ServiceWorkerVersion* version, const GURL& url) {
  EXPECT_TRUE(version);
  TestCompletionCallback cb;
  version->script_cache_map()->ClearMetadata(url, cb.callback());
  return cb.WaitForResult();
}

bool VerifyResponseMetadata(ServiceWorkerStorage* storage,
                            int64_t id,
                            const std::string& expected_metadata) {
  std::unique_ptr<ServiceWorkerResponseReader> reader =
      storage->CreateResponseReader(id);
  scoped_refptr<HttpResponseInfoIOBuffer> info_buffer =
      new HttpResponseInfoIOBuffer();
  {
    TestCompletionCallback cb;
    reader->ReadInfo(info_buffer.get(), cb.callback());
    int rv = cb.WaitForResult();
    EXPECT_LT(0, rv);
  }
  const net::HttpResponseInfo* read_head = info_buffer->http_info.get();
  if (!read_head->metadata.get())
    return false;
  EXPECT_EQ(0, memcmp(expected_metadata.data(), read_head->metadata->data(),
                      expected_metadata.length()));
  return true;
}

}  // namespace

class ServiceWorkerStorageTest : public testing::Test {
 public:
  ServiceWorkerStorageTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {
  }

  void SetUp() override { InitializeTestHelper(); }

  void TearDown() override {
    helper_.reset();
    base::RunLoop().RunUntilIdle();
  }

  base::FilePath GetUserDataDirectory() { return user_data_directory_.path(); }

  bool InitUserDataDirectory() {
    return user_data_directory_.CreateUniqueTempDir();
  }

  void InitializeTestHelper() {
    helper_.reset(new EmbeddedWorkerTestHelper(GetUserDataDirectory()));
    base::RunLoop().RunUntilIdle();
  }

  ServiceWorkerContextCore* context() { return helper_->context(); }
  ServiceWorkerStorage* storage() { return helper_->context()->storage(); }

  // A static class method for friendliness.
  static void VerifyPurgeableListStatusCallback(
      ServiceWorkerDatabase* database,
      std::set<int64_t>* purgeable_ids,
      bool* was_called,
      ServiceWorkerStatusCode* result,
      ServiceWorkerStatusCode status) {
    *was_called = true;
    *result = status;
    EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
              database->GetPurgeableResourceIds(purgeable_ids));
  }

 protected:
  void LazyInitialize() {
    storage()->LazyInitialize(base::Bind(&base::DoNothing));
    base::RunLoop().RunUntilIdle();
  }

  ServiceWorkerStatusCode StoreRegistration(
      scoped_refptr<ServiceWorkerRegistration> registration,
      scoped_refptr<ServiceWorkerVersion> version) {
    bool was_called = false;
    ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_MAX_VALUE;
    storage()->StoreRegistration(registration.get(),
                                 version.get(),
                                 MakeStatusCallback(&was_called, &result));
    EXPECT_FALSE(was_called);  // always async
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
    return result;
  }

  ServiceWorkerStatusCode DeleteRegistration(int64_t registration_id,
                                             const GURL& origin) {
    bool was_called = false;
    ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_MAX_VALUE;
    storage()->DeleteRegistration(
        registration_id, origin, MakeStatusCallback(&was_called, &result));
    EXPECT_FALSE(was_called);  // always async
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
    return result;
  }

  ServiceWorkerStatusCode GetAllRegistrationsInfos(
      std::vector<ServiceWorkerRegistrationInfo>* registrations) {
    bool was_called = false;
    ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_MAX_VALUE;
    storage()->GetAllRegistrationsInfos(
        MakeGetRegistrationsInfosCallback(&was_called, &result, registrations));
    EXPECT_FALSE(was_called);  // always async
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
    return result;
  }

  ServiceWorkerStatusCode GetRegistrationsForOrigin(
      const GURL& origin,
      std::vector<scoped_refptr<ServiceWorkerRegistration>>* registrations) {
    bool was_called = false;
    ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_MAX_VALUE;
    storage()->GetRegistrationsForOrigin(
        origin,
        MakeGetRegistrationsCallback(&was_called, &result, registrations));
    EXPECT_FALSE(was_called);  // always async
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
    return result;
  }

  ServiceWorkerStatusCode GetUserData(int64_t registration_id,
                                      const std::vector<std::string>& keys,
                                      std::vector<std::string>* data) {
    bool was_called = false;
    ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_MAX_VALUE;
    storage()->GetUserData(
        registration_id, keys,
        base::Bind(&GetUserDataCallback, &was_called, data, &result));
    EXPECT_FALSE(was_called);  // always async
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
    return result;
  }

  ServiceWorkerStatusCode StoreUserData(
      int64_t registration_id,
      const GURL& origin,
      const std::vector<std::pair<std::string, std::string>>& key_value_pairs) {
    bool was_called = false;
    ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_MAX_VALUE;
    storage()->StoreUserData(registration_id, origin, key_value_pairs,
                             MakeStatusCallback(&was_called, &result));
    EXPECT_FALSE(was_called);  // always async
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
    return result;
  }

  ServiceWorkerStatusCode ClearUserData(int64_t registration_id,
                                        const std::vector<std::string>& keys) {
    bool was_called = false;
    ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_MAX_VALUE;
    storage()->ClearUserData(registration_id, keys,
                             MakeStatusCallback(&was_called, &result));
    EXPECT_FALSE(was_called);  // always async
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
    return result;
  }

  ServiceWorkerStatusCode GetUserDataForAllRegistrations(
      const std::string& key,
      std::vector<std::pair<int64_t, std::string>>* data) {
    bool was_called = false;
    ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_MAX_VALUE;
    storage()->GetUserDataForAllRegistrations(
        key, base::Bind(&GetUserDataForAllRegistrationsCallback, &was_called,
                        data, &result));
    EXPECT_FALSE(was_called);  // always async
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
    return result;
  }

  ServiceWorkerStatusCode UpdateToActiveState(
      const scoped_refptr<ServiceWorkerRegistration>& registration) {
    bool was_called = false;
    ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_MAX_VALUE;
    storage()->UpdateToActiveState(registration.get(),
                                   MakeStatusCallback(&was_called, &result));
    EXPECT_FALSE(was_called);  // always async
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
    return result;
  }

  void UpdateLastUpdateCheckTime(
      const scoped_refptr<ServiceWorkerRegistration>& registration) {
    storage()->UpdateLastUpdateCheckTime(registration.get());
    base::RunLoop().RunUntilIdle();
  }

  ServiceWorkerStatusCode FindRegistrationForDocument(
      const GURL& document_url,
      scoped_refptr<ServiceWorkerRegistration>* registration) {
    bool was_called = false;
    ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_MAX_VALUE;
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
    ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_MAX_VALUE;
    storage()->FindRegistrationForPattern(
        scope, MakeFindCallback(&was_called, &result, registration));
    EXPECT_FALSE(was_called);  // always async
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
    return result;
  }

  ServiceWorkerStatusCode FindRegistrationForId(
      int64_t registration_id,
      const GURL& origin,
      scoped_refptr<ServiceWorkerRegistration>* registration) {
    bool was_called = false;
    ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_MAX_VALUE;
    storage()->FindRegistrationForId(
        registration_id, origin,
        MakeFindCallback(&was_called, &result, registration));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
    return result;
  }

  ServiceWorkerStatusCode FindRegistrationForIdOnly(
      int64_t registration_id,
      scoped_refptr<ServiceWorkerRegistration>* registration) {
    bool was_called = false;
    ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_MAX_VALUE;
    storage()->FindRegistrationForIdOnly(
        registration_id, MakeFindCallback(&was_called, &result, registration));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
    return result;
  }

  // user_data_directory_ must be declared first to preserve destructor order.
  base::ScopedTempDir user_data_directory_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  TestBrowserThreadBundle browser_thread_bundle_;
};

TEST_F(ServiceWorkerStorageTest, DisabledStorage) {
  const GURL kScope("http://www.example.com/scope/");
  const GURL kScript("http://www.example.com/script.js");
  const GURL kDocumentUrl("http://www.example.com/scope/document.html");
  const int64_t kRegistrationId = 0;
  const int64_t kVersionId = 0;
  const int64_t kResourceId = 0;

  LazyInitialize();
  storage()->Disable();

  scoped_refptr<ServiceWorkerRegistration> found_registration;
  EXPECT_EQ(SERVICE_WORKER_ERROR_ABORT,
            FindRegistrationForDocument(kDocumentUrl, &found_registration));
  EXPECT_EQ(SERVICE_WORKER_ERROR_ABORT,
            FindRegistrationForPattern(kScope, &found_registration));
  EXPECT_EQ(SERVICE_WORKER_ERROR_ABORT,
            FindRegistrationForId(kRegistrationId, kScope.GetOrigin(),
                                  &found_registration));
  EXPECT_EQ(SERVICE_WORKER_ERROR_ABORT,
            FindRegistrationForIdOnly(kRegistrationId, &found_registration));
  EXPECT_FALSE(storage()->GetUninstallingRegistration(kScope.GetOrigin()));

  std::vector<scoped_refptr<ServiceWorkerRegistration>> found_registrations;
  EXPECT_EQ(
      SERVICE_WORKER_ERROR_ABORT,
      GetRegistrationsForOrigin(kScope.GetOrigin(), &found_registrations));

  std::vector<ServiceWorkerRegistrationInfo> all_registrations;
  EXPECT_EQ(SERVICE_WORKER_ERROR_ABORT,
            GetAllRegistrationsInfos(&all_registrations));

  scoped_refptr<ServiceWorkerRegistration> live_registration =
      new ServiceWorkerRegistration(kScope, kRegistrationId,
                                    context()->AsWeakPtr());
  scoped_refptr<ServiceWorkerVersion> live_version = new ServiceWorkerVersion(
      live_registration.get(), kScript, kVersionId, context()->AsWeakPtr());
  EXPECT_EQ(SERVICE_WORKER_ERROR_ABORT,
            StoreRegistration(live_registration, live_version));

  EXPECT_EQ(SERVICE_WORKER_ERROR_ABORT, UpdateToActiveState(live_registration));

  EXPECT_EQ(SERVICE_WORKER_ERROR_ABORT,
            DeleteRegistration(kRegistrationId, kScope.GetOrigin()));

  // Response reader and writer created by the disabled storage should fail to
  // access the disk cache.
  scoped_refptr<HttpResponseInfoIOBuffer> info_buffer =
      new HttpResponseInfoIOBuffer();
  EXPECT_EQ(net::ERR_CACHE_MISS,
            ReadResponseInfo(storage(), kResourceId, info_buffer.get()));
  EXPECT_EQ(net::ERR_FAILED, WriteBasicResponse(storage(), kResourceId));
  EXPECT_EQ(net::ERR_FAILED,
            WriteResponseMetadata(storage(), kResourceId, "foo"));

  const std::string kUserDataKey = "key";
  std::vector<std::string> user_data_out;
  EXPECT_EQ(SERVICE_WORKER_ERROR_ABORT,
            GetUserData(kRegistrationId, {kUserDataKey}, &user_data_out));
  EXPECT_EQ(SERVICE_WORKER_ERROR_ABORT,
            StoreUserData(kRegistrationId, kScope.GetOrigin(),
                          {{kUserDataKey, "foo"}}));
  EXPECT_EQ(SERVICE_WORKER_ERROR_ABORT,
            ClearUserData(kRegistrationId, {kUserDataKey}));
  std::vector<std::pair<int64_t, std::string>> data_list_out;
  EXPECT_EQ(SERVICE_WORKER_ERROR_ABORT,
            GetUserDataForAllRegistrations(kUserDataKey, &data_list_out));

  EXPECT_FALSE(
      storage()->OriginHasForeignFetchRegistrations(kScope.GetOrigin()));

  // Next available ids should be invalid.
  EXPECT_EQ(kInvalidServiceWorkerRegistrationId,
            storage()->NewRegistrationId());
  EXPECT_EQ(kInvalidServiceWorkerVersionId, storage()->NewVersionId());
  EXPECT_EQ(kInvalidServiceWorkerResourceId, storage()->NewRegistrationId());
}

TEST_F(ServiceWorkerStorageTest, StoreFindUpdateDeleteRegistration) {
  const GURL kScope("http://www.test.not/scope/");
  const GURL kScript("http://www.test.not/script.js");
  const GURL kDocumentUrl("http://www.test.not/scope/document.html");
  const GURL kResource1("http://www.test.not/scope/resource1.js");
  const int64_t kResource1Size = 1591234;
  const GURL kResource2("http://www.test.not/scope/resource2.js");
  const int64_t kResource2Size = 51;
  const int64_t kRegistrationId = 0;
  const int64_t kVersionId = 0;
  const GURL kForeignFetchScope("http://www.test.not/scope/ff/");
  const url::Origin kForeignFetchOrigin(GURL("https://example.com/"));
  const base::Time kToday = base::Time::Now();
  const base::Time kYesterday = kToday - base::TimeDelta::FromDays(1);

  scoped_refptr<ServiceWorkerRegistration> found_registration;

  // We shouldn't find anything without having stored anything.
  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForDocument(kDocumentUrl, &found_registration));
  EXPECT_FALSE(found_registration.get());

  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForPattern(kScope, &found_registration));
  EXPECT_FALSE(found_registration.get());

  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForId(
                kRegistrationId, kScope.GetOrigin(), &found_registration));
  EXPECT_FALSE(found_registration.get());

  std::vector<ServiceWorkerDatabase::ResourceRecord> resources;
  resources.push_back(
      ServiceWorkerDatabase::ResourceRecord(1, kResource1, kResource1Size));
  resources.push_back(
      ServiceWorkerDatabase::ResourceRecord(2, kResource2, kResource2Size));

  // Store something.
  scoped_refptr<ServiceWorkerRegistration> live_registration =
      new ServiceWorkerRegistration(kScope, kRegistrationId,
                                    context()->AsWeakPtr());
  scoped_refptr<ServiceWorkerVersion> live_version = new ServiceWorkerVersion(
      live_registration.get(), kScript, kVersionId, context()->AsWeakPtr());
  live_version->SetStatus(ServiceWorkerVersion::INSTALLED);
  live_version->script_cache_map()->SetResources(resources);
  live_version->set_foreign_fetch_scopes(
      std::vector<GURL>(1, kForeignFetchScope));
  live_version->set_foreign_fetch_origins(
      std::vector<url::Origin>(1, kForeignFetchOrigin));
  live_registration->SetWaitingVersion(live_version);
  live_registration->set_last_update_check(kYesterday);
  EXPECT_EQ(SERVICE_WORKER_OK,
            StoreRegistration(live_registration, live_version));

  // Now we should find it and get the live ptr back immediately.
  EXPECT_EQ(SERVICE_WORKER_OK,
            FindRegistrationForDocument(kDocumentUrl, &found_registration));
  EXPECT_EQ(live_registration, found_registration);
  EXPECT_EQ(kResource1Size + kResource2Size,
            live_registration->resources_total_size_bytes());
  EXPECT_EQ(kResource1Size + kResource2Size,
            found_registration->resources_total_size_bytes());
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
  ASSERT_TRUE(found_registration.get());
  EXPECT_EQ(kRegistrationId, found_registration->id());
  EXPECT_EQ(live_registration, found_registration);
  found_registration = NULL;

  // Can be found by just the id too.
  EXPECT_EQ(SERVICE_WORKER_OK,
            FindRegistrationForIdOnly(kRegistrationId, &found_registration));
  ASSERT_TRUE(found_registration.get());
  EXPECT_EQ(kRegistrationId, found_registration->id());
  EXPECT_EQ(live_registration, found_registration);
  found_registration = NULL;

  // Drop the live registration, but keep the version live.
  live_registration = NULL;

  // Now FindRegistrationForDocument should be async.
  EXPECT_EQ(SERVICE_WORKER_OK,
            FindRegistrationForDocument(kDocumentUrl, &found_registration));
  ASSERT_TRUE(found_registration.get());
  EXPECT_EQ(kRegistrationId, found_registration->id());
  EXPECT_TRUE(found_registration->HasOneRef());

  // Check that sizes are populated correctly
  EXPECT_EQ(live_version.get(), found_registration->waiting_version());
  EXPECT_EQ(kResource1Size + kResource2Size,
            found_registration->resources_total_size_bytes());
  std::vector<ServiceWorkerRegistrationInfo> all_registrations;
  EXPECT_EQ(SERVICE_WORKER_OK, GetAllRegistrationsInfos(&all_registrations));
  EXPECT_EQ(1u, all_registrations.size());
  ServiceWorkerRegistrationInfo info = all_registrations[0];
  EXPECT_EQ(kResource1Size + kResource2Size, info.stored_version_size_bytes);
  all_registrations.clear();

  // Finding by origin should provide the same result if origin is kScope.
  std::vector<scoped_refptr<ServiceWorkerRegistration>>
      registrations_for_origin;
  EXPECT_EQ(
      SERVICE_WORKER_OK,
      GetRegistrationsForOrigin(kScope.GetOrigin(), &registrations_for_origin));
  EXPECT_EQ(1u, registrations_for_origin.size());
  registrations_for_origin.clear();

  EXPECT_EQ(SERVICE_WORKER_OK,
            GetRegistrationsForOrigin(GURL("http://example.com/").GetOrigin(),
                                      &registrations_for_origin));
  EXPECT_TRUE(registrations_for_origin.empty());

  found_registration = NULL;

  // Drop the live version too.
  live_version = NULL;

  // And FindRegistrationForPattern is always async.
  EXPECT_EQ(SERVICE_WORKER_OK,
            FindRegistrationForPattern(kScope, &found_registration));
  ASSERT_TRUE(found_registration.get());
  EXPECT_EQ(kRegistrationId, found_registration->id());
  EXPECT_TRUE(found_registration->HasOneRef());
  EXPECT_FALSE(found_registration->active_version());
  ASSERT_TRUE(found_registration->waiting_version());
  EXPECT_EQ(kYesterday, found_registration->last_update_check());
  EXPECT_EQ(ServiceWorkerVersion::INSTALLED,
            found_registration->waiting_version()->status());
  EXPECT_EQ(
      1u, found_registration->waiting_version()->foreign_fetch_scopes().size());
  EXPECT_EQ(kForeignFetchScope,
            found_registration->waiting_version()->foreign_fetch_scopes()[0]);
  EXPECT_EQ(
      1u,
      found_registration->waiting_version()->foreign_fetch_origins().size());
  EXPECT_EQ(kForeignFetchOrigin,
            found_registration->waiting_version()->foreign_fetch_origins()[0]);

  // Update to active and update the last check time.
  scoped_refptr<ServiceWorkerVersion> temp_version =
      found_registration->waiting_version();
  temp_version->SetStatus(ServiceWorkerVersion::ACTIVATED);
  found_registration->SetActiveVersion(temp_version);
  temp_version = NULL;
  EXPECT_EQ(SERVICE_WORKER_OK, UpdateToActiveState(found_registration));
  found_registration->set_last_update_check(kToday);
  UpdateLastUpdateCheckTime(found_registration.get());

  found_registration = NULL;

  // Trying to update a unstored registration to active should fail.
  scoped_refptr<ServiceWorkerRegistration> unstored_registration =
      new ServiceWorkerRegistration(kScope, kRegistrationId + 1,
                                    context()->AsWeakPtr());
  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            UpdateToActiveState(unstored_registration));
  unstored_registration = NULL;

  // The Find methods should return a registration with an active version
  // and the expected update time.
  EXPECT_EQ(SERVICE_WORKER_OK,
            FindRegistrationForDocument(kDocumentUrl, &found_registration));
  ASSERT_TRUE(found_registration.get());
  EXPECT_EQ(kRegistrationId, found_registration->id());
  EXPECT_TRUE(found_registration->HasOneRef());
  EXPECT_FALSE(found_registration->waiting_version());
  ASSERT_TRUE(found_registration->active_version());
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATED,
            found_registration->active_version()->status());
  EXPECT_EQ(kToday, found_registration->last_update_check());

  // Delete from storage but with a instance still live.
  EXPECT_TRUE(context()->GetLiveVersion(kRegistrationId));
  EXPECT_EQ(SERVICE_WORKER_OK,
            DeleteRegistration(kRegistrationId, kScope.GetOrigin()));
  EXPECT_TRUE(context()->GetLiveVersion(kRegistrationId));

  // Should no longer be found.
  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForId(
                kRegistrationId, kScope.GetOrigin(), &found_registration));
  EXPECT_FALSE(found_registration.get());
  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForIdOnly(kRegistrationId, &found_registration));
  EXPECT_FALSE(found_registration.get());

  // Deleting an unstored registration should succeed.
  EXPECT_EQ(SERVICE_WORKER_OK,
            DeleteRegistration(kRegistrationId + 1, kScope.GetOrigin()));
}

TEST_F(ServiceWorkerStorageTest, InstallingRegistrationsAreFindable) {
  const GURL kScope("http://www.test.not/scope/");
  const GURL kScript("http://www.test.not/script.js");
  const GURL kDocumentUrl("http://www.test.not/scope/document.html");
  const int64_t kRegistrationId = 0;
  const int64_t kVersionId = 0;

  scoped_refptr<ServiceWorkerRegistration> found_registration;

  // Create an unstored registration.
  scoped_refptr<ServiceWorkerRegistration> live_registration =
      new ServiceWorkerRegistration(kScope, kRegistrationId,
                                    context()->AsWeakPtr());
  scoped_refptr<ServiceWorkerVersion> live_version = new ServiceWorkerVersion(
      live_registration.get(), kScript, kVersionId, context()->AsWeakPtr());
  live_version->SetStatus(ServiceWorkerVersion::INSTALLING);
  live_registration->SetWaitingVersion(live_version);

  // Should not be findable, including by GetAllRegistrationsInfos.
  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForId(
                kRegistrationId, kScope.GetOrigin(), &found_registration));
  EXPECT_FALSE(found_registration.get());

  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForIdOnly(kRegistrationId, &found_registration));
  EXPECT_FALSE(found_registration.get());

  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForDocument(kDocumentUrl, &found_registration));
  EXPECT_FALSE(found_registration.get());

  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForPattern(kScope, &found_registration));
  EXPECT_FALSE(found_registration.get());

  std::vector<ServiceWorkerRegistrationInfo> all_registrations;
  EXPECT_EQ(SERVICE_WORKER_OK, GetAllRegistrationsInfos(&all_registrations));
  EXPECT_TRUE(all_registrations.empty());

  std::vector<scoped_refptr<ServiceWorkerRegistration>>
      registrations_for_origin;
  EXPECT_EQ(
      SERVICE_WORKER_OK,
      GetRegistrationsForOrigin(kScope.GetOrigin(), &registrations_for_origin));
  EXPECT_TRUE(registrations_for_origin.empty());

  EXPECT_EQ(SERVICE_WORKER_OK,
            GetRegistrationsForOrigin(GURL("http://example.com/").GetOrigin(),
                                      &registrations_for_origin));
  EXPECT_TRUE(registrations_for_origin.empty());

  // Notify storage of it being installed.
  storage()->NotifyInstallingRegistration(live_registration.get());

  // Now should be findable.
  EXPECT_EQ(SERVICE_WORKER_OK,
            FindRegistrationForId(
                kRegistrationId, kScope.GetOrigin(), &found_registration));
  EXPECT_EQ(live_registration, found_registration);
  found_registration = NULL;

  EXPECT_EQ(SERVICE_WORKER_OK,
            FindRegistrationForIdOnly(kRegistrationId, &found_registration));
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

  EXPECT_EQ(SERVICE_WORKER_OK, GetAllRegistrationsInfos(&all_registrations));
  EXPECT_EQ(1u, all_registrations.size());
  all_registrations.clear();

  // Finding by origin should provide the same result if origin is kScope.
  EXPECT_EQ(
      SERVICE_WORKER_OK,
      GetRegistrationsForOrigin(kScope.GetOrigin(), &registrations_for_origin));
  EXPECT_EQ(1u, registrations_for_origin.size());
  registrations_for_origin.clear();

  EXPECT_EQ(SERVICE_WORKER_OK,
            GetRegistrationsForOrigin(GURL("http://example.com/").GetOrigin(),
                                      &registrations_for_origin));
  EXPECT_TRUE(registrations_for_origin.empty());

  // Notify storage of installation no longer happening.
  storage()->NotifyDoneInstallingRegistration(
      live_registration.get(), NULL, SERVICE_WORKER_OK);

  // Once again, should not be findable.
  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForId(
                kRegistrationId, kScope.GetOrigin(), &found_registration));
  EXPECT_FALSE(found_registration.get());

  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForIdOnly(kRegistrationId, &found_registration));
  EXPECT_FALSE(found_registration.get());

  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForDocument(kDocumentUrl, &found_registration));
  EXPECT_FALSE(found_registration.get());

  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForPattern(kScope, &found_registration));
  EXPECT_FALSE(found_registration.get());

  EXPECT_EQ(SERVICE_WORKER_OK, GetAllRegistrationsInfos(&all_registrations));
  EXPECT_TRUE(all_registrations.empty());

  EXPECT_EQ(
      SERVICE_WORKER_OK,
      GetRegistrationsForOrigin(kScope.GetOrigin(), &registrations_for_origin));
  EXPECT_TRUE(registrations_for_origin.empty());

  EXPECT_EQ(SERVICE_WORKER_OK,
            GetRegistrationsForOrigin(GURL("http://example.com/").GetOrigin(),
                                      &registrations_for_origin));
  EXPECT_TRUE(registrations_for_origin.empty());
}

TEST_F(ServiceWorkerStorageTest, StoreUserData) {
  const GURL kScope("http://www.test.not/scope/");
  const GURL kScript("http://www.test.not/script.js");
  const int64_t kRegistrationId = 0;
  const int64_t kVersionId = 0;

  LazyInitialize();

  // Store a registration.
  scoped_refptr<ServiceWorkerRegistration> live_registration =
      new ServiceWorkerRegistration(kScope, kRegistrationId,
                                    context()->AsWeakPtr());
  scoped_refptr<ServiceWorkerVersion> live_version = new ServiceWorkerVersion(
      live_registration.get(), kScript, kVersionId, context()->AsWeakPtr());
  std::vector<ServiceWorkerDatabase::ResourceRecord> records;
  records.push_back(ServiceWorkerDatabase::ResourceRecord(
      1, live_version->script_url(), 100));
  live_version->script_cache_map()->SetResources(records);
  live_version->SetStatus(ServiceWorkerVersion::INSTALLED);
  live_registration->SetWaitingVersion(live_version);
  EXPECT_EQ(SERVICE_WORKER_OK,
            StoreRegistration(live_registration, live_version));

  // Store user data associated with the registration.
  std::vector<std::string> data_out;
  EXPECT_EQ(
      SERVICE_WORKER_OK,
      StoreUserData(kRegistrationId, kScope.GetOrigin(), {{"key", "data"}}));
  EXPECT_EQ(SERVICE_WORKER_OK,
            GetUserData(kRegistrationId, {"key"}, &data_out));
  ASSERT_EQ(1u, data_out.size());
  EXPECT_EQ("data", data_out[0]);
  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            GetUserData(kRegistrationId, {"unknown_key"}, &data_out));
  std::vector<std::pair<int64_t, std::string>> data_list_out;
  EXPECT_EQ(SERVICE_WORKER_OK,
            GetUserDataForAllRegistrations("key", &data_list_out));
  ASSERT_EQ(1u, data_list_out.size());
  EXPECT_EQ(kRegistrationId, data_list_out[0].first);
  EXPECT_EQ("data", data_list_out[0].second);
  data_list_out.clear();
  EXPECT_EQ(SERVICE_WORKER_OK,
            GetUserDataForAllRegistrations("unknown_key", &data_list_out));
  EXPECT_EQ(0u, data_list_out.size());
  EXPECT_EQ(SERVICE_WORKER_OK, ClearUserData(kRegistrationId, {"key"}));
  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            GetUserData(kRegistrationId, {"key"}, &data_out));

  // Write/overwrite multiple user data keys.
  EXPECT_EQ(SERVICE_WORKER_OK,
            StoreUserData(
                kRegistrationId, kScope.GetOrigin(),
                {{"key", "overwrite"}, {"key3", "data3"}, {"key4", "data4"}}));
  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            GetUserData(kRegistrationId, {"key2"}, &data_out));
  EXPECT_TRUE(data_out.empty());
  EXPECT_EQ(SERVICE_WORKER_OK,
            GetUserData(kRegistrationId, {"key", "key3", "key4"}, &data_out));
  ASSERT_EQ(3u, data_out.size());
  EXPECT_EQ("overwrite", data_out[0]);
  EXPECT_EQ("data3", data_out[1]);
  EXPECT_EQ("data4", data_out[2]);
  // Multiple gets fail if one is not found.
  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            GetUserData(kRegistrationId, {"key", "key2"}, &data_out));
  EXPECT_TRUE(data_out.empty());

  // Delete multiple user data keys, even if some are not found.
  EXPECT_EQ(SERVICE_WORKER_OK,
            ClearUserData(kRegistrationId, {"key", "key2", "key3"}));
  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            GetUserData(kRegistrationId, {"key"}, &data_out));
  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            GetUserData(kRegistrationId, {"key2"}, &data_out));
  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            GetUserData(kRegistrationId, {"key3"}, &data_out));
  EXPECT_EQ(SERVICE_WORKER_OK,
            GetUserData(kRegistrationId, {"key4"}, &data_out));
  ASSERT_EQ(1u, data_out.size());
  EXPECT_EQ("data4", data_out[0]);

  // User data should be deleted when the associated registration is deleted.
  ASSERT_EQ(
      SERVICE_WORKER_OK,
      StoreUserData(kRegistrationId, kScope.GetOrigin(), {{"key", "data"}}));
  ASSERT_EQ(SERVICE_WORKER_OK,
            GetUserData(kRegistrationId, {"key"}, &data_out));
  ASSERT_EQ(1u, data_out.size());
  ASSERT_EQ("data", data_out[0]);

  EXPECT_EQ(SERVICE_WORKER_OK,
            DeleteRegistration(kRegistrationId, kScope.GetOrigin()));
  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            GetUserData(kRegistrationId, {"key"}, &data_out));
  data_list_out.clear();
  EXPECT_EQ(SERVICE_WORKER_OK,
            GetUserDataForAllRegistrations("key", &data_list_out));
  EXPECT_EQ(0u, data_list_out.size());

  // Data access with an invalid registration id should be failed.
  EXPECT_EQ(SERVICE_WORKER_ERROR_FAILED,
            StoreUserData(kInvalidServiceWorkerRegistrationId,
                          kScope.GetOrigin(), {{"key", "data"}}));
  EXPECT_EQ(
      SERVICE_WORKER_ERROR_FAILED,
      GetUserData(kInvalidServiceWorkerRegistrationId, {"key"}, &data_out));
  EXPECT_EQ(SERVICE_WORKER_ERROR_FAILED,
            ClearUserData(kInvalidServiceWorkerRegistrationId, {"key"}));

  // Data access with an empty key should be failed.
  EXPECT_EQ(SERVICE_WORKER_ERROR_FAILED,
            StoreUserData(kRegistrationId, kScope.GetOrigin(),
                          std::vector<std::pair<std::string, std::string>>()));
  EXPECT_EQ(SERVICE_WORKER_ERROR_FAILED,
            StoreUserData(kRegistrationId, kScope.GetOrigin(),
                          {{std::string(), "data"}}));
  EXPECT_EQ(SERVICE_WORKER_ERROR_FAILED,
            StoreUserData(kRegistrationId, kScope.GetOrigin(),
                          {{std::string(), "data"}, {"key", "data"}}));
  EXPECT_EQ(
      SERVICE_WORKER_ERROR_FAILED,
      GetUserData(kRegistrationId, std::vector<std::string>(), &data_out));
  EXPECT_EQ(SERVICE_WORKER_ERROR_FAILED,
            GetUserData(kRegistrationId, {std::string()}, &data_out));
  EXPECT_EQ(SERVICE_WORKER_ERROR_FAILED,
            GetUserData(kRegistrationId, {std::string(), "key"}, &data_out));
  EXPECT_EQ(SERVICE_WORKER_ERROR_FAILED,
            ClearUserData(kRegistrationId, std::vector<std::string>()));
  EXPECT_EQ(SERVICE_WORKER_ERROR_FAILED,
            ClearUserData(kRegistrationId, {std::string()}));
  EXPECT_EQ(SERVICE_WORKER_ERROR_FAILED,
            ClearUserData(kRegistrationId, {std::string(), "key"}));
  data_list_out.clear();
  EXPECT_EQ(SERVICE_WORKER_ERROR_FAILED,
            GetUserDataForAllRegistrations(std::string(), &data_list_out));
}

class ServiceWorkerResourceStorageTest : public ServiceWorkerStorageTest {
 public:
  void SetUp() override {
    ServiceWorkerStorageTest::SetUp();
    LazyInitialize();

    scope_ = GURL("http://www.test.not/scope/");
    script_ = GURL("http://www.test.not/script.js");
    import_ = GURL("http://www.test.not/import.js");
    document_url_ = GURL("http://www.test.not/scope/document.html");
    registration_id_ = storage()->NewRegistrationId();
    version_id_ = storage()->NewVersionId();
    resource_id1_ = storage()->NewResourceId();
    resource_id2_ = storage()->NewResourceId();
    resource_id1_size_ = 239193;
    resource_id2_size_ = 59923;

    // Cons up a new registration+version with two script resources.
    RegistrationData data;
    data.registration_id = registration_id_;
    data.scope = scope_;
    data.script = script_;
    data.version_id = version_id_;
    data.is_active = false;
    std::vector<ResourceRecord> resources;
    resources.push_back(
        ResourceRecord(resource_id1_, script_, resource_id1_size_));
    resources.push_back(
        ResourceRecord(resource_id2_, import_, resource_id2_size_));
    registration_ = storage()->GetOrCreateRegistration(data, resources);
    registration_->waiting_version()->SetStatus(ServiceWorkerVersion::NEW);

    // Add the resources ids to the uncommitted list.
    storage()->StoreUncommittedResourceId(resource_id1_);
    storage()->StoreUncommittedResourceId(resource_id2_);
    base::RunLoop().RunUntilIdle();
    std::set<int64_t> verify_ids;
    EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
              storage()->database_->GetUncommittedResourceIds(&verify_ids));
    EXPECT_EQ(2u, verify_ids.size());

    // And dump something in the disk cache for them.
    WriteBasicResponse(storage(), resource_id1_);
    WriteBasicResponse(storage(), resource_id2_);
    EXPECT_TRUE(VerifyBasicResponse(storage(), resource_id1_, true));
    EXPECT_TRUE(VerifyBasicResponse(storage(), resource_id2_, true));

    // Storing the registration/version should take the resources ids out
    // of the uncommitted list.
    EXPECT_EQ(
        SERVICE_WORKER_OK,
        StoreRegistration(registration_, registration_->waiting_version()));
    verify_ids.clear();
    EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
              storage()->database_->GetUncommittedResourceIds(&verify_ids));
    EXPECT_TRUE(verify_ids.empty());
  }

 protected:
  GURL scope_;
  GURL script_;
  GURL import_;
  GURL document_url_;
  int64_t registration_id_;
  int64_t version_id_;
  int64_t resource_id1_;
  uint64_t resource_id1_size_;
  int64_t resource_id2_;
  uint64_t resource_id2_size_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
};

class ServiceWorkerResourceStorageDiskTest
    : public ServiceWorkerResourceStorageTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(InitUserDataDirectory());
    ServiceWorkerResourceStorageTest::SetUp();
  }

};

TEST_F(ServiceWorkerResourceStorageTest,
       WriteMetadataWithServiceWorkerResponseMetadataWriter) {
  const char kMetadata1[] = "Test metadata";
  const char kMetadata2[] = "small";
  int64_t new_resource_id_ = storage()->NewResourceId();
  // Writing metadata to nonexistent resoirce ID must fail.
  EXPECT_GE(0, WriteResponseMetadata(storage(), new_resource_id_, kMetadata1));

  // Check metadata is written.
  EXPECT_EQ(static_cast<int>(strlen(kMetadata1)),
            WriteResponseMetadata(storage(), resource_id1_, kMetadata1));
  EXPECT_TRUE(VerifyResponseMetadata(storage(), resource_id1_, kMetadata1));
  EXPECT_TRUE(VerifyBasicResponse(storage(), resource_id1_, true));

  // Check metadata is written and truncated.
  EXPECT_EQ(static_cast<int>(strlen(kMetadata2)),
            WriteResponseMetadata(storage(), resource_id1_, kMetadata2));
  EXPECT_TRUE(VerifyResponseMetadata(storage(), resource_id1_, kMetadata2));
  EXPECT_TRUE(VerifyBasicResponse(storage(), resource_id1_, true));

  // Check metadata is deleted.
  EXPECT_EQ(0, WriteResponseMetadata(storage(), resource_id1_, ""));
  EXPECT_FALSE(VerifyResponseMetadata(storage(), resource_id1_, ""));
  EXPECT_TRUE(VerifyBasicResponse(storage(), resource_id1_, true));
}

TEST_F(ServiceWorkerResourceStorageTest,
       WriteMetadataWithServiceWorkerScriptCacheMap) {
  const char kMetadata1[] = "Test metadata";
  const char kMetadata2[] = "small";
  ServiceWorkerVersion* version = registration_->waiting_version();
  EXPECT_TRUE(version);

  // Writing metadata to nonexistent URL must fail.
  EXPECT_GE(0,
            WriteMetadata(version, GURL("http://www.test.not/nonexistent.js"),
                          kMetadata1));
  // Clearing metadata of nonexistent URL must fail.
  EXPECT_GE(0,
            ClearMetadata(version, GURL("http://www.test.not/nonexistent.js")));

  // Check metadata is written.
  EXPECT_EQ(static_cast<int>(strlen(kMetadata1)),
            WriteMetadata(version, script_, kMetadata1));
  EXPECT_TRUE(VerifyResponseMetadata(storage(), resource_id1_, kMetadata1));
  EXPECT_TRUE(VerifyBasicResponse(storage(), resource_id1_, true));

  // Check metadata is written and truncated.
  EXPECT_EQ(static_cast<int>(strlen(kMetadata2)),
            WriteMetadata(version, script_, kMetadata2));
  EXPECT_TRUE(VerifyResponseMetadata(storage(), resource_id1_, kMetadata2));
  EXPECT_TRUE(VerifyBasicResponse(storage(), resource_id1_, true));

  // Check metadata is deleted.
  EXPECT_EQ(0, ClearMetadata(version, script_));
  EXPECT_FALSE(VerifyResponseMetadata(storage(), resource_id1_, ""));
  EXPECT_TRUE(VerifyBasicResponse(storage(), resource_id1_, true));
}

TEST_F(ServiceWorkerResourceStorageTest, DeleteRegistration_NoLiveVersion) {
  bool was_called = false;
  ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_FAILED;
  std::set<int64_t> verify_ids;

  registration_->SetWaitingVersion(NULL);
  registration_ = NULL;

  // Deleting the registration should result in the resources being added to the
  // purgeable list and then doomed in the disk cache and removed from that
  // list.
  storage()->DeleteRegistration(
      registration_id_,
      scope_.GetOrigin(),
      base::Bind(&VerifyPurgeableListStatusCallback,
                 base::Unretained(storage()->database_.get()),
                 &verify_ids,
                 &was_called,
                 &result));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(was_called);
  EXPECT_EQ(SERVICE_WORKER_OK, result);
  EXPECT_EQ(2u, verify_ids.size());
  verify_ids.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            storage()->database_->GetPurgeableResourceIds(&verify_ids));
  EXPECT_TRUE(verify_ids.empty());

  EXPECT_FALSE(VerifyBasicResponse(storage(), resource_id1_, false));
  EXPECT_FALSE(VerifyBasicResponse(storage(), resource_id2_, false));
}

TEST_F(ServiceWorkerResourceStorageTest, DeleteRegistration_WaitingVersion) {
  bool was_called = false;
  ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_FAILED;
  std::set<int64_t> verify_ids;

  // Deleting the registration should result in the resources being added to the
  // purgeable list and then doomed in the disk cache and removed from that
  // list.
  storage()->DeleteRegistration(
      registration_->id(),
      scope_.GetOrigin(),
      base::Bind(&VerifyPurgeableListStatusCallback,
                 base::Unretained(storage()->database_.get()),
                 &verify_ids,
                 &was_called,
                 &result));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(was_called);
  EXPECT_EQ(SERVICE_WORKER_OK, result);
  EXPECT_EQ(2u, verify_ids.size());
  verify_ids.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            storage()->database_->GetPurgeableResourceIds(&verify_ids));
  EXPECT_EQ(2u, verify_ids.size());

  EXPECT_TRUE(VerifyBasicResponse(storage(), resource_id1_, false));
  EXPECT_TRUE(VerifyBasicResponse(storage(), resource_id2_, false));

  // Doom the version, now it happens.
  registration_->waiting_version()->Doom();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, result);
  EXPECT_EQ(2u, verify_ids.size());
  verify_ids.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            storage()->database_->GetPurgeableResourceIds(&verify_ids));
  EXPECT_TRUE(verify_ids.empty());

  EXPECT_FALSE(VerifyBasicResponse(storage(), resource_id1_, false));
  EXPECT_FALSE(VerifyBasicResponse(storage(), resource_id2_, false));
}

TEST_F(ServiceWorkerResourceStorageTest, DeleteRegistration_ActiveVersion) {
  // Promote the worker to active and add a controllee.
  registration_->SetActiveVersion(registration_->waiting_version());
  storage()->UpdateToActiveState(
      registration_.get(), base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
  std::unique_ptr<ServiceWorkerProviderHost> host(new ServiceWorkerProviderHost(
      33 /* dummy render process id */, MSG_ROUTING_NONE,
      1 /* dummy provider_id */, SERVICE_WORKER_PROVIDER_FOR_WINDOW,
      ServiceWorkerProviderHost::FrameSecurityLevel::SECURE,
      context()->AsWeakPtr(), NULL));
  registration_->active_version()->AddControllee(host.get());

  bool was_called = false;
  ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_FAILED;
  std::set<int64_t> verify_ids;

  // Deleting the registration should move the resources to the purgeable list
  // but keep them available.
  storage()->DeleteRegistration(
      registration_->id(),
      scope_.GetOrigin(),
      base::Bind(&VerifyPurgeableListStatusCallback,
                 base::Unretained(storage()->database_.get()),
                 &verify_ids,
                 &was_called,
                 &result));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(was_called);
  EXPECT_EQ(SERVICE_WORKER_OK, result);
  EXPECT_EQ(2u, verify_ids.size());
  verify_ids.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            storage()->database_->GetPurgeableResourceIds(&verify_ids));
  EXPECT_EQ(2u, verify_ids.size());

  EXPECT_TRUE(VerifyBasicResponse(storage(), resource_id1_, true));
  EXPECT_TRUE(VerifyBasicResponse(storage(), resource_id2_, true));

  // Removing the controllee should cause the resources to be deleted.
  registration_->active_version()->RemoveControllee(host.get());
  registration_->active_version()->Doom();
  base::RunLoop().RunUntilIdle();
  verify_ids.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            storage()->database_->GetPurgeableResourceIds(&verify_ids));
  EXPECT_TRUE(verify_ids.empty());

  EXPECT_FALSE(VerifyBasicResponse(storage(), resource_id1_, false));
  EXPECT_FALSE(VerifyBasicResponse(storage(), resource_id2_, false));
}

TEST_F(ServiceWorkerResourceStorageDiskTest, CleanupOnRestart) {
  // Promote the worker to active and add a controllee.
  registration_->SetActiveVersion(registration_->waiting_version());
  registration_->SetWaitingVersion(NULL);
  storage()->UpdateToActiveState(
      registration_.get(), base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
  std::unique_ptr<ServiceWorkerProviderHost> host(new ServiceWorkerProviderHost(
      33 /* dummy render process id */, MSG_ROUTING_NONE,
      1 /* dummy provider_id */, SERVICE_WORKER_PROVIDER_FOR_WINDOW,
      ServiceWorkerProviderHost::FrameSecurityLevel::SECURE,
      context()->AsWeakPtr(), NULL));
  registration_->active_version()->AddControllee(host.get());

  bool was_called = false;
  ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_FAILED;
  std::set<int64_t> verify_ids;

  // Deleting the registration should move the resources to the purgeable list
  // but keep them available.
  storage()->DeleteRegistration(
      registration_->id(),
      scope_.GetOrigin(),
      base::Bind(&VerifyPurgeableListStatusCallback,
                 base::Unretained(storage()->database_.get()),
                 &verify_ids,
                 &was_called,
                 &result));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(was_called);
  EXPECT_EQ(SERVICE_WORKER_OK, result);
  EXPECT_EQ(2u, verify_ids.size());
  verify_ids.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            storage()->database_->GetPurgeableResourceIds(&verify_ids));
  EXPECT_EQ(2u, verify_ids.size());

  EXPECT_TRUE(VerifyBasicResponse(storage(), resource_id1_, true));
  EXPECT_TRUE(VerifyBasicResponse(storage(), resource_id2_, true));

  // Also add an uncommitted resource.
  int64_t kStaleUncommittedResourceId = storage()->NewResourceId();
  storage()->StoreUncommittedResourceId(kStaleUncommittedResourceId);
  base::RunLoop().RunUntilIdle();
  verify_ids.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            storage()->database_->GetUncommittedResourceIds(&verify_ids));
  EXPECT_EQ(1u, verify_ids.size());
  WriteBasicResponse(storage(), kStaleUncommittedResourceId);
  EXPECT_TRUE(
      VerifyBasicResponse(storage(), kStaleUncommittedResourceId, true));

  // Simulate browser shutdown. The purgeable and uncommitted resources are now
  // stale.
  InitializeTestHelper();
  LazyInitialize();

  // Store a new uncommitted resource. This triggers stale resource cleanup.
  int64_t kNewResourceId = storage()->NewResourceId();
  WriteBasicResponse(storage(), kNewResourceId);
  storage()->StoreUncommittedResourceId(kNewResourceId);
  base::RunLoop().RunUntilIdle();

  // The stale resources should be purged, but the new resource should persist.
  verify_ids.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            storage()->database_->GetUncommittedResourceIds(&verify_ids));
  ASSERT_EQ(1u, verify_ids.size());
  EXPECT_EQ(kNewResourceId, *verify_ids.begin());

  // Purging resources needs interactions with SimpleCache's worker thread,
  // so single RunUntilIdle() call may not be sufficient.
  while (storage()->is_purge_pending_)
    base::RunLoop().RunUntilIdle();

  verify_ids.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            storage()->database_->GetPurgeableResourceIds(&verify_ids));
  EXPECT_TRUE(verify_ids.empty());
  EXPECT_FALSE(VerifyBasicResponse(storage(), resource_id1_, false));
  EXPECT_FALSE(VerifyBasicResponse(storage(), resource_id2_, false));
  EXPECT_FALSE(
      VerifyBasicResponse(storage(), kStaleUncommittedResourceId, false));
  EXPECT_TRUE(VerifyBasicResponse(storage(), kNewResourceId, true));
}

TEST_F(ServiceWorkerResourceStorageDiskTest, DeleteAndStartOver) {
  EXPECT_FALSE(storage()->IsDisabled());
  ASSERT_TRUE(base::DirectoryExists(storage()->GetDiskCachePath()));
  ASSERT_TRUE(base::DirectoryExists(storage()->GetDatabasePath()));

  base::RunLoop run_loop;
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_MAX_VALUE;
  storage()->DeleteAndStartOver(
      base::Bind(&StatusAndQuitCallback, &status, run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_TRUE(storage()->IsDisabled());
  EXPECT_FALSE(base::DirectoryExists(storage()->GetDiskCachePath()));
  EXPECT_FALSE(base::DirectoryExists(storage()->GetDatabasePath()));
}

TEST_F(ServiceWorkerResourceStorageDiskTest,
       DeleteAndStartOver_UnrelatedFileExists) {
  EXPECT_FALSE(storage()->IsDisabled());
  ASSERT_TRUE(base::DirectoryExists(storage()->GetDiskCachePath()));
  ASSERT_TRUE(base::DirectoryExists(storage()->GetDatabasePath()));

  // Create an unrelated file in the database directory to make sure such a file
  // does not prevent DeleteAndStartOver.
  base::FilePath file_path;
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(storage()->GetDatabasePath(), &file_path));
  ASSERT_TRUE(base::PathExists(file_path));

  base::RunLoop run_loop;
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_MAX_VALUE;
  storage()->DeleteAndStartOver(
      base::Bind(&StatusAndQuitCallback, &status, run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_TRUE(storage()->IsDisabled());
  EXPECT_FALSE(base::DirectoryExists(storage()->GetDiskCachePath()));
  EXPECT_FALSE(base::DirectoryExists(storage()->GetDatabasePath()));
}

TEST_F(ServiceWorkerResourceStorageDiskTest,
       DeleteAndStartOver_OpenedFileExists) {
  EXPECT_FALSE(storage()->IsDisabled());
  ASSERT_TRUE(base::DirectoryExists(storage()->GetDiskCachePath()));
  ASSERT_TRUE(base::DirectoryExists(storage()->GetDatabasePath()));

  // Create an unrelated opened file in the database directory to make sure such
  // a file does not prevent DeleteAndStartOver on non-Windows platforms.
  base::FilePath file_path;
  base::ScopedFILE file(base::CreateAndOpenTemporaryFileInDir(
      storage()->GetDatabasePath(), &file_path));
  ASSERT_TRUE(file);
  ASSERT_TRUE(base::PathExists(file_path));

  base::RunLoop run_loop;
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_MAX_VALUE;
  storage()->DeleteAndStartOver(
      base::Bind(&StatusAndQuitCallback, &status, run_loop.QuitClosure()));
  run_loop.Run();

#if defined(OS_WIN)
  // On Windows, deleting the directory containing an opened file should fail.
  EXPECT_EQ(SERVICE_WORKER_ERROR_FAILED, status);
  EXPECT_TRUE(storage()->IsDisabled());
  EXPECT_TRUE(base::DirectoryExists(storage()->GetDiskCachePath()));
  EXPECT_TRUE(base::DirectoryExists(storage()->GetDatabasePath()));
#else
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_TRUE(storage()->IsDisabled());
  EXPECT_FALSE(base::DirectoryExists(storage()->GetDiskCachePath()));
  EXPECT_FALSE(base::DirectoryExists(storage()->GetDatabasePath()));
#endif
}

TEST_F(ServiceWorkerResourceStorageTest, UpdateRegistration) {
  // Promote the worker to active worker and add a controllee.
  registration_->SetActiveVersion(registration_->waiting_version());
  storage()->UpdateToActiveState(
      registration_.get(), base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
  std::unique_ptr<ServiceWorkerProviderHost> host(new ServiceWorkerProviderHost(
      33 /* dummy render process id */, MSG_ROUTING_NONE,
      1 /* dummy provider_id */, SERVICE_WORKER_PROVIDER_FOR_WINDOW,
      ServiceWorkerProviderHost::FrameSecurityLevel::SECURE,
      context()->AsWeakPtr(), NULL));
  registration_->active_version()->AddControllee(host.get());

  bool was_called = false;
  ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_FAILED;
  std::set<int64_t> verify_ids;

  // Make an updated registration.
  scoped_refptr<ServiceWorkerVersion> live_version = new ServiceWorkerVersion(
      registration_.get(), script_, storage()->NewVersionId(),
      context()->AsWeakPtr());
  live_version->SetStatus(ServiceWorkerVersion::NEW);
  registration_->SetWaitingVersion(live_version);
  std::vector<ServiceWorkerDatabase::ResourceRecord> records;
  records.push_back(ServiceWorkerDatabase::ResourceRecord(
      10, live_version->script_url(), 100));
  live_version->script_cache_map()->SetResources(records);

  // Writing the registration should move the old version's resources to the
  // purgeable list but keep them available.
  storage()->StoreRegistration(
      registration_.get(),
      registration_->waiting_version(),
      base::Bind(&VerifyPurgeableListStatusCallback,
                 base::Unretained(storage()->database_.get()),
                 &verify_ids,
                 &was_called,
                 &result));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(was_called);
  EXPECT_EQ(SERVICE_WORKER_OK, result);
  EXPECT_EQ(2u, verify_ids.size());
  verify_ids.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            storage()->database_->GetPurgeableResourceIds(&verify_ids));
  EXPECT_EQ(2u, verify_ids.size());

  EXPECT_TRUE(VerifyBasicResponse(storage(), resource_id1_, false));
  EXPECT_TRUE(VerifyBasicResponse(storage(), resource_id2_, false));

  // Removing the controllee should cause the old version's resources to be
  // deleted.
  registration_->active_version()->RemoveControllee(host.get());
  registration_->active_version()->Doom();
  base::RunLoop().RunUntilIdle();
  verify_ids.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            storage()->database_->GetPurgeableResourceIds(&verify_ids));
  EXPECT_TRUE(verify_ids.empty());

  EXPECT_FALSE(VerifyBasicResponse(storage(), resource_id1_, false));
  EXPECT_FALSE(VerifyBasicResponse(storage(), resource_id2_, false));
}

TEST_F(ServiceWorkerStorageTest, FindRegistration_LongestScopeMatch) {
  const GURL kDocumentUrl("http://www.example.com/scope/foo");
  scoped_refptr<ServiceWorkerRegistration> found_registration;

  // Registration for "/scope/".
  const GURL kScope1("http://www.example.com/scope/");
  const GURL kScript1("http://www.example.com/script1.js");
  const int64_t kRegistrationId1 = 1;
  const int64_t kVersionId1 = 1;
  scoped_refptr<ServiceWorkerRegistration> live_registration1 =
      new ServiceWorkerRegistration(kScope1, kRegistrationId1,
                                    context()->AsWeakPtr());
  scoped_refptr<ServiceWorkerVersion> live_version1 = new ServiceWorkerVersion(
      live_registration1.get(), kScript1, kVersionId1, context()->AsWeakPtr());
  std::vector<ServiceWorkerDatabase::ResourceRecord> records1;
  records1.push_back(ServiceWorkerDatabase::ResourceRecord(
      1, live_version1->script_url(), 100));
  live_version1->script_cache_map()->SetResources(records1);
  live_version1->SetStatus(ServiceWorkerVersion::INSTALLED);
  live_registration1->SetWaitingVersion(live_version1);

  // Registration for "/scope/foo".
  const GURL kScope2("http://www.example.com/scope/foo");
  const GURL kScript2("http://www.example.com/script2.js");
  const int64_t kRegistrationId2 = 2;
  const int64_t kVersionId2 = 2;
  scoped_refptr<ServiceWorkerRegistration> live_registration2 =
      new ServiceWorkerRegistration(kScope2, kRegistrationId2,
                                    context()->AsWeakPtr());
  scoped_refptr<ServiceWorkerVersion> live_version2 = new ServiceWorkerVersion(
      live_registration2.get(), kScript2, kVersionId2, context()->AsWeakPtr());
  std::vector<ServiceWorkerDatabase::ResourceRecord> records2;
  records2.push_back(ServiceWorkerDatabase::ResourceRecord(
      2, live_version2->script_url(), 100));
  live_version2->script_cache_map()->SetResources(records2);
  live_version2->SetStatus(ServiceWorkerVersion::INSTALLED);
  live_registration2->SetWaitingVersion(live_version2);

  // Registration for "/scope/foobar".
  const GURL kScope3("http://www.example.com/scope/foobar");
  const GURL kScript3("http://www.example.com/script3.js");
  const int64_t kRegistrationId3 = 3;
  const int64_t kVersionId3 = 3;
  scoped_refptr<ServiceWorkerRegistration> live_registration3 =
      new ServiceWorkerRegistration(kScope3, kRegistrationId3,
                                    context()->AsWeakPtr());
  scoped_refptr<ServiceWorkerVersion> live_version3 = new ServiceWorkerVersion(
      live_registration3.get(), kScript3, kVersionId3, context()->AsWeakPtr());
  std::vector<ServiceWorkerDatabase::ResourceRecord> records3;
  records3.push_back(ServiceWorkerDatabase::ResourceRecord(
      3, live_version3->script_url(), 100));
  live_version3->script_cache_map()->SetResources(records3);
  live_version3->SetStatus(ServiceWorkerVersion::INSTALLED);
  live_registration3->SetWaitingVersion(live_version3);

  // Notify storage of they being installed.
  storage()->NotifyInstallingRegistration(live_registration1.get());
  storage()->NotifyInstallingRegistration(live_registration2.get());
  storage()->NotifyInstallingRegistration(live_registration3.get());

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
      live_registration1.get(), NULL, SERVICE_WORKER_OK);
  storage()->NotifyDoneInstallingRegistration(
      live_registration2.get(), NULL, SERVICE_WORKER_OK);
  storage()->NotifyDoneInstallingRegistration(
      live_registration3.get(), NULL, SERVICE_WORKER_OK);

  // Find a registration among installed ones.
  EXPECT_EQ(SERVICE_WORKER_OK,
            FindRegistrationForDocument(kDocumentUrl, &found_registration));
  EXPECT_EQ(live_registration2, found_registration);
}

class ServiceWorkerStorageDiskTest : public ServiceWorkerStorageTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(InitUserDataDirectory());
    ServiceWorkerStorageTest::SetUp();
  }
};

TEST_F(ServiceWorkerStorageDiskTest, OriginHasForeignFetchRegistrations) {
  LazyInitialize();

  // Registration 1 for http://www.example.com
  const GURL kScope1("http://www.example.com/scope/");
  const GURL kScript1("http://www.example.com/script1.js");
  const int64_t kRegistrationId1 = 1;
  const int64_t kVersionId1 = 1;
  scoped_refptr<ServiceWorkerRegistration> live_registration1 =
      new ServiceWorkerRegistration(kScope1, kRegistrationId1,
                                    context()->AsWeakPtr());
  scoped_refptr<ServiceWorkerVersion> live_version1 = new ServiceWorkerVersion(
      live_registration1.get(), kScript1, kVersionId1, context()->AsWeakPtr());
  std::vector<ServiceWorkerDatabase::ResourceRecord> records1;
  records1.push_back(ServiceWorkerDatabase::ResourceRecord(
      1, live_version1->script_url(), 100));
  live_version1->script_cache_map()->SetResources(records1);
  live_version1->SetStatus(ServiceWorkerVersion::INSTALLED);
  live_version1->set_foreign_fetch_scopes(std::vector<GURL>(1, kScope1));
  live_registration1->SetWaitingVersion(live_version1);

  // Registration 2 for http://www.example.com
  const GURL kScope2("http://www.example.com/scope/foo");
  const GURL kScript2("http://www.example.com/script2.js");
  const int64_t kRegistrationId2 = 2;
  const int64_t kVersionId2 = 2;
  scoped_refptr<ServiceWorkerRegistration> live_registration2 =
      new ServiceWorkerRegistration(kScope2, kRegistrationId2,
                                    context()->AsWeakPtr());
  scoped_refptr<ServiceWorkerVersion> live_version2 = new ServiceWorkerVersion(
      live_registration2.get(), kScript2, kVersionId2, context()->AsWeakPtr());
  std::vector<ServiceWorkerDatabase::ResourceRecord> records2;
  records2.push_back(ServiceWorkerDatabase::ResourceRecord(
      2, live_version2->script_url(), 100));
  live_version2->script_cache_map()->SetResources(records2);
  live_version2->SetStatus(ServiceWorkerVersion::INSTALLED);
  live_version2->set_foreign_fetch_scopes(std::vector<GURL>(1, kScope2));
  live_registration2->SetWaitingVersion(live_version2);

  // Registration for http://www.test.com
  const GURL kScope3("http://www.test.com/scope/foobar");
  const GURL kScript3("http://www.test.com/script3.js");
  const int64_t kRegistrationId3 = 3;
  const int64_t kVersionId3 = 3;
  scoped_refptr<ServiceWorkerRegistration> live_registration3 =
      new ServiceWorkerRegistration(kScope3, kRegistrationId3,
                                    context()->AsWeakPtr());
  scoped_refptr<ServiceWorkerVersion> live_version3 = new ServiceWorkerVersion(
      live_registration3.get(), kScript3, kVersionId3, context()->AsWeakPtr());
  std::vector<ServiceWorkerDatabase::ResourceRecord> records3;
  records3.push_back(ServiceWorkerDatabase::ResourceRecord(
      3, live_version3->script_url(), 100));
  live_version3->script_cache_map()->SetResources(records3);
  live_version3->SetStatus(ServiceWorkerVersion::INSTALLED);
  live_registration3->SetWaitingVersion(live_version3);

  // Neither origin should have registrations before they are stored.
  const GURL kOrigin1 = kScope1.GetOrigin();
  const GURL kOrigin2 = kScope3.GetOrigin();
  EXPECT_FALSE(storage()->OriginHasForeignFetchRegistrations(kOrigin1));
  EXPECT_FALSE(storage()->OriginHasForeignFetchRegistrations(kOrigin2));

  // Store all registrations.
  EXPECT_EQ(SERVICE_WORKER_OK,
            StoreRegistration(live_registration1, live_version1));
  EXPECT_EQ(SERVICE_WORKER_OK,
            StoreRegistration(live_registration2, live_version2));
  EXPECT_EQ(SERVICE_WORKER_OK,
            StoreRegistration(live_registration3, live_version3));

  // Now first origin should have foreign fetch registrations, second doesn't.
  EXPECT_TRUE(storage()->OriginHasForeignFetchRegistrations(kOrigin1));
  EXPECT_FALSE(storage()->OriginHasForeignFetchRegistrations(kOrigin2));

  // Remove one registration at first origin.
  EXPECT_EQ(SERVICE_WORKER_OK,
            DeleteRegistration(kRegistrationId1, kScope1.GetOrigin()));

  // First origin should still have a registration left.
  EXPECT_TRUE(storage()->OriginHasForeignFetchRegistrations(kOrigin1));
  EXPECT_FALSE(storage()->OriginHasForeignFetchRegistrations(kOrigin2));

  // Simulate browser shutdown and restart.
  live_registration1 = nullptr;
  live_version1 = nullptr;
  live_registration2 = nullptr;
  live_version2 = nullptr;
  live_registration3 = nullptr;
  live_version3 = nullptr;
  InitializeTestHelper();
  LazyInitialize();

  // First origin should still have a registration left.
  EXPECT_TRUE(storage()->OriginHasForeignFetchRegistrations(kOrigin1));
  EXPECT_FALSE(storage()->OriginHasForeignFetchRegistrations(kOrigin2));

  // Remove other registration at first origin.
  EXPECT_EQ(SERVICE_WORKER_OK,
            DeleteRegistration(kRegistrationId2, kScope2.GetOrigin()));

  // No foreign fetch registrations remain.
  EXPECT_FALSE(storage()->OriginHasForeignFetchRegistrations(kOrigin1));
  EXPECT_FALSE(storage()->OriginHasForeignFetchRegistrations(kOrigin2));
}

}  // namespace content
