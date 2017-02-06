// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/notification_database.h"

#include <stddef.h>
#include <stdint.h>

#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/notification_database_data.h"
#include "content/public/common/platform_notification_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"
#include "url/gurl.h"

namespace content {

const int kExampleServiceWorkerRegistrationId = 42;

const struct {
  const char* origin;
  int64_t service_worker_registration_id;
} kExampleNotificationData[] = {
    {"https://example.com", 0},
    {"https://example.com", kExampleServiceWorkerRegistrationId},
    {"https://example.com", kExampleServiceWorkerRegistrationId},
    {"https://example.com", kExampleServiceWorkerRegistrationId + 1},
    {"https://chrome.com", 0},
    {"https://chrome.com", 0},
    {"https://chrome.com", kExampleServiceWorkerRegistrationId}};

class NotificationDatabaseTest : public ::testing::Test {
 protected:
  // Creates a new NotificationDatabase instance in memory.
  NotificationDatabase* CreateDatabaseInMemory() {
    return new NotificationDatabase(base::FilePath());
  }

  // Creates a new NotificationDatabase instance in |path|.
  NotificationDatabase* CreateDatabaseOnFileSystem(const base::FilePath& path) {
    return new NotificationDatabase(path);
  }

  // Creates a new notification for |service_worker_registration_id| belonging
  // to |origin| and writes it to the database. The written notification id
  // will be stored in |notification_id|.
  void CreateAndWriteNotification(NotificationDatabase* database,
                                  const GURL& origin,
                                  int64_t service_worker_registration_id,
                                  int64_t* notification_id) {
    NotificationDatabaseData database_data;
    database_data.origin = origin;
    database_data.service_worker_registration_id =
        service_worker_registration_id;

    ASSERT_EQ(NotificationDatabase::STATUS_OK,
              database->WriteNotificationData(origin, database_data,
                                              notification_id));
  }

  // Populates |database| with a series of example notifications that differ in
  // their origin and Service Worker registration id.
  void PopulateDatabaseWithExampleData(NotificationDatabase* database) {
    int64_t notification_id;
    for (size_t i = 0; i < arraysize(kExampleNotificationData); ++i) {
      ASSERT_NO_FATAL_FAILURE(CreateAndWriteNotification(
          database, GURL(kExampleNotificationData[i].origin),
          kExampleNotificationData[i].service_worker_registration_id,
          &notification_id));
    }
  }

  // Returns if |database| has been opened.
  bool IsDatabaseOpen(NotificationDatabase* database) {
    return database->IsOpen();
  }

  // Returns if |database| is an in-memory only database.
  bool IsInMemoryDatabase(NotificationDatabase* database) {
    return database->IsInMemoryDatabase();
  }

  // Writes a LevelDB key-value pair directly to the LevelDB backing the
  // notification database in |database|.
  void WriteLevelDBKeyValuePair(NotificationDatabase* database,
                                const std::string& key,
                                const std::string& value) {
    leveldb::Status status =
        database->GetDBForTesting()->Put(leveldb::WriteOptions(), key, value);
    ASSERT_TRUE(status.ok());
  }
};

TEST_F(NotificationDatabaseTest, OpenCloseMemory) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());

  // Should return false because the database does not exist in memory.
  EXPECT_EQ(NotificationDatabase::STATUS_ERROR_NOT_FOUND,
            database->Open(false /* create_if_missing */));

  // Should return true, indicating that the database could be created.
  EXPECT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  EXPECT_TRUE(IsDatabaseOpen(database.get()));
  EXPECT_TRUE(IsInMemoryDatabase(database.get()));

  // Verify that in-memory databases do not persist when being re-created.
  database.reset(CreateDatabaseInMemory());

  EXPECT_EQ(NotificationDatabase::STATUS_ERROR_NOT_FOUND,
            database->Open(false /* create_if_missing */));
}

TEST_F(NotificationDatabaseTest, OpenCloseFileSystem) {
  base::ScopedTempDir database_dir;
  ASSERT_TRUE(database_dir.CreateUniqueTempDir());

  std::unique_ptr<NotificationDatabase> database(
      CreateDatabaseOnFileSystem(database_dir.path()));

  // Should return false because the database does not exist on the file system.
  EXPECT_EQ(NotificationDatabase::STATUS_ERROR_NOT_FOUND,
            database->Open(false /* create_if_missing */));

  // Should return true, indicating that the database could be created.
  EXPECT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  EXPECT_TRUE(IsDatabaseOpen(database.get()));
  EXPECT_FALSE(IsInMemoryDatabase(database.get()));

  // Close the database, and re-open it without attempting to create it because
  // the files on the file system should still exist as expected.
  database.reset(CreateDatabaseOnFileSystem(database_dir.path()));
  EXPECT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(false /* create_if_missing */));
}

TEST_F(NotificationDatabaseTest, DestroyDatabase) {
  base::ScopedTempDir database_dir;
  ASSERT_TRUE(database_dir.CreateUniqueTempDir());

  std::unique_ptr<NotificationDatabase> database(
      CreateDatabaseOnFileSystem(database_dir.path()));

  EXPECT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));
  EXPECT_TRUE(IsDatabaseOpen(database.get()));

  // Destroy the database. This will immediately close it as well.
  ASSERT_EQ(NotificationDatabase::STATUS_OK, database->Destroy());
  EXPECT_FALSE(IsDatabaseOpen(database.get()));

  // Try to re-open the database (but not re-create it). This should fail as
  // the files associated with the database should have been blown away.
  database.reset(CreateDatabaseOnFileSystem(database_dir.path()));
  EXPECT_EQ(NotificationDatabase::STATUS_ERROR_NOT_FOUND,
            database->Open(false /* create_if_missing */));
}

TEST_F(NotificationDatabaseTest, NotificationIdIncrements) {
  base::ScopedTempDir database_dir;
  ASSERT_TRUE(database_dir.CreateUniqueTempDir());

  std::unique_ptr<NotificationDatabase> database(
      CreateDatabaseOnFileSystem(database_dir.path()));

  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  GURL origin("https://example.com");

  int64_t notification_id = 0;

  // Verify that getting two ids on the same database instance results in
  // incrementing values. Notification ids will start at 1.
  ASSERT_NO_FATAL_FAILURE(CreateAndWriteNotification(
      database.get(), origin, 0 /* sw_registration_id */, &notification_id));
  EXPECT_EQ(notification_id, 1);

  ASSERT_NO_FATAL_FAILURE(CreateAndWriteNotification(
      database.get(), origin, 0 /* sw_registration_id */, &notification_id));
  EXPECT_EQ(notification_id, 2);

  database.reset(CreateDatabaseOnFileSystem(database_dir.path()));
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(false /* create_if_missing */));

  // Verify that the next notification id was stored in the database, and
  // continues where we expect it to be, even after closing and opening it.
  ASSERT_NO_FATAL_FAILURE(CreateAndWriteNotification(
      database.get(), origin, 0 /* sw_registration_id */, &notification_id));
  EXPECT_EQ(notification_id, 3);
}

TEST_F(NotificationDatabaseTest, NotificationIdIncrementsStorage) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  GURL origin("https://example.com");

  NotificationDatabaseData database_data;
  database_data.notification_id = -1;

  int64_t notification_id = 0;
  ASSERT_EQ(
      NotificationDatabase::STATUS_OK,
      database->WriteNotificationData(origin, database_data, &notification_id));

  ASSERT_EQ(
      NotificationDatabase::STATUS_OK,
      database->ReadNotificationData(notification_id, origin, &database_data));

  EXPECT_EQ(notification_id, database_data.notification_id);
}

TEST_F(NotificationDatabaseTest, NotificationIdCorruption) {
  base::ScopedTempDir database_dir;
  ASSERT_TRUE(database_dir.CreateUniqueTempDir());

  std::unique_ptr<NotificationDatabase> database(
      CreateDatabaseOnFileSystem(database_dir.path()));

  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  GURL origin("https://example.com");

  NotificationDatabaseData database_data;
  int64_t notification_id = 0;

  ASSERT_EQ(
      NotificationDatabase::STATUS_OK,
      database->WriteNotificationData(origin, database_data, &notification_id));
  EXPECT_EQ(notification_id, 1);

  // Deliberately write an invalid value as the next notification id. When
  // re-opening the database, the Open() method should realize that an invalid
  // value is being read, and mark the database as corrupted.
  ASSERT_NO_FATAL_FAILURE(
      WriteLevelDBKeyValuePair(database.get(), "NEXT_NOTIFICATION_ID", "-42"));

  database.reset(CreateDatabaseOnFileSystem(database_dir.path()));
  EXPECT_EQ(NotificationDatabase::STATUS_ERROR_CORRUPTED,
            database->Open(false /* create_if_missing */));
}

TEST_F(NotificationDatabaseTest, ReadInvalidNotificationData) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  NotificationDatabaseData database_data;

  // Reading the notification data for a notification that does not exist should
  // return the ERROR_NOT_FOUND status code.
  EXPECT_EQ(NotificationDatabase::STATUS_ERROR_NOT_FOUND,
            database->ReadNotificationData(9001, GURL("https://chrome.com"),
                                           &database_data));
}

TEST_F(NotificationDatabaseTest, ReadNotificationDataDifferentOrigin) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  int64_t notification_id = 0;
  GURL origin("https://example.com");

  NotificationDatabaseData database_data, read_database_data;
  database_data.notification_data.title = base::UTF8ToUTF16("My Notification");

  ASSERT_EQ(
      NotificationDatabase::STATUS_OK,
      database->WriteNotificationData(origin, database_data, &notification_id));

  // Reading the notification from the database when given a different origin
  // should return the ERROR_NOT_FOUND status code.
  EXPECT_EQ(
      NotificationDatabase::STATUS_ERROR_NOT_FOUND,
      database->ReadNotificationData(
          notification_id, GURL("https://chrome.com"), &read_database_data));

  // However, reading the notification from the database with the same origin
  // should return STATUS_OK and the associated notification data.
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->ReadNotificationData(notification_id, origin,
                                           &read_database_data));

  EXPECT_EQ(database_data.notification_data.title,
            read_database_data.notification_data.title);
}

TEST_F(NotificationDatabaseTest, ReadNotificationDataReflection) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  int64_t notification_id = 0;

  GURL origin("https://example.com");

  PlatformNotificationData notification_data;
  notification_data.title = base::UTF8ToUTF16("My Notification");
  notification_data.direction =
      PlatformNotificationData::DIRECTION_RIGHT_TO_LEFT;
  notification_data.lang = "nl-NL";
  notification_data.body = base::UTF8ToUTF16("Hello, world!");
  notification_data.tag = "replace id";
  notification_data.icon = GURL("https://example.com/icon.png");
  notification_data.silent = true;

  NotificationDatabaseData database_data;
  database_data.notification_id = notification_id;
  database_data.origin = origin;
  database_data.service_worker_registration_id = 42;
  database_data.notification_data = notification_data;

  // Write the constructed notification to the database, and then immediately
  // read it back from the database again as well.
  ASSERT_EQ(
      NotificationDatabase::STATUS_OK,
      database->WriteNotificationData(origin, database_data, &notification_id));

  NotificationDatabaseData read_database_data;
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->ReadNotificationData(notification_id, origin,
                                           &read_database_data));

  // Verify that all members retrieved from the database are exactly the same
  // as the ones that were written to it. This tests the serialization behavior.

  EXPECT_EQ(notification_id, read_database_data.notification_id);

  EXPECT_EQ(database_data.origin, read_database_data.origin);
  EXPECT_EQ(database_data.service_worker_registration_id,
            read_database_data.service_worker_registration_id);

  const PlatformNotificationData& read_notification_data =
      read_database_data.notification_data;

  EXPECT_EQ(notification_data.title, read_notification_data.title);
  EXPECT_EQ(notification_data.direction, read_notification_data.direction);
  EXPECT_EQ(notification_data.lang, read_notification_data.lang);
  EXPECT_EQ(notification_data.body, read_notification_data.body);
  EXPECT_EQ(notification_data.tag, read_notification_data.tag);
  EXPECT_EQ(notification_data.icon, read_notification_data.icon);
  EXPECT_EQ(notification_data.silent, read_notification_data.silent);
}

TEST_F(NotificationDatabaseTest, ReadWriteMultipleNotificationData) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  GURL origin("https://example.com");
  int64_t notification_id = 0;

  // Write ten notifications to the database, each with a unique title and
  // notification id (it is the responsibility of the user to increment this).
  for (int i = 1; i <= 10; ++i) {
    ASSERT_NO_FATAL_FAILURE(CreateAndWriteNotification(
        database.get(), origin, i /* sw_registration_id */, &notification_id));
    EXPECT_EQ(notification_id, i);
  }

  NotificationDatabaseData database_data;

  // Read the ten notifications from the database, and verify that the titles
  // of each of them matches with how they were created.
  for (int i = 1; i <= 10; ++i) {
    ASSERT_EQ(NotificationDatabase::STATUS_OK,
              database->ReadNotificationData(i /* notification_id */, origin,
                                             &database_data));

    EXPECT_EQ(i, database_data.service_worker_registration_id);
  }
}

TEST_F(NotificationDatabaseTest, DeleteInvalidNotificationData) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  // Deleting non-existing notifications is not considered to be a failure.
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->DeleteNotificationData(9001, GURL("https://chrome.com")));
}

TEST_F(NotificationDatabaseTest, DeleteNotificationDataSameOrigin) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  int64_t notification_id = 0;

  NotificationDatabaseData database_data;
  GURL origin("https://example.com");

  ASSERT_EQ(
      NotificationDatabase::STATUS_OK,
      database->WriteNotificationData(origin, database_data, &notification_id));

  // Reading a notification after writing one should succeed.
  EXPECT_EQ(
      NotificationDatabase::STATUS_OK,
      database->ReadNotificationData(notification_id, origin, &database_data));

  // Delete the notification which was just written to the database, and verify
  // that reading it again will fail.
  EXPECT_EQ(NotificationDatabase::STATUS_OK,
            database->DeleteNotificationData(notification_id, origin));
  EXPECT_EQ(
      NotificationDatabase::STATUS_ERROR_NOT_FOUND,
      database->ReadNotificationData(notification_id, origin, &database_data));
}

TEST_F(NotificationDatabaseTest, DeleteNotificationDataDifferentOrigin) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  int64_t notification_id = 0;

  NotificationDatabaseData database_data;
  GURL origin("https://example.com");

  ASSERT_EQ(
      NotificationDatabase::STATUS_OK,
      database->WriteNotificationData(origin, database_data, &notification_id));

  // Attempting to delete the notification with a different origin, but with the
  // same |notification_id|, should not return an error (the notification could
  // not be found, but that's not considered a failure). However, it should not
  // remove the notification either.
  EXPECT_EQ(NotificationDatabase::STATUS_OK,
            database->DeleteNotificationData(notification_id,
                                             GURL("https://chrome.com")));

  EXPECT_EQ(
      NotificationDatabase::STATUS_OK,
      database->ReadNotificationData(notification_id, origin, &database_data));
}

TEST_F(NotificationDatabaseTest, ReadAllNotificationData) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  ASSERT_NO_FATAL_FAILURE(PopulateDatabaseWithExampleData(database.get()));

  std::vector<NotificationDatabaseData> notifications;
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->ReadAllNotificationData(&notifications));

  EXPECT_EQ(arraysize(kExampleNotificationData), notifications.size());
}

TEST_F(NotificationDatabaseTest, ReadAllNotificationDataEmpty) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  std::vector<NotificationDatabaseData> notifications;
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->ReadAllNotificationData(&notifications));

  EXPECT_EQ(0u, notifications.size());
}

TEST_F(NotificationDatabaseTest, ReadAllNotificationDataForOrigin) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  ASSERT_NO_FATAL_FAILURE(PopulateDatabaseWithExampleData(database.get()));

  GURL origin("https://example.com");

  std::vector<NotificationDatabaseData> notifications;
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->ReadAllNotificationDataForOrigin(origin, &notifications));

  EXPECT_EQ(4u, notifications.size());
}

TEST_F(NotificationDatabaseTest,
       ReadAllNotificationDataForServiceWorkerRegistration) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  ASSERT_NO_FATAL_FAILURE(PopulateDatabaseWithExampleData(database.get()));

  GURL origin("https://example.com:443");

  std::vector<NotificationDatabaseData> notifications;
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->ReadAllNotificationDataForServiceWorkerRegistration(
                origin, kExampleServiceWorkerRegistrationId, &notifications));

  EXPECT_EQ(2u, notifications.size());
}

TEST_F(NotificationDatabaseTest, DeleteAllNotificationDataForOrigin) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  ASSERT_NO_FATAL_FAILURE(PopulateDatabaseWithExampleData(database.get()));

  GURL origin("https://example.com:443");

  std::set<int64_t> deleted_notification_set;
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->DeleteAllNotificationDataForOrigin(
                origin, &deleted_notification_set));

  EXPECT_EQ(4u, deleted_notification_set.size());

  std::vector<NotificationDatabaseData> notifications;
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->ReadAllNotificationDataForOrigin(origin, &notifications));

  EXPECT_EQ(0u, notifications.size());
}

TEST_F(NotificationDatabaseTest, DeleteAllNotificationDataForOriginEmpty) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  GURL origin("https://example.com");

  std::set<int64_t> deleted_notification_set;
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->DeleteAllNotificationDataForOrigin(
                origin, &deleted_notification_set));

  EXPECT_EQ(0u, deleted_notification_set.size());
}

TEST_F(NotificationDatabaseTest,
       DeleteAllNotificationDataForServiceWorkerRegistration) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  ASSERT_NO_FATAL_FAILURE(PopulateDatabaseWithExampleData(database.get()));

  GURL origin("https://example.com:443");
  std::set<int64_t> deleted_notification_set;
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->DeleteAllNotificationDataForServiceWorkerRegistration(
                origin, kExampleServiceWorkerRegistrationId,
                &deleted_notification_set));

  EXPECT_EQ(2u, deleted_notification_set.size());

  std::vector<NotificationDatabaseData> notifications;
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->ReadAllNotificationDataForServiceWorkerRegistration(
                origin, kExampleServiceWorkerRegistrationId, &notifications));

  EXPECT_EQ(0u, notifications.size());
}

}  // namespace content
