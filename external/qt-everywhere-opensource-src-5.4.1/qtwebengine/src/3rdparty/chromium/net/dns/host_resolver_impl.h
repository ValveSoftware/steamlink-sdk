// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_HOST_RESOLVER_IMPL_H_
#define NET_DNS_HOST_RESOLVER_IMPL_H_

#include <map>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_proc.h"

namespace net {

class BoundNetLog;
class DnsClient;
class NetLog;

// For each hostname that is requested, HostResolver creates a
// HostResolverImpl::Job. When this job gets dispatched it creates a ProcTask
// which runs the given HostResolverProc on a WorkerPool thread. If requests for
// that same host are made during the job's lifetime, they are attached to the
// existing job rather than creating a new one. This avoids doing parallel
// resolves for the same host.
//
// The way these classes fit together is illustrated by:
//
//
//            +----------- HostResolverImpl -------------+
//            |                    |                     |
//           Job                  Job                   Job
//    (for host1, fam1)    (for host2, fam2)     (for hostx, famx)
//       /    |   |            /   |   |             /   |   |
//   Request ... Request  Request ... Request   Request ... Request
//  (port1)     (port2)  (port3)      (port4)  (port5)      (portX)
//
// When a HostResolverImpl::Job finishes, the callbacks of each waiting request
// are run on the origin thread.
//
// Thread safety: This class is not threadsafe, and must only be called
// from one thread!
//
// The HostResolverImpl enforces limits on the maximum number of concurrent
// threads using PrioritizedDispatcher::Limits.
//
// Jobs are ordered in the queue based on their priority and order of arrival.
class NET_EXPORT HostResolverImpl
    : public HostResolver,
      NON_EXPORTED_BASE(public base::NonThreadSafe),
      public NetworkChangeNotifier::IPAddressObserver,
      public NetworkChangeNotifier::DNSObserver {
 public:
  // Parameters for ProcTask which resolves hostnames using HostResolveProc.
  //
  // |resolver_proc| is used to perform the actual resolves; it must be
  // thread-safe since it is run from multiple worker threads. If
  // |resolver_proc| is NULL then the default host resolver procedure is
  // used (which is SystemHostResolverProc except if overridden).
  //
  // For each attempt, we could start another attempt if host is not resolved
  // within |unresponsive_delay| time. We keep attempting to resolve the host
  // for |max_retry_attempts|. For every retry attempt, we grow the
  // |unresponsive_delay| by the |retry_factor| amount (that is retry interval
  // is multiplied by the retry factor each time). Once we have retried
  // |max_retry_attempts|, we give up on additional attempts.
  //
  struct NET_EXPORT_PRIVATE ProcTaskParams {
    // Sets up defaults.
    ProcTaskParams(HostResolverProc* resolver_proc, size_t max_retry_attempts);

    ~ProcTaskParams();

    // The procedure to use for resolving host names. This will be NULL, except
    // in the case of unit-tests which inject custom host resolving behaviors.
    scoped_refptr<HostResolverProc> resolver_proc;

    // Maximum number retry attempts to resolve the hostname.
    // Pass HostResolver::kDefaultRetryAttempts to choose a default value.
    size_t max_retry_attempts;

    // This is the limit after which we make another attempt to resolve the host
    // if the worker thread has not responded yet.
    base::TimeDelta unresponsive_delay;

    // Factor to grow |unresponsive_delay| when we re-re-try.
    uint32 retry_factor;
  };

  // Creates a HostResolver as specified by |options|.
  //
  // If Options.enable_caching is true, a cache is created using
  // HostCache::CreateDefaultCache(). Otherwise no cache is used.
  //
  // Options.GetDispatcherLimits() determines the maximum number of jobs that
  // the resolver will run at once. This upper-bounds the total number of
  // outstanding DNS transactions (not counting retransmissions and retries).
  //
  // |net_log| must remain valid for the life of the HostResolverImpl.
  HostResolverImpl(const Options& options, NetLog* net_log);

  // If any completion callbacks are pending when the resolver is destroyed,
  // the host resolutions are cancelled, and the completion callbacks will not
  // be called.
  virtual ~HostResolverImpl();

  // Configures maximum number of Jobs in the queue. Exposed for testing.
  // Only allowed when the queue is empty.
  void SetMaxQueuedJobs(size_t value);

  // Set the DnsClient to be used for resolution. In case of failure, the
  // HostResolverProc from ProcTaskParams will be queried. If the DnsClient is
  // not pre-configured with a valid DnsConfig, a new config is fetched from
  // NetworkChangeNotifier.
  void SetDnsClient(scoped_ptr<DnsClient> dns_client);

  // HostResolver methods:
  virtual int Resolve(const RequestInfo& info,
                      RequestPriority priority,
                      AddressList* addresses,
                      const CompletionCallback& callback,
                      RequestHandle* out_req,
                      const BoundNetLog& source_net_log) OVERRIDE;
  virtual int ResolveFromCache(const RequestInfo& info,
                               AddressList* addresses,
                               const BoundNetLog& source_net_log) OVERRIDE;
  virtual void CancelRequest(RequestHandle req) OVERRIDE;
  virtual void SetDefaultAddressFamily(AddressFamily address_family) OVERRIDE;
  virtual AddressFamily GetDefaultAddressFamily() const OVERRIDE;
  virtual void SetDnsClientEnabled(bool enabled) OVERRIDE;
  virtual HostCache* GetHostCache() OVERRIDE;
  virtual base::Value* GetDnsConfigAsValue() const OVERRIDE;

  void set_proc_params_for_test(const ProcTaskParams& proc_params) {
    proc_params_ = proc_params;
  }

 private:
  friend class HostResolverImplTest;
  class Job;
  class ProcTask;
  class LoopbackProbeJob;
  class DnsTask;
  class Request;
  typedef HostCache::Key Key;
  typedef std::map<Key, Job*> JobMap;
  typedef ScopedVector<Request> RequestsList;

  // Number of consecutive failures of DnsTask (with successful fallback to
  // ProcTask) before the DnsClient is disabled until the next DNS change.
  static const unsigned kMaximumDnsFailures;

  // Helper used by |Resolve()| and |ResolveFromCache()|.  Performs IP
  // literal, cache and HOSTS lookup (if enabled), returns OK if successful,
  // ERR_NAME_NOT_RESOLVED if either hostname is invalid or IP literal is
  // incompatible, ERR_DNS_CACHE_MISS if entry was not found in cache and HOSTS.
  int ResolveHelper(const Key& key,
                    const RequestInfo& info,
                    AddressList* addresses,
                    const BoundNetLog& request_net_log);

  // Tries to resolve |key| as an IP, returns true and sets |net_error| if
  // succeeds, returns false otherwise.
  bool ResolveAsIP(const Key& key,
                   const RequestInfo& info,
                   int* net_error,
                   AddressList* addresses);

  // If |key| is not found in cache returns false, otherwise returns
  // true, sets |net_error| to the cached error code and fills |addresses|
  // if it is a positive entry.
  bool ServeFromCache(const Key& key,
                      const RequestInfo& info,
                      int* net_error,
                      AddressList* addresses);

  // If we have a DnsClient with a valid DnsConfig, and |key| is found in the
  // HOSTS file, returns true and fills |addresses|. Otherwise returns false.
  bool ServeFromHosts(const Key& key,
                      const RequestInfo& info,
                      AddressList* addresses);

  // Callback from HaveOnlyLoopbackAddresses probe.
  void SetHaveOnlyLoopbackAddresses(bool result);

  // Returns the (hostname, address_family) key to use for |info|, choosing an
  // "effective" address family by inheriting the resolver's default address
  // family when the request leaves it unspecified.
  Key GetEffectiveKeyForRequest(const RequestInfo& info,
                                const BoundNetLog& net_log) const;

  // Records the result in cache if cache is present.
  void CacheResult(const Key& key,
                   const HostCache::Entry& entry,
                   base::TimeDelta ttl);

  // Removes |job| from |jobs_|, only if it exists.
  void RemoveJob(Job* job);

  // Aborts all in progress jobs with ERR_NETWORK_CHANGED and notifies their
  // requests. Might start new jobs.
  void AbortAllInProgressJobs();

  // Aborts all in progress DnsTasks. In-progress jobs will fall back to
  // ProcTasks. Might start new jobs, if any jobs were taking up two dispatcher
  // slots.
  void AbortDnsTasks();

  // Attempts to serve each Job in |jobs_| from the HOSTS file if we have
  // a DnsClient with a valid DnsConfig.
  void TryServingAllJobsFromHosts();

  // NetworkChangeNotifier::IPAddressObserver:
  virtual void OnIPAddressChanged() OVERRIDE;

  // NetworkChangeNotifier::DNSObserver:
  virtual void OnDNSChanged() OVERRIDE;

  // True if have a DnsClient with a valid DnsConfig.
  bool HaveDnsConfig() const;

  // Called when a host name is successfully resolved and DnsTask was run on it
  // and resulted in |net_error|.
  void OnDnsTaskResolve(int net_error);

  // Allows the tests to catch slots leaking out of the dispatcher.  One
  // HostResolverImpl::Job could occupy multiple PrioritizedDispatcher job
  // slots.
  size_t num_running_dispatcher_jobs_for_tests() const {
    return dispatcher_->num_running_jobs();
  }

  // Cache of host resolution results.
  scoped_ptr<HostCache> cache_;

  // Map from HostCache::Key to a Job.
  JobMap jobs_;

  // Starts Jobs according to their priority and the configured limits.
  scoped_ptr<PrioritizedDispatcher> dispatcher_;

  // Limit on the maximum number of jobs queued in |dispatcher_|.
  size_t max_queued_jobs_;

  // Parameters for ProcTask.
  ProcTaskParams proc_params_;

  NetLog* net_log_;

  // Address family to use when the request doesn't specify one.
  AddressFamily default_address_family_;

  // If present, used by DnsTask and ServeFromHosts to resolve requests.
  scoped_ptr<DnsClient> dns_client_;

  // True if received valid config from |dns_config_service_|. Temporary, used
  // to measure performance of DnsConfigService: http://crbug.com/125599
  bool received_dns_config_;

  // Number of consecutive failures of DnsTask, counted when fallback succeeds.
  unsigned num_dns_failures_;

  // True if probing is done for each Request to set address family. When false,
  // explicit setting in |default_address_family_| is used.
  bool probe_ipv6_support_;

  // True if DnsConfigService detected that system configuration depends on
  // local IPv6 connectivity. Disables probing.
  bool use_local_ipv6_;

  // True iff ProcTask has successfully resolved a hostname known to have IPv6
  // addresses using ADDRESS_FAMILY_UNSPECIFIED. Reset on IP address change.
  bool resolved_known_ipv6_hostname_;

  // Any resolver flags that should be added to a request by default.
  HostResolverFlags additional_resolver_flags_;

  // Allow fallback to ProcTask if DnsTask fails.
  bool fallback_to_proctask_;

  base::WeakPtrFactory<HostResolverImpl> weak_ptr_factory_;

  base::WeakPtrFactory<HostResolverImpl> probe_weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HostResolverImpl);
};

}  // namespace net

#endif  // NET_DNS_HOST_RESOLVER_IMPL_H_
