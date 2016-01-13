// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/multi_threaded_proxy_resolver.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/proxy/proxy_info.h"

// TODO(eroman): Have the MultiThreadedProxyResolver clear its PAC script
//               data when SetPacScript fails. That will reclaim memory when
//               testing bogus scripts.

namespace net {

// An "executor" is a job-runner for PAC requests. It encapsulates a worker
// thread and a synchronous ProxyResolver (which will be operated on said
// thread.)
class MultiThreadedProxyResolver::Executor
    : public base::RefCountedThreadSafe<MultiThreadedProxyResolver::Executor > {
 public:
  // |coordinator| must remain valid throughout our lifetime. It is used to
  // signal when the executor is ready to receive work by calling
  // |coordinator->OnExecutorReady()|.
  // The constructor takes ownership of |resolver|.
  // |thread_number| is an identifier used when naming the worker thread.
  Executor(MultiThreadedProxyResolver* coordinator,
           ProxyResolver* resolver,
           int thread_number);

  // Submit a job to this executor.
  void StartJob(Job* job);

  // Callback for when a job has completed running on the executor's thread.
  void OnJobCompleted(Job* job);

  // Cleanup the executor. Cancels all outstanding work, and frees the thread
  // and resolver.
  void Destroy();

  // Returns the outstanding job, or NULL.
  Job* outstanding_job() const { return outstanding_job_.get(); }

  ProxyResolver* resolver() { return resolver_.get(); }

  int thread_number() const { return thread_number_; }

 private:
  friend class base::RefCountedThreadSafe<Executor>;
  ~Executor();

  MultiThreadedProxyResolver* coordinator_;
  const int thread_number_;

  // The currently active job for this executor (either a SetPacScript or
  // GetProxyForURL task).
  scoped_refptr<Job> outstanding_job_;

  // The synchronous resolver implementation.
  scoped_ptr<ProxyResolver> resolver_;

  // The thread where |resolver_| is run on.
  // Note that declaration ordering is important here. |thread_| needs to be
  // destroyed *before* |resolver_|, in case |resolver_| is currently
  // executing on |thread_|.
  scoped_ptr<base::Thread> thread_;
};

// MultiThreadedProxyResolver::Job ---------------------------------------------

class MultiThreadedProxyResolver::Job
    : public base::RefCountedThreadSafe<MultiThreadedProxyResolver::Job> {
 public:
  // Identifies the subclass of Job (only being used for debugging purposes).
  enum Type {
    TYPE_GET_PROXY_FOR_URL,
    TYPE_SET_PAC_SCRIPT,
    TYPE_SET_PAC_SCRIPT_INTERNAL,
  };

  Job(Type type, const CompletionCallback& callback)
      : type_(type),
        callback_(callback),
        executor_(NULL),
        was_cancelled_(false) {
  }

  void set_executor(Executor* executor) {
    executor_ = executor;
  }

  // The "executor" is the job runner that is scheduling this job. If
  // this job has not been submitted to an executor yet, this will be
  // NULL (and we know it hasn't started yet).
  Executor* executor() {
    return executor_;
  }

  // Mark the job as having been cancelled.
  void Cancel() {
    was_cancelled_ = true;
  }

  // Returns true if Cancel() has been called.
  bool was_cancelled() const { return was_cancelled_; }

  Type type() const { return type_; }

  // Returns true if this job still has a user callback. Some jobs
  // do not have a user callback, because they were helper jobs
  // scheduled internally (for example TYPE_SET_PAC_SCRIPT_INTERNAL).
  //
  // Otherwise jobs that correspond with user-initiated work will
  // have a non-null callback up until the callback is run.
  bool has_user_callback() const { return !callback_.is_null(); }

  // This method is called when the job is inserted into a wait queue
  // because no executors were ready to accept it.
  virtual void WaitingForThread() {}

  // This method is called just before the job is posted to the work thread.
  virtual void FinishedWaitingForThread() {}

  // This method is called on the worker thread to do the job's work. On
  // completion, implementors are expected to call OnJobCompleted() on
  // |origin_loop|.
  virtual void Run(scoped_refptr<base::MessageLoopProxy> origin_loop) = 0;

 protected:
  void OnJobCompleted() {
    // |executor_| will be NULL if the executor has already been deleted.
    if (executor_)
      executor_->OnJobCompleted(this);
  }

  void RunUserCallback(int result) {
    DCHECK(has_user_callback());
    CompletionCallback callback = callback_;
    // Reset the callback so has_user_callback() will now return false.
    callback_.Reset();
    callback.Run(result);
  }

  friend class base::RefCountedThreadSafe<MultiThreadedProxyResolver::Job>;

  virtual ~Job() {}

 private:
  const Type type_;
  CompletionCallback callback_;
  Executor* executor_;
  bool was_cancelled_;
};

// MultiThreadedProxyResolver::SetPacScriptJob ---------------------------------

// Runs on the worker thread to call ProxyResolver::SetPacScript.
class MultiThreadedProxyResolver::SetPacScriptJob
    : public MultiThreadedProxyResolver::Job {
 public:
  SetPacScriptJob(const scoped_refptr<ProxyResolverScriptData>& script_data,
                  const CompletionCallback& callback)
    : Job(!callback.is_null() ? TYPE_SET_PAC_SCRIPT :
                                TYPE_SET_PAC_SCRIPT_INTERNAL,
          callback),
      script_data_(script_data) {
  }

  // Runs on the worker thread.
  virtual void Run(scoped_refptr<base::MessageLoopProxy> origin_loop) OVERRIDE {
    ProxyResolver* resolver = executor()->resolver();
    int rv = resolver->SetPacScript(script_data_, CompletionCallback());

    DCHECK_NE(rv, ERR_IO_PENDING);
    origin_loop->PostTask(
        FROM_HERE,
        base::Bind(&SetPacScriptJob::RequestComplete, this, rv));
  }

 protected:
  virtual ~SetPacScriptJob() {}

 private:
  // Runs the completion callback on the origin thread.
  void RequestComplete(int result_code) {
    // The task may have been cancelled after it was started.
    if (!was_cancelled() && has_user_callback()) {
      RunUserCallback(result_code);
    }
    OnJobCompleted();
  }

  const scoped_refptr<ProxyResolverScriptData> script_data_;
};

// MultiThreadedProxyResolver::GetProxyForURLJob ------------------------------

class MultiThreadedProxyResolver::GetProxyForURLJob
    : public MultiThreadedProxyResolver::Job {
 public:
  // |url|         -- the URL of the query.
  // |results|     -- the structure to fill with proxy resolve results.
  GetProxyForURLJob(const GURL& url,
                    ProxyInfo* results,
                    const CompletionCallback& callback,
                    const BoundNetLog& net_log)
      : Job(TYPE_GET_PROXY_FOR_URL, callback),
        results_(results),
        net_log_(net_log),
        url_(url),
        was_waiting_for_thread_(false) {
    DCHECK(!callback.is_null());
    start_time_ = base::TimeTicks::Now();
  }

  BoundNetLog* net_log() { return &net_log_; }

  virtual void WaitingForThread() OVERRIDE {
    was_waiting_for_thread_ = true;
    net_log_.BeginEvent(NetLog::TYPE_WAITING_FOR_PROXY_RESOLVER_THREAD);
  }

  virtual void FinishedWaitingForThread() OVERRIDE {
    DCHECK(executor());

    submitted_to_thread_time_ = base::TimeTicks::Now();

    if (was_waiting_for_thread_) {
      net_log_.EndEvent(NetLog::TYPE_WAITING_FOR_PROXY_RESOLVER_THREAD);
    }

    net_log_.AddEvent(
        NetLog::TYPE_SUBMITTED_TO_RESOLVER_THREAD,
        NetLog::IntegerCallback("thread_number", executor()->thread_number()));
  }

  // Runs on the worker thread.
  virtual void Run(scoped_refptr<base::MessageLoopProxy> origin_loop) OVERRIDE {
    ProxyResolver* resolver = executor()->resolver();
    int rv = resolver->GetProxyForURL(
        url_, &results_buf_, CompletionCallback(), NULL, net_log_);
    DCHECK_NE(rv, ERR_IO_PENDING);

    origin_loop->PostTask(
        FROM_HERE,
        base::Bind(&GetProxyForURLJob::QueryComplete, this, rv));
  }

 protected:
  virtual ~GetProxyForURLJob() {}

 private:
  // Runs the completion callback on the origin thread.
  void QueryComplete(int result_code) {
    // The Job may have been cancelled after it was started.
    if (!was_cancelled()) {
      RecordPerformanceMetrics();
      if (result_code >= OK) {  // Note: unit-tests use values > 0.
        results_->Use(results_buf_);
      }
      RunUserCallback(result_code);
    }
    OnJobCompleted();
  }

  void RecordPerformanceMetrics() {
    DCHECK(!was_cancelled());

    base::TimeTicks now = base::TimeTicks::Now();

    // Log the total time the request took to complete.
    UMA_HISTOGRAM_MEDIUM_TIMES("Net.MTPR_GetProxyForUrl_Time",
                               now - start_time_);

    // Log the time the request was stalled waiting for a thread to free up.
    UMA_HISTOGRAM_MEDIUM_TIMES("Net.MTPR_GetProxyForUrl_Thread_Wait_Time",
                               submitted_to_thread_time_ - start_time_);
  }

  // Must only be used on the "origin" thread.
  ProxyInfo* results_;

  // Can be used on either "origin" or worker thread.
  BoundNetLog net_log_;
  const GURL url_;

  // Usable from within DoQuery on the worker thread.
  ProxyInfo results_buf_;

  base::TimeTicks start_time_;
  base::TimeTicks submitted_to_thread_time_;

  bool was_waiting_for_thread_;
};

// MultiThreadedProxyResolver::Executor ----------------------------------------

MultiThreadedProxyResolver::Executor::Executor(
    MultiThreadedProxyResolver* coordinator,
    ProxyResolver* resolver,
    int thread_number)
    : coordinator_(coordinator),
      thread_number_(thread_number),
      resolver_(resolver) {
  DCHECK(coordinator);
  DCHECK(resolver);
  // Start up the thread.
  thread_.reset(new base::Thread(base::StringPrintf("PAC thread #%d",
                                                    thread_number)));
  CHECK(thread_->Start());
}

void MultiThreadedProxyResolver::Executor::StartJob(Job* job) {
  DCHECK(!outstanding_job_.get());
  outstanding_job_ = job;

  // Run the job. Once it has completed (regardless of whether it was
  // cancelled), it will invoke OnJobCompleted() on this thread.
  job->set_executor(this);
  job->FinishedWaitingForThread();
  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&Job::Run, job, base::MessageLoopProxy::current()));
}

void MultiThreadedProxyResolver::Executor::OnJobCompleted(Job* job) {
  DCHECK_EQ(job, outstanding_job_.get());
  outstanding_job_ = NULL;
  coordinator_->OnExecutorReady(this);
}

void MultiThreadedProxyResolver::Executor::Destroy() {
  DCHECK(coordinator_);

  {
    // See http://crbug.com/69710.
    base::ThreadRestrictions::ScopedAllowIO allow_io;

    // Join the worker thread.
    thread_.reset();
  }

  // Cancel any outstanding job.
  if (outstanding_job_.get()) {
    outstanding_job_->Cancel();
    // Orphan the job (since this executor may be deleted soon).
    outstanding_job_->set_executor(NULL);
  }

  // It is now safe to free the ProxyResolver, since all the tasks that
  // were using it on the resolver thread have completed.
  resolver_.reset();

  // Null some stuff as a precaution.
  coordinator_ = NULL;
  outstanding_job_ = NULL;
}

MultiThreadedProxyResolver::Executor::~Executor() {
  // The important cleanup happens as part of Destroy(), which should always be
  // called first.
  DCHECK(!coordinator_) << "Destroy() was not called";
  DCHECK(!thread_.get());
  DCHECK(!resolver_.get());
  DCHECK(!outstanding_job_.get());
}

// MultiThreadedProxyResolver --------------------------------------------------

MultiThreadedProxyResolver::MultiThreadedProxyResolver(
    ProxyResolverFactory* resolver_factory,
    size_t max_num_threads)
    : ProxyResolver(resolver_factory->resolvers_expect_pac_bytes()),
      resolver_factory_(resolver_factory),
      max_num_threads_(max_num_threads) {
  DCHECK_GE(max_num_threads, 1u);
}

MultiThreadedProxyResolver::~MultiThreadedProxyResolver() {
  // We will cancel all outstanding requests.
  pending_jobs_.clear();
  ReleaseAllExecutors();
}

int MultiThreadedProxyResolver::GetProxyForURL(
    const GURL& url, ProxyInfo* results, const CompletionCallback& callback,
    RequestHandle* request, const BoundNetLog& net_log) {
  DCHECK(CalledOnValidThread());
  DCHECK(!callback.is_null());
  DCHECK(current_script_data_.get())
      << "Resolver is un-initialized. Must call SetPacScript() first!";

  scoped_refptr<GetProxyForURLJob> job(
      new GetProxyForURLJob(url, results, callback, net_log));

  // Completion will be notified through |callback|, unless the caller cancels
  // the request using |request|.
  if (request)
    *request = reinterpret_cast<RequestHandle>(job.get());

  // If there is an executor that is ready to run this request, submit it!
  Executor* executor = FindIdleExecutor();
  if (executor) {
    DCHECK_EQ(0u, pending_jobs_.size());
    executor->StartJob(job.get());
    return ERR_IO_PENDING;
  }

  // Otherwise queue this request. (We will schedule it to a thread once one
  // becomes available).
  job->WaitingForThread();
  pending_jobs_.push_back(job);

  // If we haven't already reached the thread limit, provision a new thread to
  // drain the requests more quickly.
  if (executors_.size() < max_num_threads_) {
    executor = AddNewExecutor();
    executor->StartJob(
        new SetPacScriptJob(current_script_data_, CompletionCallback()));
  }

  return ERR_IO_PENDING;
}

void MultiThreadedProxyResolver::CancelRequest(RequestHandle req) {
  DCHECK(CalledOnValidThread());
  DCHECK(req);

  Job* job = reinterpret_cast<Job*>(req);
  DCHECK_EQ(Job::TYPE_GET_PROXY_FOR_URL, job->type());

  if (job->executor()) {
    // If the job was already submitted to the executor, just mark it
    // as cancelled so the user callback isn't run on completion.
    job->Cancel();
  } else {
    // Otherwise the job is just sitting in a queue.
    PendingJobsQueue::iterator it =
        std::find(pending_jobs_.begin(), pending_jobs_.end(), job);
    DCHECK(it != pending_jobs_.end());
    pending_jobs_.erase(it);
  }
}

LoadState MultiThreadedProxyResolver::GetLoadState(RequestHandle req) const {
  DCHECK(CalledOnValidThread());
  DCHECK(req);
  return LOAD_STATE_RESOLVING_PROXY_FOR_URL;
}

void MultiThreadedProxyResolver::CancelSetPacScript() {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(0u, pending_jobs_.size());
  DCHECK_EQ(1u, executors_.size());
  DCHECK_EQ(Job::TYPE_SET_PAC_SCRIPT,
            executors_[0]->outstanding_job()->type());

  // Defensively clear some data which shouldn't be getting used
  // anymore.
  current_script_data_ = NULL;

  ReleaseAllExecutors();
}

int MultiThreadedProxyResolver::SetPacScript(
    const scoped_refptr<ProxyResolverScriptData>& script_data,
    const CompletionCallback&callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!callback.is_null());

  // Save the script details, so we can provision new executors later.
  current_script_data_ = script_data;

  // The user should not have any outstanding requests when they call
  // SetPacScript().
  CheckNoOutstandingUserRequests();

  // Destroy all of the current threads and their proxy resolvers.
  ReleaseAllExecutors();

  // Provision a new executor, and run the SetPacScript request. On completion
  // notification will be sent through |callback|.
  Executor* executor = AddNewExecutor();
  executor->StartJob(new SetPacScriptJob(script_data, callback));
  return ERR_IO_PENDING;
}

void MultiThreadedProxyResolver::CheckNoOutstandingUserRequests() const {
  DCHECK(CalledOnValidThread());
  CHECK_EQ(0u, pending_jobs_.size());

  for (ExecutorList::const_iterator it = executors_.begin();
       it != executors_.end(); ++it) {
    const Executor* executor = it->get();
    Job* job = executor->outstanding_job();
    // The "has_user_callback()" is to exclude jobs for which the callback
    // has already been invoked, or was not user-initiated (as in the case of
    // lazy thread provisions). User-initiated jobs may !has_user_callback()
    // when the callback has already been run. (Since we only clear the
    // outstanding job AFTER the callback has been invoked, it is possible
    // for a new request to be started from within the callback).
    CHECK(!job || job->was_cancelled() || !job->has_user_callback());
  }
}

void MultiThreadedProxyResolver::ReleaseAllExecutors() {
  DCHECK(CalledOnValidThread());
  for (ExecutorList::iterator it = executors_.begin();
       it != executors_.end(); ++it) {
    Executor* executor = it->get();
    executor->Destroy();
  }
  executors_.clear();
}

MultiThreadedProxyResolver::Executor*
MultiThreadedProxyResolver::FindIdleExecutor() {
  DCHECK(CalledOnValidThread());
  for (ExecutorList::iterator it = executors_.begin();
       it != executors_.end(); ++it) {
    Executor* executor = it->get();
    if (!executor->outstanding_job())
      return executor;
  }
  return NULL;
}

MultiThreadedProxyResolver::Executor*
MultiThreadedProxyResolver::AddNewExecutor() {
  DCHECK(CalledOnValidThread());
  DCHECK_LT(executors_.size(), max_num_threads_);
  // The "thread number" is used to give the thread a unique name.
  int thread_number = executors_.size();
  ProxyResolver* resolver = resolver_factory_->CreateProxyResolver();
  Executor* executor = new Executor(
      this, resolver, thread_number);
  executors_.push_back(make_scoped_refptr(executor));
  return executor;
}

void MultiThreadedProxyResolver::OnExecutorReady(Executor* executor) {
  DCHECK(CalledOnValidThread());
  if (pending_jobs_.empty())
    return;

  // Get the next job to process (FIFO). Transfer it from the pending queue
  // to the executor.
  scoped_refptr<Job> job = pending_jobs_.front();
  pending_jobs_.pop_front();
  executor->StartJob(job.get());
}

}  // namespace net
