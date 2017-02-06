// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/flip_server/output_ordering.h"

#include <utility>

#include "net/tools/flip_server/flip_config.h"
#include "net/tools/flip_server/sm_connection.h"

namespace net {

OutputOrdering::PriorityMapPointer::PriorityMapPointer()
    : ring(NULL), alarm_enabled(false) {}

OutputOrdering::PriorityMapPointer::PriorityMapPointer(
    const PriorityMapPointer& other) = default;

OutputOrdering::PriorityMapPointer::~PriorityMapPointer() {}

// static
double OutputOrdering::server_think_time_in_s_ = 0.0;

OutputOrdering::OutputOrdering(SMConnectionInterface* connection)
    : first_data_senders_threshold_(kInitialDataSendersThreshold),
      connection_(connection) {
  if (connection)
    epoll_server_ = connection->epoll_server();
}

OutputOrdering::~OutputOrdering() { Reset(); }

void OutputOrdering::Reset() {
  while (!stream_ids_.empty()) {
    StreamIdToPriorityMap::iterator sitpmi = stream_ids_.begin();
    PriorityMapPointer& pmp = sitpmi->second;
    if (pmp.alarm_enabled) {
      epoll_server_->UnregisterAlarm(pmp.alarm_token);
    }
    stream_ids_.erase(sitpmi);
  }
  priority_map_.clear();
  first_data_senders_.clear();
}

bool OutputOrdering::ExistsInPriorityMaps(uint32_t stream_id) const {
  StreamIdToPriorityMap::const_iterator sitpmi = stream_ids_.find(stream_id);
  return sitpmi != stream_ids_.end();
}

OutputOrdering::BeginOutputtingAlarm::BeginOutputtingAlarm(
    OutputOrdering* oo,
    OutputOrdering::PriorityMapPointer* pmp,
    const MemCacheIter& mci)
    : output_ordering_(oo), pmp_(pmp), mci_(mci), epoll_server_(NULL) {}

OutputOrdering::BeginOutputtingAlarm::~BeginOutputtingAlarm() {
  if (epoll_server_ && pmp_->alarm_enabled)
    epoll_server_->UnregisterAlarm(pmp_->alarm_token);
}

int64_t OutputOrdering::BeginOutputtingAlarm::OnAlarm() {
  OnUnregistration();
  output_ordering_->MoveToActive(pmp_, mci_);
  VLOG(2) << "ON ALARM! Should now start to output...";
  delete this;
  return 0;
}

void OutputOrdering::BeginOutputtingAlarm::OnRegistration(
    const EpollServer::AlarmRegToken& tok,
    EpollServer* eps) {
  epoll_server_ = eps;
  pmp_->alarm_token = tok;
  pmp_->alarm_enabled = true;
}

void OutputOrdering::BeginOutputtingAlarm::OnUnregistration() {
  pmp_->alarm_enabled = false;
  delete this;
}

void OutputOrdering::BeginOutputtingAlarm::OnShutdown(EpollServer* eps) {
  OnUnregistration();
}

void OutputOrdering::MoveToActive(PriorityMapPointer* pmp, MemCacheIter mci) {
  VLOG(2) << "Moving to active!";
  first_data_senders_.push_back(mci);
  pmp->ring = &first_data_senders_;
  pmp->it = first_data_senders_.end();
  --pmp->it;
  connection_->ReadyToSend();
}

void OutputOrdering::AddToOutputOrder(const MemCacheIter& mci) {
  if (ExistsInPriorityMaps(mci.stream_id))
    LOG(ERROR) << "OOps, already was inserted here?!";

  double think_time_in_s = server_think_time_in_s_;
  std::string x_server_latency =
      mci.file_data->headers()->GetHeader("X-Server-Latency").as_string();
  if (!x_server_latency.empty()) {
    char* endp;
    double tmp_think_time_in_s = strtod(x_server_latency.c_str(), &endp);
    if (endp != x_server_latency.c_str() + x_server_latency.size()) {
      LOG(ERROR) << "Unable to understand X-Server-Latency of: "
                 << x_server_latency
                 << " for resource: " << mci.file_data->filename().c_str();
    } else {
      think_time_in_s = tmp_think_time_in_s;
    }
  }
  StreamIdToPriorityMap::iterator sitpmi;
  sitpmi = stream_ids_.insert(std::pair<uint32_t, PriorityMapPointer>(
                                  mci.stream_id, PriorityMapPointer()))
               .first;
  PriorityMapPointer& pmp = sitpmi->second;

  BeginOutputtingAlarm* boa = new BeginOutputtingAlarm(this, &pmp, mci);
  VLOG(1) << "Server think time: " << think_time_in_s;
  epoll_server_->RegisterAlarmApproximateDelta(think_time_in_s * 1000000, boa);
}

void OutputOrdering::SpliceToPriorityRing(PriorityRing::iterator pri) {
  MemCacheIter& mci = *pri;
  PriorityMap::iterator pmi = priority_map_.find(mci.priority);
  if (pmi == priority_map_.end()) {
    pmi = priority_map_.insert(std::pair<uint32_t, PriorityRing>(
                                   mci.priority, PriorityRing()))
              .first;
  }

  pmi->second.splice(pmi->second.end(), first_data_senders_, pri);
  StreamIdToPriorityMap::iterator sitpmi = stream_ids_.find(mci.stream_id);
  sitpmi->second.ring = &(pmi->second);
}

MemCacheIter* OutputOrdering::GetIter() {
  while (!first_data_senders_.empty()) {
    MemCacheIter& mci = first_data_senders_.front();
    if (mci.bytes_sent >= first_data_senders_threshold_) {
      SpliceToPriorityRing(first_data_senders_.begin());
    } else {
      first_data_senders_.splice(first_data_senders_.end(),
                                 first_data_senders_,
                                 first_data_senders_.begin());
      mci.max_segment_size = kInitialDataSendersThreshold;
      return &mci;
    }
  }
  while (!priority_map_.empty()) {
    PriorityRing& first_ring = priority_map_.begin()->second;
    if (first_ring.empty()) {
      priority_map_.erase(priority_map_.begin());
      continue;
    }
    MemCacheIter& mci = first_ring.front();
    first_ring.splice(first_ring.end(), first_ring, first_ring.begin());
    mci.max_segment_size = kSpdySegmentSize;
    return &mci;
  }
  return NULL;
}

void OutputOrdering::RemoveStreamId(uint32_t stream_id) {
  StreamIdToPriorityMap::iterator sitpmi = stream_ids_.find(stream_id);
  if (sitpmi == stream_ids_.end())
    return;

  PriorityMapPointer& pmp = sitpmi->second;
  if (pmp.alarm_enabled)
    epoll_server_->UnregisterAlarm(pmp.alarm_token);
  else
    pmp.ring->erase(pmp.it);
  stream_ids_.erase(sitpmi);
}

}  // namespace net
