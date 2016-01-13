// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_session.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sample_vector.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/dns_socket_pool.h"
#include "net/socket/stream_socket.h"
#include "net/udp/datagram_client_socket.h"

namespace net {

namespace {
// Never exceed max timeout.
const unsigned kMaxTimeoutMs = 5000;
// Set min timeout, in case we are talking to a local DNS proxy.
const unsigned kMinTimeoutMs = 10;

// Number of buckets in the histogram of observed RTTs.
const size_t kRTTBucketCount = 100;
// Target percentile in the RTT histogram used for retransmission timeout.
const unsigned kRTOPercentile = 99;
}  // namespace

// Runtime statistics of DNS server.
struct DnsSession::ServerStats {
  ServerStats(base::TimeDelta rtt_estimate_param, RttBuckets* buckets)
    : last_failure_count(0), rtt_estimate(rtt_estimate_param) {
    rtt_histogram.reset(new base::SampleVector(buckets));
    // Seed histogram with 2 samples at |rtt_estimate| timeout.
    rtt_histogram->Accumulate(rtt_estimate.InMilliseconds(), 2);
  }

  // Count of consecutive failures after last success.
  int last_failure_count;

  // Last time when server returned failure or timeout.
  base::Time last_failure;
  // Last time when server returned success.
  base::Time last_success;

  // Estimated RTT using moving average.
  base::TimeDelta rtt_estimate;
  // Estimated error in the above.
  base::TimeDelta rtt_deviation;

  // A histogram of observed RTT .
  scoped_ptr<base::SampleVector> rtt_histogram;

  DISALLOW_COPY_AND_ASSIGN(ServerStats);
};

// static
base::LazyInstance<DnsSession::RttBuckets>::Leaky DnsSession::rtt_buckets_ =
    LAZY_INSTANCE_INITIALIZER;

DnsSession::RttBuckets::RttBuckets() : base::BucketRanges(kRTTBucketCount + 1) {
  base::Histogram::InitializeBucketRanges(1, 5000, this);
}

DnsSession::SocketLease::SocketLease(scoped_refptr<DnsSession> session,
                                     unsigned server_index,
                                     scoped_ptr<DatagramClientSocket> socket)
    : session_(session), server_index_(server_index), socket_(socket.Pass()) {}

DnsSession::SocketLease::~SocketLease() {
  session_->FreeSocket(server_index_, socket_.Pass());
}

DnsSession::DnsSession(const DnsConfig& config,
                       scoped_ptr<DnsSocketPool> socket_pool,
                       const RandIntCallback& rand_int_callback,
                       NetLog* net_log)
    : config_(config),
      socket_pool_(socket_pool.Pass()),
      rand_callback_(base::Bind(rand_int_callback, 0, kuint16max)),
      net_log_(net_log),
      server_index_(0) {
  socket_pool_->Initialize(&config_.nameservers, net_log);
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "AsyncDNS.ServerCount", config_.nameservers.size(), 0, 10, 10);
  for (size_t i = 0; i < config_.nameservers.size(); ++i) {
    server_stats_.push_back(new ServerStats(config_.timeout,
                                            rtt_buckets_.Pointer()));
  }
}

DnsSession::~DnsSession() {
  RecordServerStats();
}

int DnsSession::NextQueryId() const { return rand_callback_.Run(); }

unsigned DnsSession::NextFirstServerIndex() {
  unsigned index = NextGoodServerIndex(server_index_);
  if (config_.rotate)
    server_index_ = (server_index_ + 1) % config_.nameservers.size();
  return index;
}

unsigned DnsSession::NextGoodServerIndex(unsigned server_index) {
  unsigned index = server_index;
  base::Time oldest_server_failure(base::Time::Now());
  unsigned oldest_server_failure_index = 0;

  UMA_HISTOGRAM_BOOLEAN("AsyncDNS.ServerIsGood",
                        server_stats_[server_index]->last_failure.is_null());

  do {
    base::Time cur_server_failure = server_stats_[index]->last_failure;
    // If number of failures on this server doesn't exceed number of allowed
    // attempts, return its index.
    if (server_stats_[server_index]->last_failure_count < config_.attempts) {
      return index;
    }
    // Track oldest failed server.
    if (cur_server_failure < oldest_server_failure) {
      oldest_server_failure = cur_server_failure;
      oldest_server_failure_index = index;
    }
    index = (index + 1) % config_.nameservers.size();
  } while (index != server_index);

  // If we are here it means that there are no successful servers, so we have
  // to use one that has failed oldest.
  return oldest_server_failure_index;
}

void DnsSession::RecordServerFailure(unsigned server_index) {
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "AsyncDNS.ServerFailureIndex", server_index, 0, 10, 10);
  ++(server_stats_[server_index]->last_failure_count);
  server_stats_[server_index]->last_failure = base::Time::Now();
}

void DnsSession::RecordServerSuccess(unsigned server_index) {
  if (server_stats_[server_index]->last_success.is_null()) {
    UMA_HISTOGRAM_COUNTS_100("AsyncDNS.ServerFailuresAfterNetworkChange",
                           server_stats_[server_index]->last_failure_count);
  } else {
    UMA_HISTOGRAM_COUNTS_100("AsyncDNS.ServerFailuresBeforeSuccess",
                           server_stats_[server_index]->last_failure_count);
  }
  server_stats_[server_index]->last_failure_count = 0;
  server_stats_[server_index]->last_failure = base::Time();
  server_stats_[server_index]->last_success = base::Time::Now();
}

void DnsSession::RecordRTT(unsigned server_index, base::TimeDelta rtt) {
  DCHECK_LT(server_index, server_stats_.size());

  // For measurement, assume it is the first attempt (no backoff).
  base::TimeDelta timeout_jacobson = NextTimeoutFromJacobson(server_index, 0);
  base::TimeDelta timeout_histogram = NextTimeoutFromHistogram(server_index, 0);
  UMA_HISTOGRAM_TIMES("AsyncDNS.TimeoutErrorJacobson", rtt - timeout_jacobson);
  UMA_HISTOGRAM_TIMES("AsyncDNS.TimeoutErrorHistogram",
                      rtt - timeout_histogram);
  UMA_HISTOGRAM_TIMES("AsyncDNS.TimeoutErrorJacobsonUnder",
                      timeout_jacobson - rtt);
  UMA_HISTOGRAM_TIMES("AsyncDNS.TimeoutErrorHistogramUnder",
                      timeout_histogram - rtt);

  // Jacobson/Karels algorithm for TCP.
  // Using parameters: alpha = 1/8, delta = 1/4, beta = 4
  base::TimeDelta& estimate = server_stats_[server_index]->rtt_estimate;
  base::TimeDelta& deviation = server_stats_[server_index]->rtt_deviation;
  base::TimeDelta current_error = rtt - estimate;
  estimate += current_error / 8;  // * alpha
  base::TimeDelta abs_error = base::TimeDelta::FromInternalValue(
      std::abs(current_error.ToInternalValue()));
  deviation += (abs_error - deviation) / 4;  // * delta

  // Histogram-based method.
  server_stats_[server_index]->rtt_histogram
      ->Accumulate(rtt.InMilliseconds(), 1);
}

void DnsSession::RecordLostPacket(unsigned server_index, int attempt) {
  base::TimeDelta timeout_jacobson =
      NextTimeoutFromJacobson(server_index, attempt);
  base::TimeDelta timeout_histogram =
      NextTimeoutFromHistogram(server_index, attempt);
  UMA_HISTOGRAM_TIMES("AsyncDNS.TimeoutSpentJacobson", timeout_jacobson);
  UMA_HISTOGRAM_TIMES("AsyncDNS.TimeoutSpentHistogram", timeout_histogram);
}

void DnsSession::RecordServerStats() {
  for (size_t index = 0; index < server_stats_.size(); ++index) {
    if (server_stats_[index]->last_failure_count) {
      if (server_stats_[index]->last_success.is_null()) {
        UMA_HISTOGRAM_COUNTS("AsyncDNS.ServerFailuresWithoutSuccess",
                             server_stats_[index]->last_failure_count);
      } else {
        UMA_HISTOGRAM_COUNTS("AsyncDNS.ServerFailuresAfterSuccess",
                             server_stats_[index]->last_failure_count);
      }
    }
  }
}


base::TimeDelta DnsSession::NextTimeout(unsigned server_index, int attempt) {
  // Respect config timeout if it exceeds |kMaxTimeoutMs|.
  if (config_.timeout.InMilliseconds() >= kMaxTimeoutMs)
    return config_.timeout;
  return NextTimeoutFromHistogram(server_index, attempt);
}

// Allocate a socket, already connected to the server address.
scoped_ptr<DnsSession::SocketLease> DnsSession::AllocateSocket(
    unsigned server_index, const NetLog::Source& source) {
  scoped_ptr<DatagramClientSocket> socket;

  socket = socket_pool_->AllocateSocket(server_index);
  if (!socket.get())
    return scoped_ptr<SocketLease>();

  socket->NetLog().BeginEvent(NetLog::TYPE_SOCKET_IN_USE,
                              source.ToEventParametersCallback());

  SocketLease* lease = new SocketLease(this, server_index, socket.Pass());
  return scoped_ptr<SocketLease>(lease);
}

scoped_ptr<StreamSocket> DnsSession::CreateTCPSocket(
    unsigned server_index, const NetLog::Source& source) {
  return socket_pool_->CreateTCPSocket(server_index, source);
}

// Release a socket.
void DnsSession::FreeSocket(unsigned server_index,
                            scoped_ptr<DatagramClientSocket> socket) {
  DCHECK(socket.get());

  socket->NetLog().EndEvent(NetLog::TYPE_SOCKET_IN_USE);

  socket_pool_->FreeSocket(server_index, socket.Pass());
}

base::TimeDelta DnsSession::NextTimeoutFromJacobson(unsigned server_index,
                                                    int attempt) {
  DCHECK_LT(server_index, server_stats_.size());

  base::TimeDelta timeout = server_stats_[server_index]->rtt_estimate +
                            4 * server_stats_[server_index]->rtt_deviation;

  timeout = std::max(timeout, base::TimeDelta::FromMilliseconds(kMinTimeoutMs));

  // The timeout doubles every full round.
  unsigned num_backoffs = attempt / config_.nameservers.size();

  return std::min(timeout * (1 << num_backoffs),
                  base::TimeDelta::FromMilliseconds(kMaxTimeoutMs));
}

base::TimeDelta DnsSession::NextTimeoutFromHistogram(unsigned server_index,
                                                     int attempt) {
  DCHECK_LT(server_index, server_stats_.size());

  COMPILE_ASSERT(std::numeric_limits<base::HistogramBase::Count>::is_signed,
                 histogram_base_count_assumed_to_be_signed);

  // Use fixed percentile of observed samples.
  const base::SampleVector& samples =
      *server_stats_[server_index]->rtt_histogram;

  base::HistogramBase::Count total = samples.TotalCount();
  base::HistogramBase::Count remaining_count = kRTOPercentile * total / 100;
  size_t index = 0;
  while (remaining_count > 0 && index < rtt_buckets_.Get().size()) {
    remaining_count -= samples.GetCountAtIndex(index);
    ++index;
  }

  base::TimeDelta timeout =
      base::TimeDelta::FromMilliseconds(rtt_buckets_.Get().range(index));

  timeout = std::max(timeout, base::TimeDelta::FromMilliseconds(kMinTimeoutMs));

  // The timeout still doubles every full round.
  unsigned num_backoffs = attempt / config_.nameservers.size();

  return std::min(timeout * (1 << num_backoffs),
                  base::TimeDelta::FromMilliseconds(kMaxTimeoutMs));
}

}  // namespace net
