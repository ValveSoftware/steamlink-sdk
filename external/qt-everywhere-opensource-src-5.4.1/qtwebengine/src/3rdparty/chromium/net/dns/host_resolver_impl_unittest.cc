// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_impl.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const size_t kMaxJobs = 10u;
const size_t kMaxRetryAttempts = 4u;

HostResolver::Options DefaultOptions() {
  HostResolver::Options options;
  options.max_concurrent_resolves = kMaxJobs;
  options.max_retry_attempts = kMaxRetryAttempts;
  options.enable_caching = true;
  return options;
}

HostResolverImpl::ProcTaskParams DefaultParams(
    HostResolverProc* resolver_proc) {
  return HostResolverImpl::ProcTaskParams(resolver_proc, kMaxRetryAttempts);
}

// A HostResolverProc that pushes each host mapped into a list and allows
// waiting for a specific number of requests. Unlike RuleBasedHostResolverProc
// it never calls SystemHostResolverCall. By default resolves all hostnames to
// "127.0.0.1". After AddRule(), it resolves only names explicitly specified.
class MockHostResolverProc : public HostResolverProc {
 public:
  struct ResolveKey {
    ResolveKey(const std::string& hostname, AddressFamily address_family)
        : hostname(hostname), address_family(address_family) {}
    bool operator<(const ResolveKey& other) const {
      return address_family < other.address_family ||
          (address_family == other.address_family && hostname < other.hostname);
    }
    std::string hostname;
    AddressFamily address_family;
  };

  typedef std::vector<ResolveKey> CaptureList;

  MockHostResolverProc()
      : HostResolverProc(NULL),
        num_requests_waiting_(0),
        num_slots_available_(0),
        requests_waiting_(&lock_),
        slots_available_(&lock_) {
  }

  // Waits until |count| calls to |Resolve| are blocked. Returns false when
  // timed out.
  bool WaitFor(unsigned count) {
    base::AutoLock lock(lock_);
    base::Time start_time = base::Time::Now();
    while (num_requests_waiting_ < count) {
      requests_waiting_.TimedWait(TestTimeouts::action_timeout());
      if (base::Time::Now() > start_time + TestTimeouts::action_timeout())
        return false;
    }
    return true;
  }

  // Signals |count| waiting calls to |Resolve|. First come first served.
  void SignalMultiple(unsigned count) {
    base::AutoLock lock(lock_);
    num_slots_available_ += count;
    slots_available_.Broadcast();
  }

  // Signals all waiting calls to |Resolve|. Beware of races.
  void SignalAll() {
    base::AutoLock lock(lock_);
    num_slots_available_ = num_requests_waiting_;
    slots_available_.Broadcast();
  }

  void AddRule(const std::string& hostname, AddressFamily family,
               const AddressList& result) {
    base::AutoLock lock(lock_);
    rules_[ResolveKey(hostname, family)] = result;
  }

  void AddRule(const std::string& hostname, AddressFamily family,
               const std::string& ip_list) {
    AddressList result;
    int rv = ParseAddressList(ip_list, std::string(), &result);
    DCHECK_EQ(OK, rv);
    AddRule(hostname, family, result);
  }

  void AddRuleForAllFamilies(const std::string& hostname,
                             const std::string& ip_list) {
    AddressList result;
    int rv = ParseAddressList(ip_list, std::string(), &result);
    DCHECK_EQ(OK, rv);
    AddRule(hostname, ADDRESS_FAMILY_UNSPECIFIED, result);
    AddRule(hostname, ADDRESS_FAMILY_IPV4, result);
    AddRule(hostname, ADDRESS_FAMILY_IPV6, result);
  }

  virtual int Resolve(const std::string& hostname,
                      AddressFamily address_family,
                      HostResolverFlags host_resolver_flags,
                      AddressList* addrlist,
                      int* os_error) OVERRIDE {
    base::AutoLock lock(lock_);
    capture_list_.push_back(ResolveKey(hostname, address_family));
    ++num_requests_waiting_;
    requests_waiting_.Broadcast();
    while (!num_slots_available_)
      slots_available_.Wait();
    DCHECK_GT(num_requests_waiting_, 0u);
    --num_slots_available_;
    --num_requests_waiting_;
    if (rules_.empty()) {
      int rv = ParseAddressList("127.0.0.1", std::string(), addrlist);
      DCHECK_EQ(OK, rv);
      return OK;
    }
    ResolveKey key(hostname, address_family);
    if (rules_.count(key) == 0)
      return ERR_NAME_NOT_RESOLVED;
    *addrlist = rules_[key];
    return OK;
  }

  CaptureList GetCaptureList() const {
    CaptureList copy;
    {
      base::AutoLock lock(lock_);
      copy = capture_list_;
    }
    return copy;
  }

  bool HasBlockedRequests() const {
    base::AutoLock lock(lock_);
    return num_requests_waiting_ > num_slots_available_;
  }

 protected:
  virtual ~MockHostResolverProc() {}

 private:
  mutable base::Lock lock_;
  std::map<ResolveKey, AddressList> rules_;
  CaptureList capture_list_;
  unsigned num_requests_waiting_;
  unsigned num_slots_available_;
  base::ConditionVariable requests_waiting_;
  base::ConditionVariable slots_available_;

  DISALLOW_COPY_AND_ASSIGN(MockHostResolverProc);
};

bool AddressListContains(const AddressList& list, const std::string& address,
                         int port) {
  IPAddressNumber ip;
  bool rv = ParseIPLiteralToNumber(address, &ip);
  DCHECK(rv);
  return std::find(list.begin(),
                   list.end(),
                   IPEndPoint(ip, port)) != list.end();
}

// A wrapper for requests to a HostResolver.
class Request {
 public:
  // Base class of handlers to be executed on completion of requests.
  struct Handler {
    virtual ~Handler() {}
    virtual void Handle(Request* request) = 0;
  };

  Request(const HostResolver::RequestInfo& info,
          RequestPriority priority,
          size_t index,
          HostResolver* resolver,
          Handler* handler)
      : info_(info),
        priority_(priority),
        index_(index),
        resolver_(resolver),
        handler_(handler),
        quit_on_complete_(false),
        result_(ERR_UNEXPECTED),
        handle_(NULL) {}

  int Resolve() {
    DCHECK(resolver_);
    DCHECK(!handle_);
    list_ = AddressList();
    result_ = resolver_->Resolve(
        info_,
        priority_,
        &list_,
        base::Bind(&Request::OnComplete, base::Unretained(this)),
        &handle_,
        BoundNetLog());
    if (!list_.empty())
      EXPECT_EQ(OK, result_);
    return result_;
  }

  int ResolveFromCache() {
    DCHECK(resolver_);
    DCHECK(!handle_);
    return resolver_->ResolveFromCache(info_, &list_, BoundNetLog());
  }

  void Cancel() {
    DCHECK(resolver_);
    DCHECK(handle_);
    resolver_->CancelRequest(handle_);
    handle_ = NULL;
  }

  const HostResolver::RequestInfo& info() const { return info_; }
  size_t index() const { return index_; }
  const AddressList& list() const { return list_; }
  int result() const { return result_; }
  bool completed() const { return result_ != ERR_IO_PENDING; }
  bool pending() const { return handle_ != NULL; }

  bool HasAddress(const std::string& address, int port) const {
    return AddressListContains(list_, address, port);
  }

  // Returns the number of addresses in |list_|.
  unsigned NumberOfAddresses() const {
    return list_.size();
  }

  bool HasOneAddress(const std::string& address, int port) const {
    return HasAddress(address, port) && (NumberOfAddresses() == 1u);
  }

  // Returns ERR_UNEXPECTED if timed out.
  int WaitForResult() {
    if (completed())
      return result_;
    base::CancelableClosure closure(base::MessageLoop::QuitClosure());
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE, closure.callback(), TestTimeouts::action_max_timeout());
    quit_on_complete_ = true;
    base::MessageLoop::current()->Run();
    bool did_quit = !quit_on_complete_;
    quit_on_complete_ = false;
    closure.Cancel();
    if (did_quit)
      return result_;
    else
      return ERR_UNEXPECTED;
  }

 private:
  void OnComplete(int rv) {
    EXPECT_TRUE(pending());
    EXPECT_EQ(ERR_IO_PENDING, result_);
    EXPECT_NE(ERR_IO_PENDING, rv);
    result_ = rv;
    handle_ = NULL;
    if (!list_.empty()) {
      EXPECT_EQ(OK, result_);
      EXPECT_EQ(info_.port(), list_.front().port());
    }
    if (handler_)
      handler_->Handle(this);
    if (quit_on_complete_) {
      base::MessageLoop::current()->Quit();
      quit_on_complete_ = false;
    }
  }

  HostResolver::RequestInfo info_;
  RequestPriority priority_;
  size_t index_;
  HostResolver* resolver_;
  Handler* handler_;
  bool quit_on_complete_;

  AddressList list_;
  int result_;
  HostResolver::RequestHandle handle_;

  DISALLOW_COPY_AND_ASSIGN(Request);
};

// Using LookupAttemptHostResolverProc simulate very long lookups, and control
// which attempt resolves the host.
class LookupAttemptHostResolverProc : public HostResolverProc {
 public:
  LookupAttemptHostResolverProc(HostResolverProc* previous,
                                int attempt_number_to_resolve,
                                int total_attempts)
      : HostResolverProc(previous),
        attempt_number_to_resolve_(attempt_number_to_resolve),
        current_attempt_number_(0),
        total_attempts_(total_attempts),
        total_attempts_resolved_(0),
        resolved_attempt_number_(0),
        all_done_(&lock_) {
  }

  // Test harness will wait for all attempts to finish before checking the
  // results.
  void WaitForAllAttemptsToFinish(const base::TimeDelta& wait_time) {
    base::TimeTicks end_time = base::TimeTicks::Now() + wait_time;
    {
      base::AutoLock auto_lock(lock_);
      while (total_attempts_resolved_ != total_attempts_ &&
          base::TimeTicks::Now() < end_time) {
        all_done_.TimedWait(end_time - base::TimeTicks::Now());
      }
    }
  }

  // All attempts will wait for an attempt to resolve the host.
  void WaitForAnAttemptToComplete() {
    base::TimeDelta wait_time = base::TimeDelta::FromSeconds(60);
    base::TimeTicks end_time = base::TimeTicks::Now() + wait_time;
    {
      base::AutoLock auto_lock(lock_);
      while (resolved_attempt_number_ == 0 && base::TimeTicks::Now() < end_time)
        all_done_.TimedWait(end_time - base::TimeTicks::Now());
    }
    all_done_.Broadcast();  // Tell all waiting attempts to proceed.
  }

  // Returns the number of attempts that have finished the Resolve() method.
  int total_attempts_resolved() { return total_attempts_resolved_; }

  // Returns the first attempt that that has resolved the host.
  int resolved_attempt_number() { return resolved_attempt_number_; }

  // HostResolverProc methods.
  virtual int Resolve(const std::string& host,
                      AddressFamily address_family,
                      HostResolverFlags host_resolver_flags,
                      AddressList* addrlist,
                      int* os_error) OVERRIDE {
    bool wait_for_right_attempt_to_complete = true;
    {
      base::AutoLock auto_lock(lock_);
      ++current_attempt_number_;
      if (current_attempt_number_ == attempt_number_to_resolve_) {
        resolved_attempt_number_ = current_attempt_number_;
        wait_for_right_attempt_to_complete = false;
      }
    }

    if (wait_for_right_attempt_to_complete)
      // Wait for the attempt_number_to_resolve_ attempt to resolve.
      WaitForAnAttemptToComplete();

    int result = ResolveUsingPrevious(host, address_family, host_resolver_flags,
                                      addrlist, os_error);

    {
      base::AutoLock auto_lock(lock_);
      ++total_attempts_resolved_;
    }

    all_done_.Broadcast();  // Tell all attempts to proceed.

    // Since any negative number is considered a network error, with -1 having
    // special meaning (ERR_IO_PENDING). We could return the attempt that has
    // resolved the host as a negative number. For example, if attempt number 3
    // resolves the host, then this method returns -4.
    if (result == OK)
      return -1 - resolved_attempt_number_;
    else
      return result;
  }

 protected:
  virtual ~LookupAttemptHostResolverProc() {}

 private:
  int attempt_number_to_resolve_;
  int current_attempt_number_;  // Incremented whenever Resolve is called.
  int total_attempts_;
  int total_attempts_resolved_;
  int resolved_attempt_number_;

  // All attempts wait for right attempt to be resolve.
  base::Lock lock_;
  base::ConditionVariable all_done_;
};

}  // namespace

class HostResolverImplTest : public testing::Test {
 public:
  static const int kDefaultPort = 80;

  HostResolverImplTest() : proc_(new MockHostResolverProc()) {}

  void CreateResolver() {
    CreateResolverWithLimitsAndParams(kMaxJobs,
                                      DefaultParams(proc_.get()));
  }

  // This HostResolverImpl will only allow 1 outstanding resolve at a time and
  // perform no retries.
  void CreateSerialResolver() {
    HostResolverImpl::ProcTaskParams params = DefaultParams(proc_.get());
    params.max_retry_attempts = 0u;
    CreateResolverWithLimitsAndParams(1u, params);
  }

 protected:
  // A Request::Handler which is a proxy to the HostResolverImplTest fixture.
  struct Handler : public Request::Handler {
    virtual ~Handler() {}

    // Proxy functions so that classes derived from Handler can access them.
    Request* CreateRequest(const HostResolver::RequestInfo& info,
                           RequestPriority priority) {
      return test->CreateRequest(info, priority);
    }
    Request* CreateRequest(const std::string& hostname, int port) {
      return test->CreateRequest(hostname, port);
    }
    Request* CreateRequest(const std::string& hostname) {
      return test->CreateRequest(hostname);
    }
    ScopedVector<Request>& requests() { return test->requests_; }

    void DeleteResolver() { test->resolver_.reset(); }

    HostResolverImplTest* test;
  };

  // testing::Test implementation:
  virtual void SetUp() OVERRIDE {
    CreateResolver();
  }

  virtual void TearDown() OVERRIDE {
    if (resolver_.get())
      EXPECT_EQ(0u, resolver_->num_running_dispatcher_jobs_for_tests());
    EXPECT_FALSE(proc_->HasBlockedRequests());
  }

  virtual void CreateResolverWithLimitsAndParams(
      size_t max_concurrent_resolves,
      const HostResolverImpl::ProcTaskParams& params) {
    HostResolverImpl::Options options = DefaultOptions();
    options.max_concurrent_resolves = max_concurrent_resolves;
    resolver_.reset(new HostResolverImpl(options, NULL));
    resolver_->set_proc_params_for_test(params);
  }

  // The Request will not be made until a call to |Resolve()|, and the Job will
  // not start until released by |proc_->SignalXXX|.
  Request* CreateRequest(const HostResolver::RequestInfo& info,
                         RequestPriority priority) {
    Request* req = new Request(
        info, priority, requests_.size(), resolver_.get(), handler_.get());
    requests_.push_back(req);
    return req;
  }

  Request* CreateRequest(const std::string& hostname,
                         int port,
                         RequestPriority priority,
                         AddressFamily family) {
    HostResolver::RequestInfo info(HostPortPair(hostname, port));
    info.set_address_family(family);
    return CreateRequest(info, priority);
  }

  Request* CreateRequest(const std::string& hostname,
                         int port,
                         RequestPriority priority) {
    return CreateRequest(hostname, port, priority, ADDRESS_FAMILY_UNSPECIFIED);
  }

  Request* CreateRequest(const std::string& hostname, int port) {
    return CreateRequest(hostname, port, MEDIUM);
  }

  Request* CreateRequest(const std::string& hostname) {
    return CreateRequest(hostname, kDefaultPort);
  }

  void set_handler(Handler* handler) {
    handler_.reset(handler);
    handler_->test = this;
  }

  // Friendship is not inherited, so use proxies to access those.
  size_t num_running_dispatcher_jobs() const {
    DCHECK(resolver_.get());
    return resolver_->num_running_dispatcher_jobs_for_tests();
  }

  void set_fallback_to_proctask(bool fallback_to_proctask) {
    DCHECK(resolver_.get());
    resolver_->fallback_to_proctask_ = fallback_to_proctask;
  }

  static unsigned maximum_dns_failures() {
    return HostResolverImpl::kMaximumDnsFailures;
  }

  scoped_refptr<MockHostResolverProc> proc_;
  scoped_ptr<HostResolverImpl> resolver_;
  ScopedVector<Request> requests_;

  scoped_ptr<Handler> handler_;
};

TEST_F(HostResolverImplTest, AsynchronousLookup) {
  proc_->AddRuleForAllFamilies("just.testing", "192.168.1.42");
  proc_->SignalMultiple(1u);

  Request* req = CreateRequest("just.testing", 80);
  EXPECT_EQ(ERR_IO_PENDING, req->Resolve());
  EXPECT_EQ(OK, req->WaitForResult());

  EXPECT_TRUE(req->HasOneAddress("192.168.1.42", 80));

  EXPECT_EQ("just.testing", proc_->GetCaptureList()[0].hostname);
}

TEST_F(HostResolverImplTest, EmptyListMeansNameNotResolved) {
  proc_->AddRuleForAllFamilies("just.testing", "");
  proc_->SignalMultiple(1u);

  Request* req = CreateRequest("just.testing", 80);
  EXPECT_EQ(ERR_IO_PENDING, req->Resolve());
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, req->WaitForResult());
  EXPECT_EQ(0u, req->NumberOfAddresses());
  EXPECT_EQ("just.testing", proc_->GetCaptureList()[0].hostname);
}

TEST_F(HostResolverImplTest, FailedAsynchronousLookup) {
  proc_->AddRuleForAllFamilies(std::string(),
                               "0.0.0.0");  // Default to failures.
  proc_->SignalMultiple(1u);

  Request* req = CreateRequest("just.testing", 80);
  EXPECT_EQ(ERR_IO_PENDING, req->Resolve());
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, req->WaitForResult());

  EXPECT_EQ("just.testing", proc_->GetCaptureList()[0].hostname);

  // Also test that the error is not cached.
  EXPECT_EQ(ERR_DNS_CACHE_MISS, req->ResolveFromCache());
}

TEST_F(HostResolverImplTest, AbortedAsynchronousLookup) {
  Request* req0 = CreateRequest("just.testing", 80);
  EXPECT_EQ(ERR_IO_PENDING, req0->Resolve());

  EXPECT_TRUE(proc_->WaitFor(1u));

  // Resolver is destroyed while job is running on WorkerPool.
  resolver_.reset();

  proc_->SignalAll();

  // To ensure there was no spurious callback, complete with a new resolver.
  CreateResolver();
  Request* req1 = CreateRequest("just.testing", 80);
  EXPECT_EQ(ERR_IO_PENDING, req1->Resolve());

  proc_->SignalMultiple(2u);

  EXPECT_EQ(OK, req1->WaitForResult());

  // This request was canceled.
  EXPECT_FALSE(req0->completed());
}

#if defined(THREAD_SANITIZER)
// Use of WorkerPool in HostResolverImpl causes a data race. crbug.com/334140
#define MAYBE_NumericIPv4Address DISABLED_NumericIPv4Address
#else
#define MAYBE_NumericIPv4Address NumericIPv4Address
#endif
TEST_F(HostResolverImplTest, MAYBE_NumericIPv4Address) {
  // Stevens says dotted quads with AI_UNSPEC resolve to a single sockaddr_in.
  Request* req = CreateRequest("127.1.2.3", 5555);
  EXPECT_EQ(OK, req->Resolve());

  EXPECT_TRUE(req->HasOneAddress("127.1.2.3", 5555));
}

#if defined(THREAD_SANITIZER)
// Use of WorkerPool in HostResolverImpl causes a data race. crbug.com/334140
#define MAYBE_NumericIPv6Address DISABLED_NumericIPv6Address
#else
#define MAYBE_NumericIPv6Address NumericIPv6Address
#endif
TEST_F(HostResolverImplTest, MAYBE_NumericIPv6Address) {
  // Resolve a plain IPv6 address.  Don't worry about [brackets], because
  // the caller should have removed them.
  Request* req = CreateRequest("2001:db8::1", 5555);
  EXPECT_EQ(OK, req->Resolve());

  EXPECT_TRUE(req->HasOneAddress("2001:db8::1", 5555));
}

#if defined(THREAD_SANITIZER)
// Use of WorkerPool in HostResolverImpl causes a data race. crbug.com/334140
#define MAYBE_EmptyHost DISABLED_EmptyHost
#else
#define MAYBE_EmptyHost EmptyHost
#endif
TEST_F(HostResolverImplTest, MAYBE_EmptyHost) {
  Request* req = CreateRequest(std::string(), 5555);
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, req->Resolve());
}

#if defined(THREAD_SANITIZER)
// There's a data race in this test that may lead to use-after-free.
// If the test starts to crash without ThreadSanitizer it needs to be disabled
// globally. See http://crbug.com/268946 (stacks for this test in
// crbug.com/333567).
#define MAYBE_EmptyDotsHost DISABLED_EmptyDotsHost
#else
#define MAYBE_EmptyDotsHost EmptyDotsHost
#endif
TEST_F(HostResolverImplTest, MAYBE_EmptyDotsHost) {
  for (int i = 0; i < 16; ++i) {
    Request* req = CreateRequest(std::string(i, '.'), 5555);
    EXPECT_EQ(ERR_NAME_NOT_RESOLVED, req->Resolve());
  }
}

#if defined(THREAD_SANITIZER)
// There's a data race in this test that may lead to use-after-free.
// If the test starts to crash without ThreadSanitizer it needs to be disabled
// globally. See http://crbug.com/268946.
#define MAYBE_LongHost DISABLED_LongHost
#else
#define MAYBE_LongHost LongHost
#endif
TEST_F(HostResolverImplTest, MAYBE_LongHost) {
  Request* req = CreateRequest(std::string(4097, 'a'), 5555);
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, req->Resolve());
}

TEST_F(HostResolverImplTest, DeDupeRequests) {
  // Start 5 requests, duplicating hosts "a" and "b". Since the resolver_proc is
  // blocked, these should all pile up until we signal it.
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("a", 80)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("b", 80)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("b", 81)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("a", 82)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("b", 83)->Resolve());

  proc_->SignalMultiple(2u);  // One for "a", one for "b".

  for (size_t i = 0; i < requests_.size(); ++i) {
    EXPECT_EQ(OK, requests_[i]->WaitForResult()) << i;
  }
}

TEST_F(HostResolverImplTest, CancelMultipleRequests) {
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("a", 80)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("b", 80)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("b", 81)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("a", 82)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("b", 83)->Resolve());

  // Cancel everything except request for ("a", 82).
  requests_[0]->Cancel();
  requests_[1]->Cancel();
  requests_[2]->Cancel();
  requests_[4]->Cancel();

  proc_->SignalMultiple(2u);  // One for "a", one for "b".

  EXPECT_EQ(OK, requests_[3]->WaitForResult());
}

TEST_F(HostResolverImplTest, CanceledRequestsReleaseJobSlots) {
  // Fill up the dispatcher and queue.
  for (unsigned i = 0; i < kMaxJobs + 1; ++i) {
    std::string hostname = "a_";
    hostname[1] = 'a' + i;
    EXPECT_EQ(ERR_IO_PENDING, CreateRequest(hostname, 80)->Resolve());
    EXPECT_EQ(ERR_IO_PENDING, CreateRequest(hostname, 81)->Resolve());
  }

  EXPECT_TRUE(proc_->WaitFor(kMaxJobs));

  // Cancel all but last two.
  for (unsigned i = 0; i < requests_.size() - 2; ++i) {
    requests_[i]->Cancel();
  }

  EXPECT_TRUE(proc_->WaitFor(kMaxJobs + 1));

  proc_->SignalAll();

  size_t num_requests = requests_.size();
  EXPECT_EQ(OK, requests_[num_requests - 1]->WaitForResult());
  EXPECT_EQ(OK, requests_[num_requests - 2]->result());
}

TEST_F(HostResolverImplTest, CancelWithinCallback) {
  struct MyHandler : public Handler {
    virtual void Handle(Request* req) OVERRIDE {
      // Port 80 is the first request that the callback will be invoked for.
      // While we are executing within that callback, cancel the other requests
      // in the job and start another request.
      if (req->index() == 0) {
        // Once "a:80" completes, it will cancel "a:81" and "a:82".
        requests()[1]->Cancel();
        requests()[2]->Cancel();
      }
    }
  };
  set_handler(new MyHandler());

  for (size_t i = 0; i < 4; ++i) {
    EXPECT_EQ(ERR_IO_PENDING, CreateRequest("a", 80 + i)->Resolve()) << i;
  }

  proc_->SignalMultiple(2u);  // One for "a". One for "finalrequest".

  EXPECT_EQ(OK, requests_[0]->WaitForResult());

  Request* final_request = CreateRequest("finalrequest", 70);
  EXPECT_EQ(ERR_IO_PENDING, final_request->Resolve());
  EXPECT_EQ(OK, final_request->WaitForResult());
  EXPECT_TRUE(requests_[3]->completed());
}

TEST_F(HostResolverImplTest, DeleteWithinCallback) {
  struct MyHandler : public Handler {
    virtual void Handle(Request* req) OVERRIDE {
      EXPECT_EQ("a", req->info().hostname());
      EXPECT_EQ(80, req->info().port());

      DeleteResolver();

      // Quit after returning from OnCompleted (to give it a chance at
      // incorrectly running the cancelled tasks).
      base::MessageLoop::current()->PostTask(FROM_HERE,
                                             base::MessageLoop::QuitClosure());
    }
  };
  set_handler(new MyHandler());

  for (size_t i = 0; i < 4; ++i) {
    EXPECT_EQ(ERR_IO_PENDING, CreateRequest("a", 80 + i)->Resolve()) << i;
  }

  proc_->SignalMultiple(1u);  // One for "a".

  // |MyHandler| will send quit message once all the requests have finished.
  base::MessageLoop::current()->Run();
}

TEST_F(HostResolverImplTest, DeleteWithinAbortedCallback) {
  struct MyHandler : public Handler {
    virtual void Handle(Request* req) OVERRIDE {
      EXPECT_EQ("a", req->info().hostname());
      EXPECT_EQ(80, req->info().port());

      DeleteResolver();

      // Quit after returning from OnCompleted (to give it a chance at
      // incorrectly running the cancelled tasks).
      base::MessageLoop::current()->PostTask(FROM_HERE,
                                             base::MessageLoop::QuitClosure());
    }
  };
  set_handler(new MyHandler());

  // This test assumes that the Jobs will be Aborted in order ["a", "b"]
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("a", 80)->Resolve());
  // HostResolverImpl will be deleted before later Requests can complete.
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("a", 81)->Resolve());
  // Job for 'b' will be aborted before it can complete.
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("b", 82)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("b", 83)->Resolve());

  EXPECT_TRUE(proc_->WaitFor(1u));

  // Triggering an IP address change.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();

  // |MyHandler| will send quit message once all the requests have finished.
  base::MessageLoop::current()->Run();

  EXPECT_EQ(ERR_NETWORK_CHANGED, requests_[0]->result());
  EXPECT_EQ(ERR_IO_PENDING, requests_[1]->result());
  EXPECT_EQ(ERR_IO_PENDING, requests_[2]->result());
  EXPECT_EQ(ERR_IO_PENDING, requests_[3]->result());
  // Clean up.
  proc_->SignalMultiple(requests_.size());
}

TEST_F(HostResolverImplTest, StartWithinCallback) {
  struct MyHandler : public Handler {
    virtual void Handle(Request* req) OVERRIDE {
      if (req->index() == 0) {
        // On completing the first request, start another request for "a".
        // Since caching is disabled, this will result in another async request.
        EXPECT_EQ(ERR_IO_PENDING, CreateRequest("a", 70)->Resolve());
      }
    }
  };
  set_handler(new MyHandler());

  // Turn off caching for this host resolver.
  HostResolver::Options options = DefaultOptions();
  options.enable_caching = false;
  resolver_.reset(new HostResolverImpl(options, NULL));
  resolver_->set_proc_params_for_test(DefaultParams(proc_.get()));

  for (size_t i = 0; i < 4; ++i) {
    EXPECT_EQ(ERR_IO_PENDING, CreateRequest("a", 80 + i)->Resolve()) << i;
  }

  proc_->SignalMultiple(2u);  // One for "a". One for the second "a".

  EXPECT_EQ(OK, requests_[0]->WaitForResult());
  ASSERT_EQ(5u, requests_.size());
  EXPECT_EQ(OK, requests_.back()->WaitForResult());

  EXPECT_EQ(2u, proc_->GetCaptureList().size());
}

TEST_F(HostResolverImplTest, BypassCache) {
  struct MyHandler : public Handler {
    virtual void Handle(Request* req) OVERRIDE {
      if (req->index() == 0) {
        // On completing the first request, start another request for "a".
        // Since caching is enabled, this should complete synchronously.
        std::string hostname = req->info().hostname();
        EXPECT_EQ(OK, CreateRequest(hostname, 70)->Resolve());
        EXPECT_EQ(OK, CreateRequest(hostname, 75)->ResolveFromCache());

        // Ok good. Now make sure that if we ask to bypass the cache, it can no
        // longer service the request synchronously.
        HostResolver::RequestInfo info(HostPortPair(hostname, 71));
        info.set_allow_cached_response(false);
        EXPECT_EQ(ERR_IO_PENDING,
                  CreateRequest(info, DEFAULT_PRIORITY)->Resolve());
      } else if (71 == req->info().port()) {
        // Test is done.
        base::MessageLoop::current()->Quit();
      } else {
        FAIL() << "Unexpected request";
      }
    }
  };
  set_handler(new MyHandler());

  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("a", 80)->Resolve());
  proc_->SignalMultiple(3u);  // Only need two, but be generous.

  // |verifier| will send quit message once all the requests have finished.
  base::MessageLoop::current()->Run();
  EXPECT_EQ(2u, proc_->GetCaptureList().size());
}

// Test that IP address changes flush the cache.
TEST_F(HostResolverImplTest, FlushCacheOnIPAddressChange) {
  proc_->SignalMultiple(2u);  // One before the flush, one after.

  Request* req = CreateRequest("host1", 70);
  EXPECT_EQ(ERR_IO_PENDING, req->Resolve());
  EXPECT_EQ(OK, req->WaitForResult());

  req = CreateRequest("host1", 75);
  EXPECT_EQ(OK, req->Resolve());  // Should complete synchronously.

  // Flush cache by triggering an IP address change.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  base::MessageLoop::current()->RunUntilIdle();  // Notification happens async.

  // Resolve "host1" again -- this time it won't be served from cache, so it
  // will complete asynchronously.
  req = CreateRequest("host1", 80);
  EXPECT_EQ(ERR_IO_PENDING, req->Resolve());
  EXPECT_EQ(OK, req->WaitForResult());
}

// Test that IP address changes send ERR_NETWORK_CHANGED to pending requests.
TEST_F(HostResolverImplTest, AbortOnIPAddressChanged) {
  Request* req = CreateRequest("host1", 70);
  EXPECT_EQ(ERR_IO_PENDING, req->Resolve());

  EXPECT_TRUE(proc_->WaitFor(1u));
  // Triggering an IP address change.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  base::MessageLoop::current()->RunUntilIdle();  // Notification happens async.
  proc_->SignalAll();

  EXPECT_EQ(ERR_NETWORK_CHANGED, req->WaitForResult());
  EXPECT_EQ(0u, resolver_->GetHostCache()->size());
}

// Obey pool constraints after IP address has changed.
TEST_F(HostResolverImplTest, ObeyPoolConstraintsAfterIPAddressChange) {
  // Runs at most one job at a time.
  CreateSerialResolver();
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("a")->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("b")->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("c")->Resolve());

  EXPECT_TRUE(proc_->WaitFor(1u));
  // Triggering an IP address change.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  base::MessageLoop::current()->RunUntilIdle();  // Notification happens async.
  proc_->SignalMultiple(3u);  // Let the false-start go so that we can catch it.

  EXPECT_EQ(ERR_NETWORK_CHANGED, requests_[0]->WaitForResult());

  EXPECT_EQ(1u, num_running_dispatcher_jobs());

  EXPECT_FALSE(requests_[1]->completed());
  EXPECT_FALSE(requests_[2]->completed());

  EXPECT_EQ(OK, requests_[2]->WaitForResult());
  EXPECT_EQ(OK, requests_[1]->result());
}

// Tests that a new Request made from the callback of a previously aborted one
// will not be aborted.
TEST_F(HostResolverImplTest, AbortOnlyExistingRequestsOnIPAddressChange) {
  struct MyHandler : public Handler {
    virtual void Handle(Request* req) OVERRIDE {
      // Start new request for a different hostname to ensure that the order
      // of jobs in HostResolverImpl is not stable.
      std::string hostname;
      if (req->index() == 0)
        hostname = "zzz";
      else if (req->index() == 1)
        hostname = "aaa";
      else if (req->index() == 2)
        hostname = "eee";
      else
        return;  // A request started from within MyHandler.
      EXPECT_EQ(ERR_IO_PENDING, CreateRequest(hostname)->Resolve()) << hostname;
    }
  };
  set_handler(new MyHandler());

  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("bbb")->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("eee")->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("ccc")->Resolve());

  // Wait until all are blocked;
  EXPECT_TRUE(proc_->WaitFor(3u));
  // Trigger an IP address change.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  // This should abort all running jobs.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(ERR_NETWORK_CHANGED, requests_[0]->result());
  EXPECT_EQ(ERR_NETWORK_CHANGED, requests_[1]->result());
  EXPECT_EQ(ERR_NETWORK_CHANGED, requests_[2]->result());
  ASSERT_EQ(6u, requests_.size());
  // Unblock all calls to proc.
  proc_->SignalMultiple(requests_.size());
  // Run until the re-started requests finish.
  EXPECT_EQ(OK, requests_[3]->WaitForResult());
  EXPECT_EQ(OK, requests_[4]->WaitForResult());
  EXPECT_EQ(OK, requests_[5]->WaitForResult());
  // Verify that results of aborted Jobs were not cached.
  EXPECT_EQ(6u, proc_->GetCaptureList().size());
  EXPECT_EQ(3u, resolver_->GetHostCache()->size());
}

// Tests that when the maximum threads is set to 1, requests are dequeued
// in order of priority.
TEST_F(HostResolverImplTest, HigherPriorityRequestsStartedFirst) {
  CreateSerialResolver();

  // Note that at this point the MockHostResolverProc is blocked, so any
  // requests we make will not complete.
  CreateRequest("req0", 80, LOW);
  CreateRequest("req1", 80, MEDIUM);
  CreateRequest("req2", 80, MEDIUM);
  CreateRequest("req3", 80, LOW);
  CreateRequest("req4", 80, HIGHEST);
  CreateRequest("req5", 80, LOW);
  CreateRequest("req6", 80, LOW);
  CreateRequest("req5", 80, HIGHEST);

  for (size_t i = 0; i < requests_.size(); ++i) {
    EXPECT_EQ(ERR_IO_PENDING, requests_[i]->Resolve()) << i;
  }

  // Unblock the resolver thread so the requests can run.
  proc_->SignalMultiple(requests_.size());  // More than needed.

  // Wait for all the requests to complete succesfully.
  for (size_t i = 0; i < requests_.size(); ++i) {
    EXPECT_EQ(OK, requests_[i]->WaitForResult()) << i;
  }

  // Since we have restricted to a single concurrent thread in the jobpool,
  // the requests should complete in order of priority (with the exception
  // of the first request, which gets started right away, since there is
  // nothing outstanding).
  MockHostResolverProc::CaptureList capture_list = proc_->GetCaptureList();
  ASSERT_EQ(7u, capture_list.size());

  EXPECT_EQ("req0", capture_list[0].hostname);
  EXPECT_EQ("req4", capture_list[1].hostname);
  EXPECT_EQ("req5", capture_list[2].hostname);
  EXPECT_EQ("req1", capture_list[3].hostname);
  EXPECT_EQ("req2", capture_list[4].hostname);
  EXPECT_EQ("req3", capture_list[5].hostname);
  EXPECT_EQ("req6", capture_list[6].hostname);
}

// Try cancelling a job which has not started yet.
TEST_F(HostResolverImplTest, CancelPendingRequest) {
  CreateSerialResolver();

  CreateRequest("req0", 80, LOWEST);
  CreateRequest("req1", 80, HIGHEST);  // Will cancel.
  CreateRequest("req2", 80, MEDIUM);
  CreateRequest("req3", 80, LOW);
  CreateRequest("req4", 80, HIGHEST);  // Will cancel.
  CreateRequest("req5", 80, LOWEST);   // Will cancel.
  CreateRequest("req6", 80, MEDIUM);

  // Start all of the requests.
  for (size_t i = 0; i < requests_.size(); ++i) {
    EXPECT_EQ(ERR_IO_PENDING, requests_[i]->Resolve()) << i;
  }

  // Cancel some requests
  requests_[1]->Cancel();
  requests_[4]->Cancel();
  requests_[5]->Cancel();

  // Unblock the resolver thread so the requests can run.
  proc_->SignalMultiple(requests_.size());  // More than needed.

  // Wait for all the requests to complete succesfully.
  for (size_t i = 0; i < requests_.size(); ++i) {
    if (!requests_[i]->pending())
      continue;  // Don't wait for the requests we cancelled.
    EXPECT_EQ(OK, requests_[i]->WaitForResult()) << i;
  }

  // Verify that they called out the the resolver proc (which runs on the
  // resolver thread) in the expected order.
  MockHostResolverProc::CaptureList capture_list = proc_->GetCaptureList();
  ASSERT_EQ(4u, capture_list.size());

  EXPECT_EQ("req0", capture_list[0].hostname);
  EXPECT_EQ("req2", capture_list[1].hostname);
  EXPECT_EQ("req6", capture_list[2].hostname);
  EXPECT_EQ("req3", capture_list[3].hostname);
}

// Test that when too many requests are enqueued, old ones start to be aborted.
TEST_F(HostResolverImplTest, QueueOverflow) {
  CreateSerialResolver();

  // Allow only 3 queued jobs.
  const size_t kMaxPendingJobs = 3u;
  resolver_->SetMaxQueuedJobs(kMaxPendingJobs);

  // Note that at this point the MockHostResolverProc is blocked, so any
  // requests we make will not complete.

  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("req0", 80, LOWEST)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("req1", 80, HIGHEST)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("req2", 80, MEDIUM)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("req3", 80, MEDIUM)->Resolve());

  // At this point, there are 3 enqueued jobs.
  // Insertion of subsequent requests will cause evictions
  // based on priority.

  EXPECT_EQ(ERR_HOST_RESOLVER_QUEUE_TOO_LARGE,
            CreateRequest("req4", 80, LOW)->Resolve());  // Evicts itself!

  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("req5", 80, MEDIUM)->Resolve());
  EXPECT_EQ(ERR_HOST_RESOLVER_QUEUE_TOO_LARGE, requests_[2]->result());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("req6", 80, HIGHEST)->Resolve());
  EXPECT_EQ(ERR_HOST_RESOLVER_QUEUE_TOO_LARGE, requests_[3]->result());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("req7", 80, MEDIUM)->Resolve());
  EXPECT_EQ(ERR_HOST_RESOLVER_QUEUE_TOO_LARGE, requests_[5]->result());

  // Unblock the resolver thread so the requests can run.
  proc_->SignalMultiple(4u);

  // The rest should succeed.
  EXPECT_EQ(OK, requests_[7]->WaitForResult());
  EXPECT_EQ(OK, requests_[0]->result());
  EXPECT_EQ(OK, requests_[1]->result());
  EXPECT_EQ(OK, requests_[6]->result());

  // Verify that they called out the the resolver proc (which runs on the
  // resolver thread) in the expected order.
  MockHostResolverProc::CaptureList capture_list = proc_->GetCaptureList();
  ASSERT_EQ(4u, capture_list.size());

  EXPECT_EQ("req0", capture_list[0].hostname);
  EXPECT_EQ("req1", capture_list[1].hostname);
  EXPECT_EQ("req6", capture_list[2].hostname);
  EXPECT_EQ("req7", capture_list[3].hostname);

  // Verify that the evicted (incomplete) requests were not cached.
  EXPECT_EQ(4u, resolver_->GetHostCache()->size());

  for (size_t i = 0; i < requests_.size(); ++i) {
    EXPECT_TRUE(requests_[i]->completed()) << i;
  }
}

// Tests that after changing the default AddressFamily to IPV4, requests
// with UNSPECIFIED address family map to IPV4.
TEST_F(HostResolverImplTest, SetDefaultAddressFamily_IPv4) {
  CreateSerialResolver();  // To guarantee order of resolutions.

  proc_->AddRule("h1", ADDRESS_FAMILY_IPV4, "1.0.0.1");
  proc_->AddRule("h1", ADDRESS_FAMILY_IPV6, "::2");

  resolver_->SetDefaultAddressFamily(ADDRESS_FAMILY_IPV4);

  CreateRequest("h1", 80, MEDIUM, ADDRESS_FAMILY_UNSPECIFIED);
  CreateRequest("h1", 80, MEDIUM, ADDRESS_FAMILY_IPV4);
  CreateRequest("h1", 80, MEDIUM, ADDRESS_FAMILY_IPV6);

  // Start all of the requests.
  for (size_t i = 0; i < requests_.size(); ++i) {
    EXPECT_EQ(ERR_IO_PENDING, requests_[i]->Resolve()) << i;
  }

  proc_->SignalMultiple(requests_.size());

  // Wait for all the requests to complete.
  for (size_t i = 0u; i < requests_.size(); ++i) {
    EXPECT_EQ(OK, requests_[i]->WaitForResult()) << i;
  }

  // Since the requests all had the same priority and we limited the thread
  // count to 1, they should have completed in the same order as they were
  // requested. Moreover, request0 and request1 will have been serviced by
  // the same job.

  MockHostResolverProc::CaptureList capture_list = proc_->GetCaptureList();
  ASSERT_EQ(2u, capture_list.size());

  EXPECT_EQ("h1", capture_list[0].hostname);
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, capture_list[0].address_family);

  EXPECT_EQ("h1", capture_list[1].hostname);
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, capture_list[1].address_family);

  // Now check that the correct resolved IP addresses were returned.
  EXPECT_TRUE(requests_[0]->HasOneAddress("1.0.0.1", 80));
  EXPECT_TRUE(requests_[1]->HasOneAddress("1.0.0.1", 80));
  EXPECT_TRUE(requests_[2]->HasOneAddress("::2", 80));
}

// This is the exact same test as SetDefaultAddressFamily_IPv4, except the
// default family is set to IPv6 and the family of requests is flipped where
// specified.
TEST_F(HostResolverImplTest, SetDefaultAddressFamily_IPv6) {
  CreateSerialResolver();  // To guarantee order of resolutions.

  // Don't use IPv6 replacements here since some systems don't support it.
  proc_->AddRule("h1", ADDRESS_FAMILY_IPV4, "1.0.0.1");
  proc_->AddRule("h1", ADDRESS_FAMILY_IPV6, "::2");

  resolver_->SetDefaultAddressFamily(ADDRESS_FAMILY_IPV6);

  CreateRequest("h1", 80, MEDIUM, ADDRESS_FAMILY_UNSPECIFIED);
  CreateRequest("h1", 80, MEDIUM, ADDRESS_FAMILY_IPV6);
  CreateRequest("h1", 80, MEDIUM, ADDRESS_FAMILY_IPV4);

  // Start all of the requests.
  for (size_t i = 0; i < requests_.size(); ++i) {
    EXPECT_EQ(ERR_IO_PENDING, requests_[i]->Resolve()) << i;
  }

  proc_->SignalMultiple(requests_.size());

  // Wait for all the requests to complete.
  for (size_t i = 0u; i < requests_.size(); ++i) {
    EXPECT_EQ(OK, requests_[i]->WaitForResult()) << i;
  }

  // Since the requests all had the same priority and we limited the thread
  // count to 1, they should have completed in the same order as they were
  // requested. Moreover, request0 and request1 will have been serviced by
  // the same job.

  MockHostResolverProc::CaptureList capture_list = proc_->GetCaptureList();
  ASSERT_EQ(2u, capture_list.size());

  EXPECT_EQ("h1", capture_list[0].hostname);
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, capture_list[0].address_family);

  EXPECT_EQ("h1", capture_list[1].hostname);
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, capture_list[1].address_family);

  // Now check that the correct resolved IP addresses were returned.
  EXPECT_TRUE(requests_[0]->HasOneAddress("::2", 80));
  EXPECT_TRUE(requests_[1]->HasOneAddress("::2", 80));
  EXPECT_TRUE(requests_[2]->HasOneAddress("1.0.0.1", 80));
}

TEST_F(HostResolverImplTest, ResolveFromCache) {
  proc_->AddRuleForAllFamilies("just.testing", "192.168.1.42");
  proc_->SignalMultiple(1u);  // Need only one.

  HostResolver::RequestInfo info(HostPortPair("just.testing", 80));

  // First hit will miss the cache.
  EXPECT_EQ(ERR_DNS_CACHE_MISS,
            CreateRequest(info, DEFAULT_PRIORITY)->ResolveFromCache());

  // This time, we fetch normally.
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest(info, DEFAULT_PRIORITY)->Resolve());
  EXPECT_EQ(OK, requests_[1]->WaitForResult());

  // Now we should be able to fetch from the cache.
  EXPECT_EQ(OK, CreateRequest(info, DEFAULT_PRIORITY)->ResolveFromCache());
  EXPECT_TRUE(requests_[2]->HasOneAddress("192.168.1.42", 80));
}

// Test the retry attempts simulating host resolver proc that takes too long.
TEST_F(HostResolverImplTest, MultipleAttempts) {
  // Total number of attempts would be 3 and we want the 3rd attempt to resolve
  // the host. First and second attempt will be forced to sleep until they get
  // word that a resolution has completed. The 3rd resolution attempt will try
  // to get done ASAP, and won't sleep..
  int kAttemptNumberToResolve = 3;
  int kTotalAttempts = 3;

  scoped_refptr<LookupAttemptHostResolverProc> resolver_proc(
      new LookupAttemptHostResolverProc(
          NULL, kAttemptNumberToResolve, kTotalAttempts));

  HostResolverImpl::ProcTaskParams params = DefaultParams(resolver_proc.get());

  // Specify smaller interval for unresponsive_delay_ for HostResolverImpl so
  // that unit test runs faster. For example, this test finishes in 1.5 secs
  // (500ms * 3).
  params.unresponsive_delay = base::TimeDelta::FromMilliseconds(500);

  resolver_.reset(new HostResolverImpl(DefaultOptions(), NULL));
  resolver_->set_proc_params_for_test(params);

  // Resolve "host1".
  HostResolver::RequestInfo info(HostPortPair("host1", 70));
  Request* req = CreateRequest(info, DEFAULT_PRIORITY);
  EXPECT_EQ(ERR_IO_PENDING, req->Resolve());

  // Resolve returns -4 to indicate that 3rd attempt has resolved the host.
  EXPECT_EQ(-4, req->WaitForResult());

  resolver_proc->WaitForAllAttemptsToFinish(
      base::TimeDelta::FromMilliseconds(60000));
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(resolver_proc->total_attempts_resolved(), kTotalAttempts);
  EXPECT_EQ(resolver_proc->resolved_attempt_number(), kAttemptNumberToResolve);
}

DnsConfig CreateValidDnsConfig() {
  IPAddressNumber dns_ip;
  bool rv = ParseIPLiteralToNumber("192.168.1.0", &dns_ip);
  EXPECT_TRUE(rv);

  DnsConfig config;
  config.nameservers.push_back(IPEndPoint(dns_ip, dns_protocol::kDefaultPort));
  EXPECT_TRUE(config.IsValid());
  return config;
}

// Specialized fixture for tests of DnsTask.
class HostResolverImplDnsTest : public HostResolverImplTest {
 public:
  HostResolverImplDnsTest() : dns_client_(NULL) {}

 protected:
  // testing::Test implementation:
  virtual void SetUp() OVERRIDE {
    AddDnsRule("nx", dns_protocol::kTypeA, MockDnsClientRule::FAIL, false);
    AddDnsRule("nx", dns_protocol::kTypeAAAA, MockDnsClientRule::FAIL, false);
    AddDnsRule("ok", dns_protocol::kTypeA, MockDnsClientRule::OK, false);
    AddDnsRule("ok", dns_protocol::kTypeAAAA, MockDnsClientRule::OK, false);
    AddDnsRule("4ok", dns_protocol::kTypeA, MockDnsClientRule::OK, false);
    AddDnsRule("4ok", dns_protocol::kTypeAAAA, MockDnsClientRule::EMPTY, false);
    AddDnsRule("6ok", dns_protocol::kTypeA, MockDnsClientRule::EMPTY, false);
    AddDnsRule("6ok", dns_protocol::kTypeAAAA, MockDnsClientRule::OK, false);
    AddDnsRule("4nx", dns_protocol::kTypeA, MockDnsClientRule::OK, false);
    AddDnsRule("4nx", dns_protocol::kTypeAAAA, MockDnsClientRule::FAIL, false);
    AddDnsRule("empty", dns_protocol::kTypeA, MockDnsClientRule::EMPTY, false);
    AddDnsRule("empty", dns_protocol::kTypeAAAA, MockDnsClientRule::EMPTY,
               false);

    AddDnsRule("slow_nx", dns_protocol::kTypeA, MockDnsClientRule::FAIL, true);
    AddDnsRule("slow_nx", dns_protocol::kTypeAAAA, MockDnsClientRule::FAIL,
               true);

    AddDnsRule("4slow_ok", dns_protocol::kTypeA, MockDnsClientRule::OK, true);
    AddDnsRule("4slow_ok", dns_protocol::kTypeAAAA, MockDnsClientRule::OK,
               false);
    AddDnsRule("6slow_ok", dns_protocol::kTypeA, MockDnsClientRule::OK, false);
    AddDnsRule("6slow_ok", dns_protocol::kTypeAAAA, MockDnsClientRule::OK,
               true);
    AddDnsRule("4slow_4ok", dns_protocol::kTypeA, MockDnsClientRule::OK, true);
    AddDnsRule("4slow_4ok", dns_protocol::kTypeAAAA, MockDnsClientRule::EMPTY,
               false);
    AddDnsRule("4slow_4timeout", dns_protocol::kTypeA,
               MockDnsClientRule::TIMEOUT, true);
    AddDnsRule("4slow_4timeout", dns_protocol::kTypeAAAA, MockDnsClientRule::OK,
               false);
    AddDnsRule("4slow_6timeout", dns_protocol::kTypeA,
               MockDnsClientRule::OK, true);
    AddDnsRule("4slow_6timeout", dns_protocol::kTypeAAAA,
               MockDnsClientRule::TIMEOUT, false);
    CreateResolver();
  }

  // HostResolverImplTest implementation:
  virtual void CreateResolverWithLimitsAndParams(
      size_t max_concurrent_resolves,
      const HostResolverImpl::ProcTaskParams& params) OVERRIDE {
    HostResolverImpl::Options options = DefaultOptions();
    options.max_concurrent_resolves = max_concurrent_resolves;
    resolver_.reset(new HostResolverImpl(options, NULL));
    resolver_->set_proc_params_for_test(params);
    // Disable IPv6 support probing.
    resolver_->SetDefaultAddressFamily(ADDRESS_FAMILY_UNSPECIFIED);
    dns_client_ = new MockDnsClient(DnsConfig(), dns_rules_);
    resolver_->SetDnsClient(scoped_ptr<DnsClient>(dns_client_));
  }

  // Adds a rule to |dns_rules_|. Must be followed by |CreateResolver| to apply.
  void AddDnsRule(const std::string& prefix,
                  uint16 qtype,
                  MockDnsClientRule::Result result,
                  bool delay) {
    dns_rules_.push_back(MockDnsClientRule(prefix, qtype, result, delay));
  }

  void ChangeDnsConfig(const DnsConfig& config) {
    NetworkChangeNotifier::SetDnsConfig(config);
    // Notification is delivered asynchronously.
    base::MessageLoop::current()->RunUntilIdle();
  }

  MockDnsClientRuleList dns_rules_;
  // Owned by |resolver_|.
  MockDnsClient* dns_client_;
};

// TODO(szym): Test AbortAllInProgressJobs due to DnsConfig change.

// TODO(cbentzel): Test a mix of requests with different HostResolverFlags.

// Test successful and fallback resolutions in HostResolverImpl::DnsTask.
TEST_F(HostResolverImplDnsTest, DnsTask) {
  resolver_->SetDefaultAddressFamily(ADDRESS_FAMILY_IPV4);

  proc_->AddRuleForAllFamilies("nx_succeed", "192.168.1.102");
  // All other hostnames will fail in proc_.

  // Initially there is no config, so client should not be invoked.
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("ok_fail", 80)->Resolve());
  proc_->SignalMultiple(requests_.size());

  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, requests_[0]->WaitForResult());

  ChangeDnsConfig(CreateValidDnsConfig());

  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("ok_fail", 80)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("nx_fail", 80)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("nx_succeed", 80)->Resolve());

  proc_->SignalMultiple(requests_.size());

  for (size_t i = 1; i < requests_.size(); ++i)
    EXPECT_NE(ERR_UNEXPECTED, requests_[i]->WaitForResult()) << i;

  EXPECT_EQ(OK, requests_[1]->result());
  // Resolved by MockDnsClient.
  EXPECT_TRUE(requests_[1]->HasOneAddress("127.0.0.1", 80));
  // Fallback to ProcTask.
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, requests_[2]->result());
  EXPECT_EQ(OK, requests_[3]->result());
  EXPECT_TRUE(requests_[3]->HasOneAddress("192.168.1.102", 80));
}

// Test successful and failing resolutions in HostResolverImpl::DnsTask when
// fallback to ProcTask is disabled.
TEST_F(HostResolverImplDnsTest, NoFallbackToProcTask) {
  resolver_->SetDefaultAddressFamily(ADDRESS_FAMILY_IPV4);
  set_fallback_to_proctask(false);

  proc_->AddRuleForAllFamilies("nx_succeed", "192.168.1.102");
  // All other hostnames will fail in proc_.

  // Set empty DnsConfig.
  ChangeDnsConfig(DnsConfig());
  // Initially there is no config, so client should not be invoked.
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("ok_fail", 80)->Resolve());
  // There is no config, so fallback to ProcTask must work.
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("nx_succeed", 80)->Resolve());
  proc_->SignalMultiple(requests_.size());

  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, requests_[0]->WaitForResult());
  EXPECT_EQ(OK, requests_[1]->WaitForResult());
  EXPECT_TRUE(requests_[1]->HasOneAddress("192.168.1.102", 80));

  ChangeDnsConfig(CreateValidDnsConfig());

  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("ok_abort", 80)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("nx_abort", 80)->Resolve());

  // Simulate the case when the preference or policy has disabled the DNS client
  // causing AbortDnsTasks.
  resolver_->SetDnsClient(
      scoped_ptr<DnsClient>(new MockDnsClient(DnsConfig(), dns_rules_)));
  ChangeDnsConfig(CreateValidDnsConfig());

  // First request is resolved by MockDnsClient, others should fail due to
  // disabled fallback to ProcTask.
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("ok_fail", 80)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("nx_fail", 80)->Resolve());
  proc_->SignalMultiple(requests_.size());

  // Aborted due to Network Change.
  EXPECT_EQ(ERR_NETWORK_CHANGED, requests_[2]->WaitForResult());
  EXPECT_EQ(ERR_NETWORK_CHANGED, requests_[3]->WaitForResult());
  // Resolved by MockDnsClient.
  EXPECT_EQ(OK, requests_[4]->WaitForResult());
  EXPECT_TRUE(requests_[4]->HasOneAddress("127.0.0.1", 80));
  // Fallback to ProcTask is disabled.
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, requests_[5]->WaitForResult());
}

// Test behavior of OnDnsTaskFailure when Job is aborted.
TEST_F(HostResolverImplDnsTest, OnDnsTaskFailureAbortedJob) {
  resolver_->SetDefaultAddressFamily(ADDRESS_FAMILY_IPV4);
  ChangeDnsConfig(CreateValidDnsConfig());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("nx_abort", 80)->Resolve());
  // Abort all jobs here.
  CreateResolver();
  proc_->SignalMultiple(requests_.size());
  // Run to completion.
  base::MessageLoop::current()->RunUntilIdle();  // Notification happens async.
  // It shouldn't crash during OnDnsTaskFailure callbacks.
  EXPECT_EQ(ERR_IO_PENDING, requests_[0]->result());

  // Repeat test with Fallback to ProcTask disabled
  resolver_->SetDefaultAddressFamily(ADDRESS_FAMILY_IPV4);
  set_fallback_to_proctask(false);
  ChangeDnsConfig(CreateValidDnsConfig());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("nx_abort", 80)->Resolve());
  // Abort all jobs here.
  CreateResolver();
  // Run to completion.
  base::MessageLoop::current()->RunUntilIdle();  // Notification happens async.
  // It shouldn't crash during OnDnsTaskFailure callbacks.
  EXPECT_EQ(ERR_IO_PENDING, requests_[1]->result());
}

TEST_F(HostResolverImplDnsTest, DnsTaskUnspec) {
  ChangeDnsConfig(CreateValidDnsConfig());

  proc_->AddRuleForAllFamilies("4nx", "192.168.1.101");
  // All other hostnames will fail in proc_.

  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("ok", 80)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("4ok", 80)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("6ok", 80)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("4nx", 80)->Resolve());

  proc_->SignalMultiple(requests_.size());

  for (size_t i = 0; i < requests_.size(); ++i)
    EXPECT_EQ(OK, requests_[i]->WaitForResult()) << i;

  EXPECT_EQ(2u, requests_[0]->NumberOfAddresses());
  EXPECT_TRUE(requests_[0]->HasAddress("127.0.0.1", 80));
  EXPECT_TRUE(requests_[0]->HasAddress("::1", 80));
  EXPECT_EQ(1u, requests_[1]->NumberOfAddresses());
  EXPECT_TRUE(requests_[1]->HasAddress("127.0.0.1", 80));
  EXPECT_EQ(1u, requests_[2]->NumberOfAddresses());
  EXPECT_TRUE(requests_[2]->HasAddress("::1", 80));
  EXPECT_EQ(1u, requests_[3]->NumberOfAddresses());
  EXPECT_TRUE(requests_[3]->HasAddress("192.168.1.101", 80));
}

TEST_F(HostResolverImplDnsTest, ServeFromHosts) {
  // Initially, use empty HOSTS file.
  DnsConfig config = CreateValidDnsConfig();
  ChangeDnsConfig(config);

  proc_->AddRuleForAllFamilies(std::string(),
                               std::string());  // Default to failures.
  proc_->SignalMultiple(1u);  // For the first request which misses.

  Request* req0 = CreateRequest("nx_ipv4", 80);
  EXPECT_EQ(ERR_IO_PENDING, req0->Resolve());
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, req0->WaitForResult());

  IPAddressNumber local_ipv4, local_ipv6;
  ASSERT_TRUE(ParseIPLiteralToNumber("127.0.0.1", &local_ipv4));
  ASSERT_TRUE(ParseIPLiteralToNumber("::1", &local_ipv6));

  DnsHosts hosts;
  hosts[DnsHostsKey("nx_ipv4", ADDRESS_FAMILY_IPV4)] = local_ipv4;
  hosts[DnsHostsKey("nx_ipv6", ADDRESS_FAMILY_IPV6)] = local_ipv6;
  hosts[DnsHostsKey("nx_both", ADDRESS_FAMILY_IPV4)] = local_ipv4;
  hosts[DnsHostsKey("nx_both", ADDRESS_FAMILY_IPV6)] = local_ipv6;

  // Update HOSTS file.
  config.hosts = hosts;
  ChangeDnsConfig(config);

  Request* req1 = CreateRequest("nx_ipv4", 80);
  EXPECT_EQ(OK, req1->Resolve());
  EXPECT_TRUE(req1->HasOneAddress("127.0.0.1", 80));

  Request* req2 = CreateRequest("nx_ipv6", 80);
  EXPECT_EQ(OK, req2->Resolve());
  EXPECT_TRUE(req2->HasOneAddress("::1", 80));

  Request* req3 = CreateRequest("nx_both", 80);
  EXPECT_EQ(OK, req3->Resolve());
  EXPECT_TRUE(req3->HasAddress("127.0.0.1", 80) &&
              req3->HasAddress("::1", 80));

  // Requests with specified AddressFamily.
  Request* req4 = CreateRequest("nx_ipv4", 80, MEDIUM, ADDRESS_FAMILY_IPV4);
  EXPECT_EQ(OK, req4->Resolve());
  EXPECT_TRUE(req4->HasOneAddress("127.0.0.1", 80));

  Request* req5 = CreateRequest("nx_ipv6", 80, MEDIUM, ADDRESS_FAMILY_IPV6);
  EXPECT_EQ(OK, req5->Resolve());
  EXPECT_TRUE(req5->HasOneAddress("::1", 80));

  // Request with upper case.
  Request* req6 = CreateRequest("nx_IPV4", 80);
  EXPECT_EQ(OK, req6->Resolve());
  EXPECT_TRUE(req6->HasOneAddress("127.0.0.1", 80));
}

TEST_F(HostResolverImplDnsTest, BypassDnsTask) {
  ChangeDnsConfig(CreateValidDnsConfig());

  proc_->AddRuleForAllFamilies(std::string(),
                               std::string());  // Default to failures.

  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("ok.local", 80)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("ok.local.", 80)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("oklocal", 80)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("oklocal.", 80)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("ok", 80)->Resolve());

  proc_->SignalMultiple(requests_.size());

  for (size_t i = 0; i < 2; ++i)
    EXPECT_EQ(ERR_NAME_NOT_RESOLVED, requests_[i]->WaitForResult()) << i;

  for (size_t i = 2; i < requests_.size(); ++i)
    EXPECT_EQ(OK, requests_[i]->WaitForResult()) << i;
}

TEST_F(HostResolverImplDnsTest, SystemOnlyBypassesDnsTask) {
  ChangeDnsConfig(CreateValidDnsConfig());

  proc_->AddRuleForAllFamilies(std::string(), std::string());

  HostResolver::RequestInfo info_bypass(HostPortPair("ok", 80));
  info_bypass.set_host_resolver_flags(HOST_RESOLVER_SYSTEM_ONLY);
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest(info_bypass, MEDIUM)->Resolve());

  HostResolver::RequestInfo info(HostPortPair("ok", 80));
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest(info, MEDIUM)->Resolve());

  proc_->SignalMultiple(requests_.size());

  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, requests_[0]->WaitForResult());
  EXPECT_EQ(OK, requests_[1]->WaitForResult());
}

TEST_F(HostResolverImplDnsTest, DisableDnsClientOnPersistentFailure) {
  ChangeDnsConfig(CreateValidDnsConfig());

  proc_->AddRuleForAllFamilies(std::string(),
                               std::string());  // Default to failures.

  // Check that DnsTask works.
  Request* req = CreateRequest("ok_1", 80);
  EXPECT_EQ(ERR_IO_PENDING, req->Resolve());
  EXPECT_EQ(OK, req->WaitForResult());

  for (unsigned i = 0; i < maximum_dns_failures(); ++i) {
    // Use custom names to require separate Jobs.
    std::string hostname = base::StringPrintf("nx_%u", i);
    // Ensure fallback to ProcTask succeeds.
    proc_->AddRuleForAllFamilies(hostname, "192.168.1.101");
    EXPECT_EQ(ERR_IO_PENDING, CreateRequest(hostname, 80)->Resolve()) << i;
  }

  proc_->SignalMultiple(requests_.size());

  for (size_t i = 0; i < requests_.size(); ++i)
    EXPECT_EQ(OK, requests_[i]->WaitForResult()) << i;

  ASSERT_FALSE(proc_->HasBlockedRequests());

  // DnsTask should be disabled by now.
  req = CreateRequest("ok_2", 80);
  EXPECT_EQ(ERR_IO_PENDING, req->Resolve());
  proc_->SignalMultiple(1u);
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, req->WaitForResult());

  // Check that it is re-enabled after DNS change.
  ChangeDnsConfig(CreateValidDnsConfig());
  req = CreateRequest("ok_3", 80);
  EXPECT_EQ(ERR_IO_PENDING, req->Resolve());
  EXPECT_EQ(OK, req->WaitForResult());
}

TEST_F(HostResolverImplDnsTest, DontDisableDnsClientOnSporadicFailure) {
  ChangeDnsConfig(CreateValidDnsConfig());

  // |proc_| defaults to successes.

  // 20 failures interleaved with 20 successes.
  for (unsigned i = 0; i < 40; ++i) {
    // Use custom names to require separate Jobs.
    std::string hostname = (i % 2) == 0 ? base::StringPrintf("nx_%u", i)
                                        : base::StringPrintf("ok_%u", i);
    EXPECT_EQ(ERR_IO_PENDING, CreateRequest(hostname, 80)->Resolve()) << i;
  }

  proc_->SignalMultiple(requests_.size());

  for (size_t i = 0; i < requests_.size(); ++i)
    EXPECT_EQ(OK, requests_[i]->WaitForResult()) << i;

  // Make |proc_| default to failures.
  proc_->AddRuleForAllFamilies(std::string(), std::string());

  // DnsTask should still be enabled.
  Request* req = CreateRequest("ok_last", 80);
  EXPECT_EQ(ERR_IO_PENDING, req->Resolve());
  EXPECT_EQ(OK, req->WaitForResult());
}

// Confirm that resolving "localhost" is unrestricted even if there are no
// global IPv6 address. See SystemHostResolverCall for rationale.
// Test both the DnsClient and system host resolver paths.
TEST_F(HostResolverImplDnsTest, DualFamilyLocalhost) {
  // Use regular SystemHostResolverCall!
  scoped_refptr<HostResolverProc> proc(new SystemHostResolverProc());
  resolver_.reset(new HostResolverImpl(DefaultOptions(), NULL));
  resolver_->set_proc_params_for_test(DefaultParams(proc.get()));

  resolver_->SetDnsClient(
      scoped_ptr<DnsClient>(new MockDnsClient(DnsConfig(), dns_rules_)));
  resolver_->SetDefaultAddressFamily(ADDRESS_FAMILY_IPV4);

  // Get the expected output.
  AddressList addrlist;
  int rv = proc->Resolve("localhost", ADDRESS_FAMILY_UNSPECIFIED, 0, &addrlist,
                         NULL);
  if (rv != OK)
    return;

  for (unsigned i = 0; i < addrlist.size(); ++i)
    LOG(WARNING) << addrlist[i].ToString();

  bool saw_ipv4 = AddressListContains(addrlist, "127.0.0.1", 0);
  bool saw_ipv6 = AddressListContains(addrlist, "::1", 0);
  if (!saw_ipv4 && !saw_ipv6)
    return;

  HostResolver::RequestInfo info(HostPortPair("localhost", 80));
  info.set_address_family(ADDRESS_FAMILY_UNSPECIFIED);
  info.set_host_resolver_flags(HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6);

  // Try without DnsClient.
  ChangeDnsConfig(DnsConfig());
  Request* req = CreateRequest(info, DEFAULT_PRIORITY);
  // It is resolved via getaddrinfo, so expect asynchronous result.
  EXPECT_EQ(ERR_IO_PENDING, req->Resolve());
  EXPECT_EQ(OK, req->WaitForResult());

  EXPECT_EQ(saw_ipv4, req->HasAddress("127.0.0.1", 80));
  EXPECT_EQ(saw_ipv6, req->HasAddress("::1", 80));

  // Configure DnsClient with dual-host HOSTS file.
  DnsConfig config = CreateValidDnsConfig();
  DnsHosts hosts;
  IPAddressNumber local_ipv4, local_ipv6;
  ASSERT_TRUE(ParseIPLiteralToNumber("127.0.0.1", &local_ipv4));
  ASSERT_TRUE(ParseIPLiteralToNumber("::1", &local_ipv6));
  if (saw_ipv4)
    hosts[DnsHostsKey("localhost", ADDRESS_FAMILY_IPV4)] = local_ipv4;
  if (saw_ipv6)
    hosts[DnsHostsKey("localhost", ADDRESS_FAMILY_IPV6)] = local_ipv6;
  config.hosts = hosts;

  ChangeDnsConfig(config);
  req = CreateRequest(info, DEFAULT_PRIORITY);
  // Expect synchronous resolution from DnsHosts.
  EXPECT_EQ(OK, req->Resolve());

  EXPECT_EQ(saw_ipv4, req->HasAddress("127.0.0.1", 80));
  EXPECT_EQ(saw_ipv6, req->HasAddress("::1", 80));
}

// Cancel a request with a single DNS transaction active.
TEST_F(HostResolverImplDnsTest, CancelWithOneTransactionActive) {
  resolver_->SetDefaultAddressFamily(ADDRESS_FAMILY_IPV4);
  ChangeDnsConfig(CreateValidDnsConfig());

  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("ok", 80)->Resolve());
  EXPECT_EQ(1u, num_running_dispatcher_jobs());
  requests_[0]->Cancel();

  // Dispatcher state checked in TearDown.
}

// Cancel a request with a single DNS transaction active and another pending.
TEST_F(HostResolverImplDnsTest, CancelWithOneTransactionActiveOnePending) {
  CreateSerialResolver();
  resolver_->SetDefaultAddressFamily(ADDRESS_FAMILY_UNSPECIFIED);
  ChangeDnsConfig(CreateValidDnsConfig());

  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("ok", 80)->Resolve());
  EXPECT_EQ(1u, num_running_dispatcher_jobs());
  requests_[0]->Cancel();

  // Dispatcher state checked in TearDown.
}

// Cancel a request with two DNS transactions active.
TEST_F(HostResolverImplDnsTest, CancelWithTwoTransactionsActive) {
  resolver_->SetDefaultAddressFamily(ADDRESS_FAMILY_UNSPECIFIED);
  ChangeDnsConfig(CreateValidDnsConfig());

  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("ok", 80)->Resolve());
  EXPECT_EQ(2u, num_running_dispatcher_jobs());
  requests_[0]->Cancel();

  // Dispatcher state checked in TearDown.
}

// Delete a resolver with some active requests and some queued requests.
TEST_F(HostResolverImplDnsTest, DeleteWithActiveTransactions) {
  // At most 10 Jobs active at once.
  CreateResolverWithLimitsAndParams(10u, DefaultParams(proc_.get()));

  resolver_->SetDefaultAddressFamily(ADDRESS_FAMILY_UNSPECIFIED);
  ChangeDnsConfig(CreateValidDnsConfig());

  // First active job is an IPv4 request.
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("ok", 80, MEDIUM,
                                          ADDRESS_FAMILY_IPV4)->Resolve());

  // Add 10 more DNS lookups for different hostnames.  First 4 should have two
  // active jobs, next one has a single active job, and one pending.  Others
  // should all be queued.
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(ERR_IO_PENDING, CreateRequest(
        base::StringPrintf("ok%i", i))->Resolve());
  }
  EXPECT_EQ(10u, num_running_dispatcher_jobs());

  resolver_.reset();
}

// Cancel a request with only the IPv6 transaction active.
TEST_F(HostResolverImplDnsTest, CancelWithIPv6TransactionActive) {
  resolver_->SetDefaultAddressFamily(ADDRESS_FAMILY_UNSPECIFIED);
  ChangeDnsConfig(CreateValidDnsConfig());

  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("6slow_ok", 80)->Resolve());
  EXPECT_EQ(2u, num_running_dispatcher_jobs());

  // The IPv4 request should complete, the IPv6 request is still pending.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, num_running_dispatcher_jobs());
  requests_[0]->Cancel();

  // Dispatcher state checked in TearDown.
}

// Cancel a request with only the IPv4 transaction pending.
TEST_F(HostResolverImplDnsTest, CancelWithIPv4TransactionPending) {
  set_fallback_to_proctask(false);
  resolver_->SetDefaultAddressFamily(ADDRESS_FAMILY_UNSPECIFIED);
  ChangeDnsConfig(CreateValidDnsConfig());

  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("4slow_ok", 80)->Resolve());
  EXPECT_EQ(2u, num_running_dispatcher_jobs());

  // The IPv6 request should complete, the IPv4 request is still pending.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, num_running_dispatcher_jobs());

  requests_[0]->Cancel();
}

// Test cases where AAAA completes first.
TEST_F(HostResolverImplDnsTest, AAAACompletesFirst) {
  set_fallback_to_proctask(false);
  resolver_->SetDefaultAddressFamily(ADDRESS_FAMILY_UNSPECIFIED);
  ChangeDnsConfig(CreateValidDnsConfig());

  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("4slow_ok", 80)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("4slow_4ok", 80)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("4slow_4timeout", 80)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("4slow_6timeout", 80)->Resolve());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(requests_[0]->completed());
  EXPECT_FALSE(requests_[1]->completed());
  EXPECT_FALSE(requests_[2]->completed());
  // The IPv6 of the third request should have failed and resulted in cancelling
  // the IPv4 request.
  EXPECT_TRUE(requests_[3]->completed());
  EXPECT_EQ(ERR_DNS_TIMED_OUT, requests_[3]->result());
  EXPECT_EQ(3u, num_running_dispatcher_jobs());

  dns_client_->CompleteDelayedTransactions();
  EXPECT_TRUE(requests_[0]->completed());
  EXPECT_EQ(OK, requests_[0]->result());
  EXPECT_EQ(2u, requests_[0]->NumberOfAddresses());
  EXPECT_TRUE(requests_[0]->HasAddress("127.0.0.1", 80));
  EXPECT_TRUE(requests_[0]->HasAddress("::1", 80));

  EXPECT_TRUE(requests_[1]->completed());
  EXPECT_EQ(OK, requests_[1]->result());
  EXPECT_EQ(1u, requests_[1]->NumberOfAddresses());
  EXPECT_TRUE(requests_[1]->HasAddress("127.0.0.1", 80));

  EXPECT_TRUE(requests_[2]->completed());
  EXPECT_EQ(ERR_DNS_TIMED_OUT, requests_[2]->result());
}

// Test the case where only a single transaction slot is available.
TEST_F(HostResolverImplDnsTest, SerialResolver) {
  CreateSerialResolver();
  set_fallback_to_proctask(false);
  resolver_->SetDefaultAddressFamily(ADDRESS_FAMILY_UNSPECIFIED);
  ChangeDnsConfig(CreateValidDnsConfig());

  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("ok", 80)->Resolve());
  EXPECT_EQ(1u, num_running_dispatcher_jobs());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(requests_[0]->completed());
  EXPECT_EQ(OK, requests_[0]->result());
  EXPECT_EQ(2u, requests_[0]->NumberOfAddresses());
  EXPECT_TRUE(requests_[0]->HasAddress("127.0.0.1", 80));
  EXPECT_TRUE(requests_[0]->HasAddress("::1", 80));
}

// Test the case where the AAAA query is started when another transaction
// completes.
TEST_F(HostResolverImplDnsTest, AAAAStartsAfterOtherJobFinishes) {
  CreateResolverWithLimitsAndParams(2u, DefaultParams(proc_.get()));
  set_fallback_to_proctask(false);
  resolver_->SetDefaultAddressFamily(ADDRESS_FAMILY_UNSPECIFIED);
  ChangeDnsConfig(CreateValidDnsConfig());

  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("ok", 80, MEDIUM,
                                          ADDRESS_FAMILY_IPV4)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING,
            CreateRequest("4slow_ok", 80, MEDIUM)->Resolve());
  // An IPv4 request should have been started pending for each job.
  EXPECT_EQ(2u, num_running_dispatcher_jobs());

  // Request 0's IPv4 request should complete, starting Request 1's IPv6
  // request, which should also complete.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, num_running_dispatcher_jobs());
  EXPECT_TRUE(requests_[0]->completed());
  EXPECT_FALSE(requests_[1]->completed());

  dns_client_->CompleteDelayedTransactions();
  EXPECT_TRUE(requests_[1]->completed());
  EXPECT_EQ(OK, requests_[1]->result());
  EXPECT_EQ(2u, requests_[1]->NumberOfAddresses());
  EXPECT_TRUE(requests_[1]->HasAddress("127.0.0.1", 80));
  EXPECT_TRUE(requests_[1]->HasAddress("::1", 80));
}

// Tests the case that a Job with a single transaction receives an empty address
// list, triggering fallback to ProcTask.
TEST_F(HostResolverImplDnsTest, IPv4EmptyFallback) {
  ChangeDnsConfig(CreateValidDnsConfig());
  proc_->AddRuleForAllFamilies("empty_fallback", "192.168.0.1");
  proc_->SignalMultiple(1u);
  EXPECT_EQ(ERR_IO_PENDING,
            CreateRequest("empty_fallback", 80, MEDIUM,
                          ADDRESS_FAMILY_IPV4)->Resolve());
  EXPECT_EQ(OK, requests_[0]->WaitForResult());
  EXPECT_TRUE(requests_[0]->HasOneAddress("192.168.0.1", 80));
}

// Tests the case that a Job with two transactions receives two empty address
// lists, triggering fallback to ProcTask.
TEST_F(HostResolverImplDnsTest, UnspecEmptyFallback) {
  ChangeDnsConfig(CreateValidDnsConfig());
  proc_->AddRuleForAllFamilies("empty_fallback", "192.168.0.1");
  proc_->SignalMultiple(1u);
  EXPECT_EQ(ERR_IO_PENDING,
            CreateRequest("empty_fallback", 80, MEDIUM,
                          ADDRESS_FAMILY_UNSPECIFIED)->Resolve());
  EXPECT_EQ(OK, requests_[0]->WaitForResult());
  EXPECT_TRUE(requests_[0]->HasOneAddress("192.168.0.1", 80));
}

// Tests getting a new invalid DnsConfig while there are active DnsTasks.
TEST_F(HostResolverImplDnsTest, InvalidDnsConfigWithPendingRequests) {
  // At most 3 jobs active at once.  This number is important, since we want to
  // make sure that aborting the first HostResolverImpl::Job does not trigger
  // another DnsTransaction on the second Job when it releases its second
  // prioritized dispatcher slot.
  CreateResolverWithLimitsAndParams(3u, DefaultParams(proc_.get()));

  resolver_->SetDefaultAddressFamily(ADDRESS_FAMILY_UNSPECIFIED);
  ChangeDnsConfig(CreateValidDnsConfig());

  proc_->AddRuleForAllFamilies("slow_nx1", "192.168.0.1");
  proc_->AddRuleForAllFamilies("slow_nx2", "192.168.0.2");
  proc_->AddRuleForAllFamilies("ok", "192.168.0.3");

  // First active job gets two slots.
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("slow_nx1")->Resolve());
  // Next job gets one slot, and waits on another.
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("slow_nx2")->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("ok")->Resolve());

  EXPECT_EQ(3u, num_running_dispatcher_jobs());

  // Clear DNS config.  Two in-progress jobs should be aborted, and the next one
  // should use a ProcTask.
  ChangeDnsConfig(DnsConfig());
  EXPECT_EQ(ERR_NETWORK_CHANGED, requests_[0]->WaitForResult());
  EXPECT_EQ(ERR_NETWORK_CHANGED, requests_[1]->WaitForResult());

  // Finish up the third job.  Should bypass the DnsClient, and get its results
  // from MockHostResolverProc.
  EXPECT_FALSE(requests_[2]->completed());
  proc_->SignalMultiple(1u);
  EXPECT_EQ(OK, requests_[2]->WaitForResult());
  EXPECT_TRUE(requests_[2]->HasOneAddress("192.168.0.3", 80));
}

// Tests the case that DnsClient is automatically disabled due to failures
// while there are active DnsTasks.
TEST_F(HostResolverImplDnsTest,
       AutomaticallyDisableDnsClientWithPendingRequests) {
  // Trying different limits is important for this test:  Different limits
  // result in different behavior when aborting in-progress DnsTasks.  Having
  // a DnsTask that has one job active and one in the queue when another job
  // occupying two slots has its DnsTask aborted is the case most likely to run
  // into problems.
  for (size_t limit = 1u; limit < 6u; ++limit) {
    CreateResolverWithLimitsAndParams(limit, DefaultParams(proc_.get()));

    resolver_->SetDefaultAddressFamily(ADDRESS_FAMILY_UNSPECIFIED);
    ChangeDnsConfig(CreateValidDnsConfig());

    // Queue up enough failures to disable DnsTasks.  These will all fall back
    // to ProcTasks, and succeed there.
    for (unsigned i = 0u; i < maximum_dns_failures(); ++i) {
      std::string host = base::StringPrintf("nx%u", i);
      proc_->AddRuleForAllFamilies(host, "192.168.0.1");
      EXPECT_EQ(ERR_IO_PENDING, CreateRequest(host)->Resolve());
    }

    // These requests should all bypass DnsTasks, due to the above failures,
    // so should end up using ProcTasks.
    proc_->AddRuleForAllFamilies("slow_ok1", "192.168.0.2");
    EXPECT_EQ(ERR_IO_PENDING, CreateRequest("slow_ok1")->Resolve());
    proc_->AddRuleForAllFamilies("slow_ok2", "192.168.0.3");
    EXPECT_EQ(ERR_IO_PENDING, CreateRequest("slow_ok2")->Resolve());
    proc_->AddRuleForAllFamilies("slow_ok3", "192.168.0.4");
    EXPECT_EQ(ERR_IO_PENDING, CreateRequest("slow_ok3")->Resolve());
    proc_->SignalMultiple(maximum_dns_failures() + 3);

    for (size_t i = 0u; i < maximum_dns_failures(); ++i) {
      EXPECT_EQ(OK, requests_[i]->WaitForResult());
      EXPECT_TRUE(requests_[i]->HasOneAddress("192.168.0.1", 80));
    }

    EXPECT_EQ(OK, requests_[maximum_dns_failures()]->WaitForResult());
    EXPECT_TRUE(requests_[maximum_dns_failures()]->HasOneAddress(
                    "192.168.0.2", 80));
    EXPECT_EQ(OK, requests_[maximum_dns_failures() + 1]->WaitForResult());
    EXPECT_TRUE(requests_[maximum_dns_failures() + 1]->HasOneAddress(
                    "192.168.0.3", 80));
    EXPECT_EQ(OK, requests_[maximum_dns_failures() + 2]->WaitForResult());
    EXPECT_TRUE(requests_[maximum_dns_failures() + 2]->HasOneAddress(
                    "192.168.0.4", 80));
    requests_.clear();
  }
}

// Tests a call to SetDnsClient while there are active DnsTasks.
TEST_F(HostResolverImplDnsTest, ManuallyDisableDnsClientWithPendingRequests) {
  // At most 3 jobs active at once.  This number is important, since we want to
  // make sure that aborting the first HostResolverImpl::Job does not trigger
  // another DnsTransaction on the second Job when it releases its second
  // prioritized dispatcher slot.
  CreateResolverWithLimitsAndParams(3u, DefaultParams(proc_.get()));

  resolver_->SetDefaultAddressFamily(ADDRESS_FAMILY_UNSPECIFIED);
  ChangeDnsConfig(CreateValidDnsConfig());

  proc_->AddRuleForAllFamilies("slow_ok1", "192.168.0.1");
  proc_->AddRuleForAllFamilies("slow_ok2", "192.168.0.2");
  proc_->AddRuleForAllFamilies("ok", "192.168.0.3");

  // First active job gets two slots.
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("slow_ok1")->Resolve());
  // Next job gets one slot, and waits on another.
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("slow_ok2")->Resolve());
  // Next one is queued.
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("ok")->Resolve());

  EXPECT_EQ(3u, num_running_dispatcher_jobs());

  // Clear DnsClient.  The two in-progress jobs should fall back to a ProcTask,
  // and the next one should be started with a ProcTask.
  resolver_->SetDnsClient(scoped_ptr<DnsClient>());

  // All three in-progress requests should now be running a ProcTask.
  EXPECT_EQ(3u, num_running_dispatcher_jobs());
  proc_->SignalMultiple(3u);

  EXPECT_EQ(OK, requests_[0]->WaitForResult());
  EXPECT_TRUE(requests_[0]->HasOneAddress("192.168.0.1", 80));
  EXPECT_EQ(OK, requests_[1]->WaitForResult());
  EXPECT_TRUE(requests_[1]->HasOneAddress("192.168.0.2", 80));
  EXPECT_EQ(OK, requests_[2]->WaitForResult());
  EXPECT_TRUE(requests_[2]->HasOneAddress("192.168.0.3", 80));
}

}  // namespace net
