// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/sync_point_manager.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include <climits>

#include "base/bind.h"
#include "base/containers/hash_tables.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"

namespace gpu {

namespace {

void RunOnThread(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                 const base::Closure& callback) {
  if (task_runner->BelongsToCurrentThread()) {
    callback.Run();
  } else {
    task_runner->PostTask(FROM_HERE, callback);
  }
}

}  // namespace

scoped_refptr<SyncPointOrderData> SyncPointOrderData::Create() {
  return new SyncPointOrderData;
}

void SyncPointOrderData::Destroy() {
  // Because of circular references between the SyncPointOrderData and
  // SyncPointClientState, we must remove the references on destroy. Releasing
  // the fence syncs in the order fence queue would be redundant at this point
  // because they are assumed to be released on the destruction of the
  // SyncPointClient.
  base::AutoLock auto_lock(lock_);
  destroyed_ = true;
  while (!order_fence_queue_.empty()) {
    order_fence_queue_.pop();
  }
}

uint32_t SyncPointOrderData::GenerateUnprocessedOrderNumber(
    SyncPointManager* sync_point_manager) {
  const uint32_t order_num = sync_point_manager->GenerateOrderNumber();
  base::AutoLock auto_lock(lock_);
  unprocessed_order_num_ = order_num;
  return order_num;
}

void SyncPointOrderData::BeginProcessingOrderNumber(uint32_t order_num) {
  DCHECK(processing_thread_checker_.CalledOnValidThread());
  DCHECK_GE(order_num, current_order_num_);
  // Use thread-safe accessors here because |processed_order_num_| and
  // |unprocessed_order_num_| are protected by a lock.
  DCHECK_GT(order_num, processed_order_num());
  DCHECK_LE(order_num, unprocessed_order_num());
  current_order_num_ = order_num;
  paused_ = false;

  // Catch invalid waits which were waiting on fence syncs that do not exist.
  // When we begin processing an order number, we should release any fence
  // syncs which were enqueued but the order number never existed.
  // Release without the lock to avoid possible deadlocks.
  std::vector<OrderFence> ensure_releases;
  {
    base::AutoLock auto_lock(lock_);
    while (!order_fence_queue_.empty()) {
      const OrderFence& order_fence = order_fence_queue_.top();
      if (order_fence_queue_.top().order_num < order_num) {
        ensure_releases.push_back(order_fence);
        order_fence_queue_.pop();
        continue;
      }
      break;
    }
  }

  for (OrderFence& order_fence : ensure_releases) {
    order_fence.client_state->EnsureWaitReleased(order_fence.fence_release,
                                                 order_fence.release_callback);
  }
}

void SyncPointOrderData::PauseProcessingOrderNumber(uint32_t order_num) {
  DCHECK(processing_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(current_order_num_, order_num);
  DCHECK(!paused_);
  paused_ = true;
}

void SyncPointOrderData::FinishProcessingOrderNumber(uint32_t order_num) {
  DCHECK(processing_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(current_order_num_, order_num);
  DCHECK(!paused_);

  // Catch invalid waits which were waiting on fence syncs that do not exist.
  // When we end processing an order number, we should release any fence syncs
  // which were suppose to be released during this order number.
  // Release without the lock to avoid possible deadlocks.
  std::vector<OrderFence> ensure_releases;
  {
    base::AutoLock auto_lock(lock_);
    DCHECK_GT(order_num, processed_order_num_);
    processed_order_num_ = order_num;

    while (!order_fence_queue_.empty()) {
      const OrderFence& order_fence = order_fence_queue_.top();
      if (order_fence_queue_.top().order_num <= order_num) {
        ensure_releases.push_back(order_fence);
        order_fence_queue_.pop();
        continue;
      }
      break;
    }
  }

  for (OrderFence& order_fence : ensure_releases) {
    order_fence.client_state->EnsureWaitReleased(order_fence.fence_release,
                                                 order_fence.release_callback);
  }
}

SyncPointOrderData::OrderFence::OrderFence(
    uint32_t order,
    uint64_t release,
    const base::Closure& callback,
    scoped_refptr<SyncPointClientState> state)
    : order_num(order),
      fence_release(release),
      release_callback(callback),
      client_state(state) {}

SyncPointOrderData::OrderFence::OrderFence(const OrderFence& other) = default;

SyncPointOrderData::OrderFence::~OrderFence() {}

SyncPointOrderData::SyncPointOrderData()
    : current_order_num_(0),
      paused_(false),
      destroyed_(false),
      processed_order_num_(0),
      unprocessed_order_num_(0) {}

SyncPointOrderData::~SyncPointOrderData() {}

bool SyncPointOrderData::ValidateReleaseOrderNumber(
    scoped_refptr<SyncPointClientState> client_state,
    uint32_t wait_order_num,
    uint64_t fence_release,
    const base::Closure& release_callback) {
  base::AutoLock auto_lock(lock_);
  if (destroyed_)
    return false;

  // Release should have a possible unprocessed order number lower
  // than the wait order number.
  if ((processed_order_num_ + 1) >= wait_order_num)
    return false;

  // Release should have more unprocessed numbers if we are waiting.
  if (unprocessed_order_num_ <= processed_order_num_)
    return false;

  // So far it could be valid, but add an order fence guard to be sure it
  // gets released eventually.
  const uint32_t expected_order_num =
      std::min(unprocessed_order_num_, wait_order_num);
  order_fence_queue_.push(OrderFence(expected_order_num, fence_release,
                                     release_callback, client_state));
  return true;
}

SyncPointClientState::ReleaseCallback::ReleaseCallback(
    uint64_t release,
    const base::Closure& callback)
    : release_count(release), callback_closure(callback) {}

SyncPointClientState::ReleaseCallback::ReleaseCallback(
    const ReleaseCallback& other) = default;

SyncPointClientState::ReleaseCallback::~ReleaseCallback() {}

SyncPointClientState::SyncPointClientState(
    scoped_refptr<SyncPointOrderData> order_data)
    : order_data_(order_data), fence_sync_release_(0) {}

SyncPointClientState::~SyncPointClientState() {
}

bool SyncPointClientState::WaitForRelease(CommandBufferNamespace namespace_id,
                                          CommandBufferId client_id,
                                          uint32_t wait_order_num,
                                          uint64_t release,
                                          const base::Closure& callback) {
  // Lock must be held the whole time while we validate otherwise it could be
  // released while we are checking.
  {
    base::AutoLock auto_lock(fence_sync_lock_);
    if (release > fence_sync_release_) {
      if (!order_data_->ValidateReleaseOrderNumber(this, wait_order_num,
                                                   release, callback)) {
        return false;
      } else {
        // Add the callback which will be called upon release.
        release_callback_queue_.push(ReleaseCallback(release, callback));
        if (!on_wait_callback_.is_null())
          on_wait_callback_.Run(namespace_id, client_id);
        return true;
      }
    }
  }

  // Already released, run the callback now.
  callback.Run();
  return true;
}

void SyncPointClientState::ReleaseFenceSync(uint64_t release) {
  // Call callbacks without the lock to avoid possible deadlocks.
  std::vector<base::Closure> callback_list;
  {
    base::AutoLock auto_lock(fence_sync_lock_);
    DLOG_IF(ERROR, release <= fence_sync_release_)
        << "Client submitted fence releases out of order.";

    fence_sync_release_ = release;
    while (!release_callback_queue_.empty() &&
           release_callback_queue_.top().release_count <= release) {
      callback_list.push_back(release_callback_queue_.top().callback_closure);
      release_callback_queue_.pop();
    }
  }

  for (const base::Closure& closure : callback_list) {
    closure.Run();
  }
}

void SyncPointClientState::EnsureWaitReleased(uint64_t release,
                                              const base::Closure& callback) {
  // Call callbacks without the lock to avoid possible deadlocks.
  bool call_callback = false;
  {
    base::AutoLock auto_lock(fence_sync_lock_);
    if (release <= fence_sync_release_)
      return;

    std::vector<ReleaseCallback> popped_callbacks;
    popped_callbacks.reserve(release_callback_queue_.size());

    while (!release_callback_queue_.empty() &&
           release_callback_queue_.top().release_count <= release) {
      const ReleaseCallback& top_item = release_callback_queue_.top();
      if (top_item.release_count == release &&
          top_item.callback_closure.Equals(callback)) {
        // Call the callback, and discard this item from the callback queue.
        call_callback = true;
      } else {
        // Store the item to be placed back into the callback queue later.
        popped_callbacks.push_back(top_item);
      }
      release_callback_queue_.pop();
    }

    // Add back in popped items.
    for (const ReleaseCallback& popped_callback : popped_callbacks) {
      release_callback_queue_.push(popped_callback);
    }
  }

  if (call_callback) {
    // This effectively releases the wait without releasing the fence.
    callback.Run();
  }
}

void SyncPointClientState::SetOnWaitCallback(const OnWaitCallback& callback) {
  on_wait_callback_ = callback;
}

SyncPointClient::~SyncPointClient() {
  if (namespace_id_ != gpu::CommandBufferNamespace::INVALID) {
    // Release all fences on destruction.
    client_state_->ReleaseFenceSync(UINT64_MAX);

    sync_point_manager_->DestroySyncPointClient(namespace_id_, client_id_);
  }
}

bool SyncPointClient::Wait(SyncPointClientState* release_state,
                           uint64_t release_count,
                           const base::Closure& wait_complete_callback) {
  // Validate that this Wait call is between BeginProcessingOrderNumber() and
  // FinishProcessingOrderNumber(), or else we may deadlock.
  DCHECK(client_state_->order_data()->IsProcessingOrderNumber());

  const uint32_t wait_order_number =
      client_state_->order_data()->current_order_num();

  // If waiting on self or wait was invalid, call the callback and return false.
  if (client_state_ == release_state ||
      !release_state->WaitForRelease(namespace_id_, client_id_,
                                     wait_order_number, release_count,
                                     wait_complete_callback)) {
    wait_complete_callback.Run();
    return false;
  }
  return true;
}

bool SyncPointClient::WaitNonThreadSafe(
    SyncPointClientState* release_state,
    uint64_t release_count,
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    const base::Closure& wait_complete_callback) {
  return Wait(release_state, release_count,
              base::Bind(&RunOnThread, runner, wait_complete_callback));
}

bool SyncPointClient::WaitOutOfOrder(
    SyncPointClientState* release_state,
    uint64_t release_count,
    const base::Closure& wait_complete_callback) {
  // Validate that this Wait call is not between BeginProcessingOrderNumber()
  // and FinishProcessingOrderNumber(), or else we may deadlock.
  DCHECK(!client_state_ ||
         !client_state_->order_data()->IsProcessingOrderNumber());

  // No order number associated with the current execution context, using
  // UINT32_MAX will just assume the release is in the SyncPointClientState's
  // order numbers to be executed.
  if (!release_state->WaitForRelease(namespace_id_, client_id_, UINT32_MAX,
                                     release_count, wait_complete_callback)) {
    wait_complete_callback.Run();
    return false;
  }
  return true;
}

bool SyncPointClient::WaitOutOfOrderNonThreadSafe(
    SyncPointClientState* release_state,
    uint64_t release_count,
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    const base::Closure& wait_complete_callback) {
  return WaitOutOfOrder(
      release_state, release_count,
      base::Bind(&RunOnThread, runner, wait_complete_callback));
}

void SyncPointClient::ReleaseFenceSync(uint64_t release) {
  // Validate that this Release call is between BeginProcessingOrderNumber() and
  // FinishProcessingOrderNumber(), or else we may deadlock.
  DCHECK(client_state_->order_data()->IsProcessingOrderNumber());
  client_state_->ReleaseFenceSync(release);
}

void SyncPointClient::SetOnWaitCallback(const OnWaitCallback& callback) {
  client_state_->SetOnWaitCallback(callback);
}

SyncPointClient::SyncPointClient()
    : sync_point_manager_(nullptr),
      namespace_id_(gpu::CommandBufferNamespace::INVALID),
      client_id_() {}

SyncPointClient::SyncPointClient(SyncPointManager* sync_point_manager,
                                 scoped_refptr<SyncPointOrderData> order_data,
                                 CommandBufferNamespace namespace_id,
                                 CommandBufferId client_id)
    : sync_point_manager_(sync_point_manager),
      client_state_(new SyncPointClientState(order_data)),
      namespace_id_(namespace_id),
      client_id_(client_id) {}

SyncPointManager::SyncPointManager(bool allow_threaded_wait) {
  global_order_num_.GetNext();
}

SyncPointManager::~SyncPointManager() {
  for (const ClientMap& client_map : client_maps_) {
    DCHECK(client_map.empty());
  }
}

std::unique_ptr<SyncPointClient> SyncPointManager::CreateSyncPointClient(
    scoped_refptr<SyncPointOrderData> order_data,
    CommandBufferNamespace namespace_id,
    CommandBufferId client_id) {
  DCHECK_GE(namespace_id, 0);
  DCHECK_LT(static_cast<size_t>(namespace_id), arraysize(client_maps_));
  base::AutoLock auto_lock(client_maps_lock_);

  ClientMap& client_map = client_maps_[namespace_id];
  std::pair<ClientMap::iterator, bool> result = client_map.insert(
      std::make_pair(client_id, new SyncPointClient(this, order_data,
                                                    namespace_id, client_id)));
  DCHECK(result.second);

  return base::WrapUnique(result.first->second);
}

std::unique_ptr<SyncPointClient>
SyncPointManager::CreateSyncPointClientWaiter() {
  return base::WrapUnique(new SyncPointClient);
}

scoped_refptr<SyncPointClientState> SyncPointManager::GetSyncPointClientState(
    CommandBufferNamespace namespace_id,
    CommandBufferId client_id) {
  if (namespace_id >= 0) {
    DCHECK_LT(static_cast<size_t>(namespace_id), arraysize(client_maps_));
    base::AutoLock auto_lock(client_maps_lock_);
    ClientMap& client_map = client_maps_[namespace_id];
    ClientMap::iterator it = client_map.find(client_id);
    if (it != client_map.end()) {
      return it->second->client_state();
    }
  }
  return nullptr;
}

uint32_t SyncPointManager::GenerateOrderNumber() {
  return global_order_num_.GetNext();
}

void SyncPointManager::DestroySyncPointClient(
    CommandBufferNamespace namespace_id,
    CommandBufferId client_id) {
  DCHECK_GE(namespace_id, 0);
  DCHECK_LT(static_cast<size_t>(namespace_id), arraysize(client_maps_));

  base::AutoLock auto_lock(client_maps_lock_);
  ClientMap& client_map = client_maps_[namespace_id];
  ClientMap::iterator it = client_map.find(client_id);
  DCHECK(it != client_map.end());
  client_map.erase(it);
}

}  // namespace gpu
