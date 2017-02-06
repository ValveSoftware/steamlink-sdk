// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/notification_event_dispatcher_impl.h"

#include "base/callback.h"
#include "build/build_config.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/platform_notification_data.h"

namespace content {
namespace {

using NotificationDispatchCompleteCallback =
    NotificationEventDispatcher::NotificationDispatchCompleteCallback;
using NotificationOperationCallback =
    base::Callback<void(const ServiceWorkerRegistration*,
                        const NotificationDatabaseData&)>;
using NotificationOperationCallbackWithContext =
    base::Callback<void(const scoped_refptr<PlatformNotificationContext>&,
                        const ServiceWorkerRegistration*,
                        const NotificationDatabaseData&)>;

// To be called when a notification event has finished executing. Will post a
// task to call |dispatch_complete_callback| on the UI thread.
void NotificationEventFinished(
    const NotificationDispatchCompleteCallback& dispatch_complete_callback,
    PersistentNotificationStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(dispatch_complete_callback, status));
}

// To be called when a notification event has finished with a
// ServiceWorkerStatusCode result. Will call NotificationEventFinished with a
// PersistentNotificationStatus derived from the service worker status.
void ServiceWorkerNotificationEventFinished(
    const NotificationDispatchCompleteCallback& dispatch_complete_callback,
    ServiceWorkerStatusCode service_worker_status) {
#if defined(OS_ANDROID)
  // This LOG(INFO) deliberately exists to help track down the cause of
  // https://crbug.com/534537, where notifications sometimes do not react to
  // the user clicking on them. It should be removed once that's fixed.
  LOG(INFO) << "The notification event has finished: " << service_worker_status;
#endif

  PersistentNotificationStatus status = PERSISTENT_NOTIFICATION_STATUS_SUCCESS;
  switch (service_worker_status) {
    case SERVICE_WORKER_OK:
      // Success status was initialized above.
      break;
    case SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED:
      status = PERSISTENT_NOTIFICATION_STATUS_EVENT_WAITUNTIL_REJECTED;
      break;
    case SERVICE_WORKER_ERROR_FAILED:
    case SERVICE_WORKER_ERROR_ABORT:
    case SERVICE_WORKER_ERROR_START_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_PROCESS_NOT_FOUND:
    case SERVICE_WORKER_ERROR_NOT_FOUND:
    case SERVICE_WORKER_ERROR_EXISTS:
    case SERVICE_WORKER_ERROR_INSTALL_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_ACTIVATE_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_IPC_FAILED:
    case SERVICE_WORKER_ERROR_NETWORK:
    case SERVICE_WORKER_ERROR_SECURITY:
    case SERVICE_WORKER_ERROR_STATE:
    case SERVICE_WORKER_ERROR_TIMEOUT:
    case SERVICE_WORKER_ERROR_SCRIPT_EVALUATE_FAILED:
    case SERVICE_WORKER_ERROR_DISK_CACHE:
    case SERVICE_WORKER_ERROR_REDUNDANT:
    case SERVICE_WORKER_ERROR_DISALLOWED:
    case SERVICE_WORKER_ERROR_MAX_VALUE:
      status = PERSISTENT_NOTIFICATION_STATUS_SERVICE_WORKER_ERROR;
      break;
  }
  NotificationEventFinished(dispatch_complete_callback, status);
}

// Dispatches the given notification action event on
// |service_worker_registration| if the registration was available. Must be
// called on the IO thread.
void DispatchNotificationEventOnRegistration(
    const NotificationDatabaseData& notification_database_data,
    const scoped_refptr<PlatformNotificationContext>& notification_context,
    const NotificationOperationCallback& dispatch_event_action,
    const NotificationDispatchCompleteCallback& dispatch_error_callback,
    ServiceWorkerStatusCode service_worker_status,
    const scoped_refptr<ServiceWorkerRegistration>&
        service_worker_registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
#if defined(OS_ANDROID)
  // This LOG(INFO) deliberately exists to help track down the cause of
  // https://crbug.com/534537, where notifications sometimes do not react to
  // the user clicking on them. It should be removed once that's fixed.
  LOG(INFO) << "Trying to dispatch notification for SW with status: "
            << service_worker_status;
#endif
  if (service_worker_status == SERVICE_WORKER_OK) {
    DCHECK(service_worker_registration->active_version());

    dispatch_event_action.Run(service_worker_registration.get(),
                              notification_database_data);
    return;
  }

  PersistentNotificationStatus status = PERSISTENT_NOTIFICATION_STATUS_SUCCESS;
  switch (service_worker_status) {
    case SERVICE_WORKER_ERROR_NOT_FOUND:
      status = PERSISTENT_NOTIFICATION_STATUS_NO_SERVICE_WORKER;
      break;
    case SERVICE_WORKER_ERROR_FAILED:
    case SERVICE_WORKER_ERROR_ABORT:
    case SERVICE_WORKER_ERROR_START_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_PROCESS_NOT_FOUND:
    case SERVICE_WORKER_ERROR_EXISTS:
    case SERVICE_WORKER_ERROR_INSTALL_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_ACTIVATE_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_IPC_FAILED:
    case SERVICE_WORKER_ERROR_NETWORK:
    case SERVICE_WORKER_ERROR_SECURITY:
    case SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED:
    case SERVICE_WORKER_ERROR_STATE:
    case SERVICE_WORKER_ERROR_TIMEOUT:
    case SERVICE_WORKER_ERROR_SCRIPT_EVALUATE_FAILED:
    case SERVICE_WORKER_ERROR_DISK_CACHE:
    case SERVICE_WORKER_ERROR_REDUNDANT:
    case SERVICE_WORKER_ERROR_DISALLOWED:
    case SERVICE_WORKER_ERROR_MAX_VALUE:
      status = PERSISTENT_NOTIFICATION_STATUS_SERVICE_WORKER_ERROR;
      break;
    case SERVICE_WORKER_OK:
      NOTREACHED();
      break;
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(dispatch_error_callback, status));
}

// Finds the ServiceWorkerRegistration associated with the |origin| and
// |service_worker_registration_id|. Must be called on the IO thread.
void FindServiceWorkerRegistration(
    const GURL& origin,
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context,
    const scoped_refptr<PlatformNotificationContext>& notification_context,
    const NotificationOperationCallback& notification_action_callback,
    const NotificationDispatchCompleteCallback& dispatch_error_callback,
    bool success,
    const NotificationDatabaseData& notification_database_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

#if defined(OS_ANDROID)
  // This LOG(INFO) deliberately exists to help track down the cause of
  // https://crbug.com/534537, where notifications sometimes do not react to
  // the user clicking on them. It should be removed once that's fixed.
  LOG(INFO) << "Lookup for ServiceWoker Registration: success: " << success;
#endif
  if (!success) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(dispatch_error_callback,
                   PERSISTENT_NOTIFICATION_STATUS_DATABASE_ERROR));
    return;
  }

  service_worker_context->FindReadyRegistrationForId(
      notification_database_data.service_worker_registration_id, origin,
      base::Bind(&DispatchNotificationEventOnRegistration,
                 notification_database_data, notification_context,
                 notification_action_callback, dispatch_error_callback));
}

// Reads the data associated with the |persistent_notification_id| belonging to
// |origin| from the notification context.
void ReadNotificationDatabaseData(
    int64_t persistent_notification_id,
    const GURL& origin,
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context,
    const scoped_refptr<PlatformNotificationContext>& notification_context,
    const NotificationOperationCallback& notification_read_callback,
    const NotificationDispatchCompleteCallback& dispatch_error_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  notification_context->ReadNotificationData(
      persistent_notification_id, origin,
      base::Bind(&FindServiceWorkerRegistration, origin, service_worker_context,
                 notification_context, notification_read_callback,
                 dispatch_error_callback));
}

// -----------------------------------------------------------------------------

// Dispatches the notificationclick event on |service_worker|. Must be called on
// the IO thread, and with the worker running.
void DispatchNotificationClickEventOnWorker(
    const scoped_refptr<ServiceWorkerVersion>& service_worker,
    const NotificationDatabaseData& notification_database_data,
    int action_index,
    const ServiceWorkerVersion::StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  int request_id = service_worker->StartRequest(
      ServiceWorkerMetrics::EventType::NOTIFICATION_CLICK, callback);
  service_worker->DispatchSimpleEvent<
      ServiceWorkerHostMsg_NotificationClickEventFinished>(
      request_id,
      ServiceWorkerMsg_NotificationClickEvent(
          request_id, notification_database_data.notification_id,
          notification_database_data.notification_data, action_index));
}

// Dispatches the notification click event on the |service_worker_registration|.
void DoDispatchNotificationClickEvent(
    int action_index,
    const NotificationDispatchCompleteCallback& dispatch_complete_callback,
    const scoped_refptr<PlatformNotificationContext>& notification_context,
    const ServiceWorkerRegistration* service_worker_registration,
    const NotificationDatabaseData& notification_database_data) {
  ServiceWorkerVersion::StatusCallback status_callback = base::Bind(
      &ServiceWorkerNotificationEventFinished, dispatch_complete_callback);
  service_worker_registration->active_version()->RunAfterStartWorker(
      ServiceWorkerMetrics::EventType::NOTIFICATION_CLICK,
      base::Bind(
          &DispatchNotificationClickEventOnWorker,
          make_scoped_refptr(service_worker_registration->active_version()),
          notification_database_data, action_index, status_callback),
      status_callback);
}

// -----------------------------------------------------------------------------

// Called when the notification data has been deleted to finish the notification
// close event.
void OnPersistentNotificationDataDeleted(
    ServiceWorkerStatusCode service_worker_status,
    const NotificationDispatchCompleteCallback& dispatch_complete_callback,
    bool success) {
  if (service_worker_status != SERVICE_WORKER_OK) {
    ServiceWorkerNotificationEventFinished(dispatch_complete_callback,
                                           service_worker_status);
    return;
  }
  NotificationEventFinished(
      dispatch_complete_callback,
      success ? PERSISTENT_NOTIFICATION_STATUS_SUCCESS
              : PERSISTENT_NOTIFICATION_STATUS_DATABASE_ERROR);
}

// Called when the persistent notification close event has been handled
// to remove the notification from the database.
void DeleteNotificationDataFromDatabase(
    const int64_t notification_id,
    const GURL& origin,
    const scoped_refptr<PlatformNotificationContext>& notification_context,
    const NotificationDispatchCompleteCallback& dispatch_complete_callback,
    ServiceWorkerStatusCode status_code) {
  notification_context->DeleteNotificationData(
      notification_id, origin,
      base::Bind(&OnPersistentNotificationDataDeleted, status_code,
                 dispatch_complete_callback));
}

// Dispatches the notificationclose event on |service_worker|. Must be called on
// the IO thread, and with the worker running.
void DispatchNotificationCloseEventOnWorker(
    const scoped_refptr<ServiceWorkerVersion>& service_worker,
    const NotificationDatabaseData& notification_database_data,
    const ServiceWorkerVersion::StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  int request_id = service_worker->StartRequest(
      ServiceWorkerMetrics::EventType::NOTIFICATION_CLOSE, callback);
  service_worker->DispatchSimpleEvent<
      ServiceWorkerHostMsg_NotificationCloseEventFinished>(
      request_id, ServiceWorkerMsg_NotificationCloseEvent(
                      request_id, notification_database_data.notification_id,
                      notification_database_data.notification_data));
}

// Actually dispatches the notification close event on the service worker
// registration.
void DoDispatchNotificationCloseEvent(
    bool by_user,
    const NotificationDispatchCompleteCallback& dispatch_complete_callback,
    const scoped_refptr<PlatformNotificationContext>& notification_context,
    const ServiceWorkerRegistration* service_worker_registration,
    const NotificationDatabaseData& notification_database_data) {
  const ServiceWorkerVersion::StatusCallback dispatch_event_callback =
      base::Bind(&DeleteNotificationDataFromDatabase,
                 notification_database_data.notification_id,
                 notification_database_data.origin, notification_context,
                 dispatch_complete_callback);
  if (by_user) {
    service_worker_registration->active_version()->RunAfterStartWorker(
        ServiceWorkerMetrics::EventType::NOTIFICATION_CLOSE,
        base::Bind(
            &DispatchNotificationCloseEventOnWorker,
            make_scoped_refptr(service_worker_registration->active_version()),
            notification_database_data, dispatch_event_callback),
        dispatch_event_callback);
  } else {
    dispatch_event_callback.Run(ServiceWorkerStatusCode::SERVICE_WORKER_OK);
  }
}

// Dispatches any notification event. The actual, specific event dispatch should
// be done by the |notification_action_callback|.
void DispatchNotificationEvent(
    BrowserContext* browser_context,
    int64_t persistent_notification_id,
    const GURL& origin,
    const NotificationOperationCallbackWithContext&
        notification_action_callback,
    const NotificationDispatchCompleteCallback& notification_error_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_GT(persistent_notification_id, 0);
  DCHECK(origin.is_valid());

  StoragePartition* partition =
      BrowserContext::GetStoragePartitionForSite(browser_context, origin);

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context =
      static_cast<ServiceWorkerContextWrapper*>(
          partition->GetServiceWorkerContext());
  scoped_refptr<PlatformNotificationContext> notification_context =
      partition->GetPlatformNotificationContext();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ReadNotificationDatabaseData, persistent_notification_id,
                 origin, service_worker_context, notification_context,
                 base::Bind(notification_action_callback, notification_context),
                 notification_error_callback));
}

}  // namespace

// static
NotificationEventDispatcher* NotificationEventDispatcher::GetInstance() {
  return NotificationEventDispatcherImpl::GetInstance();
}

NotificationEventDispatcherImpl*
NotificationEventDispatcherImpl::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return base::Singleton<NotificationEventDispatcherImpl>::get();
}

NotificationEventDispatcherImpl::NotificationEventDispatcherImpl() {}

NotificationEventDispatcherImpl::~NotificationEventDispatcherImpl() {}

void NotificationEventDispatcherImpl::DispatchNotificationClickEvent(
    BrowserContext* browser_context,
    int64_t persistent_notification_id,
    const GURL& origin,
    int action_index,
    const NotificationDispatchCompleteCallback& dispatch_complete_callback) {
  DispatchNotificationEvent(
      browser_context, persistent_notification_id, origin,
      base::Bind(&DoDispatchNotificationClickEvent, action_index,
                 dispatch_complete_callback),
      dispatch_complete_callback);
}

void NotificationEventDispatcherImpl::DispatchNotificationCloseEvent(
    BrowserContext* browser_context,
    int64_t persistent_notification_id,
    const GURL& origin,
    bool by_user,
    const NotificationDispatchCompleteCallback& dispatch_complete_callback) {
  DispatchNotificationEvent(browser_context, persistent_notification_id, origin,
                            base::Bind(&DoDispatchNotificationCloseEvent,
                                       by_user, dispatch_complete_callback),
                            dispatch_complete_callback);
}

}  // namespace content
