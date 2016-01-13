// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/request_sender.h"

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "google_apis/drive/auth_service.h"
#include "google_apis/drive/base_requests.h"
#include "net/url_request/url_request_context_getter.h"

namespace google_apis {

RequestSender::RequestSender(
    AuthServiceInterface* auth_service,
    net::URLRequestContextGetter* url_request_context_getter,
    base::SequencedTaskRunner* blocking_task_runner,
    const std::string& custom_user_agent)
    : auth_service_(auth_service),
      url_request_context_getter_(url_request_context_getter),
      blocking_task_runner_(blocking_task_runner),
      custom_user_agent_(custom_user_agent),
      weak_ptr_factory_(this) {
}

RequestSender::~RequestSender() {
  DCHECK(thread_checker_.CalledOnValidThread());
  STLDeleteContainerPointers(in_flight_requests_.begin(),
                             in_flight_requests_.end());
}

base::Closure RequestSender::StartRequestWithRetry(
    AuthenticatedRequestInterface* request) {
  DCHECK(thread_checker_.CalledOnValidThread());

  in_flight_requests_.insert(request);

  // TODO(kinaba): Stop relying on weak pointers. Move lifetime management
  // of the requests to request sender.
  base::Closure cancel_closure =
      base::Bind(&RequestSender::CancelRequest,
                 weak_ptr_factory_.GetWeakPtr(),
                 request->GetWeakPtr());

  if (!auth_service_->HasAccessToken()) {
    // Fetch OAuth2 access token from the refresh token first.
    auth_service_->StartAuthentication(
        base::Bind(&RequestSender::OnAccessTokenFetched,
                   weak_ptr_factory_.GetWeakPtr(),
                   request->GetWeakPtr()));
  } else {
    request->Start(auth_service_->access_token(),
                   custom_user_agent_,
                   base::Bind(&RequestSender::RetryRequest,
                              weak_ptr_factory_.GetWeakPtr()));
  }

  return cancel_closure;
}

void RequestSender::OnAccessTokenFetched(
    const base::WeakPtr<AuthenticatedRequestInterface>& request,
    GDataErrorCode code,
    const std::string& /* access_token */) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Do nothing if the request is canceled during authentication.
  if (!request.get())
    return;

  if (code == HTTP_SUCCESS) {
    DCHECK(auth_service_->HasAccessToken());
    StartRequestWithRetry(request.get());
  } else {
    request->OnAuthFailed(code);
  }
}

void RequestSender::RetryRequest(AuthenticatedRequestInterface* request) {
  DCHECK(thread_checker_.CalledOnValidThread());

  auth_service_->ClearAccessToken();
  // User authentication might have expired - rerun the request to force
  // auth token refresh.
  StartRequestWithRetry(request);
}

void RequestSender::CancelRequest(
    const base::WeakPtr<AuthenticatedRequestInterface>& request) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Do nothing if the request is already finished.
  if (!request.get())
    return;
  request->Cancel();
}

void RequestSender::RequestFinished(AuthenticatedRequestInterface* request) {
  in_flight_requests_.erase(request);
  delete request;
}

}  // namespace google_apis
