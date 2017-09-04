// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SYNC_POINT_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SYNC_POINT_MANAGER_H_

#include <stdint.h>

#include <functional>
#include <memory>
#include <queue>
#include <unordered_map>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/gpu_export.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace gpu {

class SyncPointClient;
class SyncPointClientState;
class SyncPointManager;

class GPU_EXPORT SyncPointOrderData
    : public base::RefCountedThreadSafe<SyncPointOrderData> {
 public:
  static scoped_refptr<SyncPointOrderData> Create();
  void Destroy();

  uint32_t GenerateUnprocessedOrderNumber(SyncPointManager* sync_point_manager);
  void BeginProcessingOrderNumber(uint32_t order_num);
  void PauseProcessingOrderNumber(uint32_t order_num);
  void FinishProcessingOrderNumber(uint32_t order_num);

  uint32_t processed_order_num() const {
    base::AutoLock auto_lock(lock_);
    return processed_order_num_;
  }

  uint32_t unprocessed_order_num() const {
    base::AutoLock auto_lock(lock_);
    return unprocessed_order_num_;
  }

  uint32_t current_order_num() const {
    DCHECK(processing_thread_checker_.CalledOnValidThread());
    return current_order_num_;
  }

  bool IsProcessingOrderNumber() {
    DCHECK(processing_thread_checker_.CalledOnValidThread());
    return !paused_ && current_order_num_ > processed_order_num();
  }

 private:
  friend class base::RefCountedThreadSafe<SyncPointOrderData>;
  friend class SyncPointClientState;

  struct OrderFence {
    uint32_t order_num;
    uint64_t fence_release;
    base::Closure release_callback;
    scoped_refptr<SyncPointClientState> client_state;

    OrderFence(uint32_t order,
               uint64_t release,
               const base::Closure& release_callback,
               scoped_refptr<SyncPointClientState> state);
    OrderFence(const OrderFence& other);
    ~OrderFence();

    bool operator>(const OrderFence& rhs) const {
      return (order_num > rhs.order_num) ||
             ((order_num == rhs.order_num) &&
              (fence_release > rhs.fence_release));
    }
  };
  typedef std::priority_queue<OrderFence,
                              std::vector<OrderFence>,
                              std::greater<OrderFence>> OrderFenceQueue;

  SyncPointOrderData();
  ~SyncPointOrderData();

  bool ValidateReleaseOrderNumber(
      scoped_refptr<SyncPointClientState> client_state,
      uint32_t wait_order_num,
      uint64_t fence_release,
      const base::Closure& release_callback);

  // Non thread-safe functions need to be called from a single thread.
  base::ThreadChecker processing_thread_checker_;

  // Current IPC order number being processed (only used on processing thread).
  uint32_t current_order_num_;

  // Whether or not the current order number is being processed or paused.
  bool paused_;

  // This lock protects destroyed_, processed_order_num_,
  // unprocessed_order_num_, and order_fence_queue_. All order numbers (n) in
  // order_fence_queue_ must follow the invariant:
  //   processed_order_num_ < n <= unprocessed_order_num_.
  mutable base::Lock lock_;

  bool destroyed_;

  // Last finished IPC order number.
  uint32_t processed_order_num_;

  // Unprocessed order number expected to be processed under normal execution.
  uint32_t unprocessed_order_num_;

  // In situations where we are waiting on fence syncs that do not exist, we
  // validate by making sure the order number does not pass the order number
  // which the wait command was issued. If the order number reaches the
  // wait command's, we should automatically release up to the expected
  // release count. Note that this also releases other lower release counts,
  // so a single misbehaved fence sync is enough to invalidate/signal all
  // previous fence syncs.
  OrderFenceQueue order_fence_queue_;

  DISALLOW_COPY_AND_ASSIGN(SyncPointOrderData);
};

class GPU_EXPORT SyncPointClientState
    : public base::RefCountedThreadSafe<SyncPointClientState> {
 public:
  scoped_refptr<SyncPointOrderData> order_data() { return order_data_; }

  bool IsFenceSyncReleased(uint64_t release) {
    return release <= fence_sync_release();
  }

  uint64_t fence_sync_release() {
    base::AutoLock auto_lock(fence_sync_lock_);
    return fence_sync_release_;
  }

 private:
  friend class base::RefCountedThreadSafe<SyncPointClientState>;
  friend class SyncPointClient;
  friend class SyncPointOrderData;

  struct ReleaseCallback {
    uint64_t release_count;
    base::Closure callback_closure;

    ReleaseCallback(uint64_t release, const base::Closure& callback);
    ReleaseCallback(const ReleaseCallback& other);
    ~ReleaseCallback();

    bool operator>(const ReleaseCallback& rhs) const {
      return release_count > rhs.release_count;
    }
  };
  typedef std::priority_queue<ReleaseCallback,
                              std::vector<ReleaseCallback>,
                              std::greater<ReleaseCallback>>
      ReleaseCallbackQueue;

  SyncPointClientState(scoped_refptr<SyncPointOrderData> order_data);
  ~SyncPointClientState();

  // Queues the callback to be called if the release is valid. If the release
  // is invalid this function will return False and the callback will never
  // be called.
  bool WaitForRelease(CommandBufferNamespace namespace_id,
                      CommandBufferId client_id,
                      uint32_t wait_order_num,
                      uint64_t release,
                      const base::Closure& callback);

  // Releases a fence sync and all fence syncs below.
  void ReleaseFenceSync(uint64_t release);

  // Does not release the fence sync, but releases callbacks waiting on that
  // fence sync.
  void EnsureWaitReleased(uint64_t release, const base::Closure& callback);

  typedef base::Callback<void(CommandBufferNamespace, CommandBufferId)>
      OnWaitCallback;
  void SetOnWaitCallback(const OnWaitCallback& callback);

  // Global order data where releases will originate from.
  scoped_refptr<SyncPointOrderData> order_data_;

  // Protects fence_sync_release_, fence_callback_queue_.
  base::Lock fence_sync_lock_;

  // Current fence sync release that has been signaled.
  uint64_t fence_sync_release_;

  // In well defined fence sync operations, fence syncs are released in order
  // so simply having a priority queue for callbacks is enough.
  ReleaseCallbackQueue release_callback_queue_;

  // Called when a release callback is queued.
  OnWaitCallback on_wait_callback_;

  DISALLOW_COPY_AND_ASSIGN(SyncPointClientState);
};

class GPU_EXPORT SyncPointClient {
 public:
  ~SyncPointClient();

  scoped_refptr<SyncPointClientState> client_state() { return client_state_; }

  // Wait for a release count to be reached on a SyncPointClientState. If this
  // function returns false, that means the wait was invalid. Otherwise if it
  // returns True it means the release was valid. In the case where the release
  // is valid but has happened already, it will still return true. In all cases
  // wait_complete_callback will be called eventually. The callback function
  // may be called on another thread so it should be thread-safe. For
  // convenience, another non-threadsafe version is defined below where you
  // can supply a task runner.
  bool Wait(SyncPointClientState* release_state,
            uint64_t release_count,
            const base::Closure& wait_complete_callback);

  bool WaitNonThreadSafe(SyncPointClientState* release_state,
                         uint64_t release_count,
                         scoped_refptr<base::SingleThreadTaskRunner> runner,
                         const base::Closure& wait_complete_callback);

  // Unordered waits are waits which do not occur within the global order number
  // processing order (IE. Not between the corresponding
  // SyncPointOrderData::BeginProcessingOrderNumber() and
  // SyncPointOrderData::FinishProcessingOrderNumber() calls). Because fence
  // sync releases must occur within a corresponding order number, these waits
  // cannot deadlock because they can never depend on any fence sync releases.
  // This is useful for IPC messages that may be processed out of order with
  // respect to regular command buffer processing.
  bool WaitOutOfOrder(SyncPointClientState* release_state,
                      uint64_t release_count,
                      const base::Closure& wait_complete_callback);

  bool WaitOutOfOrderNonThreadSafe(
      SyncPointClientState* release_state,
      uint64_t release_count,
      scoped_refptr<base::SingleThreadTaskRunner> runner,
      const base::Closure& wait_complete_callback);

  void ReleaseFenceSync(uint64_t release);

  // This callback is called with the namespace and id of the waiting client
  // when a release callback is queued. The callback is called on the thread
  // where the Wait... happens and synchronization is the responsibility of the
  // caller.
  typedef base::Callback<void(CommandBufferNamespace, CommandBufferId)>
      OnWaitCallback;
  void SetOnWaitCallback(const OnWaitCallback& callback);

 private:
  friend class SyncPointManager;

  SyncPointClient();
  SyncPointClient(SyncPointManager* sync_point_manager,
                  scoped_refptr<SyncPointOrderData> order_data,
                  CommandBufferNamespace namespace_id,
                  CommandBufferId client_id);

  // Sync point manager is guaranteed to exist in the lifetime of the client.
  SyncPointManager* sync_point_manager_;

  // Keep the state that is sharable across multiple threads.
  scoped_refptr<SyncPointClientState> client_state_;

  // Unique namespace/client id pair for this sync point client.
  const CommandBufferNamespace namespace_id_;
  const CommandBufferId client_id_;

  DISALLOW_COPY_AND_ASSIGN(SyncPointClient);
};

// This class manages the sync points, which allow cross-channel
// synchronization.
class GPU_EXPORT SyncPointManager {
 public:
  explicit SyncPointManager(bool allow_threaded_wait);
  ~SyncPointManager();

  // Creates/Destroy a sync point client which message processors should hold.
  std::unique_ptr<SyncPointClient> CreateSyncPointClient(
      scoped_refptr<SyncPointOrderData> order_data,
      CommandBufferNamespace namespace_id,
      CommandBufferId client_id);

  // Creates a sync point client which cannot process order numbers but can only
  // Wait out of order.
  std::unique_ptr<SyncPointClient> CreateSyncPointClientWaiter();

  // Finds the state of an already created sync point client.
  scoped_refptr<SyncPointClientState> GetSyncPointClientState(
      CommandBufferNamespace namespace_id,
      CommandBufferId client_id);

 private:
  friend class SyncPointClient;
  friend class SyncPointOrderData;

  using ClientMap = std::unordered_map<CommandBufferId,
                                       SyncPointClient*,
                                       CommandBufferId::Hasher>;

  uint32_t GenerateOrderNumber();
  void DestroySyncPointClient(CommandBufferNamespace namespace_id,
                              CommandBufferId client_id);

  // Order number is global for all clients.
  base::AtomicSequenceNumber global_order_num_;

  // Client map holds a map of clients id to client for each namespace.
  base::Lock client_maps_lock_;
  ClientMap client_maps_[NUM_COMMAND_BUFFER_NAMESPACES];

  DISALLOW_COPY_AND_ASSIGN(SyncPointManager);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SYNC_POINT_MANAGER_H_
