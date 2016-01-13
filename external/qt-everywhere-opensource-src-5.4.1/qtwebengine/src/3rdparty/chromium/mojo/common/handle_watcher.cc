// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/handle_watcher.h"

#include <map>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "mojo/common/message_pump_mojo.h"
#include "mojo/common/message_pump_mojo_handler.h"
#include "mojo/common/time_helper.h"

namespace mojo {
namespace common {

typedef int WatcherID;

namespace {

const char kWatcherThreadName[] = "handle-watcher-thread";

// TODO(sky): this should be unnecessary once MessageLoop has been refactored.
MessagePumpMojo* message_pump_mojo = NULL;

scoped_ptr<base::MessagePump> CreateMessagePumpMojo() {
  message_pump_mojo = new MessagePumpMojo;
  return scoped_ptr<base::MessagePump>(message_pump_mojo).Pass();
}

base::TimeTicks MojoDeadlineToTimeTicks(MojoDeadline deadline) {
  return deadline == MOJO_DEADLINE_INDEFINITE ? base::TimeTicks() :
      internal::NowTicks() + base::TimeDelta::FromMicroseconds(deadline);
}

// Tracks the data for a single call to Start().
struct WatchData {
  WatchData()
      : id(0),
        handle_signals(MOJO_HANDLE_SIGNAL_NONE),
        message_loop(NULL) {}

  WatcherID id;
  Handle handle;
  MojoHandleSignals handle_signals;
  base::TimeTicks deadline;
  base::Callback<void(MojoResult)> callback;
  scoped_refptr<base::MessageLoopProxy> message_loop;
};

// WatcherBackend --------------------------------------------------------------

// WatcherBackend is responsible for managing the requests and interacting with
// MessagePumpMojo. All access (outside of creation/destruction) is done on the
// thread WatcherThreadManager creates.
class WatcherBackend : public MessagePumpMojoHandler {
 public:
  WatcherBackend();
  virtual ~WatcherBackend();

  void StartWatching(const WatchData& data);
  void StopWatching(WatcherID watcher_id);

 private:
  typedef std::map<Handle, WatchData> HandleToWatchDataMap;

  // Invoked when a handle needs to be removed and notified.
  void RemoveAndNotify(const Handle& handle, MojoResult result);

  // Searches through |handle_to_data_| for |watcher_id|. Returns true if found
  // and sets |handle| to the Handle. Returns false if not a known id.
  bool GetMojoHandleByWatcherID(WatcherID watcher_id, Handle* handle) const;

  // MessagePumpMojoHandler overrides:
  virtual void OnHandleReady(const Handle& handle) OVERRIDE;
  virtual void OnHandleError(const Handle& handle, MojoResult result) OVERRIDE;

  // Maps from assigned id to WatchData.
  HandleToWatchDataMap handle_to_data_;

  DISALLOW_COPY_AND_ASSIGN(WatcherBackend);
};

WatcherBackend::WatcherBackend() {
}

WatcherBackend::~WatcherBackend() {
}

void WatcherBackend::StartWatching(const WatchData& data) {
  RemoveAndNotify(data.handle, MOJO_RESULT_CANCELLED);

  DCHECK_EQ(0u, handle_to_data_.count(data.handle));

  handle_to_data_[data.handle] = data;
  message_pump_mojo->AddHandler(this, data.handle,
                                data.handle_signals,
                                data.deadline);
}

void WatcherBackend::StopWatching(WatcherID watcher_id) {
  // Because of the thread hop it is entirely possible to get here and not
  // have a valid handle registered for |watcher_id|.
  Handle handle;
  if (!GetMojoHandleByWatcherID(watcher_id, &handle))
    return;

  handle_to_data_.erase(handle);
  message_pump_mojo->RemoveHandler(handle);
}

void WatcherBackend::RemoveAndNotify(const Handle& handle,
                                     MojoResult result) {
  if (handle_to_data_.count(handle) == 0)
    return;

  const WatchData data(handle_to_data_[handle]);
  handle_to_data_.erase(handle);
  message_pump_mojo->RemoveHandler(handle);
  data.message_loop->PostTask(FROM_HERE, base::Bind(data.callback, result));
}

bool WatcherBackend::GetMojoHandleByWatcherID(WatcherID watcher_id,
                                              Handle* handle) const {
  for (HandleToWatchDataMap::const_iterator i = handle_to_data_.begin();
       i != handle_to_data_.end(); ++i) {
    if (i->second.id == watcher_id) {
      *handle = i->second.handle;
      return true;
    }
  }
  return false;
}

void WatcherBackend::OnHandleReady(const Handle& handle) {
  RemoveAndNotify(handle, MOJO_RESULT_OK);
}

void WatcherBackend::OnHandleError(const Handle& handle, MojoResult result) {
  RemoveAndNotify(handle, result);
}

// WatcherThreadManager --------------------------------------------------------

// WatcherThreadManager manages the background thread that listens for handles
// to be ready. All requests are handled by WatcherBackend.
class WatcherThreadManager {
 public:
  ~WatcherThreadManager();

  // Returns the shared instance.
  static WatcherThreadManager* GetInstance();

  // Starts watching the requested handle. Returns a unique ID that is used to
  // stop watching the handle. When the handle is ready |callback| is notified
  // on the thread StartWatching() was invoked on.
  // This may be invoked on any thread.
  WatcherID StartWatching(const Handle& handle,
                          MojoHandleSignals handle_signals,
                          base::TimeTicks deadline,
                          const base::Callback<void(MojoResult)>& callback);

  // Stops watching a handle.
  // This may be invoked on any thread.
  void StopWatching(WatcherID watcher_id);

 private:
  friend struct DefaultSingletonTraits<WatcherThreadManager>;
  WatcherThreadManager();

  base::Thread thread_;

  base::AtomicSequenceNumber watcher_id_generator_;

  WatcherBackend backend_;

  DISALLOW_COPY_AND_ASSIGN(WatcherThreadManager);
};

WatcherThreadManager::~WatcherThreadManager() {
  thread_.Stop();
}

WatcherThreadManager* WatcherThreadManager::GetInstance() {
  return Singleton<WatcherThreadManager>::get();
}

WatcherID WatcherThreadManager::StartWatching(
    const Handle& handle,
    MojoHandleSignals handle_signals,
    base::TimeTicks deadline,
    const base::Callback<void(MojoResult)>& callback) {
  WatchData data;
  data.id = watcher_id_generator_.GetNext();
  data.handle = handle;
  data.callback = callback;
  data.handle_signals = handle_signals;
  data.deadline = deadline;
  data.message_loop = base::MessageLoopProxy::current();
  DCHECK_NE(static_cast<base::MessageLoopProxy*>(NULL),
            data.message_loop.get());
  // We outlive |thread_|, so it's safe to use Unretained() here.
  thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&WatcherBackend::StartWatching,
                 base::Unretained(&backend_),
                 data));
  return data.id;
}

void WatcherThreadManager::StopWatching(WatcherID watcher_id) {
  // We outlive |thread_|, so it's safe to use Unretained() here.
  thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&WatcherBackend::StopWatching,
                 base::Unretained(&backend_),
                 watcher_id));
}

WatcherThreadManager::WatcherThreadManager()
    : thread_(kWatcherThreadName) {
  base::Thread::Options thread_options;
  thread_options.message_pump_factory = base::Bind(&CreateMessagePumpMojo);
  thread_.StartWithOptions(thread_options);
}

}  // namespace

// HandleWatcher::State --------------------------------------------------------

// Represents the state of the HandleWatcher. Owns the user's callback and
// monitors the current thread's MessageLoop to know when to force the callback
// to run (with an error) even though the pipe hasn't been signaled yet.
class HandleWatcher::State : public base::MessageLoop::DestructionObserver {
 public:
  State(HandleWatcher* watcher,
        const Handle& handle,
        MojoHandleSignals handle_signals,
        MojoDeadline deadline,
        const base::Callback<void(MojoResult)>& callback)
      : watcher_(watcher),
        callback_(callback),
        weak_factory_(this) {
    base::MessageLoop::current()->AddDestructionObserver(this);

    watcher_id_ = WatcherThreadManager::GetInstance()->StartWatching(
        handle,
        handle_signals,
        MojoDeadlineToTimeTicks(deadline),
        base::Bind(&State::OnHandleReady, weak_factory_.GetWeakPtr()));
  }

  virtual ~State() {
    base::MessageLoop::current()->RemoveDestructionObserver(this);

    WatcherThreadManager::GetInstance()->StopWatching(watcher_id_);
  }

 private:
  virtual void WillDestroyCurrentMessageLoop() OVERRIDE {
    // The current thread is exiting. Simulate a watch error.
    OnHandleReady(MOJO_RESULT_ABORTED);
  }

  void OnHandleReady(MojoResult result) {
    base::Callback<void(MojoResult)> callback = callback_;
    watcher_->Stop();  // Destroys |this|.

    callback.Run(result);
  }

  HandleWatcher* watcher_;
  WatcherID watcher_id_;
  base::Callback<void(MojoResult)> callback_;

  // Used to weakly bind |this| to the WatcherThreadManager.
  base::WeakPtrFactory<State> weak_factory_;
};

// HandleWatcher ---------------------------------------------------------------

HandleWatcher::HandleWatcher() {
}

HandleWatcher::~HandleWatcher() {
}

void HandleWatcher::Start(const Handle& handle,
                          MojoHandleSignals handle_signals,
                          MojoDeadline deadline,
                          const base::Callback<void(MojoResult)>& callback) {
  DCHECK(handle.is_valid());
  DCHECK_NE(MOJO_HANDLE_SIGNAL_NONE, handle_signals);

  state_.reset(new State(this, handle, handle_signals, deadline, callback));
}

void HandleWatcher::Stop() {
  state_.reset();
}

}  // namespace common
}  // namespace mojo
