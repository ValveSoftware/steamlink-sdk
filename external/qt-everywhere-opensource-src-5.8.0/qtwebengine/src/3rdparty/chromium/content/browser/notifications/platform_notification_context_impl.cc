// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/platform_notification_context_impl.h"

#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/browser/notifications/blink_notification_service_impl.h"
#include "content/browser/notifications/notification_database.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/notification_database_data.h"
#include "content/public/browser/platform_notification_service.h"

using base::DoNothing;

namespace content {

// Name of the directory in the user's profile directory where the notification
// database files should be stored.
const base::FilePath::CharType kPlatformNotificationsDirectory[] =
    FILE_PATH_LITERAL("Platform Notifications");

PlatformNotificationContextImpl::PlatformNotificationContextImpl(
    const base::FilePath& path,
    BrowserContext* browser_context,
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context)
    : path_(path),
      browser_context_(browser_context),
      service_worker_context_(service_worker_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

PlatformNotificationContextImpl::~PlatformNotificationContextImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If the database has been initialized, it must be deleted on the task runner
  // thread as closing it may cause file I/O.
  if (database_) {
    DCHECK(task_runner_);
    task_runner_->DeleteSoon(FROM_HERE, database_.release());
  }
}

void PlatformNotificationContextImpl::Initialize() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PlatformNotificationService* service =
      GetContentClient()->browser()->GetPlatformNotificationService();
  if (service) {
    std::set<std::string> displayed_notifications;

    bool notification_synchronization_supported =
        service->GetDisplayedPersistentNotifications(browser_context_,
                                                     &displayed_notifications);

    // Synchronize the notifications stored in the database with the set of
    // displaying notifications in |displayed_notifications|. This is necessary
    // because flakiness may cause a platform to inform Chrome of a notification
    // that has since been closed, or because the platform does not support
    // notifications that exceed the lifetime of the browser process.

    // TODO(peter): Synchronizing the actual notifications will be done when the
    // persistent notification ids are stable. For M44 we need to support the
    // case where there may be no notifications after a Chrome restart.
    if (notification_synchronization_supported &&
        !displayed_notifications.size()) {
      prune_database_on_open_ = true;
    }
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PlatformNotificationContextImpl::InitializeOnIO, this));
}

void PlatformNotificationContextImpl::InitializeOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // |service_worker_context_| may be NULL in tests.
  if (service_worker_context_)
    service_worker_context_->AddObserver(this);
}

void PlatformNotificationContextImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PlatformNotificationContextImpl::ShutdownOnIO, this));
}

void PlatformNotificationContextImpl::ShutdownOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  services_.clear();

  // |service_worker_context_| may be NULL in tests.
  if (service_worker_context_)
    service_worker_context_->RemoveObserver(this);
}

void PlatformNotificationContextImpl::CreateService(
    int render_process_id,
    mojo::InterfaceRequest<blink::mojom::NotificationService> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PlatformNotificationContextImpl::CreateServiceOnIO, this,
                 render_process_id, browser_context_->GetResourceContext(),
                 base::Passed(&request)));
}

void PlatformNotificationContextImpl::CreateServiceOnIO(
    int render_process_id,
    ResourceContext* resource_context,
    mojo::InterfaceRequest<blink::mojom::NotificationService> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  services_.push_back(base::WrapUnique(new BlinkNotificationServiceImpl(
      this, resource_context, render_process_id, std::move(request))));
}

void PlatformNotificationContextImpl::RemoveService(
    BlinkNotificationServiceImpl* service) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto services_to_remove = std::remove_if(
      services_.begin(), services_.end(),
      [service](const std::unique_ptr<BlinkNotificationServiceImpl>& ptr) {
        return ptr.get() == service;
      });

  services_.erase(services_to_remove, services_.end());
}

void PlatformNotificationContextImpl::ReadNotificationData(
    int64_t notification_id,
    const GURL& origin,
    const ReadResultCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  LazyInitialize(
      base::Bind(&PlatformNotificationContextImpl::DoReadNotificationData, this,
                 notification_id, origin, callback),
      base::Bind(callback, false /* success */, NotificationDatabaseData()));
}

void PlatformNotificationContextImpl::DoReadNotificationData(
    int64_t notification_id,
    const GURL& origin,
    const ReadResultCallback& callback) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  NotificationDatabaseData database_data;
  NotificationDatabase::Status status =
      database_->ReadNotificationData(notification_id, origin, &database_data);

  UMA_HISTOGRAM_ENUMERATION("Notifications.Database.ReadResult", status,
                            NotificationDatabase::STATUS_COUNT);

  if (status == NotificationDatabase::STATUS_OK) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(callback, true /* success */, database_data));
    return;
  }

  // Blow away the database if reading data failed due to corruption.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(callback, false /* success */, NotificationDatabaseData()));
}

void PlatformNotificationContextImpl::
    ReadAllNotificationDataForServiceWorkerRegistration(
        const GURL& origin,
        int64_t service_worker_registration_id,
        const ReadAllResultCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  LazyInitialize(
      base::Bind(&PlatformNotificationContextImpl::
                     DoReadAllNotificationDataForServiceWorkerRegistration,
                 this, origin, service_worker_registration_id, callback),
      base::Bind(callback, false /* success */,
                 std::vector<NotificationDatabaseData>()));
}

void PlatformNotificationContextImpl::
    DoReadAllNotificationDataForServiceWorkerRegistration(
        const GURL& origin,
        int64_t service_worker_registration_id,
        const ReadAllResultCallback& callback) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  std::vector<NotificationDatabaseData> notification_datas;

  NotificationDatabase::Status status =
      database_->ReadAllNotificationDataForServiceWorkerRegistration(
          origin, service_worker_registration_id, &notification_datas);

  UMA_HISTOGRAM_ENUMERATION("Notifications.Database.ReadForServiceWorkerResult",
                            status, NotificationDatabase::STATUS_COUNT);

  if (status == NotificationDatabase::STATUS_OK) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(callback, true /* success */, notification_datas));
    return;
  }

  // Blow away the database if reading data failed due to corruption.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(callback, false /* success */,
                                     std::vector<NotificationDatabaseData>()));
}

void PlatformNotificationContextImpl::WriteNotificationData(
    const GURL& origin,
    const NotificationDatabaseData& database_data,
    const WriteResultCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  LazyInitialize(
      base::Bind(&PlatformNotificationContextImpl::DoWriteNotificationData,
                 this, origin, database_data, callback),
      base::Bind(callback, false /* success */, 0 /* notification_id */));
}

void PlatformNotificationContextImpl::DoWriteNotificationData(
    const GURL& origin,
    const NotificationDatabaseData& database_data,
    const WriteResultCallback& callback) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  int64_t notification_id = 0;
  NotificationDatabase::Status status =
      database_->WriteNotificationData(origin, database_data, &notification_id);

  UMA_HISTOGRAM_ENUMERATION("Notifications.Database.WriteResult", status,
                            NotificationDatabase::STATUS_COUNT);

  if (status == NotificationDatabase::STATUS_OK) {
    DCHECK_GT(notification_id, 0);
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(callback, true /* success */, notification_id));
    return;
  }

  // Blow away the database if writing data failed due to corruption.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(callback, false /* success */, 0 /* notification_id */));
}

void PlatformNotificationContextImpl::DeleteNotificationData(
    int64_t notification_id,
    const GURL& origin,
    const DeleteResultCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  LazyInitialize(
      base::Bind(&PlatformNotificationContextImpl::DoDeleteNotificationData,
                 this, notification_id, origin, callback),
      base::Bind(callback, false /* success */));
}

void PlatformNotificationContextImpl::DoDeleteNotificationData(
    int64_t notification_id,
    const GURL& origin,
    const DeleteResultCallback& callback) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  NotificationDatabase::Status status =
      database_->DeleteNotificationData(notification_id, origin);

  UMA_HISTOGRAM_ENUMERATION("Notifications.Database.DeleteResult", status,
                            NotificationDatabase::STATUS_COUNT);

  bool success = status == NotificationDatabase::STATUS_OK;

  // Blow away the database if deleting data failed due to corruption. Following
  // the contract of the delete methods, consider this to be a success as the
  // caller's goal has been achieved: the data is gone.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED) {
    DestroyDatabase();
    success = true;
  }

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(callback, success));
}

void PlatformNotificationContextImpl::OnRegistrationDeleted(
    int64_t registration_id,
    const GURL& pattern) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  LazyInitialize(
      base::Bind(&PlatformNotificationContextImpl::
                     DoDeleteNotificationsForServiceWorkerRegistration,
                 this, pattern.GetOrigin(), registration_id),
      base::Bind(&DoNothing));
}

void PlatformNotificationContextImpl::
    DoDeleteNotificationsForServiceWorkerRegistration(
        const GURL& origin,
        int64_t service_worker_registration_id) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  std::set<int64_t> deleted_notifications_set;
  NotificationDatabase::Status status =
      database_->DeleteAllNotificationDataForServiceWorkerRegistration(
          origin, service_worker_registration_id, &deleted_notifications_set);

  UMA_HISTOGRAM_ENUMERATION(
      "Notifications.Database.DeleteServiceWorkerRegistrationResult", status,
      NotificationDatabase::STATUS_COUNT);

  // Blow away the database if a corruption error occurred during the deletion.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  // TODO(peter): Close the notifications in |deleted_notifications_set|. See
  // https://crbug.com/532436.
}

void PlatformNotificationContextImpl::OnStorageWiped() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  LazyInitialize(
      base::Bind(
          base::IgnoreResult(&PlatformNotificationContextImpl::DestroyDatabase),
          this),
      base::Bind(&DoNothing));
}

void PlatformNotificationContextImpl::LazyInitialize(
    const base::Closure& success_closure,
    const base::Closure& failure_closure) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!task_runner_) {
    base::SequencedWorkerPool* pool = BrowserThread::GetBlockingPool();
    base::SequencedWorkerPool::SequenceToken token = pool->GetSequenceToken();

    task_runner_ = pool->GetSequencedTaskRunner(token);
  }

  task_runner_->PostTask(
      FROM_HERE, base::Bind(&PlatformNotificationContextImpl::OpenDatabase,
                            this, success_closure, failure_closure));
}

void PlatformNotificationContextImpl::OpenDatabase(
    const base::Closure& success_closure,
    const base::Closure& failure_closure) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  if (database_) {
    success_closure.Run();
    return;
  }

  database_.reset(new NotificationDatabase(GetDatabasePath()));
  NotificationDatabase::Status status =
      database_->Open(true /* create_if_missing */);

  UMA_HISTOGRAM_ENUMERATION("Notifications.Database.OpenResult", status,
                            NotificationDatabase::STATUS_COUNT);

  // TODO(peter): Do finer-grained synchronization here.
  if (prune_database_on_open_) {
    prune_database_on_open_ = false;
    DestroyDatabase();

    database_.reset(new NotificationDatabase(GetDatabasePath()));
    status = database_->Open(true /* create_if_missing */);

    // TODO(peter): Find the appropriate UMA to cover in regards to
    // synchronizing notifications after the implementation is complete.
  }

  // When the database could not be opened due to corruption, destroy it, blow
  // away the contents of the directory and try re-opening the database.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED) {
    if (DestroyDatabase()) {
      database_.reset(new NotificationDatabase(GetDatabasePath()));
      status = database_->Open(true /* create_if_missing */);

      UMA_HISTOGRAM_ENUMERATION(
          "Notifications.Database.OpenAfterCorruptionResult", status,
          NotificationDatabase::STATUS_COUNT);
    }
  }

  if (status == NotificationDatabase::STATUS_OK) {
    success_closure.Run();
    return;
  }

  database_.reset();

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, failure_closure);
}

bool PlatformNotificationContextImpl::DestroyDatabase() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  DCHECK(database_);

  NotificationDatabase::Status status = database_->Destroy();
  UMA_HISTOGRAM_ENUMERATION("Notifications.Database.DestroyResult", status,
                            NotificationDatabase::STATUS_COUNT);

  database_.reset();

  // TODO(peter): Close any existing persistent notifications on the platform.

  // Remove all files in the directory that the database was previously located
  // in, to make sure that any left-over files are gone as well.
  base::FilePath database_path = GetDatabasePath();
  if (!database_path.empty())
    return base::DeleteFile(database_path, true);

  return true;
}

base::FilePath PlatformNotificationContextImpl::GetDatabasePath() const {
  if (path_.empty())
    return path_;

  return path_.Append(kPlatformNotificationsDirectory);
}

void PlatformNotificationContextImpl::SetTaskRunnerForTesting(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner) {
  task_runner_ = task_runner;
}

}  // namespace content
