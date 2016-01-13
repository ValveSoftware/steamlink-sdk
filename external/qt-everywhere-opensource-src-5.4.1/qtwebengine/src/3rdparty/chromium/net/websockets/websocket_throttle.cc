// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_throttle.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>

#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/io_buffer.h"
#include "net/socket_stream/socket_stream.h"
#include "net/websockets/websocket_job.h"

namespace net {

namespace {

const size_t kMaxWebSocketJobsThrottled = 1024;

}  // namespace

WebSocketThrottle::WebSocketThrottle() {
}

WebSocketThrottle::~WebSocketThrottle() {
  DCHECK(queue_.empty());
  DCHECK(addr_map_.empty());
}

// static
WebSocketThrottle* WebSocketThrottle::GetInstance() {
  return Singleton<WebSocketThrottle>::get();
}

bool WebSocketThrottle::PutInQueue(WebSocketJob* job) {
  if (queue_.size() >= kMaxWebSocketJobsThrottled)
    return false;

  queue_.push_back(job);
  const AddressList& address_list = job->address_list();
  std::set<IPEndPoint> address_set;
  for (AddressList::const_iterator addr_iter = address_list.begin();
       addr_iter != address_list.end();
       ++addr_iter) {
    const IPEndPoint& address = *addr_iter;
    // If |address| is already processed, don't do it again.
    if (!address_set.insert(address).second)
      continue;

    ConnectingAddressMap::iterator iter = addr_map_.find(address);
    if (iter == addr_map_.end()) {
      ConnectingAddressMap::iterator new_queue =
          addr_map_.insert(make_pair(address, ConnectingQueue())).first;
      new_queue->second.push_back(job);
    } else {
      DCHECK(!iter->second.empty());
      iter->second.push_back(job);
      job->SetWaiting();
      DVLOG(1) << "Waiting on " << address.ToString();
    }
  }

  return true;
}

void WebSocketThrottle::RemoveFromQueue(WebSocketJob* job) {
  ConnectingQueue::iterator queue_iter =
      std::find(queue_.begin(), queue_.end(), job);
  if (queue_iter == queue_.end())
    return;
  queue_.erase(queue_iter);

  std::set<WebSocketJob*> wakeup_candidates;

  const AddressList& resolved_address_list = job->address_list();
  std::set<IPEndPoint> address_set;
  for (AddressList::const_iterator addr_iter = resolved_address_list.begin();
       addr_iter != resolved_address_list.end();
       ++addr_iter) {
    const IPEndPoint& address = *addr_iter;
    // If |address| is already processed, don't do it again.
    if (!address_set.insert(address).second)
      continue;

    ConnectingAddressMap::iterator map_iter = addr_map_.find(address);
    DCHECK(map_iter != addr_map_.end());

    ConnectingQueue& per_address_queue = map_iter->second;
    DCHECK(!per_address_queue.empty());
    // Job may not be front of the queue if the socket is closed while waiting.
    ConnectingQueue::iterator per_address_queue_iter =
        std::find(per_address_queue.begin(), per_address_queue.end(), job);
    bool was_front = false;
    if (per_address_queue_iter != per_address_queue.end()) {
      was_front = (per_address_queue_iter == per_address_queue.begin());
      per_address_queue.erase(per_address_queue_iter);
    }
    if (per_address_queue.empty()) {
      addr_map_.erase(map_iter);
    } else if (was_front) {
      // The new front is a wake-up candidate.
      wakeup_candidates.insert(per_address_queue.front());
    }
  }

  WakeupSocketIfNecessary(wakeup_candidates);
}

void WebSocketThrottle::WakeupSocketIfNecessary(
    const std::set<WebSocketJob*>& wakeup_candidates) {
  for (std::set<WebSocketJob*>::const_iterator iter = wakeup_candidates.begin();
       iter != wakeup_candidates.end();
       ++iter) {
    WebSocketJob* job = *iter;
    if (!job->IsWaiting())
      continue;

    bool should_wakeup = true;
    const AddressList& resolved_address_list = job->address_list();
    for (AddressList::const_iterator addr_iter = resolved_address_list.begin();
         addr_iter != resolved_address_list.end();
         ++addr_iter) {
      const IPEndPoint& address = *addr_iter;
      ConnectingAddressMap::iterator map_iter = addr_map_.find(address);
      DCHECK(map_iter != addr_map_.end());
      const ConnectingQueue& per_address_queue = map_iter->second;
      if (job != per_address_queue.front()) {
        should_wakeup = false;
        break;
      }
    }
    if (should_wakeup)
      job->Wakeup();
  }
}

}  // namespace net
