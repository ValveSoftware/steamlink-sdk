// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/webrtc_identity_store.h"

#include <map>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/threading/worker_pool.h"
#include "content/browser/media/webrtc_identity_store_backend.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/rsa_private_key.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_util.h"
#include "url/gurl.h"

namespace content {

struct WebRTCIdentityRequestResult {
  WebRTCIdentityRequestResult(int error,
                              const std::string& certificate,
                              const std::string& private_key)
      : error(error), certificate(certificate), private_key(private_key) {}

  int error;
  std::string certificate;
  std::string private_key;
};

// Generates a new identity using |common_name| which expires after
// |validity_period| and returns the result in |result|.
static void GenerateIdentityWorker(const std::string& common_name,
                                   base::TimeDelta validity_period,
                                   WebRTCIdentityRequestResult* result) {
  result->error = net::OK;
  int serial_number = base::RandInt(0, std::numeric_limits<int>::max());

  scoped_ptr<crypto::RSAPrivateKey> key;
  base::Time now = base::Time::Now();
  bool success = net::x509_util::CreateKeyAndSelfSignedCert(
      "CN=" + common_name,
      serial_number,
      now,
      now + validity_period,
      &key,
      &result->certificate);

  if (!success) {
    DLOG(ERROR) << "Unable to create x509 cert for client";
    result->error = net::ERR_SELF_SIGNED_CERT_GENERATION_FAILED;
    return;
  }

  std::vector<uint8> private_key_info;
  if (!key->ExportPrivateKey(&private_key_info)) {
    DLOG(ERROR) << "Unable to export private key";
    result->error = net::ERR_PRIVATE_KEY_EXPORT_FAILED;
    return;
  }

  result->private_key =
      std::string(private_key_info.begin(), private_key_info.end());
}

class WebRTCIdentityRequestHandle;

// The class represents an identity request internal to WebRTCIdentityStore.
// It has a one-to-many mapping to the external version of the request,
// WebRTCIdentityRequestHandle, i.e. multiple identical external requests are
// combined into one internal request.
// It's deleted automatically when the request is completed.
class WebRTCIdentityRequest {
 public:
  WebRTCIdentityRequest(const GURL& origin,
                        const std::string& identity_name,
                        const std::string& common_name)
      : origin_(origin),
        identity_name_(identity_name),
        common_name_(common_name) {}

  void Cancel(WebRTCIdentityRequestHandle* handle) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    if (callbacks_.find(handle) == callbacks_.end())
      return;
    callbacks_.erase(handle);
  }

 private:
  friend class WebRTCIdentityStore;

  void AddCallback(WebRTCIdentityRequestHandle* handle,
                   const WebRTCIdentityStore::CompletionCallback& callback) {
    DCHECK(callbacks_.find(handle) == callbacks_.end());
    callbacks_[handle] = callback;
  }

  // This method deletes "this" and no one should access it after the request
  // completes.
  // We do not use base::Owned to tie its lifetime to the callback for
  // WebRTCIdentityStoreBackend::FindIdentity, because it needs to live longer
  // than that if the identity does not exist in DB.
  void Post(const WebRTCIdentityRequestResult& result) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    for (CallbackMap::iterator it = callbacks_.begin(); it != callbacks_.end();
         ++it)
      it->second.Run(result.error, result.certificate, result.private_key);
    delete this;
  }

  GURL origin_;
  std::string identity_name_;
  std::string common_name_;
  typedef std::map<WebRTCIdentityRequestHandle*,
                   WebRTCIdentityStore::CompletionCallback> CallbackMap;
  CallbackMap callbacks_;
};

// The class represents an identity request which calls back to the external
// client when the request completes.
// Its lifetime is tied with the Callback held by the corresponding
// WebRTCIdentityRequest.
class WebRTCIdentityRequestHandle {
 public:
  WebRTCIdentityRequestHandle(
      WebRTCIdentityStore* store,
      const WebRTCIdentityStore::CompletionCallback& callback)
      : store_(store), request_(NULL), callback_(callback) {}

 private:
  friend class WebRTCIdentityStore;

  // Cancel the request.  Does nothing if the request finished or was already
  // cancelled.
  void Cancel() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    if (!request_)
      return;

    callback_.Reset();
    WebRTCIdentityRequest* request = request_;
    request_ = NULL;
    // "this" will be deleted after the following call, because "this" is
    // owned by the Callback held by |request|.
    request->Cancel(this);
  }

  void OnRequestStarted(WebRTCIdentityRequest* request) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    DCHECK(request);
    request_ = request;
  }

  void OnRequestComplete(int error,
                         const std::string& certificate,
                         const std::string& private_key) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    DCHECK(request_);
    request_ = NULL;
    base::ResetAndReturn(&callback_).Run(error, certificate, private_key);
  }

  WebRTCIdentityStore* store_;
  WebRTCIdentityRequest* request_;
  WebRTCIdentityStore::CompletionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(WebRTCIdentityRequestHandle);
};

WebRTCIdentityStore::WebRTCIdentityStore(const base::FilePath& path,
                                         quota::SpecialStoragePolicy* policy)
    : validity_period_(base::TimeDelta::FromDays(30)),
      task_runner_(base::WorkerPool::GetTaskRunner(true)),
      backend_(new WebRTCIdentityStoreBackend(path, policy, validity_period_)) {
  }

WebRTCIdentityStore::~WebRTCIdentityStore() { backend_->Close(); }

base::Closure WebRTCIdentityStore::RequestIdentity(
    const GURL& origin,
    const std::string& identity_name,
    const std::string& common_name,
    const CompletionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  WebRTCIdentityRequest* request =
      FindRequest(origin, identity_name, common_name);
  // If there is no identical request in flight, create a new one, queue it,
  // and make the backend request.
  if (!request) {
    request = new WebRTCIdentityRequest(origin, identity_name, common_name);
    // |request| will delete itself after the result is posted.
    if (!backend_->FindIdentity(
            origin,
            identity_name,
            common_name,
            base::Bind(
                &WebRTCIdentityStore::BackendFindCallback, this, request))) {
      // Bail out if the backend failed to start the task.
      delete request;
      return base::Closure();
    }
    in_flight_requests_.push_back(request);
  }

  WebRTCIdentityRequestHandle* handle =
    new WebRTCIdentityRequestHandle(this, callback);

  request->AddCallback(
      handle,
      base::Bind(&WebRTCIdentityRequestHandle::OnRequestComplete,
                 base::Owned(handle)));
  handle->OnRequestStarted(request);
  return base::Bind(&WebRTCIdentityRequestHandle::Cancel,
                    base::Unretained(handle));
}

void WebRTCIdentityStore::DeleteBetween(base::Time delete_begin,
                                        base::Time delete_end,
                                        const base::Closure& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  backend_->DeleteBetween(delete_begin, delete_end, callback);
}

void WebRTCIdentityStore::SetValidityPeriodForTesting(
    base::TimeDelta validity_period) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  validity_period_ = validity_period;
  backend_->SetValidityPeriodForTesting(validity_period);
}

void WebRTCIdentityStore::SetTaskRunnerForTesting(
    const scoped_refptr<base::TaskRunner>& task_runner) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  task_runner_ = task_runner;
}

void WebRTCIdentityStore::BackendFindCallback(WebRTCIdentityRequest* request,
                                              int error,
                                              const std::string& certificate,
                                              const std::string& private_key) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (error == net::OK) {
    DVLOG(2) << "Identity found in DB.";
    WebRTCIdentityRequestResult result(error, certificate, private_key);
    PostRequestResult(request, result);
    return;
  }
  // Generate a new identity if not found in the DB.
  WebRTCIdentityRequestResult* result =
      new WebRTCIdentityRequestResult(0, "", "");
  if (!task_runner_->PostTaskAndReply(
           FROM_HERE,
           base::Bind(&GenerateIdentityWorker,
                      request->common_name_,
                      validity_period_,
                      result),
           base::Bind(&WebRTCIdentityStore::GenerateIdentityCallback,
                      this,
                      request,
                      base::Owned(result)))) {
    // Completes the request with error if failed to post the task.
    WebRTCIdentityRequestResult result(net::ERR_UNEXPECTED, "", "");
    PostRequestResult(request, result);
  }
}

void WebRTCIdentityStore::GenerateIdentityCallback(
    WebRTCIdentityRequest* request,
    WebRTCIdentityRequestResult* result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (result->error == net::OK) {
    DVLOG(2) << "New identity generated and added to the backend.";
    backend_->AddIdentity(request->origin_,
                          request->identity_name_,
                          request->common_name_,
                          result->certificate,
                          result->private_key);
  }
  PostRequestResult(request, *result);
}

void WebRTCIdentityStore::PostRequestResult(
    WebRTCIdentityRequest* request,
    const WebRTCIdentityRequestResult& result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Removes the in flight request from the queue.
  for (size_t i = 0; i < in_flight_requests_.size(); ++i) {
    if (in_flight_requests_[i] == request) {
      in_flight_requests_.erase(in_flight_requests_.begin() + i);
      break;
    }
  }
  // |request| will be deleted after this call.
  request->Post(result);
}

// Find an identical request from the in flight requests.
WebRTCIdentityRequest* WebRTCIdentityStore::FindRequest(
    const GURL& origin,
    const std::string& identity_name,
    const std::string& common_name) {
  for (size_t i = 0; i < in_flight_requests_.size(); ++i) {
    if (in_flight_requests_[i]->origin_ == origin &&
        in_flight_requests_[i]->identity_name_ == identity_name &&
        in_flight_requests_[i]->common_name_ == common_name) {
      return in_flight_requests_[i];
    }
  }
  return NULL;
}

}  // namespace content
