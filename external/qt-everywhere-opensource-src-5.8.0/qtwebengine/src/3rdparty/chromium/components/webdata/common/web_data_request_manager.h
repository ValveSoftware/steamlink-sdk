// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chromium settings and storage represent user-selected preferences and
// information and MUST not be extracted, overwritten or modified except
// through Chromium defined APIs.

#ifndef COMPONENTS_WEBDATA_COMMON_WEB_DATA_REQUEST_MANAGER_H__
#define COMPONENTS_WEBDATA_COMMON_WEB_DATA_REQUEST_MANAGER_H__

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "components/webdata/common/web_database_service.h"

class WebDataServiceConsumer;
class WebDataRequestManager;

//////////////////////////////////////////////////////////////////////////////
//
// Webdata requests
//
// Every request is processed using a request object. The object contains
// both the request parameters and the results.
//////////////////////////////////////////////////////////////////////////////
class WebDataRequest {
 public:
  WebDataRequest(WebDataServiceConsumer* consumer,
                 WebDataRequestManager* manager);

  virtual ~WebDataRequest();

  WebDataServiceBase::Handle GetHandle() const;

  // Retrieves the |consumer_| set in the constructor.
  WebDataServiceConsumer* GetConsumer() const;

  // Retrieves the original task runner of the request.
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() const;

  // Returns |true| if the request was cancelled via the |Cancel()| method.
  bool IsCancelled() const;

  // This can be invoked from any thread. From this point we assume that
  // our consumer_ reference is invalid.
  void Cancel();

  // Invoked when the request has been completed.
  void OnComplete();

  // The result is owned by the request.
  void SetResult(std::unique_ptr<WDTypedResult> r);

  // Transfers ownership pof result to caller. Should only be called once per
  // result.
  std::unique_ptr<WDTypedResult> GetResult();

 private:
  // Used to notify manager if request is cancelled. Uses a raw ptr instead of
  // a ref_ptr so that it can be set to NULL when a request is cancelled.
  WebDataRequestManager* manager_;

  // Tracks task runner that the request originated on.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Identifier for this request.
  WebDataServiceBase::Handle handle_;

  // A lock to protect against simultaneous cancellations of the request.
  // Cancellation affects both the |cancelled_| flag and |consumer_|.
  mutable base::Lock cancel_lock_;
  bool cancelled_;

  // The originator of the service request.
  WebDataServiceConsumer* consumer_;

  std::unique_ptr<WDTypedResult> result_;

  DISALLOW_COPY_AND_ASSIGN(WebDataRequest);
};

//////////////////////////////////////////////////////////////////////////////
//
// Webdata Request Manager
//
// Tracks all WebDataRequests for a WebDataService.
//
// Note: This is an internal interface, not to be used outside of webdata/
//////////////////////////////////////////////////////////////////////////////
class WebDataRequestManager
    : public base::RefCountedThreadSafe<WebDataRequestManager> {
 public:
  WebDataRequestManager();

  // Cancel any pending request.
  void CancelRequest(WebDataServiceBase::Handle h);

  // Invoked by the WebDataService when |request| has been completed.
  void RequestCompleted(std::unique_ptr<WebDataRequest> request);

  // Register the request as a pending request.
  void RegisterRequest(WebDataRequest* request);

  // Return the next request handle.
  int GetNextRequestHandle();

 private:
  friend class base::RefCountedThreadSafe<WebDataRequestManager>;

  ~WebDataRequestManager();

  // This will notify the consumer in whatever thread was used to create this
  // request.
  void RequestCompletedOnThread(std::unique_ptr<WebDataRequest> request);

  // A lock to protect pending requests and next request handle.
  base::Lock pending_lock_;

  // Next handle to be used for requests. Incremented for each use.
  WebDataServiceBase::Handle next_request_handle_;

  typedef std::map<WebDataServiceBase::Handle, WebDataRequest*> RequestMap;
  RequestMap pending_requests_;

  DISALLOW_COPY_AND_ASSIGN(WebDataRequestManager);
};

#endif  // COMPONENTS_WEBDATA_COMMON_WEB_DATA_REQUEST_MANAGER_H__
