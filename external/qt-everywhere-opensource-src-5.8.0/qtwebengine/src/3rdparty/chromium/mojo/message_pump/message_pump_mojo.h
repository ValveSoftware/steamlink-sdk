// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_MESSAGE_PUMP_MESSAGE_PUMP_MOJO_H_
#define MOJO_MESSAGE_PUMP_MESSAGE_PUMP_MOJO_H_

#include <stdint.h>

#include <functional>
#include <memory>
#include <set>
#include <unordered_map>

#include "base/macros.h"
#include "base/message_loop/message_pump.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "mojo/message_pump/mojo_message_pump_export.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {
namespace common {

class MessagePumpMojoHandler;

// Mojo implementation of MessagePump.
class MOJO_MESSAGE_PUMP_EXPORT MessagePumpMojo : public base::MessagePump {
 public:
  class MOJO_MESSAGE_PUMP_EXPORT Observer {
   public:
    Observer() {}

    virtual void WillSignalHandler() = 0;
    virtual void DidSignalHandler() = 0;

   protected:
    virtual ~Observer() {}
  };

  MessagePumpMojo();
  ~MessagePumpMojo() override;

  // Static factory function (for using with |base::Thread::Options|, wrapped
  // using |base::Bind()|).
  static std::unique_ptr<base::MessagePump> Create();

  // Returns the MessagePumpMojo instance of the current thread, if it exists.
  static MessagePumpMojo* current();

  static bool IsCurrent() { return !!current(); }

  // Registers a MessagePumpMojoHandler for the specified handle. Only one
  // handler can be registered for a specified handle.
  // NOTE: a value of 0 for |deadline| indicates an indefinite timeout.
  void AddHandler(MessagePumpMojoHandler* handler,
                  const Handle& handle,
                  MojoHandleSignals wait_signals,
                  base::TimeTicks deadline);

  void RemoveHandler(const Handle& handle);

  void AddObserver(Observer*);
  void RemoveObserver(Observer*);

  // MessagePump:
  void Run(Delegate* delegate) override;
  void Quit() override;
  void ScheduleWork() override;
  void ScheduleDelayedWork(const base::TimeTicks& delayed_work_time) override;

 private:
  struct RunState;

  // Contains the data needed to track a request to AddHandler().
  struct Handler {
    Handler() : handler(NULL), wait_signals(MOJO_HANDLE_SIGNAL_NONE), id(0) {}

    MessagePumpMojoHandler* handler;
    MojoHandleSignals wait_signals;
    base::TimeTicks deadline;
    // See description of |MessagePumpMojo::next_handler_id_| for details.
    int id;
  };

  struct HandleHasher {
    size_t operator()(const Handle& handle) const {
      return std::hash<uint32_t>()(static_cast<uint32_t>(handle.value()));
    }
  };

  using HandleToHandler = std::unordered_map<Handle, Handler, HandleHasher>;

  // Implementation of Run().
  void DoRunLoop(RunState* run_state, Delegate* delegate);

  // Services the set of handles ready. If |block| is true this waits for a
  // handle to become ready, otherwise this does not block. Returns |true| if a
  // handle has become ready, |false| otherwise.
  bool DoInternalWork(const RunState& run_state, bool block);

  bool DoNonMojoWork(const RunState& run_state, bool block);

  // Waits for handles in the wait set to become ready. Returns |true| if ready
  // handles may be available, or |false| if the wait's deadline was exceeded.
  // Note, ready handles may be unavailable, even though |true| was returned.
  bool WaitForReadyHandles(const RunState& run_state) const;

  // Retrieves any 'ready' handles from the wait set, and runs the handler's
  // OnHandleReady() or OnHandleError() functions as necessary. Returns |true|
  // if any handles were ready and processed.
  bool ProcessReadyHandles();

  // Removes any handles that have expired their deadline. Runs the handler's
  // OnHandleError() function with |MOJO_RESULT_DEADLINE_EXCEEDED| as the
  // result. Returns |true| if any handles were removed.
  bool RemoveExpiredHandles();

  void SignalControlPipe();

  // Returns the deadline for the call to MojoWait().
  MojoDeadline GetDeadlineForWait(const RunState& run_state) const;

  // Run |OnHandleReady()| for the handler registered with |handle|. |handle|
  // must be registered.
  void SignalHandleReady(Handle handle);

  // Run |OnHandleError()| for the handler registered with |handle| and the
  // error code |result|. |handle| must be registered, and will be removed
  // before calling |OnHandleError()|.
  void SignalHandleError(Handle handle, MojoResult result);

  void WillSignalHandler();
  void DidSignalHandler();

  // If non-NULL we're running (inside Run()). Member is reference to value on
  // stack.
  RunState* run_state_;

  // Lock for accessing |run_state_|. In general the only method that we have to
  // worry about is ScheduleWork(). All other methods are invoked on the same
  // thread.
  base::Lock run_state_lock_;

  HandleToHandler handlers_;
  // Set of handles that have a deadline set. Avoids iterating over all elements
  // in |handles_| in the common case (no deadline set).
  // TODO(amistry): Make this better and avoid special-casing deadlines.
  std::set<Handle> deadline_handles_;

  // An ever increasing value assigned to each Handler::id. Used to detect
  // uniqueness while notifying. That is, while notifying expired timers we copy
  // |handlers_| and only notify handlers whose id match. If the id does not
  // match it means the handler was removed then added so that we shouldn't
  // notify it.
  int next_handler_id_;

  base::ObserverList<Observer> observers_;

  // Mojo handle for the wait set.
  ScopedHandle wait_set_handle_;
  // Used to wake up run loop from |SignalControlPipe()|.
  ScopedMessagePipeHandle read_handle_;
  ScopedMessagePipeHandle write_handle_;

  // Used to sleep until there is more work to do, when the Mojo EDK is shutting
  // down.
  base::WaitableEvent event_;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpMojo);
};

}  // namespace common
}  // namespace mojo

#endif  // MOJO_MESSAGE_PUMP_MESSAGE_PUMP_MOJO_H_
