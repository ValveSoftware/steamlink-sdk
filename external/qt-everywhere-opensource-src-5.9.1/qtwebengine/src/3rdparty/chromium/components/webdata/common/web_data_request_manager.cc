// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webdata/common/web_data_request_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/profiler/scoped_tracker.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"

////////////////////////////////////////////////////////////////////////////////
//
// WebDataRequest implementation.
//
////////////////////////////////////////////////////////////////////////////////

WebDataRequest::WebDataRequest(WebDataServiceConsumer* consumer,
                               WebDataRequestManager* manager)
    : manager_(manager), cancelled_(false), consumer_(consumer) {
  handle_ = manager_->GetNextRequestHandle();
  task_runner_ = base::ThreadTaskRunnerHandle::Get();
  manager_->RegisterRequest(this);
}

WebDataRequest::~WebDataRequest() {
  if (manager_) {
    manager_->CancelRequest(handle_);
  }
}

WebDataServiceBase::Handle WebDataRequest::GetHandle() const {
  return handle_;
}

WebDataServiceConsumer* WebDataRequest::GetConsumer() const {
  return consumer_;
}

scoped_refptr<base::SingleThreadTaskRunner> WebDataRequest::GetTaskRunner()
    const {
  return task_runner_;
}

bool WebDataRequest::IsCancelled() const {
  base::AutoLock l(cancel_lock_);
  return cancelled_;
}

void WebDataRequest::Cancel() {
  base::AutoLock l(cancel_lock_);
  cancelled_ = true;
  consumer_ = NULL;
  manager_ = NULL;
}

void WebDataRequest::OnComplete() {
  manager_= NULL;
}

void WebDataRequest::SetResult(std::unique_ptr<WDTypedResult> r) {
  result_ = std::move(r);
}

std::unique_ptr<WDTypedResult> WebDataRequest::GetResult() {
  return std::move(result_);
}

////////////////////////////////////////////////////////////////////////////////
//
// WebDataRequestManager implementation.
//
////////////////////////////////////////////////////////////////////////////////

WebDataRequestManager::WebDataRequestManager()
    : next_request_handle_(1) {
}

WebDataRequestManager::~WebDataRequestManager() {
  base::AutoLock l(pending_lock_);
  for (auto i = pending_requests_.begin(); i != pending_requests_.end(); ++i)
    i->second->Cancel();
  pending_requests_.clear();
}

void WebDataRequestManager::RegisterRequest(WebDataRequest* request) {
  base::AutoLock l(pending_lock_);
  pending_requests_[request->GetHandle()] = request;
}

int WebDataRequestManager::GetNextRequestHandle() {
  base::AutoLock l(pending_lock_);
  return ++next_request_handle_;
}

void WebDataRequestManager::CancelRequest(WebDataServiceBase::Handle h) {
  base::AutoLock l(pending_lock_);
  auto i = pending_requests_.find(h);
  DCHECK(i != pending_requests_.end());
  i->second->Cancel();
  pending_requests_.erase(i);
}

void WebDataRequestManager::RequestCompleted(
    std::unique_ptr<WebDataRequest> request) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      request->GetTaskRunner();
  task_runner->PostTask(
      FROM_HERE, base::Bind(&WebDataRequestManager::RequestCompletedOnThread,
                            this, base::Passed(&request)));
}

void WebDataRequestManager::RequestCompletedOnThread(
    std::unique_ptr<WebDataRequest> request) {
  if (request->IsCancelled())
    return;

  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/422460 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "422460 WebDataRequestManager::RequestCompletedOnThread::UpdateMap"));
  {
    base::AutoLock l(pending_lock_);
    auto i = pending_requests_.find(request->GetHandle());
    DCHECK(i != pending_requests_.end());

    // Take ownership of the request object and remove it from the map.
    pending_requests_.erase(i);
  }

  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/422460 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile2(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "422460 "
          "WebDataRequestManager::RequestCompletedOnThread::NotifyConsumer"));

  // Notify the consumer if needed.
  if (!request->IsCancelled()) {
    WebDataServiceConsumer* consumer = request->GetConsumer();
    request->OnComplete();
    if (consumer) {
      consumer->OnWebDataServiceRequestDone(request->GetHandle(),
                                            request->GetResult());
    }
  }

}
