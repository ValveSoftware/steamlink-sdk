// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_CONTEXT_IMPL_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/service_worker/service_worker_context_observer.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/platform_notification_context.h"
#include "third_party/WebKit/public/platform/modules/notifications/notification_service.mojom.h"

class GURL;

namespace base {
class SequencedTaskRunner;
}

namespace content {

class BlinkNotificationServiceImpl;
class BrowserContext;
class NotificationDatabase;
struct NotificationDatabaseData;
class ResourceContext;
class ServiceWorkerContextWrapper;

// Implementation of the Web Notification storage context. The public methods
// defined in this interface must only be called on the IO thread unless
// otherwise specified.
class CONTENT_EXPORT PlatformNotificationContextImpl
    : NON_EXPORTED_BASE(public PlatformNotificationContext),
      NON_EXPORTED_BASE(public ServiceWorkerContextObserver) {
 public:
  // Constructs a new platform notification context. If |path| is non-empty, the
  // database will be initialized in the "Platform Notifications" subdirectory
  // of |path|. Otherwise, the database will be initialized in memory. The
  // constructor must only be called on the IO thread.
  PlatformNotificationContextImpl(
      const base::FilePath& path,
      BrowserContext* browser_context,
      const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context);

  // To be called on the UI thread to initialize the instance.
  void Initialize();

  // To be called on the UI thread when the context is being shut down.
  void Shutdown();

  // Creates a BlinkNotificationServiceImpl that is owned by this context. Must
  // be called on the UI thread, although the service will be created on and
  // bound to the IO thread.
  void CreateService(
      int render_process_id,
      mojo::InterfaceRequest<blink::mojom::NotificationService> request);

  // Removes |service| from the list of owned services, for example because the
  // Mojo pipe disconnected. Must be called on the IO thread.
  void RemoveService(BlinkNotificationServiceImpl* service);

  // PlatformNotificationContext implementation.
  void ReadNotificationData(int64_t notification_id,
                            const GURL& origin,
                            const ReadResultCallback& callback) override;
  void WriteNotificationData(const GURL& origin,
                             const NotificationDatabaseData& database_data,
                             const WriteResultCallback& callback) override;
  void DeleteNotificationData(int64_t notification_id,
                              const GURL& origin,
                              const DeleteResultCallback& callback) override;
  void ReadAllNotificationDataForServiceWorkerRegistration(
      const GURL& origin,
      int64_t service_worker_registration_id,
      const ReadAllResultCallback& callback) override;

  // ServiceWorkerContextObserver implementation.
  void OnRegistrationDeleted(int64_t registration_id,
                             const GURL& pattern) override;
  void OnStorageWiped() override;

 private:
  friend class PlatformNotificationContextTest;

  ~PlatformNotificationContextImpl() override;

  void InitializeOnIO();
  void ShutdownOnIO();
  void CreateServiceOnIO(
      int render_process_id,
      ResourceContext* resource_context,
      mojo::InterfaceRequest<blink::mojom::NotificationService> request);

  // Initializes the database if neccesary. Must be called on the IO thread.
  // |success_closure| will be invoked on a the |task_runner_| thread when
  // everything is available, or |failure_closure_| will be invoked on the
  // IO thread when initialization fails.
  void LazyInitialize(const base::Closure& success_closure,
                      const base::Closure& failure_closure);

  // Opens the database. Must be called on the |task_runner_| thread. When the
  // database has been opened, |success_closure| will be invoked on the task
  // thread, otherwise |failure_closure_| will be invoked on the IO thread.
  void OpenDatabase(const base::Closure& success_closure,
                    const base::Closure& failure_closure);

  // Actually reads the notification data from the database. Must only be
  // called on the |task_runner_| thread. |callback| will be invoked on the
  // IO thread when the operation has completed.
  void DoReadNotificationData(int64_t notification_id,
                              const GURL& origin,
                              const ReadResultCallback& callback);

  // Actually reads all notification data from the database. Must only be
  // called on the |task_runner_| thread. |callback| will be invoked on the
  // IO thread when the operation has completed.
  void DoReadAllNotificationDataForServiceWorkerRegistration(
      const GURL& origin,
      int64_t service_worker_registration_id,
      const ReadAllResultCallback& callback);

  // Actually writes the notification database to the database. Must only be
  // called on the |task_runner_| thread. |callback| will be invoked on the
  // IO thread when the operation has completed.
  void DoWriteNotificationData(const GURL& origin,
                               const NotificationDatabaseData& database_data,
                               const WriteResultCallback& callback);

  // Actually deletes the notification information from the database. Must only
  // be called on the |task_runner_| thread. |callback| will be invoked on the
  // IO thread when the operation has completed.
  void DoDeleteNotificationData(int64_t notification_id,
                                const GURL& origin,
                                const DeleteResultCallback& callback);

  // Deletes all notifications associated with |service_worker_registration_id|
  // belonging to |origin|. Must be called on the |task_runner_| thread.
  void DoDeleteNotificationsForServiceWorkerRegistration(
      const GURL& origin,
      int64_t service_worker_registration_id);

  // Destroys the database regardless of its initialization status. This method
  // must only be called on the |task_runner_| thread. Returns if the directory
  // the database was stored in could be emptied.
  bool DestroyDatabase();

  // Returns the path in which the database should be initialized. May be empty.
  base::FilePath GetDatabasePath() const;

  // Sets the task runner to use for testing purposes.
  void SetTaskRunnerForTesting(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  base::FilePath path_;
  BrowserContext* browser_context_;

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<NotificationDatabase> database_;

  // Indicates whether the database should be pruned when it's opened.
  bool prune_database_on_open_ = false;

  // The notification services are owned by the platform context, and will be
  // removed when either this class is destroyed or the Mojo pipe disconnects.
  std::vector<std::unique_ptr<BlinkNotificationServiceImpl>> services_;

  DISALLOW_COPY_AND_ASSIGN(PlatformNotificationContextImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_CONTEXT_IMPL_H_
