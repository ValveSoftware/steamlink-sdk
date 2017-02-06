// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_METRICS_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_METRICS_H_

#include <stddef.h>

#include "base/macros.h"
#include "content/browser/service_worker/service_worker_database.h"
#include "content/common/service_worker/service_worker_types.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerResponseError.h"

class GURL;

namespace content {

enum class EmbeddedWorkerStatus;

class ServiceWorkerMetrics {
 public:
  // Used for UMA. Append-only.
  enum ReadResponseResult {
    READ_OK,
    READ_HEADERS_ERROR,
    READ_DATA_ERROR,
    NUM_READ_RESPONSE_RESULT_TYPES,
  };

  // Used for UMA. Append-only.
  enum WriteResponseResult {
    WRITE_OK,
    WRITE_HEADERS_ERROR,
    WRITE_DATA_ERROR,
    NUM_WRITE_RESPONSE_RESULT_TYPES,
  };

  // Used for UMA. Append-only.
  enum DeleteAndStartOverResult {
    DELETE_OK,
    DELETE_DATABASE_ERROR,
    DELETE_DISK_CACHE_ERROR,
    NUM_DELETE_AND_START_OVER_RESULT_TYPES,
  };

  // Used for UMA. Append-only.
  enum URLRequestJobResult {
    REQUEST_JOB_FALLBACK_RESPONSE,
    REQUEST_JOB_FALLBACK_FOR_CORS,
    REQUEST_JOB_HEADERS_ONLY_RESPONSE,
    REQUEST_JOB_STREAM_RESPONSE,
    REQUEST_JOB_BLOB_RESPONSE,
    REQUEST_JOB_ERROR_RESPONSE_STATUS_ZERO,
    REQUEST_JOB_ERROR_BAD_BLOB,
    REQUEST_JOB_ERROR_NO_PROVIDER_HOST,
    REQUEST_JOB_ERROR_NO_ACTIVE_VERSION,
    REQUEST_JOB_ERROR_NO_REQUEST,
    REQUEST_JOB_ERROR_FETCH_EVENT_DISPATCH,
    REQUEST_JOB_ERROR_BLOB_READ,
    REQUEST_JOB_ERROR_STREAM_ABORTED,
    REQUEST_JOB_ERROR_KILLED,
    REQUEST_JOB_ERROR_KILLED_WITH_BLOB,
    REQUEST_JOB_ERROR_KILLED_WITH_STREAM,
    REQUEST_JOB_ERROR_DESTROYED,
    REQUEST_JOB_ERROR_DESTROYED_WITH_BLOB,
    REQUEST_JOB_ERROR_DESTROYED_WITH_STREAM,
    REQUEST_JOB_ERROR_BAD_DELEGATE,
    REQUEST_JOB_ERROR_REQUEST_BODY_BLOB_FAILED,
    NUM_REQUEST_JOB_RESULT_TYPES,
  };

  // Used for UMA. Append-only.
  enum class StopStatus {
    NORMAL,
    DETACH_BY_REGISTRY,
    TIMEOUT,
    // Add new types here.
    NUM_TYPES
  };

  // Used for UMA. Append-only.
  // This class is used to indicate which event is fired/finished. Most events
  // have only one request that starts the event and one response that finishes
  // the event, but the fetch and the foreign fetch event have two responses, so
  // there are two types of EventType to break down the measurement into two:
  // FETCH/FOREIGN_FETCH and FETCH_WAITUNTIL/FOREIGN_FETCH_WAITUNTIL.
  // Moreover, FETCH is separated into the four: MAIN_FRAME, SUB_FRAME,
  // SHARED_WORKER and SUB_RESOURCE for more detailed UMA.
  enum class EventType {
    ACTIVATE = 0,
    INSTALL = 1,
    // FETCH = 2,  // Obsolete
    SYNC = 3,
    NOTIFICATION_CLICK = 4,
    PUSH = 5,
    // GEOFENCING = 6,  // Obsolete
    // SERVICE_PORT_CONNECT = 7,  // Obsolete
    MESSAGE = 8,
    NOTIFICATION_CLOSE = 9,
    FETCH_MAIN_FRAME = 10,
    FETCH_SUB_FRAME = 11,
    FETCH_SHARED_WORKER = 12,
    FETCH_SUB_RESOURCE = 13,
    UNKNOWN = 14,  // Used when event type is not known.
    FOREIGN_FETCH = 15,
    FETCH_WAITUNTIL = 16,
    FOREIGN_FETCH_WAITUNTIL = 17,
    // Add new events to record here.
    NUM_TYPES
  };

  // Used for UMA. Append only.
  enum class Site {
    OTHER,  // Obsolete
    NEW_TAB_PAGE,
    WITH_FETCH_HANDLER,
    WITHOUT_FETCH_HANDLER,
    NUM_TYPES
  };

  // Not used for UMA.
  enum class StartSituation {
    UNKNOWN,
    DURING_STARTUP,
    EXISTING_PROCESS,
    NEW_PROCESS
  };

  // Not used for UMA.
  enum class LoadSource { NETWORK, HTTP_CACHE, SERVICE_WORKER_STORAGE };

  // Converts an event type to a string. Used for tracing.
  static const char* EventTypeToString(EventType event_type);

  // Excludes NTP scope from UMA for now as it tends to dominate the stats and
  // makes the results largely skewed. Some metrics don't follow this policy
  // and hence don't call this function.
  static bool ShouldExcludeSiteFromHistogram(Site site);
  static bool ShouldExcludeURLFromHistogram(const GURL& url);

  // Used for ServiceWorkerDiskCache.
  static void CountInitDiskCacheResult(bool result);
  static void CountReadResponseResult(ReadResponseResult result);
  static void CountWriteResponseResult(WriteResponseResult result);

  // Used for ServiceWorkerDatabase.
  static void CountOpenDatabaseResult(ServiceWorkerDatabase::Status status);
  static void CountReadDatabaseResult(ServiceWorkerDatabase::Status status);
  static void CountWriteDatabaseResult(ServiceWorkerDatabase::Status status);
  static void RecordDestroyDatabaseResult(ServiceWorkerDatabase::Status status);

  // Used for ServiceWorkerStorage.
  static void RecordPurgeResourceResult(int net_error);
  static void RecordDeleteAndStartOverResult(DeleteAndStartOverResult result);

  // Counts the number of page loads controlled by a Service Worker.
  static void CountControlledPageLoad(const GURL& url,
                                      bool has_fetch_handler,
                                      bool is_main_frame_load);

  // Records the result of trying to start a worker. |is_installed| indicates
  // whether the version has been installed.
  static void RecordStartWorkerStatus(ServiceWorkerStatusCode status,
                                      EventType purpose,
                                      bool is_installed);

  // Records the time taken to successfully start a worker. |is_installed|
  // indicates whether the version has been installed.
  static void RecordStartWorkerTime(base::TimeDelta time,
                                    bool is_installed,
                                    StartSituation start_situation,
                                    EventType purpose);

  // Records the time taken to prepare an activated Service Worker for a main
  // frame fetch.
  static void RecordActivatedWorkerPreparationTimeForMainFrame(
      base::TimeDelta time,
      EmbeddedWorkerStatus initial_worker_status,
      StartSituation start_situation);

  // Records the result of trying to stop a worker.
  static void RecordWorkerStopped(StopStatus status);

  // Records the time taken to successfully stop a worker.
  static void RecordStopWorkerTime(base::TimeDelta time);

  static void RecordActivateEventStatus(ServiceWorkerStatusCode status,
                                        bool is_shutdown);
  static void RecordInstallEventStatus(ServiceWorkerStatusCode status);

  // Records how much of dispatched events are handled while a Service
  // Worker is awake (i.e. after it is woken up until it gets stopped).
  static void RecordEventHandledRatio(EventType event,
                                      size_t handled_events,
                                      size_t fired_events);

  // Records how often a dispatched event times out.
  static void RecordEventTimeout(EventType event);

  // Records the amount of time spent handling an event.
  static void RecordEventDuration(EventType event,
                                  base::TimeDelta time,
                                  bool was_handled);

  // Records the result of dispatching a fetch event to a service worker.
  static void RecordFetchEventStatus(bool is_main_resource,
                                     ServiceWorkerStatusCode status);

  // Records result of a ServiceWorkerURLRequestJob that was forwarded to
  // the service worker.
  static void RecordURLRequestJobResult(bool is_main_resource,
                                        URLRequestJobResult result);

  // Records the error code provided when the renderer returns a response with
  // status zero to a fetch request.
  static void RecordStatusZeroResponseError(
      bool is_main_resource,
      blink::WebServiceWorkerResponseError error);

  // Records the mode of request that was fallbacked to the network.
  static void RecordFallbackedRequestMode(FetchRequestMode mode);

  // Called at the beginning of each ServiceWorkerVersion::Dispatch*Event
  // function. Records the time elapsed since idle (generally the time since the
  // previous event ended).
  static void RecordTimeBetweenEvents(base::TimeDelta time);

  // The following record steps of EmbeddedWorkerInstance's start sequence.
  static void RecordProcessCreated(bool is_new_process);
  static void RecordTimeToSendStartWorker(base::TimeDelta duration,
                                          StartSituation start_situation);
  static void RecordTimeToURLJob(base::TimeDelta duration,
                                 StartSituation start_situation);
  static void RecordTimeToLoad(base::TimeDelta duration,
                               LoadSource source,
                               StartSituation start_situation);
  static void RecordTimeToStartThread(base::TimeDelta duration,
                                      StartSituation start_situation);
  static void RecordTimeToEvaluateScript(base::TimeDelta duration,
                                         StartSituation start_situation);

  static const char* LoadSourceToString(LoadSource source);
  static StartSituation GetStartSituation(bool is_browser_startup_complete,
                                          bool is_new_process);

  // Records the result of a start attempt that occurred after the worker had
  // failed |failure_count| consecutive times.
  static void RecordStartStatusAfterFailure(int failure_count,
                                            ServiceWorkerStatusCode status);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ServiceWorkerMetrics);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_METRICS_H_
