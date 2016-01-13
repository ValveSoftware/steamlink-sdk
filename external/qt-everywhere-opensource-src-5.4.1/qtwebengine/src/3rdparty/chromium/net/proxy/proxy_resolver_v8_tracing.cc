// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_resolver_v8_tracing.h"

#include "base/bind.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/dns/host_resolver.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_resolver_error_observer.h"
#include "net/proxy/proxy_resolver_v8.h"

// The intent of this class is explained in the design document:
// https://docs.google.com/a/chromium.org/document/d/16Ij5OcVnR3s0MH4Z5XkhI9VTPoMJdaBn9rKreAmGOdE/edit
//
// In a nutshell, PAC scripts are Javascript programs and may depend on
// network I/O, by calling functions like dnsResolve().
//
// This is problematic since functions such as dnsResolve() will block the
// Javascript execution until the DNS result is availble, thereby stalling the
// PAC thread, which hurts the ability to process parallel proxy resolves.
// An obvious solution is to simply start more PAC threads, however this scales
// poorly, which hurts the ability to process parallel proxy resolves.
//
// The solution in ProxyResolverV8Tracing is to model PAC scripts as being
// deterministic, and depending only on the inputted URL. When the script
// issues a dnsResolve() for a yet unresolved hostname, the Javascript
// execution is "aborted", and then re-started once the DNS result is
// known.
namespace net {

namespace {

// Upper bound on how many *unique* DNS resolves a PAC script is allowed
// to make. This is a failsafe both for scripts that do a ridiculous
// number of DNS resolves, as well as scripts which are misbehaving
// under the tracing optimization. It is not expected to hit this normally.
const size_t kMaxUniqueResolveDnsPerExec = 20;

// Approximate number of bytes to use for buffering alerts() and errors.
// This is a failsafe in case repeated executions of the script causes
// too much memory bloat. It is not expected for well behaved scripts to
// hit this. (In fact normal scripts should not even have alerts() or errors).
const size_t kMaxAlertsAndErrorsBytes = 2048;

// Returns event parameters for a PAC error message (line number + message).
base::Value* NetLogErrorCallback(int line_number,
                                 const base::string16* message,
                                 NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetInteger("line_number", line_number);
  dict->SetString("message", *message);
  return dict;
}

void IncrementWithoutOverflow(uint8* x) {
  if (*x != 0xFF)
    *x += 1;
}

}  // namespace

// The Job class is responsible for executing GetProxyForURL() and
// SetPacScript(), since both of these operations share similar code.
//
// The DNS for these operations can operate in either blocking or
// non-blocking mode. Blocking mode is used as a fallback when the PAC script
// seems to be misbehaving under the tracing optimization.
//
// Note that this class runs on both the origin thread and a worker
// thread. Most methods are expected to be used exclusively on one thread
// or the other.
//
// The lifetime of Jobs does not exceed that of the ProxyResolverV8Tracing that
// spawned it. Destruction might happen on either the origin thread or the
// worker thread.
class ProxyResolverV8Tracing::Job
    : public base::RefCountedThreadSafe<ProxyResolverV8Tracing::Job>,
      public ProxyResolverV8::JSBindings {
 public:
  // |parent| is non-owned. It is the ProxyResolverV8Tracing that spawned this
  // Job, and must oulive it.
  explicit Job(ProxyResolverV8Tracing* parent);

  // Called from origin thread.
  void StartSetPacScript(
      const scoped_refptr<ProxyResolverScriptData>& script_data,
      const CompletionCallback& callback);

  // Called from origin thread.
  void StartGetProxyForURL(const GURL& url,
                           ProxyInfo* results,
                           const BoundNetLog& net_log,
                           const CompletionCallback& callback);

  // Called from origin thread.
  void Cancel();

  // Called from origin thread.
  LoadState GetLoadState() const;

 private:
  typedef std::map<std::string, std::string> DnsCache;
  friend class base::RefCountedThreadSafe<ProxyResolverV8Tracing::Job>;

  enum Operation {
    SET_PAC_SCRIPT,
    GET_PROXY_FOR_URL,
  };

  struct AlertOrError {
    bool is_alert;
    int line_number;
    base::string16 message;
  };

  virtual ~Job();

  void CheckIsOnWorkerThread() const;
  void CheckIsOnOriginThread() const;

  void SetCallback(const CompletionCallback& callback);
  void ReleaseCallback();

  ProxyResolverV8* v8_resolver();
  base::MessageLoop* worker_loop();
  HostResolver* host_resolver();
  ProxyResolverErrorObserver* error_observer();
  NetLog* net_log();

  // Invokes the user's callback.
  void NotifyCaller(int result);
  void NotifyCallerOnOriginLoop(int result);

  void RecordMetrics() const;

  void Start(Operation op, bool blocking_dns,
             const CompletionCallback& callback);

  void ExecuteBlocking();
  void ExecuteNonBlocking();
  int ExecuteProxyResolver();

  // Implementation of ProxyResolverv8::JSBindings
  virtual bool ResolveDns(const std::string& host,
                          ResolveDnsOperation op,
                          std::string* output,
                          bool* terminate) OVERRIDE;
  virtual void Alert(const base::string16& message) OVERRIDE;
  virtual void OnError(int line_number, const base::string16& error) OVERRIDE;

  bool ResolveDnsBlocking(const std::string& host,
                          ResolveDnsOperation op,
                          std::string* output);

  bool ResolveDnsNonBlocking(const std::string& host,
                             ResolveDnsOperation op,
                             std::string* output,
                             bool* terminate);

  bool PostDnsOperationAndWait(const std::string& host,
                               ResolveDnsOperation op,
                               bool* completed_synchronously)
                               WARN_UNUSED_RESULT;

  void DoDnsOperation();
  void OnDnsOperationComplete(int result);

  void ScheduleRestartWithBlockingDns();

  bool GetDnsFromLocalCache(const std::string& host, ResolveDnsOperation op,
                            std::string* output, bool* return_value);

  void SaveDnsToLocalCache(const std::string& host, ResolveDnsOperation op,
                           int net_error, const net::AddressList& addresses);

  // Builds a RequestInfo to service the specified PAC DNS operation.
  static HostResolver::RequestInfo MakeDnsRequestInfo(const std::string& host,
                                                      ResolveDnsOperation op);

  // Makes a key for looking up |host, op| in |dns_cache_|. Strings are used for
  // convenience, to avoid defining custom comparators.
  static std::string MakeDnsCacheKey(const std::string& host,
                                     ResolveDnsOperation op);

  void HandleAlertOrError(bool is_alert, int line_number,
                          const base::string16& message);
  void DispatchBufferedAlertsAndErrors();
  void DispatchAlertOrError(bool is_alert, int line_number,
                            const base::string16& message);

  void LogEventToCurrentRequestAndGlobally(
      NetLog::EventType type,
      const NetLog::ParametersCallback& parameters_callback);

  // The thread which called into ProxyResolverV8Tracing, and on which the
  // completion callback is expected to run.
  scoped_refptr<base::MessageLoopProxy> origin_loop_;

  // The ProxyResolverV8Tracing which spawned this Job.
  // Initialized on origin thread and then accessed from both threads.
  ProxyResolverV8Tracing* parent_;

  // The callback to run (on the origin thread) when the Job finishes.
  // Should only be accessed from origin thread.
  CompletionCallback callback_;

  // Flag to indicate whether the request has been cancelled.
  base::CancellationFlag cancelled_;

  // The operation that this Job is running.
  // Initialized on origin thread and then accessed from both threads.
  Operation operation_;

  // The DNS mode for this Job.
  // Initialized on origin thread, mutated on worker thread, and accessed
  // by both the origin thread and worker thread.
  bool blocking_dns_;

  // Used to block the worker thread on a DNS operation taking place on the
  // origin thread.
  base::WaitableEvent event_;

  // Map of DNS operations completed so far. Written into on the origin thread
  // and read on the worker thread.
  DnsCache dns_cache_;

  // The job holds a reference to itself to ensure that it remains alive until
  // either completion or cancellation.
  scoped_refptr<Job> owned_self_reference_;

  // -------------------------------------------------------
  // State specific to SET_PAC_SCRIPT.
  // -------------------------------------------------------

  scoped_refptr<ProxyResolverScriptData> script_data_;

  // -------------------------------------------------------
  // State specific to GET_PROXY_FOR_URL.
  // -------------------------------------------------------

  ProxyInfo* user_results_;  // Owned by caller, lives on origin thread.
  GURL url_;
  ProxyInfo results_;
  BoundNetLog bound_net_log_;

  // ---------------------------------------------------------------------------
  // State for ExecuteNonBlocking()
  // ---------------------------------------------------------------------------
  // These variables are used exclusively on the worker thread and are only
  // meaningful when executing inside of ExecuteNonBlocking().

  // Whether this execution was abandoned due to a missing DNS dependency.
  bool abandoned_;

  // Number of calls made to ResolveDns() by this execution.
  int num_dns_;

  // Sequence of calls made to Alert() or OnError() by this execution.
  std::vector<AlertOrError> alerts_and_errors_;
  size_t alerts_and_errors_byte_cost_;  // Approximate byte cost of the above.

  // Number of calls made to ResolveDns() by the PREVIOUS execution.
  int last_num_dns_;

  // Whether the current execution needs to be restarted in blocking mode.
  bool should_restart_with_blocking_dns_;

  // ---------------------------------------------------------------------------
  // State for pending DNS request.
  // ---------------------------------------------------------------------------

  // Handle to the outstanding request in the HostResolver, or NULL.
  // This is mutated and used on the origin thread, however it may be read by
  // the worker thread for some DCHECKS().
  HostResolver::RequestHandle pending_dns_;

  // Indicates if the outstanding DNS request completed synchronously. Written
  // on the origin thread, and read by the worker thread.
  bool pending_dns_completed_synchronously_;

  // These are the inputs to DoDnsOperation(). Written on the worker thread,
  // read by the origin thread.
  std::string pending_dns_host_;
  ResolveDnsOperation pending_dns_op_;

  // This contains the resolved address list that DoDnsOperation() fills in.
  // Used exclusively on the origin thread.
  AddressList pending_dns_addresses_;

  // ---------------------------------------------------------------------------
  // Metrics for histograms
  // ---------------------------------------------------------------------------
  // These values are used solely for logging histograms. They do not affect
  // the execution flow of requests.

  // The time when the proxy resolve request started. Used exclusively on the
  // origin thread.
  base::TimeTicks metrics_start_time_;

  // The time when the proxy resolve request completes on the worker thread.
  // Written on the worker thread, read on the origin thread.
  base::TimeTicks metrics_end_time_;

  // The time when PostDnsOperationAndWait() was called. Written on the worker
  // thread, read by the origin thread.
  base::TimeTicks metrics_pending_dns_start_;

  // The total amount of time that has been spent by the script waiting for
  // DNS dependencies. This includes the time spent posting the task to
  // the origin thread, up until the DNS result is found on the origin
  // thread. It does not include any time spent waiting in the message loop
  // for the worker thread, nor any time restarting or executing the
  // script. Used exclusively on the origin thread.
  base::TimeDelta metrics_dns_total_time_;

  // The following variables are initialized on the origin thread,
  // incremented on the worker thread, and then read upon completion on the
  // origin thread. The values are not expected to exceed the range of a uint8.
  // If they do, then they will be clamped to 0xFF.
  uint8 metrics_num_executions_;
  uint8 metrics_num_unique_dns_;
  uint8 metrics_num_alerts_;
  uint8 metrics_num_errors_;

  // The time that the latest execution took (time spent inside of
  // ExecuteProxyResolver(), which includes time spent in bindings too).
  // Written on the worker thread, read on the origin thread.
  base::TimeDelta metrics_execution_time_;

  // The cumulative time spent in ExecuteProxyResolver() that was ultimately
  // discarded work.
  // Written on the worker thread, read on the origin thread.
  base::TimeDelta metrics_abandoned_execution_total_time_;

  // The duration that the worker thread was blocked waiting on DNS results from
  // the origin thread, when operating in nonblocking mode.
  // Written on the worker thread, read on the origin thread.
  base::TimeDelta metrics_nonblocking_dns_wait_total_time_;
};

ProxyResolverV8Tracing::Job::Job(ProxyResolverV8Tracing* parent)
    : origin_loop_(base::MessageLoopProxy::current()),
      parent_(parent),
      event_(true, false),
      last_num_dns_(0),
      pending_dns_(NULL),
      metrics_num_executions_(0),
      metrics_num_unique_dns_(0),
      metrics_num_alerts_(0),
      metrics_num_errors_(0) {
  CheckIsOnOriginThread();
}

void ProxyResolverV8Tracing::Job::StartSetPacScript(
    const scoped_refptr<ProxyResolverScriptData>& script_data,
    const CompletionCallback& callback) {
  CheckIsOnOriginThread();

  script_data_ = script_data;

  // Script initialization uses blocking DNS since there isn't any
  // advantage to using non-blocking mode here. That is because the
  // parent ProxyService can't submit any ProxyResolve requests until
  // initialization has completed successfully!
  Start(SET_PAC_SCRIPT, true /*blocking*/, callback);
}

void ProxyResolverV8Tracing::Job::StartGetProxyForURL(
    const GURL& url,
    ProxyInfo* results,
    const BoundNetLog& net_log,
    const CompletionCallback& callback) {
  CheckIsOnOriginThread();

  url_ = url;
  user_results_ = results;
  bound_net_log_ = net_log;

  Start(GET_PROXY_FOR_URL, false /*non-blocking*/, callback);
}

void ProxyResolverV8Tracing::Job::Cancel() {
  CheckIsOnOriginThread();

  // There are several possibilities to consider for cancellation:
  // (a) The job has been posted to the worker thread, however script execution
  //     has not yet started.
  // (b) The script is executing on the worker thread.
  // (c) The script is executing on the worker thread, however is blocked inside
  //     of dnsResolve() waiting for a response from the origin thread.
  // (d) Nothing is running on the worker thread, however the host resolver has
  //     a pending DNS request which upon completion will restart the script
  //     execution.
  // (e) The worker thread has a pending task to restart execution, which was
  //     posted after the DNS dependency was resolved and saved to local cache.
  // (f) The script execution completed entirely, and posted a task to the
  //     origin thread to notify the caller.
  //
  // |cancelled_| is read on both the origin thread and worker thread. The
  // code that runs on the worker thread is littered with checks on
  // |cancelled_| to break out early.
  cancelled_.Set();

  ReleaseCallback();

  if (pending_dns_) {
    host_resolver()->CancelRequest(pending_dns_);
    pending_dns_ = NULL;
  }

  // The worker thread might be blocked waiting for DNS.
  event_.Signal();

  owned_self_reference_ = NULL;
}

LoadState ProxyResolverV8Tracing::Job::GetLoadState() const {
  CheckIsOnOriginThread();

  if (pending_dns_)
    return LOAD_STATE_RESOLVING_HOST_IN_PROXY_SCRIPT;

  return LOAD_STATE_RESOLVING_PROXY_FOR_URL;
}

ProxyResolverV8Tracing::Job::~Job() {
  DCHECK(!pending_dns_);
  DCHECK(callback_.is_null());
}

void ProxyResolverV8Tracing::Job::CheckIsOnWorkerThread() const {
  DCHECK_EQ(base::MessageLoop::current(), parent_->thread_->message_loop());
}

void ProxyResolverV8Tracing::Job::CheckIsOnOriginThread() const {
  DCHECK(origin_loop_->BelongsToCurrentThread());
}

void ProxyResolverV8Tracing::Job::SetCallback(
    const CompletionCallback& callback) {
  CheckIsOnOriginThread();
  DCHECK(callback_.is_null());
  parent_->num_outstanding_callbacks_++;
  callback_ = callback;
}

void ProxyResolverV8Tracing::Job::ReleaseCallback() {
  CheckIsOnOriginThread();
  DCHECK(!callback_.is_null());
  CHECK_GT(parent_->num_outstanding_callbacks_, 0);
  parent_->num_outstanding_callbacks_--;
  callback_.Reset();

  // For good measure, clear this other user-owned pointer.
  user_results_ = NULL;
}

ProxyResolverV8* ProxyResolverV8Tracing::Job::v8_resolver() {
  return parent_->v8_resolver_.get();
}

base::MessageLoop* ProxyResolverV8Tracing::Job::worker_loop() {
  return parent_->thread_->message_loop();
}

HostResolver* ProxyResolverV8Tracing::Job::host_resolver() {
  return parent_->host_resolver_;
}

ProxyResolverErrorObserver* ProxyResolverV8Tracing::Job::error_observer() {
  return parent_->error_observer_.get();
}

NetLog* ProxyResolverV8Tracing::Job::net_log() {
  return parent_->net_log_;
}

void ProxyResolverV8Tracing::Job::NotifyCaller(int result) {
  CheckIsOnWorkerThread();

  metrics_end_time_ = base::TimeTicks::Now();

  origin_loop_->PostTask(
      FROM_HERE,
      base::Bind(&Job::NotifyCallerOnOriginLoop, this, result));
}

void ProxyResolverV8Tracing::Job::NotifyCallerOnOriginLoop(int result) {
  CheckIsOnOriginThread();

  if (cancelled_.IsSet())
    return;

  DCHECK(!callback_.is_null());
  DCHECK(!pending_dns_);

  if (operation_ == GET_PROXY_FOR_URL) {
    RecordMetrics();
    *user_results_ = results_;
  }

  // There is only ever 1 outstanding SET_PAC_SCRIPT job. It needs to be
  // tracked to support cancellation.
  if (operation_ == SET_PAC_SCRIPT) {
    DCHECK_EQ(parent_->set_pac_script_job_.get(), this);
    parent_->set_pac_script_job_ = NULL;
  }

  CompletionCallback callback = callback_;
  ReleaseCallback();
  callback.Run(result);

  owned_self_reference_ = NULL;
}

void ProxyResolverV8Tracing::Job::RecordMetrics() const {
  CheckIsOnOriginThread();
  DCHECK_EQ(GET_PROXY_FOR_URL, operation_);

  base::TimeTicks now = base::TimeTicks::Now();

  // Metrics are output for each completed request to GetProxyForURL()).
  //
  // Note that a different set of histograms is used to record the metrics for
  // requests that completed in non-blocking mode versus blocking mode. The
  // expectation is for requests to complete in non-blocking mode each time.
  // If they don't then something strange is happening, and the purpose of the
  // seprate statistics is to better understand that trend.
#define UPDATE_HISTOGRAMS(base_name) \
  do {\
  UMA_HISTOGRAM_MEDIUM_TIMES(base_name "TotalTime", now - metrics_start_time_);\
  UMA_HISTOGRAM_MEDIUM_TIMES(base_name "TotalTimeWorkerThread",\
                             metrics_end_time_ - metrics_start_time_);\
  UMA_HISTOGRAM_TIMES(base_name "OriginThreadLatency",\
                      now - metrics_end_time_);\
  UMA_HISTOGRAM_MEDIUM_TIMES(base_name "TotalTimeDNS",\
                             metrics_dns_total_time_);\
  UMA_HISTOGRAM_MEDIUM_TIMES(base_name "ExecutionTime",\
                             metrics_execution_time_);\
  UMA_HISTOGRAM_MEDIUM_TIMES(base_name "AbandonedExecutionTotalTime",\
                             metrics_abandoned_execution_total_time_);\
  UMA_HISTOGRAM_MEDIUM_TIMES(base_name "DnsWaitTotalTime",\
                             metrics_nonblocking_dns_wait_total_time_);\
  UMA_HISTOGRAM_CUSTOM_COUNTS(\
      base_name "NumRestarts", metrics_num_executions_ - 1,\
      1, kMaxUniqueResolveDnsPerExec, kMaxUniqueResolveDnsPerExec);\
  UMA_HISTOGRAM_CUSTOM_COUNTS(\
      base_name "UniqueDNS", metrics_num_unique_dns_,\
      1, kMaxUniqueResolveDnsPerExec, kMaxUniqueResolveDnsPerExec);\
  UMA_HISTOGRAM_COUNTS_100(base_name "NumAlerts", metrics_num_alerts_);\
  UMA_HISTOGRAM_CUSTOM_COUNTS(\
      base_name "NumErrors", metrics_num_errors_, 1, 10, 10);\
  } while (false)

  if (!blocking_dns_)
    UPDATE_HISTOGRAMS("Net.ProxyResolver.");
  else
    UPDATE_HISTOGRAMS("Net.ProxyResolver.BlockingDNSMode.");

#undef UPDATE_HISTOGRAMS

  // Histograms to better understand http://crbug.com/240536 -- long
  // URLs can cause a significant slowdown in PAC execution. Figure out how
  // severe this is in the wild.
  if (!blocking_dns_) {
    size_t url_size = url_.spec().size();

    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Net.ProxyResolver.URLSize", url_size, 1, 200000, 50);

    if (url_size > 2048) {
      UMA_HISTOGRAM_MEDIUM_TIMES("Net.ProxyResolver.ExecutionTime_UrlOver2K",
                                 metrics_execution_time_);
    }

    if (url_size > 4096) {
      UMA_HISTOGRAM_MEDIUM_TIMES("Net.ProxyResolver.ExecutionTime_UrlOver4K",
                                 metrics_execution_time_);
    }

    if (url_size > 8192) {
      UMA_HISTOGRAM_MEDIUM_TIMES("Net.ProxyResolver.ExecutionTime_UrlOver8K",
                                 metrics_execution_time_);
    }

    if (url_size > 131072) {
      UMA_HISTOGRAM_MEDIUM_TIMES("Net.ProxyResolver.ExecutionTime_UrlOver128K",
                                 metrics_execution_time_);
    }
  }
}


void ProxyResolverV8Tracing::Job::Start(Operation op, bool blocking_dns,
                                        const CompletionCallback& callback) {
  CheckIsOnOriginThread();

  metrics_start_time_ = base::TimeTicks::Now();
  operation_ = op;
  blocking_dns_ = blocking_dns;
  SetCallback(callback);

  owned_self_reference_ = this;

  worker_loop()->PostTask(FROM_HERE,
      blocking_dns_ ? base::Bind(&Job::ExecuteBlocking, this) :
                      base::Bind(&Job::ExecuteNonBlocking, this));
}

void ProxyResolverV8Tracing::Job::ExecuteBlocking() {
  CheckIsOnWorkerThread();
  DCHECK(blocking_dns_);

  if (cancelled_.IsSet())
    return;

  NotifyCaller(ExecuteProxyResolver());
}

void ProxyResolverV8Tracing::Job::ExecuteNonBlocking() {
  CheckIsOnWorkerThread();
  DCHECK(!blocking_dns_);

  if (cancelled_.IsSet())
    return;

  // Reset state for the current execution.
  abandoned_ = false;
  num_dns_ = 0;
  alerts_and_errors_.clear();
  alerts_and_errors_byte_cost_ = 0;
  should_restart_with_blocking_dns_ = false;

  int result = ExecuteProxyResolver();

  if (abandoned_)
    metrics_abandoned_execution_total_time_ += metrics_execution_time_;

  if (should_restart_with_blocking_dns_) {
    DCHECK(!blocking_dns_);
    DCHECK(abandoned_);
    blocking_dns_ = true;
    ExecuteBlocking();
    return;
  }

  if (abandoned_)
    return;

  DispatchBufferedAlertsAndErrors();
  NotifyCaller(result);
}

int ProxyResolverV8Tracing::Job::ExecuteProxyResolver() {
  IncrementWithoutOverflow(&metrics_num_executions_);

  base::TimeTicks start = base::TimeTicks::Now();

  JSBindings* prev_bindings = v8_resolver()->js_bindings();
  v8_resolver()->set_js_bindings(this);

  int result = ERR_UNEXPECTED;  // Initialized to silence warnings.

  switch (operation_) {
    case SET_PAC_SCRIPT:
      result = v8_resolver()->SetPacScript(
          script_data_, CompletionCallback());
      break;
    case GET_PROXY_FOR_URL:
      result = v8_resolver()->GetProxyForURL(
        url_,
        // Important: Do not write directly into |user_results_|, since if the
        // request were to be cancelled from the origin thread, must guarantee
        // that |user_results_| is not accessed anymore.
        &results_,
        CompletionCallback(),
        NULL,
        bound_net_log_);
      break;
  }

  v8_resolver()->set_js_bindings(prev_bindings);

  metrics_execution_time_ = base::TimeTicks::Now() - start;

  return result;
}

bool ProxyResolverV8Tracing::Job::ResolveDns(const std::string& host,
                                             ResolveDnsOperation op,
                                             std::string* output,
                                             bool* terminate) {
  if (cancelled_.IsSet()) {
    *terminate = true;
    return false;
  }

  if ((op == DNS_RESOLVE || op == DNS_RESOLVE_EX) && host.empty()) {
    // a DNS resolve with an empty hostname is considered an error.
    return false;
  }

  return blocking_dns_ ?
      ResolveDnsBlocking(host, op, output) :
      ResolveDnsNonBlocking(host, op, output, terminate);
}

void ProxyResolverV8Tracing::Job::Alert(const base::string16& message) {
  HandleAlertOrError(true, -1, message);
}

void ProxyResolverV8Tracing::Job::OnError(int line_number,
                                          const base::string16& error) {
  HandleAlertOrError(false, line_number, error);
}

bool ProxyResolverV8Tracing::Job::ResolveDnsBlocking(const std::string& host,
                                                     ResolveDnsOperation op,
                                                     std::string* output) {
  CheckIsOnWorkerThread();

  // Check if the DNS result for this host has already been cached.
  bool rv;
  if (GetDnsFromLocalCache(host, op, output, &rv)) {
    // Yay, cache hit!
    return rv;
  }

  // If the host was not in the local cache, this is a new hostname.
  IncrementWithoutOverflow(&metrics_num_unique_dns_);

  if (dns_cache_.size() >= kMaxUniqueResolveDnsPerExec) {
    // Safety net for scripts with unexpectedly many DNS calls.
    // We will continue running to completion, but will fail every
    // subsequent DNS request.
    return false;
  }

  if (!PostDnsOperationAndWait(host, op, NULL))
    return false;  // Was cancelled.

  CHECK(GetDnsFromLocalCache(host, op, output, &rv));
  return rv;
}

bool ProxyResolverV8Tracing::Job::ResolveDnsNonBlocking(const std::string& host,
                                                        ResolveDnsOperation op,
                                                        std::string* output,
                                                        bool* terminate) {
  CheckIsOnWorkerThread();

  if (abandoned_) {
    // If this execution was already abandoned can fail right away. Only 1 DNS
    // dependency will be traced at a time (for more predictable outcomes).
    return false;
  }

  num_dns_ += 1;

  // Check if the DNS result for this host has already been cached.
  bool rv;
  if (GetDnsFromLocalCache(host, op, output, &rv)) {
    // Yay, cache hit!
    return rv;
  }

  // If the host was not in the local cache, then this is a new hostname.
  IncrementWithoutOverflow(&metrics_num_unique_dns_);

  if (num_dns_ <= last_num_dns_) {
    // The sequence of DNS operations is different from last time!
    ScheduleRestartWithBlockingDns();
    *terminate = true;
    return false;
  }

  if (dns_cache_.size() >= kMaxUniqueResolveDnsPerExec) {
    // Safety net for scripts with unexpectedly many DNS calls.
    return false;
  }

  DCHECK(!should_restart_with_blocking_dns_);

  bool completed_synchronously;
  if (!PostDnsOperationAndWait(host, op, &completed_synchronously))
    return false;  // Was cancelled.

  if (completed_synchronously) {
    CHECK(GetDnsFromLocalCache(host, op, output, &rv));
    return rv;
  }

  // Otherwise if the result was not in the cache, then a DNS request has
  // been started. Abandon this invocation of FindProxyForURL(), it will be
  // restarted once the DNS request completes.
  abandoned_ = true;
  *terminate = true;
  last_num_dns_ = num_dns_;
  return false;
}

bool ProxyResolverV8Tracing::Job::PostDnsOperationAndWait(
    const std::string& host, ResolveDnsOperation op,
    bool* completed_synchronously) {

  base::TimeTicks start = base::TimeTicks::Now();

  // Post the DNS request to the origin thread.
  DCHECK(!pending_dns_);
  metrics_pending_dns_start_ = base::TimeTicks::Now();
  pending_dns_host_ = host;
  pending_dns_op_ = op;
  origin_loop_->PostTask(FROM_HERE, base::Bind(&Job::DoDnsOperation, this));

  event_.Wait();
  event_.Reset();

  if (cancelled_.IsSet())
    return false;

  if (completed_synchronously)
    *completed_synchronously = pending_dns_completed_synchronously_;

  if (!blocking_dns_)
    metrics_nonblocking_dns_wait_total_time_ += base::TimeTicks::Now() - start;

  return true;
}

void ProxyResolverV8Tracing::Job::DoDnsOperation() {
  CheckIsOnOriginThread();
  DCHECK(!pending_dns_);

  if (cancelled_.IsSet())
    return;

  HostResolver::RequestHandle dns_request = NULL;
  int result = host_resolver()->Resolve(
      MakeDnsRequestInfo(pending_dns_host_, pending_dns_op_),
      DEFAULT_PRIORITY,
      &pending_dns_addresses_,
      base::Bind(&Job::OnDnsOperationComplete, this),
      &dns_request,
      bound_net_log_);

  pending_dns_completed_synchronously_ = result != ERR_IO_PENDING;

  // Check if the request was cancelled as a side-effect of calling into the
  // HostResolver. This isn't the ordinary execution flow, however it is
  // exercised by unit-tests.
  if (cancelled_.IsSet()) {
    if (!pending_dns_completed_synchronously_)
      host_resolver()->CancelRequest(dns_request);
    return;
  }

  if (pending_dns_completed_synchronously_) {
    OnDnsOperationComplete(result);
  } else {
    DCHECK(dns_request);
    pending_dns_ = dns_request;
    // OnDnsOperationComplete() will be called by host resolver on completion.
  }

  if (!blocking_dns_) {
    // The worker thread always blocks waiting to see if the result can be
    // serviced from cache before restarting.
    event_.Signal();
  }
}

void ProxyResolverV8Tracing::Job::OnDnsOperationComplete(int result) {
  CheckIsOnOriginThread();

  DCHECK(!cancelled_.IsSet());
  DCHECK(pending_dns_completed_synchronously_ == (pending_dns_ == NULL));

  SaveDnsToLocalCache(pending_dns_host_, pending_dns_op_, result,
                      pending_dns_addresses_);
  pending_dns_ = NULL;

  metrics_dns_total_time_ +=
      base::TimeTicks::Now() - metrics_pending_dns_start_;

  if (blocking_dns_) {
    event_.Signal();
    return;
  }

  if (!blocking_dns_ && !pending_dns_completed_synchronously_) {
    // Restart. This time it should make more progress due to having
    // cached items.
    worker_loop()->PostTask(FROM_HERE,
                            base::Bind(&Job::ExecuteNonBlocking, this));
  }
}

void ProxyResolverV8Tracing::Job::ScheduleRestartWithBlockingDns() {
  CheckIsOnWorkerThread();

  DCHECK(!should_restart_with_blocking_dns_);
  DCHECK(!abandoned_);
  DCHECK(!blocking_dns_);

  abandoned_ = true;

  // The restart will happen after ExecuteNonBlocking() finishes.
  should_restart_with_blocking_dns_ = true;
}

bool ProxyResolverV8Tracing::Job::GetDnsFromLocalCache(
    const std::string& host,
    ResolveDnsOperation op,
    std::string* output,
    bool* return_value) {
  CheckIsOnWorkerThread();

  DnsCache::const_iterator it = dns_cache_.find(MakeDnsCacheKey(host, op));
  if (it == dns_cache_.end())
    return false;

  *output = it->second;
  *return_value = !it->second.empty();
  return true;
}

void ProxyResolverV8Tracing::Job::SaveDnsToLocalCache(
    const std::string& host,
    ResolveDnsOperation op,
    int net_error,
    const net::AddressList& addresses) {
  CheckIsOnOriginThread();

  // Serialize the result into a string to save to the cache.
  std::string cache_value;
  if (net_error != OK) {
    cache_value = std::string();
  } else if (op == DNS_RESOLVE || op == MY_IP_ADDRESS) {
    // dnsResolve() and myIpAddress() are expected to return a single IP
    // address.
    cache_value = addresses.front().ToStringWithoutPort();
  } else {
    // The *Ex versions are expected to return a semi-colon separated list.
    for (AddressList::const_iterator iter = addresses.begin();
         iter != addresses.end(); ++iter) {
      if (!cache_value.empty())
        cache_value += ";";
      cache_value += iter->ToStringWithoutPort();
    }
  }

  dns_cache_[MakeDnsCacheKey(host, op)] = cache_value;
}

// static
HostResolver::RequestInfo ProxyResolverV8Tracing::Job::MakeDnsRequestInfo(
    const std::string& host, ResolveDnsOperation op) {
  HostPortPair host_port = HostPortPair(host, 80);
  if (op == MY_IP_ADDRESS || op == MY_IP_ADDRESS_EX) {
    host_port.set_host(GetHostName());
  }

  HostResolver::RequestInfo info(host_port);
  // Flag myIpAddress requests.
  if (op == MY_IP_ADDRESS || op == MY_IP_ADDRESS_EX) {
    // TODO: Provide a RequestInfo construction mechanism that does not
    // require a hostname and sets is_my_ip_address to true instead of this.
    info.set_is_my_ip_address(true);
  }
  // The non-ex flavors are limited to IPv4 results.
  if (op == MY_IP_ADDRESS || op == DNS_RESOLVE) {
    info.set_address_family(ADDRESS_FAMILY_IPV4);
  }

  return info;
}

std::string ProxyResolverV8Tracing::Job::MakeDnsCacheKey(
    const std::string& host, ResolveDnsOperation op) {
  return base::StringPrintf("%d:%s", op, host.c_str());
}

void ProxyResolverV8Tracing::Job::HandleAlertOrError(
    bool is_alert,
    int line_number,
    const base::string16& message) {
  CheckIsOnWorkerThread();

  if (cancelled_.IsSet())
    return;

  if (blocking_dns_) {
    // In blocking DNS mode the events can be dispatched immediately.
    DispatchAlertOrError(is_alert, line_number, message);
    return;
  }

  // Otherwise in nonblocking mode, buffer all the messages until
  // the end.

  if (abandoned_)
    return;

  alerts_and_errors_byte_cost_ += sizeof(AlertOrError) + message.size() * 2;

  // If there have been lots of messages, enqueing could be expensive on
  // memory. Consider a script which does megabytes worth of alerts().
  // Avoid this by falling back to blocking mode.
  if (alerts_and_errors_byte_cost_ > kMaxAlertsAndErrorsBytes) {
    ScheduleRestartWithBlockingDns();
    return;
  }

  AlertOrError entry = {is_alert, line_number, message};
  alerts_and_errors_.push_back(entry);
}

void ProxyResolverV8Tracing::Job::DispatchBufferedAlertsAndErrors() {
  CheckIsOnWorkerThread();
  DCHECK(!blocking_dns_);
  DCHECK(!abandoned_);

  for (size_t i = 0; i < alerts_and_errors_.size(); ++i) {
    const AlertOrError& x = alerts_and_errors_[i];
    DispatchAlertOrError(x.is_alert, x.line_number, x.message);
  }
}

void ProxyResolverV8Tracing::Job::DispatchAlertOrError(
    bool is_alert, int line_number, const base::string16& message) {
  CheckIsOnWorkerThread();

  // Note that the handling of cancellation is racy with regard to
  // alerts/errors. The request might get cancelled shortly after this
  // check! (There is no lock being held to guarantee otherwise).
  //
  // If this happens, then some information will get written to the NetLog
  // needlessly, however the NetLog will still be alive so it shouldn't cause
  // problems.
  if (cancelled_.IsSet())
    return;

  if (is_alert) {
    // -------------------
    // alert
    // -------------------
    IncrementWithoutOverflow(&metrics_num_alerts_);
    VLOG(1) << "PAC-alert: " << message;

    // Send to the NetLog.
    LogEventToCurrentRequestAndGlobally(
        NetLog::TYPE_PAC_JAVASCRIPT_ALERT,
        NetLog::StringCallback("message", &message));
  } else {
    // -------------------
    // error
    // -------------------
    IncrementWithoutOverflow(&metrics_num_errors_);
    if (line_number == -1)
      VLOG(1) << "PAC-error: " << message;
    else
      VLOG(1) << "PAC-error: " << "line: " << line_number << ": " << message;

    // Send the error to the NetLog.
    LogEventToCurrentRequestAndGlobally(
        NetLog::TYPE_PAC_JAVASCRIPT_ERROR,
        base::Bind(&NetLogErrorCallback, line_number, &message));

    if (error_observer())
      error_observer()->OnPACScriptError(line_number, message);
  }
}

void ProxyResolverV8Tracing::Job::LogEventToCurrentRequestAndGlobally(
    NetLog::EventType type,
    const NetLog::ParametersCallback& parameters_callback) {
  CheckIsOnWorkerThread();
  bound_net_log_.AddEvent(type, parameters_callback);

  // Emit to the global NetLog event stream.
  if (net_log())
    net_log()->AddGlobalEntry(type, parameters_callback);
}

ProxyResolverV8Tracing::ProxyResolverV8Tracing(
    HostResolver* host_resolver,
    ProxyResolverErrorObserver* error_observer,
    NetLog* net_log)
    : ProxyResolver(true /*expects_pac_bytes*/),
      host_resolver_(host_resolver),
      error_observer_(error_observer),
      net_log_(net_log),
      num_outstanding_callbacks_(0) {
  DCHECK(host_resolver);
  // Start up the thread.
  thread_.reset(new base::Thread("Proxy resolver"));
  CHECK(thread_->Start());

  v8_resolver_.reset(new ProxyResolverV8);
}

ProxyResolverV8Tracing::~ProxyResolverV8Tracing() {
  // Note, all requests should have been cancelled.
  CHECK(!set_pac_script_job_.get());
  CHECK_EQ(0, num_outstanding_callbacks_);

  // Join the worker thread. See http://crbug.com/69710. Note that we call
  // Stop() here instead of simply clearing thread_ since there may be pending
  // callbacks on the worker thread which want to dereference thread_.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  thread_->Stop();
}

int ProxyResolverV8Tracing::GetProxyForURL(const GURL& url,
                                           ProxyInfo* results,
                                           const CompletionCallback& callback,
                                           RequestHandle* request,
                                           const BoundNetLog& net_log) {
  DCHECK(CalledOnValidThread());
  DCHECK(!callback.is_null());
  DCHECK(!set_pac_script_job_.get());

  scoped_refptr<Job> job = new Job(this);

  if (request)
    *request = job.get();

  job->StartGetProxyForURL(url, results, net_log, callback);
  return ERR_IO_PENDING;
}

void ProxyResolverV8Tracing::CancelRequest(RequestHandle request) {
  Job* job = reinterpret_cast<Job*>(request);
  job->Cancel();
}

LoadState ProxyResolverV8Tracing::GetLoadState(RequestHandle request) const {
  Job* job = reinterpret_cast<Job*>(request);
  return job->GetLoadState();
}

void ProxyResolverV8Tracing::CancelSetPacScript() {
  DCHECK(set_pac_script_job_.get());
  set_pac_script_job_->Cancel();
  set_pac_script_job_ = NULL;
}

int ProxyResolverV8Tracing::SetPacScript(
    const scoped_refptr<ProxyResolverScriptData>& script_data,
    const CompletionCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!callback.is_null());

  // Note that there should not be any outstanding (non-cancelled) Jobs when
  // setting the PAC script (ProxyService should guarantee this). If there are,
  // then they might complete in strange ways after the new script is set.
  DCHECK(!set_pac_script_job_.get());
  CHECK_EQ(0, num_outstanding_callbacks_);

  set_pac_script_job_ = new Job(this);
  set_pac_script_job_->StartSetPacScript(script_data, callback);

  return ERR_IO_PENDING;
}

}  // namespace net
