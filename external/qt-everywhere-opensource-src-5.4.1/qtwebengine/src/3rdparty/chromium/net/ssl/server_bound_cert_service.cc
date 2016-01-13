// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/server_bound_cert_service.h"

#include <algorithm>
#include <limits>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/task_runner.h"
#include "crypto/ec_private_key.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "url/gurl.h"

#if defined(USE_NSS)
#include <private/pprthred.h>  // PR_DetachThread
#endif

namespace net {

namespace {

const int kValidityPeriodInDays = 365;
// When we check the system time, we add this many days to the end of the check
// so the result will still hold even after chrome has been running for a
// while.
const int kSystemTimeValidityBufferInDays = 90;

// Used by the GetDomainBoundCertResult histogram to record the final
// outcome of each GetDomainBoundCert or GetOrCreateDomainBoundCert call.
// Do not re-use values.
enum GetCertResult {
  // Synchronously found and returned an existing domain bound cert.
  SYNC_SUCCESS = 0,
  // Retrieved or generated and returned a domain bound cert asynchronously.
  ASYNC_SUCCESS = 1,
  // Retrieval/generation request was cancelled before the cert generation
  // completed.
  ASYNC_CANCELLED = 2,
  // Cert generation failed.
  ASYNC_FAILURE_KEYGEN = 3,
  ASYNC_FAILURE_CREATE_CERT = 4,
  ASYNC_FAILURE_EXPORT_KEY = 5,
  ASYNC_FAILURE_UNKNOWN = 6,
  // GetDomainBoundCert or GetOrCreateDomainBoundCert was called with
  // invalid arguments.
  INVALID_ARGUMENT = 7,
  // We don't support any of the cert types the server requested.
  UNSUPPORTED_TYPE = 8,
  // Server asked for a different type of certs while we were generating one.
  TYPE_MISMATCH = 9,
  // Couldn't start a worker to generate a cert.
  WORKER_FAILURE = 10,
  GET_CERT_RESULT_MAX
};

void RecordGetDomainBoundCertResult(GetCertResult result) {
  UMA_HISTOGRAM_ENUMERATION("DomainBoundCerts.GetDomainBoundCertResult", result,
                            GET_CERT_RESULT_MAX);
}

void RecordGetCertTime(base::TimeDelta request_time) {
  UMA_HISTOGRAM_CUSTOM_TIMES("DomainBoundCerts.GetCertTime",
                             request_time,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMinutes(5),
                             50);
}

// On success, returns a ServerBoundCert object and sets |*error| to OK.
// Otherwise, returns NULL, and |*error| will be set to a net error code.
// |serial_number| is passed in because base::RandInt cannot be called from an
// unjoined thread, due to relying on a non-leaked LazyInstance
scoped_ptr<ServerBoundCertStore::ServerBoundCert> GenerateCert(
    const std::string& server_identifier,
    uint32 serial_number,
    int* error) {
  scoped_ptr<ServerBoundCertStore::ServerBoundCert> result;

  base::TimeTicks start = base::TimeTicks::Now();
  base::Time not_valid_before = base::Time::Now();
  base::Time not_valid_after =
      not_valid_before + base::TimeDelta::FromDays(kValidityPeriodInDays);
  std::string der_cert;
  std::vector<uint8> private_key_info;
  scoped_ptr<crypto::ECPrivateKey> key;
  if (!x509_util::CreateKeyAndDomainBoundCertEC(server_identifier,
                                                serial_number,
                                                not_valid_before,
                                                not_valid_after,
                                                &key,
                                                &der_cert)) {
    DLOG(ERROR) << "Unable to create x509 cert for client";
    *error = ERR_ORIGIN_BOUND_CERT_GENERATION_FAILED;
    return result.Pass();
  }

  if (!key->ExportEncryptedPrivateKey(ServerBoundCertService::kEPKIPassword,
                                      1, &private_key_info)) {
    DLOG(ERROR) << "Unable to export private key";
    *error = ERR_PRIVATE_KEY_EXPORT_FAILED;
    return result.Pass();
  }

  // TODO(rkn): Perhaps ExportPrivateKey should be changed to output a
  // std::string* to prevent this copying.
  std::string key_out(private_key_info.begin(), private_key_info.end());

  result.reset(new ServerBoundCertStore::ServerBoundCert(
      server_identifier,
      not_valid_before,
      not_valid_after,
      key_out,
      der_cert));
  UMA_HISTOGRAM_CUSTOM_TIMES("DomainBoundCerts.GenerateCertTime",
                             base::TimeTicks::Now() - start,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMinutes(5),
                             50);
  *error = OK;
  return result.Pass();
}

}  // namespace

// Represents the output and result callback of a request.
class ServerBoundCertServiceRequest {
 public:
  ServerBoundCertServiceRequest(base::TimeTicks request_start,
                                const CompletionCallback& callback,
                                std::string* private_key,
                                std::string* cert)
      : request_start_(request_start),
        callback_(callback),
        private_key_(private_key),
        cert_(cert) {
  }

  // Ensures that the result callback will never be made.
  void Cancel() {
    RecordGetDomainBoundCertResult(ASYNC_CANCELLED);
    callback_.Reset();
    private_key_ = NULL;
    cert_ = NULL;
  }

  // Copies the contents of |private_key| and |cert| to the caller's output
  // arguments and calls the callback.
  void Post(int error,
            const std::string& private_key,
            const std::string& cert) {
    switch (error) {
      case OK: {
        base::TimeDelta request_time = base::TimeTicks::Now() - request_start_;
        UMA_HISTOGRAM_CUSTOM_TIMES("DomainBoundCerts.GetCertTimeAsync",
                                   request_time,
                                   base::TimeDelta::FromMilliseconds(1),
                                   base::TimeDelta::FromMinutes(5),
                                   50);
        RecordGetCertTime(request_time);
        RecordGetDomainBoundCertResult(ASYNC_SUCCESS);
        break;
      }
      case ERR_KEY_GENERATION_FAILED:
        RecordGetDomainBoundCertResult(ASYNC_FAILURE_KEYGEN);
        break;
      case ERR_ORIGIN_BOUND_CERT_GENERATION_FAILED:
        RecordGetDomainBoundCertResult(ASYNC_FAILURE_CREATE_CERT);
        break;
      case ERR_PRIVATE_KEY_EXPORT_FAILED:
        RecordGetDomainBoundCertResult(ASYNC_FAILURE_EXPORT_KEY);
        break;
      case ERR_INSUFFICIENT_RESOURCES:
        RecordGetDomainBoundCertResult(WORKER_FAILURE);
        break;
      default:
        RecordGetDomainBoundCertResult(ASYNC_FAILURE_UNKNOWN);
        break;
    }
    if (!callback_.is_null()) {
      *private_key_ = private_key;
      *cert_ = cert;
      callback_.Run(error);
    }
    delete this;
  }

  bool canceled() const { return callback_.is_null(); }

 private:
  base::TimeTicks request_start_;
  CompletionCallback callback_;
  std::string* private_key_;
  std::string* cert_;
};

// ServerBoundCertServiceWorker runs on a worker thread and takes care of the
// blocking process of performing key generation. Will take care of deleting
// itself once Start() is called.
class ServerBoundCertServiceWorker {
 public:
  typedef base::Callback<void(
      const std::string&,
      int,
      scoped_ptr<ServerBoundCertStore::ServerBoundCert>)> WorkerDoneCallback;

  ServerBoundCertServiceWorker(
      const std::string& server_identifier,
      const WorkerDoneCallback& callback)
      : server_identifier_(server_identifier),
        serial_number_(base::RandInt(0, std::numeric_limits<int>::max())),
        origin_loop_(base::MessageLoopProxy::current()),
        callback_(callback) {
  }

  // Starts the worker on |task_runner|. If the worker fails to start, such as
  // if the task runner is shutting down, then it will take care of deleting
  // itself.
  bool Start(const scoped_refptr<base::TaskRunner>& task_runner) {
    DCHECK(origin_loop_->RunsTasksOnCurrentThread());

    return task_runner->PostTask(
        FROM_HERE,
        base::Bind(&ServerBoundCertServiceWorker::Run, base::Owned(this)));
  }

 private:
  void Run() {
    // Runs on a worker thread.
    int error = ERR_FAILED;
    scoped_ptr<ServerBoundCertStore::ServerBoundCert> cert =
        GenerateCert(server_identifier_, serial_number_, &error);
    DVLOG(1) << "GenerateCert " << server_identifier_ << " returned " << error;
#if defined(USE_NSS)
    // Detach the thread from NSPR.
    // Calling NSS functions attaches the thread to NSPR, which stores
    // the NSPR thread ID in thread-specific data.
    // The threads in our thread pool terminate after we have called
    // PR_Cleanup. Unless we detach them from NSPR, net_unittests gets
    // segfaults on shutdown when the threads' thread-specific data
    // destructors run.
    PR_DetachThread();
#endif
    origin_loop_->PostTask(FROM_HERE,
                           base::Bind(callback_, server_identifier_, error,
                                      base::Passed(&cert)));
  }

  const std::string server_identifier_;
  // Note that serial_number_ must be initialized on a non-worker thread
  // (see documentation for GenerateCert).
  uint32 serial_number_;
  scoped_refptr<base::SequencedTaskRunner> origin_loop_;
  WorkerDoneCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(ServerBoundCertServiceWorker);
};

// A ServerBoundCertServiceJob is a one-to-one counterpart of an
// ServerBoundCertServiceWorker. It lives only on the ServerBoundCertService's
// origin message loop.
class ServerBoundCertServiceJob {
 public:
  ServerBoundCertServiceJob(bool create_if_missing)
      : create_if_missing_(create_if_missing) {
  }

  ~ServerBoundCertServiceJob() {
    if (!requests_.empty())
      DeleteAllCanceled();
  }

  void AddRequest(ServerBoundCertServiceRequest* request,
                  bool create_if_missing = false) {
    create_if_missing_ |= create_if_missing;
    requests_.push_back(request);
  }

  void HandleResult(int error,
                    const std::string& private_key,
                    const std::string& cert) {
    PostAll(error, private_key, cert);
  }

  bool CreateIfMissing() const { return create_if_missing_; }

 private:
  void PostAll(int error,
               const std::string& private_key,
               const std::string& cert) {
    std::vector<ServerBoundCertServiceRequest*> requests;
    requests_.swap(requests);

    for (std::vector<ServerBoundCertServiceRequest*>::iterator
         i = requests.begin(); i != requests.end(); i++) {
      (*i)->Post(error, private_key, cert);
      // Post() causes the ServerBoundCertServiceRequest to delete itself.
    }
  }

  void DeleteAllCanceled() {
    for (std::vector<ServerBoundCertServiceRequest*>::iterator
         i = requests_.begin(); i != requests_.end(); i++) {
      if ((*i)->canceled()) {
        delete *i;
      } else {
        LOG(DFATAL) << "ServerBoundCertServiceRequest leaked!";
      }
    }
  }

  std::vector<ServerBoundCertServiceRequest*> requests_;
  bool create_if_missing_;
};

// static
const char ServerBoundCertService::kEPKIPassword[] = "";

ServerBoundCertService::RequestHandle::RequestHandle()
    : service_(NULL),
      request_(NULL) {}

ServerBoundCertService::RequestHandle::~RequestHandle() {
  Cancel();
}

void ServerBoundCertService::RequestHandle::Cancel() {
  if (request_) {
    service_->CancelRequest(request_);
    request_ = NULL;
    callback_.Reset();
  }
}

void ServerBoundCertService::RequestHandle::RequestStarted(
    ServerBoundCertService* service,
    ServerBoundCertServiceRequest* request,
    const CompletionCallback& callback) {
  DCHECK(request_ == NULL);
  service_ = service;
  request_ = request;
  callback_ = callback;
}

void ServerBoundCertService::RequestHandle::OnRequestComplete(int result) {
  request_ = NULL;
  // Running the callback might delete |this|, so we can't touch any of our
  // members afterwards. Reset callback_ first.
  base::ResetAndReturn(&callback_).Run(result);
}

ServerBoundCertService::ServerBoundCertService(
    ServerBoundCertStore* server_bound_cert_store,
    const scoped_refptr<base::TaskRunner>& task_runner)
    : server_bound_cert_store_(server_bound_cert_store),
      task_runner_(task_runner),
      requests_(0),
      cert_store_hits_(0),
      inflight_joins_(0),
      workers_created_(0),
      weak_ptr_factory_(this) {
  base::Time start = base::Time::Now();
  base::Time end = start + base::TimeDelta::FromDays(
      kValidityPeriodInDays + kSystemTimeValidityBufferInDays);
  is_system_time_valid_ = x509_util::IsSupportedValidityRange(start, end);
}

ServerBoundCertService::~ServerBoundCertService() {
  STLDeleteValues(&inflight_);
}

//static
std::string ServerBoundCertService::GetDomainForHost(const std::string& host) {
  std::string domain =
      registry_controlled_domains::GetDomainAndRegistry(
          host, registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (domain.empty())
    return host;
  return domain;
}

int ServerBoundCertService::GetOrCreateDomainBoundCert(
    const std::string& host,
    std::string* private_key,
    std::string* cert,
    const CompletionCallback& callback,
    RequestHandle* out_req) {
  DVLOG(1) << __FUNCTION__ << " " << host;
  DCHECK(CalledOnValidThread());
  base::TimeTicks request_start = base::TimeTicks::Now();

  if (callback.is_null() || !private_key || !cert || host.empty()) {
    RecordGetDomainBoundCertResult(INVALID_ARGUMENT);
    return ERR_INVALID_ARGUMENT;
  }

  std::string domain = GetDomainForHost(host);
  if (domain.empty()) {
    RecordGetDomainBoundCertResult(INVALID_ARGUMENT);
    return ERR_INVALID_ARGUMENT;
  }

  requests_++;

  // See if a request for the same domain is currently in flight.
  bool create_if_missing = true;
  if (JoinToInFlightRequest(request_start, domain, private_key, cert,
                            create_if_missing, callback, out_req)) {
    return ERR_IO_PENDING;
  }

  int err = LookupDomainBoundCert(request_start, domain, private_key, cert,
                                  create_if_missing, callback, out_req);
  if (err == ERR_FILE_NOT_FOUND) {
    // Sync lookup did not find a valid cert.  Start generating a new one.
    workers_created_++;
    ServerBoundCertServiceWorker* worker = new ServerBoundCertServiceWorker(
        domain,
        base::Bind(&ServerBoundCertService::GeneratedServerBoundCert,
                   weak_ptr_factory_.GetWeakPtr()));
    if (!worker->Start(task_runner_)) {
      // TODO(rkn): Log to the NetLog.
      LOG(ERROR) << "ServerBoundCertServiceWorker couldn't be started.";
      RecordGetDomainBoundCertResult(WORKER_FAILURE);
      return ERR_INSUFFICIENT_RESOURCES;
    }
    // We are waiting for cert generation.  Create a job & request to track it.
    ServerBoundCertServiceJob* job =
        new ServerBoundCertServiceJob(create_if_missing);
    inflight_[domain] = job;

    ServerBoundCertServiceRequest* request = new ServerBoundCertServiceRequest(
        request_start,
        base::Bind(&RequestHandle::OnRequestComplete,
                   base::Unretained(out_req)),
        private_key,
        cert);
    job->AddRequest(request);
    out_req->RequestStarted(this, request, callback);
    return ERR_IO_PENDING;
  }

  return err;
}

int ServerBoundCertService::GetDomainBoundCert(
    const std::string& host,
    std::string* private_key,
    std::string* cert,
    const CompletionCallback& callback,
    RequestHandle* out_req) {
  DVLOG(1) << __FUNCTION__ << " " << host;
  DCHECK(CalledOnValidThread());
  base::TimeTicks request_start = base::TimeTicks::Now();

  if (callback.is_null() || !private_key || !cert || host.empty()) {
    RecordGetDomainBoundCertResult(INVALID_ARGUMENT);
    return ERR_INVALID_ARGUMENT;
  }

  std::string domain = GetDomainForHost(host);
  if (domain.empty()) {
    RecordGetDomainBoundCertResult(INVALID_ARGUMENT);
    return ERR_INVALID_ARGUMENT;
  }

  requests_++;

  // See if a request for the same domain currently in flight.
  bool create_if_missing = false;
  if (JoinToInFlightRequest(request_start, domain, private_key, cert,
                            create_if_missing, callback, out_req)) {
    return ERR_IO_PENDING;
  }

  int err = LookupDomainBoundCert(request_start, domain, private_key, cert,
                                  create_if_missing, callback, out_req);
  return err;
}

void ServerBoundCertService::GotServerBoundCert(
    int err,
    const std::string& server_identifier,
    base::Time expiration_time,
    const std::string& key,
    const std::string& cert) {
  DCHECK(CalledOnValidThread());

  std::map<std::string, ServerBoundCertServiceJob*>::iterator j;
  j = inflight_.find(server_identifier);
  if (j == inflight_.end()) {
    NOTREACHED();
    return;
  }

  if (err == OK) {
    // Async DB lookup found a valid cert.
    DVLOG(1) << "Cert store had valid cert for " << server_identifier;
    cert_store_hits_++;
    // ServerBoundCertServiceRequest::Post will do the histograms and stuff.
    HandleResult(OK, server_identifier, key, cert);
    return;
  }
  // Async lookup did not find a valid cert. If no request asked to create one,
  // return the error directly.
  if (!j->second->CreateIfMissing()) {
    HandleResult(err, server_identifier, key, cert);
    return;
  }
  // At least one request asked to create a cert => start generating a new one.
  workers_created_++;
  ServerBoundCertServiceWorker* worker = new ServerBoundCertServiceWorker(
      server_identifier,
      base::Bind(&ServerBoundCertService::GeneratedServerBoundCert,
                 weak_ptr_factory_.GetWeakPtr()));
  if (!worker->Start(task_runner_)) {
    // TODO(rkn): Log to the NetLog.
    LOG(ERROR) << "ServerBoundCertServiceWorker couldn't be started.";
    HandleResult(ERR_INSUFFICIENT_RESOURCES,
                 server_identifier,
                 std::string(),
                 std::string());
  }
}

ServerBoundCertStore* ServerBoundCertService::GetCertStore() {
  return server_bound_cert_store_.get();
}

void ServerBoundCertService::CancelRequest(ServerBoundCertServiceRequest* req) {
  DCHECK(CalledOnValidThread());
  req->Cancel();
}

void ServerBoundCertService::GeneratedServerBoundCert(
    const std::string& server_identifier,
    int error,
    scoped_ptr<ServerBoundCertStore::ServerBoundCert> cert) {
  DCHECK(CalledOnValidThread());

  if (error == OK) {
    // TODO(mattm): we should just Pass() the cert object to
    // SetServerBoundCert().
    server_bound_cert_store_->SetServerBoundCert(
        cert->server_identifier(),
        cert->creation_time(),
        cert->expiration_time(),
        cert->private_key(),
        cert->cert());

    HandleResult(error, server_identifier, cert->private_key(), cert->cert());
  } else {
    HandleResult(error, server_identifier, std::string(), std::string());
  }
}

void ServerBoundCertService::HandleResult(
    int error,
    const std::string& server_identifier,
    const std::string& private_key,
    const std::string& cert) {
  DCHECK(CalledOnValidThread());

  std::map<std::string, ServerBoundCertServiceJob*>::iterator j;
  j = inflight_.find(server_identifier);
  if (j == inflight_.end()) {
    NOTREACHED();
    return;
  }
  ServerBoundCertServiceJob* job = j->second;
  inflight_.erase(j);

  job->HandleResult(error, private_key, cert);
  delete job;
}

bool ServerBoundCertService::JoinToInFlightRequest(
    const base::TimeTicks& request_start,
    const std::string& domain,
    std::string* private_key,
    std::string* cert,
    bool create_if_missing,
    const CompletionCallback& callback,
    RequestHandle* out_req) {
  ServerBoundCertServiceJob* job = NULL;
  std::map<std::string, ServerBoundCertServiceJob*>::const_iterator j =
      inflight_.find(domain);
  if (j != inflight_.end()) {
    // A request for the same domain is in flight already. We'll attach our
    // callback, but we'll also mark it as requiring a cert if one's mising.
    job = j->second;
    inflight_joins_++;

    ServerBoundCertServiceRequest* request = new ServerBoundCertServiceRequest(
        request_start,
        base::Bind(&RequestHandle::OnRequestComplete,
                   base::Unretained(out_req)),
        private_key,
        cert);
    job->AddRequest(request, create_if_missing);
    out_req->RequestStarted(this, request, callback);
    return true;
  }
  return false;
}

int ServerBoundCertService::LookupDomainBoundCert(
    const base::TimeTicks& request_start,
    const std::string& domain,
    std::string* private_key,
    std::string* cert,
    bool create_if_missing,
    const CompletionCallback& callback,
    RequestHandle* out_req) {
  // Check if a domain bound cert already exists for this domain. Note that
  // |expiration_time| is ignored, and expired certs are considered valid.
  base::Time expiration_time;
  int err = server_bound_cert_store_->GetServerBoundCert(
      domain,
      &expiration_time  /* ignored */,
      private_key,
      cert,
      base::Bind(&ServerBoundCertService::GotServerBoundCert,
                 weak_ptr_factory_.GetWeakPtr()));

  if (err == OK) {
    // Sync lookup found a valid cert.
    DVLOG(1) << "Cert store had valid cert for " << domain;
    cert_store_hits_++;
    RecordGetDomainBoundCertResult(SYNC_SUCCESS);
    base::TimeDelta request_time = base::TimeTicks::Now() - request_start;
    UMA_HISTOGRAM_TIMES("DomainBoundCerts.GetCertTimeSync", request_time);
    RecordGetCertTime(request_time);
    return OK;
  }

  if (err == ERR_IO_PENDING) {
    // We are waiting for async DB lookup.  Create a job & request to track it.
    ServerBoundCertServiceJob* job =
        new ServerBoundCertServiceJob(create_if_missing);
    inflight_[domain] = job;

    ServerBoundCertServiceRequest* request = new ServerBoundCertServiceRequest(
        request_start,
        base::Bind(&RequestHandle::OnRequestComplete,
                   base::Unretained(out_req)),
        private_key,
        cert);
    job->AddRequest(request);
    out_req->RequestStarted(this, request, callback);
    return ERR_IO_PENDING;
  }

  return err;
}

int ServerBoundCertService::cert_count() {
  return server_bound_cert_store_->GetCertCount();
}

}  // namespace net
