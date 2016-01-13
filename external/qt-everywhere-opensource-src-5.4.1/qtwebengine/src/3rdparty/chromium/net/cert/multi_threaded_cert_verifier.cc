// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/multi_threaded_cert_verifier.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/synchronization/lock.h"
#include "base/threading/worker_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/hash_value.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/cert/cert_trust_anchor_provider.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/crl_set.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_certificate_net_log_param.h"

#if defined(USE_NSS) || defined(OS_IOS)
#include <private/pprthred.h>  // PR_DetachThread
#endif

namespace net {

////////////////////////////////////////////////////////////////////////////

// Life of a request:
//
// MultiThreadedCertVerifier  CertVerifierJob  CertVerifierWorker     Request
//      |                                         (origin loop)    (worker loop)
//      |
//   Verify()
//      |---->-------------------------------------<creates>
//      |
//      |---->-------------------<creates>
//      |
//      |---->-------------------------------------------------------<creates>
//      |
//      |---->---------------------------------------Start
//      |                                              |
//      |                                           PostTask
//      |
//      |                                                     <starts verifying>
//      |---->-------------------AddRequest                           |
//                                                                    |
//                                                                    |
//                                                                    |
//                                                                  Finish
//                                                                    |
//                                                                 PostTask
//
//                                                     |
//                                                  DoReply
//      |----<-----------------------------------------|
//  HandleResult
//      |
//      |---->------------------HandleResult
//                                   |
//                                   |------>---------------------------Post
//
//
//
// On a cache hit, MultiThreadedCertVerifier::Verify() returns synchronously
// without posting a task to a worker thread.

namespace {

// The default value of max_cache_entries_.
const unsigned kMaxCacheEntries = 256;

// The number of seconds for which we'll cache a cache entry.
const unsigned kTTLSecs = 1800;  // 30 minutes.

base::Value* CertVerifyResultCallback(const CertVerifyResult& verify_result,
                                      NetLog::LogLevel log_level) {
  base::DictionaryValue* results = new base::DictionaryValue();
  results->SetBoolean("has_md5", verify_result.has_md5);
  results->SetBoolean("has_md2", verify_result.has_md2);
  results->SetBoolean("has_md4", verify_result.has_md4);
  results->SetBoolean("is_issued_by_known_root",
                      verify_result.is_issued_by_known_root);
  results->SetBoolean("is_issued_by_additional_trust_anchor",
                      verify_result.is_issued_by_additional_trust_anchor);
  results->SetBoolean("common_name_fallback_used",
                      verify_result.common_name_fallback_used);
  results->SetInteger("cert_status", verify_result.cert_status);
  results->Set(
      "verified_cert",
      NetLogX509CertificateCallback(verify_result.verified_cert, log_level));

  base::ListValue* hashes = new base::ListValue();
  for (std::vector<HashValue>::const_iterator it =
           verify_result.public_key_hashes.begin();
       it != verify_result.public_key_hashes.end();
       ++it) {
    hashes->AppendString(it->ToString());
  }
  results->Set("public_key_hashes", hashes);

  return results;
}

}  // namespace

MultiThreadedCertVerifier::CachedResult::CachedResult() : error(ERR_FAILED) {}

MultiThreadedCertVerifier::CachedResult::~CachedResult() {}

MultiThreadedCertVerifier::CacheValidityPeriod::CacheValidityPeriod(
    const base::Time& now)
    : verification_time(now),
      expiration_time(now) {
}

MultiThreadedCertVerifier::CacheValidityPeriod::CacheValidityPeriod(
    const base::Time& now,
    const base::Time& expiration)
    : verification_time(now),
      expiration_time(expiration) {
}

bool MultiThreadedCertVerifier::CacheExpirationFunctor::operator()(
    const CacheValidityPeriod& now,
    const CacheValidityPeriod& expiration) const {
  // Ensure this functor is being used for expiration only, and not strict
  // weak ordering/sorting. |now| should only ever contain a single
  // base::Time.
  // Note: DCHECK_EQ is not used due to operator<< overloading requirements.
  DCHECK(now.verification_time == now.expiration_time);

  // |now| contains only a single time (verification_time), while |expiration|
  // contains the validity range - both when the certificate was verified and
  // when the verification result should expire.
  //
  // If the user receives a "not yet valid" message, and adjusts their clock
  // foward to the correct time, this will (typically) cause
  // now.verification_time to advance past expiration.expiration_time, thus
  // treating the cached result as an expired entry and re-verifying.
  // If the user receives a "expired" message, and adjusts their clock
  // backwards to the correct time, this will cause now.verification_time to
  // be less than expiration_verification_time, thus treating the cached
  // result as an expired entry and re-verifying.
  // If the user receives either of those messages, and does not adjust their
  // clock, then the result will be (typically) be cached until the expiration
  // TTL.
  //
  // This algorithm is only problematic if the user consistently keeps
  // adjusting their clock backwards in increments smaller than the expiration
  // TTL, in which case, cached elements continue to be added. However,
  // because the cache has a fixed upper bound, if no entries are expired, a
  // 'random' entry will be, thus keeping the memory constraints bounded over
  // time.
  return now.verification_time >= expiration.verification_time &&
         now.verification_time < expiration.expiration_time;
};


// Represents the output and result callback of a request.
class CertVerifierRequest {
 public:
  CertVerifierRequest(const CompletionCallback& callback,
                      CertVerifyResult* verify_result,
                      const BoundNetLog& net_log)
      : callback_(callback),
        verify_result_(verify_result),
        net_log_(net_log) {
    net_log_.BeginEvent(NetLog::TYPE_CERT_VERIFIER_REQUEST);
  }

  ~CertVerifierRequest() {
  }

  // Ensures that the result callback will never be made.
  void Cancel() {
    callback_.Reset();
    verify_result_ = NULL;
    net_log_.AddEvent(NetLog::TYPE_CANCELLED);
    net_log_.EndEvent(NetLog::TYPE_CERT_VERIFIER_REQUEST);
  }

  // Copies the contents of |verify_result| to the caller's
  // CertVerifyResult and calls the callback.
  void Post(const MultiThreadedCertVerifier::CachedResult& verify_result) {
    if (!callback_.is_null()) {
      net_log_.EndEvent(NetLog::TYPE_CERT_VERIFIER_REQUEST);
      *verify_result_ = verify_result.result;
      callback_.Run(verify_result.error);
    }
    delete this;
  }

  bool canceled() const { return callback_.is_null(); }

  const BoundNetLog& net_log() const { return net_log_; }

 private:
  CompletionCallback callback_;
  CertVerifyResult* verify_result_;
  const BoundNetLog net_log_;
};


// CertVerifierWorker runs on a worker thread and takes care of the blocking
// process of performing the certificate verification.  Deletes itself
// eventually if Start() succeeds.
class CertVerifierWorker {
 public:
  CertVerifierWorker(CertVerifyProc* verify_proc,
                     X509Certificate* cert,
                     const std::string& hostname,
                     int flags,
                     CRLSet* crl_set,
                     const CertificateList& additional_trust_anchors,
                     MultiThreadedCertVerifier* cert_verifier)
      : verify_proc_(verify_proc),
        cert_(cert),
        hostname_(hostname),
        flags_(flags),
        crl_set_(crl_set),
        additional_trust_anchors_(additional_trust_anchors),
        origin_loop_(base::MessageLoop::current()),
        cert_verifier_(cert_verifier),
        canceled_(false),
        error_(ERR_FAILED) {
  }

  // Returns the certificate being verified. May only be called /before/
  // Start() is called.
  X509Certificate* certificate() const { return cert_.get(); }

  bool Start() {
    DCHECK_EQ(base::MessageLoop::current(), origin_loop_);

    return base::WorkerPool::PostTask(
        FROM_HERE, base::Bind(&CertVerifierWorker::Run, base::Unretained(this)),
        true /* task is slow */);
  }

  // Cancel is called from the origin loop when the MultiThreadedCertVerifier is
  // getting deleted.
  void Cancel() {
    DCHECK_EQ(base::MessageLoop::current(), origin_loop_);
    base::AutoLock locked(lock_);
    canceled_ = true;
  }

 private:
  void Run() {
    // Runs on a worker thread.
    error_ = verify_proc_->Verify(cert_.get(),
                                  hostname_,
                                  flags_,
                                  crl_set_.get(),
                                  additional_trust_anchors_,
                                  &verify_result_);
#if defined(USE_NSS) || defined(OS_IOS)
    // Detach the thread from NSPR.
    // Calling NSS functions attaches the thread to NSPR, which stores
    // the NSPR thread ID in thread-specific data.
    // The threads in our thread pool terminate after we have called
    // PR_Cleanup.  Unless we detach them from NSPR, net_unittests gets
    // segfaults on shutdown when the threads' thread-specific data
    // destructors run.
    PR_DetachThread();
#endif
    Finish();
  }

  // DoReply runs on the origin thread.
  void DoReply() {
    DCHECK_EQ(base::MessageLoop::current(), origin_loop_);
    {
      // We lock here because the worker thread could still be in Finished,
      // after the PostTask, but before unlocking |lock_|. If we do not lock in
      // this case, we will end up deleting a locked Lock, which can lead to
      // memory leaks or worse errors.
      base::AutoLock locked(lock_);
      if (!canceled_) {
        cert_verifier_->HandleResult(cert_.get(),
                                     hostname_,
                                     flags_,
                                     additional_trust_anchors_,
                                     error_,
                                     verify_result_);
      }
    }
    delete this;
  }

  void Finish() {
    // Runs on the worker thread.
    // We assume that the origin loop outlives the MultiThreadedCertVerifier. If
    // the MultiThreadedCertVerifier is deleted, it will call Cancel on us. If
    // it does so before the Acquire, we'll delete ourselves and return. If it's
    // trying to do so concurrently, then it'll block on the lock and we'll call
    // PostTask while the MultiThreadedCertVerifier (and therefore the
    // MessageLoop) is still alive.
    // If it does so after this function, we assume that the MessageLoop will
    // process pending tasks. In which case we'll notice the |canceled_| flag
    // in DoReply.

    bool canceled;
    {
      base::AutoLock locked(lock_);
      canceled = canceled_;
      if (!canceled) {
        origin_loop_->PostTask(
            FROM_HERE, base::Bind(
                &CertVerifierWorker::DoReply, base::Unretained(this)));
      }
    }

    if (canceled)
      delete this;
  }

  scoped_refptr<CertVerifyProc> verify_proc_;
  scoped_refptr<X509Certificate> cert_;
  const std::string hostname_;
  const int flags_;
  scoped_refptr<CRLSet> crl_set_;
  const CertificateList additional_trust_anchors_;
  base::MessageLoop* const origin_loop_;
  MultiThreadedCertVerifier* const cert_verifier_;

  // lock_ protects canceled_.
  base::Lock lock_;

  // If canceled_ is true,
  // * origin_loop_ cannot be accessed by the worker thread,
  // * cert_verifier_ cannot be accessed by any thread.
  bool canceled_;

  int error_;
  CertVerifyResult verify_result_;

  DISALLOW_COPY_AND_ASSIGN(CertVerifierWorker);
};

// A CertVerifierJob is a one-to-one counterpart of a CertVerifierWorker. It
// lives only on the CertVerifier's origin message loop.
class CertVerifierJob {
 public:
  CertVerifierJob(CertVerifierWorker* worker,
                  const BoundNetLog& net_log)
      : start_time_(base::TimeTicks::Now()),
        worker_(worker),
        net_log_(net_log) {
    net_log_.BeginEvent(
        NetLog::TYPE_CERT_VERIFIER_JOB,
        base::Bind(&NetLogX509CertificateCallback,
                   base::Unretained(worker_->certificate())));
  }

  ~CertVerifierJob() {
    if (worker_) {
      net_log_.AddEvent(NetLog::TYPE_CANCELLED);
      net_log_.EndEvent(NetLog::TYPE_CERT_VERIFIER_JOB);
      worker_->Cancel();
      DeleteAllCanceled();
    }
  }

  void AddRequest(CertVerifierRequest* request) {
    request->net_log().AddEvent(
        NetLog::TYPE_CERT_VERIFIER_REQUEST_BOUND_TO_JOB,
        net_log_.source().ToEventParametersCallback());

    requests_.push_back(request);
  }

  void HandleResult(
      const MultiThreadedCertVerifier::CachedResult& verify_result,
      bool is_first_job) {
    worker_ = NULL;
    net_log_.EndEvent(
        NetLog::TYPE_CERT_VERIFIER_JOB,
        base::Bind(&CertVerifyResultCallback, verify_result.result));
    base::TimeDelta latency = base::TimeTicks::Now() - start_time_;
    UMA_HISTOGRAM_CUSTOM_TIMES("Net.CertVerifier_Job_Latency",
                               latency,
                               base::TimeDelta::FromMilliseconds(1),
                               base::TimeDelta::FromMinutes(10),
                               100);
    if (is_first_job) {
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.CertVerifier_First_Job_Latency",
                                 latency,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(10),
                                 100);
    }
    PostAll(verify_result);
  }

 private:
  void PostAll(const MultiThreadedCertVerifier::CachedResult& verify_result) {
    std::vector<CertVerifierRequest*> requests;
    requests_.swap(requests);

    for (std::vector<CertVerifierRequest*>::iterator
         i = requests.begin(); i != requests.end(); i++) {
      (*i)->Post(verify_result);
      // Post() causes the CertVerifierRequest to delete itself.
    }
  }

  void DeleteAllCanceled() {
    for (std::vector<CertVerifierRequest*>::iterator
         i = requests_.begin(); i != requests_.end(); i++) {
      if ((*i)->canceled()) {
        delete *i;
      } else {
        LOG(DFATAL) << "CertVerifierRequest leaked!";
      }
    }
  }

  const base::TimeTicks start_time_;
  std::vector<CertVerifierRequest*> requests_;
  CertVerifierWorker* worker_;
  const BoundNetLog net_log_;
};

MultiThreadedCertVerifier::MultiThreadedCertVerifier(
    CertVerifyProc* verify_proc)
    : cache_(kMaxCacheEntries),
      first_job_(NULL),
      requests_(0),
      cache_hits_(0),
      inflight_joins_(0),
      verify_proc_(verify_proc),
      trust_anchor_provider_(NULL) {
  CertDatabase::GetInstance()->AddObserver(this);
}

MultiThreadedCertVerifier::~MultiThreadedCertVerifier() {
  STLDeleteValues(&inflight_);
  CertDatabase::GetInstance()->RemoveObserver(this);
}

void MultiThreadedCertVerifier::SetCertTrustAnchorProvider(
    CertTrustAnchorProvider* trust_anchor_provider) {
  DCHECK(CalledOnValidThread());
  trust_anchor_provider_ = trust_anchor_provider;
}

int MultiThreadedCertVerifier::Verify(X509Certificate* cert,
                                      const std::string& hostname,
                                      int flags,
                                      CRLSet* crl_set,
                                      CertVerifyResult* verify_result,
                                      const CompletionCallback& callback,
                                      RequestHandle* out_req,
                                      const BoundNetLog& net_log) {
  DCHECK(CalledOnValidThread());

  if (callback.is_null() || !verify_result || hostname.empty()) {
    *out_req = NULL;
    return ERR_INVALID_ARGUMENT;
  }

  requests_++;

  const CertificateList empty_cert_list;
  const CertificateList& additional_trust_anchors =
      trust_anchor_provider_ ?
          trust_anchor_provider_->GetAdditionalTrustAnchors() : empty_cert_list;

  const RequestParams key(cert->fingerprint(), cert->ca_fingerprint(),
                          hostname, flags, additional_trust_anchors);
  const CertVerifierCache::value_type* cached_entry =
      cache_.Get(key, CacheValidityPeriod(base::Time::Now()));
  if (cached_entry) {
    ++cache_hits_;
    *out_req = NULL;
    *verify_result = cached_entry->result;
    return cached_entry->error;
  }

  // No cache hit. See if an identical request is currently in flight.
  CertVerifierJob* job;
  std::map<RequestParams, CertVerifierJob*>::const_iterator j;
  j = inflight_.find(key);
  if (j != inflight_.end()) {
    // An identical request is in flight already. We'll just attach our
    // callback.
    inflight_joins_++;
    job = j->second;
  } else {
    // Need to make a new request.
    CertVerifierWorker* worker =
        new CertVerifierWorker(verify_proc_.get(),
                               cert,
                               hostname,
                               flags,
                               crl_set,
                               additional_trust_anchors,
                               this);
    job = new CertVerifierJob(
        worker,
        BoundNetLog::Make(net_log.net_log(), NetLog::SOURCE_CERT_VERIFIER_JOB));
    if (!worker->Start()) {
      delete job;
      delete worker;
      *out_req = NULL;
      // TODO(wtc): log to the NetLog.
      LOG(ERROR) << "CertVerifierWorker couldn't be started.";
      return ERR_INSUFFICIENT_RESOURCES;  // Just a guess.
    }
    inflight_.insert(std::make_pair(key, job));
    if (requests_ == 1) {
      // Cleared in HandleResult.
      first_job_ = job;
    }
  }

  CertVerifierRequest* request =
      new CertVerifierRequest(callback, verify_result, net_log);
  job->AddRequest(request);
  *out_req = request;
  return ERR_IO_PENDING;
}

void MultiThreadedCertVerifier::CancelRequest(RequestHandle req) {
  DCHECK(CalledOnValidThread());
  CertVerifierRequest* request = reinterpret_cast<CertVerifierRequest*>(req);
  request->Cancel();
}

MultiThreadedCertVerifier::RequestParams::RequestParams(
    const SHA1HashValue& cert_fingerprint_arg,
    const SHA1HashValue& ca_fingerprint_arg,
    const std::string& hostname_arg,
    int flags_arg,
    const CertificateList& additional_trust_anchors)
    : hostname(hostname_arg),
      flags(flags_arg) {
  hash_values.reserve(2 + additional_trust_anchors.size());
  hash_values.push_back(cert_fingerprint_arg);
  hash_values.push_back(ca_fingerprint_arg);
  for (size_t i = 0; i < additional_trust_anchors.size(); ++i)
    hash_values.push_back(additional_trust_anchors[i]->fingerprint());
}

MultiThreadedCertVerifier::RequestParams::~RequestParams() {}

bool MultiThreadedCertVerifier::RequestParams::operator<(
    const RequestParams& other) const {
  // |flags| is compared before |cert_fingerprint|, |ca_fingerprint|, and
  // |hostname| under assumption that integer comparisons are faster than
  // memory and string comparisons.
  if (flags != other.flags)
    return flags < other.flags;
  if (hostname != other.hostname)
    return hostname < other.hostname;
  return std::lexicographical_compare(
      hash_values.begin(), hash_values.end(),
      other.hash_values.begin(), other.hash_values.end(),
      net::SHA1HashValueLessThan());
}

// HandleResult is called by CertVerifierWorker on the origin message loop.
// It deletes CertVerifierJob.
void MultiThreadedCertVerifier::HandleResult(
    X509Certificate* cert,
    const std::string& hostname,
    int flags,
    const CertificateList& additional_trust_anchors,
    int error,
    const CertVerifyResult& verify_result) {
  DCHECK(CalledOnValidThread());

  const RequestParams key(cert->fingerprint(), cert->ca_fingerprint(),
                          hostname, flags, additional_trust_anchors);

  CachedResult cached_result;
  cached_result.error = error;
  cached_result.result = verify_result;
  base::Time now = base::Time::Now();
  cache_.Put(
      key, cached_result, CacheValidityPeriod(now),
      CacheValidityPeriod(now, now + base::TimeDelta::FromSeconds(kTTLSecs)));

  std::map<RequestParams, CertVerifierJob*>::iterator j;
  j = inflight_.find(key);
  if (j == inflight_.end()) {
    NOTREACHED();
    return;
  }
  CertVerifierJob* job = j->second;
  inflight_.erase(j);
  bool is_first_job = false;
  if (first_job_ == job) {
    is_first_job = true;
    first_job_ = NULL;
  }

  job->HandleResult(cached_result, is_first_job);
  delete job;
}

void MultiThreadedCertVerifier::OnCACertChanged(
    const X509Certificate* cert) {
  DCHECK(CalledOnValidThread());

  ClearCache();
}

}  // namespace net

