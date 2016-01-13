// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_MULTI_THREADED_PROXY_RESOLVER_H_
#define NET_PROXY_MULTI_THREADED_PROXY_RESOLVER_H_

#include <deque>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/net_export.h"
#include "net/proxy/proxy_resolver.h"

namespace base {
class Thread;
}  // namespace base

namespace net {

// ProxyResolverFactory is an interface for creating ProxyResolver instances.
class ProxyResolverFactory {
 public:
  explicit ProxyResolverFactory(bool resolvers_expect_pac_bytes)
      : resolvers_expect_pac_bytes_(resolvers_expect_pac_bytes) {}

  virtual ~ProxyResolverFactory() {}

  // Creates a new ProxyResolver. The caller is responsible for freeing this
  // object.
  virtual ProxyResolver* CreateProxyResolver() = 0;

  bool resolvers_expect_pac_bytes() const {
    return resolvers_expect_pac_bytes_;
  }

 private:
  bool resolvers_expect_pac_bytes_;
  DISALLOW_COPY_AND_ASSIGN(ProxyResolverFactory);
};

// MultiThreadedProxyResolver is a ProxyResolver implementation that runs
// synchronous ProxyResolver implementations on worker threads.
//
// Threads are created lazily on demand, up to a maximum total. The advantage
// of having a pool of threads, is faster performance. In particular, being
// able to keep servicing PAC requests even if one blocks its execution.
//
// During initialization (SetPacScript), a single thread is spun up to test
// the script. If this succeeds, we cache the input script, and will re-use
// this to lazily provision any new threads as needed.
//
// For each new thread that we spawn, a corresponding new ProxyResolver is
// created using ProxyResolverFactory.
//
// Because we are creating multiple ProxyResolver instances, this means we
// are duplicating script contexts for what is ordinarily seen as being a
// single script. This can affect compatibility on some classes of PAC
// script:
//
// (a) Scripts whose initialization has external dependencies on network or
//     time may end up successfully initializing on some threads, but not
//     others. So depending on what thread services the request, the result
//     may jump between several possibilities.
//
// (b) Scripts whose FindProxyForURL() depends on side-effects may now
//     work differently. For example, a PAC script which was incrementing
//     a global counter and using that to make a decision. In the
//     multi-threaded model, each thread may have a different value for this
//     counter, so it won't globally be seen as monotonically increasing!
class NET_EXPORT_PRIVATE MultiThreadedProxyResolver
    : public ProxyResolver,
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  // Creates an asynchronous ProxyResolver that runs requests on up to
  // |max_num_threads|.
  //
  // For each thread that is created, an accompanying synchronous ProxyResolver
  // will be provisioned using |resolver_factory|. All methods on these
  // ProxyResolvers will be called on the one thread, with the exception of
  // ProxyResolver::Shutdown() which will be called from the origin thread
  // prior to destruction.
  //
  // The constructor takes ownership of |resolver_factory|.
  MultiThreadedProxyResolver(ProxyResolverFactory* resolver_factory,
                             size_t max_num_threads);

  virtual ~MultiThreadedProxyResolver();

  // ProxyResolver implementation:
  virtual int GetProxyForURL(const GURL& url,
                             ProxyInfo* results,
                             const CompletionCallback& callback,
                             RequestHandle* request,
                             const BoundNetLog& net_log) OVERRIDE;
  virtual void CancelRequest(RequestHandle request) OVERRIDE;
  virtual LoadState GetLoadState(RequestHandle request) const OVERRIDE;
  virtual void CancelSetPacScript() OVERRIDE;
  virtual int SetPacScript(
      const scoped_refptr<ProxyResolverScriptData>& script_data,
      const CompletionCallback& callback) OVERRIDE;

 private:
  class Executor;
  class Job;
  class SetPacScriptJob;
  class GetProxyForURLJob;
  // FIFO queue of pending jobs waiting to be started.
  // TODO(eroman): Make this priority queue.
  typedef std::deque<scoped_refptr<Job> > PendingJobsQueue;
  typedef std::vector<scoped_refptr<Executor> > ExecutorList;

  // Asserts that there are no outstanding user-initiated jobs on any of the
  // worker threads.
  void CheckNoOutstandingUserRequests() const;

  // Stops and deletes all of the worker threads.
  void ReleaseAllExecutors();

  // Returns an idle worker thread which is ready to receive GetProxyForURL()
  // requests. If all threads are occupied, returns NULL.
  Executor* FindIdleExecutor();

  // Creates a new worker thread, and appends it to |executors_|.
  Executor* AddNewExecutor();

  // Starts the next job from |pending_jobs_| if possible.
  void OnExecutorReady(Executor* executor);

  const scoped_ptr<ProxyResolverFactory> resolver_factory_;
  const size_t max_num_threads_;
  PendingJobsQueue pending_jobs_;
  ExecutorList executors_;
  scoped_refptr<ProxyResolverScriptData> current_script_data_;
};

}  // namespace net

#endif  // NET_PROXY_MULTI_THREADED_PROXY_RESOLVER_H_
