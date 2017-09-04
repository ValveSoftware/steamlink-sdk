// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_WAIT_SET_DISPATCHER_H_
#define MOJO_EDK_SYSTEM_WAIT_SET_DISPATCHER_H_

#include <stdint.h>

#include <deque>
#include <memory>
#include <unordered_map>
#include <utility>

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "mojo/edk/system/awakable_list.h"
#include "mojo/edk/system/dispatcher.h"
#include "mojo/edk/system/system_impl_export.h"

namespace mojo {
namespace edk {

class MOJO_SYSTEM_IMPL_EXPORT WaitSetDispatcher : public Dispatcher {
 public:
  WaitSetDispatcher();

  // Dispatcher:
  Type GetType() const override;
  MojoResult Close() override;
  MojoResult AddWaitingDispatcher(const scoped_refptr<Dispatcher>& dispatcher,
                                  MojoHandleSignals signals,
                                  uintptr_t context) override;
  MojoResult RemoveWaitingDispatcher(
      const scoped_refptr<Dispatcher>& dispatcher) override;
  MojoResult GetReadyDispatchers(uint32_t* count,
                                 DispatcherVector* dispatchers,
                                 MojoResult* results,
                                 uintptr_t* contexts) override;
  HandleSignalsState GetHandleSignalsState() const override;
  MojoResult AddAwakable(Awakable* awakable,
                         MojoHandleSignals signals,
                         uintptr_t context,
                         HandleSignalsState* signals_state) override;
  void RemoveAwakable(Awakable* awakable,
                      HandleSignalsState* signals_state) override;
  bool BeginTransit() override;

 private:
  // Internal implementation of Awakable.
  class Waiter;

  struct WaitState {
    WaitState();
    WaitState(const WaitState& other);
    ~WaitState();

    scoped_refptr<Dispatcher> dispatcher;
    MojoHandleSignals signals;
    uintptr_t context;
  };

  ~WaitSetDispatcher() override;

  HandleSignalsState GetHandleSignalsStateNoLock() const;

  // Signal that the dispatcher indexed by |context| has been woken up with
  // |result| and is now ready.
  void WakeDispatcher(MojoResult result, uintptr_t context);

  // Guards |is_closed_|, |waiting_dispatchers_|, and |waiter_|.
  //
  // TODO: Consider removing this.
  mutable base::Lock lock_;
  bool is_closed_ = false;

  // Map of dispatchers being waited on. Key is a Dispatcher* casted to a
  // uintptr_t, and should be treated as an opaque value and not casted back.
  std::unordered_map<uintptr_t, WaitState> waiting_dispatchers_;

  // Separate lock that can be locked without locking |lock_|.
  mutable base::Lock awoken_lock_;
  // List of dispatchers that have been woken up. Any dispatcher in this queue
  // will NOT currently be waited on.
  std::deque<std::pair<uintptr_t, MojoResult>> awoken_queue_;
  // List of dispatchers that have been woken up and retrieved.
  std::deque<uintptr_t> processed_dispatchers_;

  // Separate lock that can be locked without locking |lock_|.
  base::Lock awakable_lock_;
  // List of dispatchers being waited on.
  AwakableList awakable_list_;

  // Waiter used to wait on dispatchers.
  std::unique_ptr<Waiter> waiter_;

  DISALLOW_COPY_AND_ASSIGN(WaitSetDispatcher);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_WAIT_SET_DISPATCHER_H_
