// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PROXY_LOCK_H_
#define PPAPI_SHARED_IMPL_PROXY_LOCK_H_

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/threading/thread_checker.h"

#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace base {
class Lock;
}

namespace content {
class HostGlobals;
}

namespace ppapi {

// This is the one lock to rule them all for the ppapi proxy. All PPB interface
// functions that need to be synchronized should lock this lock on entry. This
// is normally accomplished by using an appropriate Enter RAII object at the
// beginning of each thunk function.
//
// TODO(dmichael): If this turns out to be too slow and contentious, we'll want
// to use multiple locks. E.g., one for the var tracker, one for the resource
// tracker, etc.
class PPAPI_SHARED_EXPORT ProxyLock {
 public:
  // Return the global ProxyLock. Normally, you should not access this
  // directly but instead use ProxyAutoLock or ProxyAutoUnlock. But sometimes
  // you need access to the ProxyLock, for example to create a condition
  // variable.
  static base::Lock* Get();

  // Acquire the proxy lock. If it is currently held by another thread, block
  // until it is available. If the lock has not been set using the 'Set' method,
  // this operation does nothing. That is the normal case for the host side;
  // see PluginResourceTracker for where the lock gets set for the out-of-
  // process plugin case.
  static void Acquire();
  // Relinquish the proxy lock. If the lock has not been set, this does nothing.
  static void Release();

  // Assert that the lock is owned by the current thread (in the plugin
  // process). Does nothing when running in-process (or in the host process).
  static void AssertAcquired();
  static void AssertAcquiredDebugOnly() {
#ifndef NDEBUG
    AssertAcquired();
#endif
  }

  // We have some unit tests where one thread pretends to be the host and one
  // pretends to be the plugin. This allows the lock to do nothing on only one
  // thread to support these tests. See TwoWayTest for more information.
  static void DisableLockingOnThreadForTest();

  // Enables locking on the current thread. Although locking is enabled by
  // default, unit tests that rely on the lock being enabled should *still*
  // call this, since a previous test may have disabled locking.
  static void EnableLockingOnThreadForTest();

 private:
  friend class content::HostGlobals;
  // On the host side, we do not lock. This must be called at most once at
  // startup, before other threads that may access the ProxyLock have had a
  // chance to run.
  static void DisableLocking();

  DISALLOW_IMPLICIT_CONSTRUCTORS(ProxyLock);
};

// A simple RAII class for locking the PPAPI proxy lock on entry and releasing
// on exit. This is for simple interfaces that don't use the 'thunk' system,
// such as PPB_Var and PPB_Core.
class ProxyAutoLock {
 public:
  ProxyAutoLock() { ProxyLock::Acquire(); }
  ~ProxyAutoLock() { ProxyLock::Release(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyAutoLock);
};

// The inverse of the above; unlock on construction, lock on destruction. This
// is useful for calling out to the plugin, when we need to unlock but ensure
// that we re-acquire the lock when the plugin is returns or raises an
// exception.
class ProxyAutoUnlock {
 public:
  ProxyAutoUnlock() { ProxyLock::Release(); }
  ~ProxyAutoUnlock() { ProxyLock::Acquire(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyAutoUnlock);
};

// A set of function template overloads for invoking a function pointer while
// the ProxyLock is unlocked. This assumes that the luck is held.
// CallWhileUnlocked unlocks the ProxyLock just before invoking the given
// function. The lock is immediately re-acquired when the invoked function
// function returns. CallWhileUnlocked returns whatever the given function
// returned.
//
// Example usage:
//   *result = CallWhileUnlocked(ppp_input_event_impl_->HandleInputEvent,
//                               instance,
//                               resource->pp_resource());
template <class ReturnType>
ReturnType CallWhileUnlocked(ReturnType (*function)()) {
  ProxyAutoUnlock unlock;
  return function();
}
template <class ReturnType, class P1>
ReturnType CallWhileUnlocked(ReturnType (*function)(P1), const P1& p1) {
  ProxyAutoUnlock unlock;
  return function(p1);
}
template <class ReturnType, class P1, class P2>
ReturnType CallWhileUnlocked(ReturnType (*function)(P1, P2),
                             const P1& p1,
                             const P2& p2) {
  ProxyAutoUnlock unlock;
  return function(p1, p2);
}
template <class ReturnType, class P1, class P2, class P3>
ReturnType CallWhileUnlocked(ReturnType (*function)(P1, P2, P3),
                             const P1& p1,
                             const P2& p2,
                             const P3& p3) {
  ProxyAutoUnlock unlock;
  return function(p1, p2, p3);
}
template <class ReturnType, class P1, class P2, class P3, class P4>
ReturnType CallWhileUnlocked(ReturnType (*function)(P1, P2, P3, P4),
                             const P1& p1,
                             const P2& p2,
                             const P3& p3,
                             const P4& p4) {
  ProxyAutoUnlock unlock;
  return function(p1, p2, p3, p4);
}
template <class ReturnType, class P1, class P2, class P3, class P4, class P5>
ReturnType CallWhileUnlocked(ReturnType (*function)(P1, P2, P3, P4, P5),
                             const P1& p1,
                             const P2& p2,
                             const P3& p3,
                             const P4& p4,
                             const P5& p5) {
  ProxyAutoUnlock unlock;
  return function(p1, p2, p3, p4, p5);
}
void PPAPI_SHARED_EXPORT CallWhileUnlocked(const base::Closure& closure);

namespace internal {

template <typename RunType>
class RunWhileLockedHelper;

template <>
class RunWhileLockedHelper<void()> {
 public:
  typedef base::Callback<void()> CallbackType;
  explicit RunWhileLockedHelper(const CallbackType& callback)
      : callback_(new CallbackType(callback)) {
    // Copying |callback| may adjust reference counts for bound Vars or
    // Resources; we should have the lock already.
    ProxyLock::AssertAcquired();
    // CallWhileLocked and destruction might happen on a different thread from
    // creation.
    thread_checker_.DetachFromThread();
  }
  void CallWhileLocked() {
    // Bind thread_checker_ to this thread so we can check in the destructor.
    DCHECK(thread_checker_.CalledOnValidThread());
    ProxyAutoLock lock;
    {
      // Use a scope and local Callback to ensure that the callback is cleared
      // before the lock is released, even in the unlikely event that Run()
      // throws an exception.
      scoped_ptr<CallbackType> temp_callback(callback_.Pass());
      temp_callback->Run();
    }
  }

  ~RunWhileLockedHelper() {
    // Check that the Callback is destroyed on the same thread as where
    // CallWhileLocked happened (if CallWhileLocked happened).
    DCHECK(thread_checker_.CalledOnValidThread());
    // Here we read callback_ without the lock. This is why the callback must be
    // destroyed on the same thread where it runs. There are 2 cases where
    // callback_ will be NULL:
    //   1) This is the original RunWhileLockedHelper that RunWhileLocked
    //      created. When it was copied somewhere else (e.g., to a MessageLoop
    //      queue), callback_ was passed to the new copy, and the original
    //      RunWhileLockedHelper's callback_ was set to NULL (since scoped_ptrs
    //      only ever have 1 owner). In this case, we don't want to acquire the
    //      lock, because we already have it.
    //   2) callback_ has already been run via CallWhileLocked. In this case,
    //      there's no need to acquire the lock, because we don't touch any
    //      shared data.
    if (callback_) {
      // If the callback was not run, we still need to have the lock when we
      // destroy the callback in case it had a Resource bound to it. This
      // ensures that the Resource's destructor is invoked only with the lock
      // held.
      //
      // Also: Resource and Var inherit RefCounted (not ThreadSafeRefCounted),
      // and these callbacks need to be usable on any thread. So we need to lock
      // when releasing the callback to avoid ref counting races.
      ProxyAutoLock lock;
      callback_.reset();
    }
  }

 private:
  scoped_ptr<CallbackType> callback_;

  // Used to ensure that the Callback is run and deleted on the same thread.
  base::ThreadChecker thread_checker_;
};

template <typename P1>
class RunWhileLockedHelper<void(P1)> {
 public:
  typedef base::Callback<void(P1)> CallbackType;
  explicit RunWhileLockedHelper(const CallbackType& callback)
      : callback_(new CallbackType(callback)) {
    ProxyLock::AssertAcquired();
    thread_checker_.DetachFromThread();
  }
  void CallWhileLocked(P1 p1) {
    DCHECK(thread_checker_.CalledOnValidThread());
    ProxyAutoLock lock;
    {
      scoped_ptr<CallbackType> temp_callback(callback_.Pass());
      temp_callback->Run(p1);
    }
  }
  ~RunWhileLockedHelper() {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (callback_) {
      ProxyAutoLock lock;
      callback_.reset();
    }
  }

 private:
  scoped_ptr<CallbackType> callback_;
  base::ThreadChecker thread_checker_;
};

template <typename P1, typename P2>
class RunWhileLockedHelper<void(P1, P2)> {
 public:
  typedef base::Callback<void(P1, P2)> CallbackType;
  explicit RunWhileLockedHelper(const CallbackType& callback)
      : callback_(new CallbackType(callback)) {
    ProxyLock::AssertAcquired();
    thread_checker_.DetachFromThread();
  }
  void CallWhileLocked(P1 p1, P2 p2) {
    DCHECK(thread_checker_.CalledOnValidThread());
    ProxyAutoLock lock;
    {
      scoped_ptr<CallbackType> temp_callback(callback_.Pass());
      temp_callback->Run(p1, p2);
    }
  }
  ~RunWhileLockedHelper() {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (callback_) {
      ProxyAutoLock lock;
      callback_.reset();
    }
  }

 private:
  scoped_ptr<CallbackType> callback_;
  base::ThreadChecker thread_checker_;
};

template <typename P1, typename P2, typename P3>
class RunWhileLockedHelper<void(P1, P2, P3)> {
 public:
  typedef base::Callback<void(P1, P2, P3)> CallbackType;
  explicit RunWhileLockedHelper(const CallbackType& callback)
      : callback_(new CallbackType(callback)) {
    ProxyLock::AssertAcquired();
    thread_checker_.DetachFromThread();
  }
  void CallWhileLocked(P1 p1, P2 p2, P3 p3) {
    DCHECK(thread_checker_.CalledOnValidThread());
    ProxyAutoLock lock;
    {
      scoped_ptr<CallbackType> temp_callback(callback_.Pass());
      temp_callback->Run(p1, p2, p3);
    }
  }
  ~RunWhileLockedHelper() {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (callback_) {
      ProxyAutoLock lock;
      callback_.reset();
    }
  }

 private:
  scoped_ptr<CallbackType> callback_;
  base::ThreadChecker thread_checker_;
};

}  // namespace internal

// RunWhileLocked wraps the given Callback in a new Callback that, when invoked:
//  1) Locks the ProxyLock.
//  2) Runs the original Callback (forwarding arguments, if any).
//  3) Clears the original Callback (while the lock is held).
//  4) Unlocks the ProxyLock.
// Note that it's important that the callback is cleared in step (3), in case
// clearing the Callback causes a destructor (e.g., for a Resource) to run,
// which should hold the ProxyLock to avoid data races.
//
// This is for cases where you want to run a task or store a Callback, but you
// want to ensure that the ProxyLock is acquired for the duration of the task
// that the Callback runs.
// EXAMPLE USAGE:
//   GetMainThreadMessageLoop()->PostDelayedTask(
//     FROM_HERE,
//     RunWhileLocked(base::Bind(&CallbackWrapper, callback, result)),
//     delay_in_ms);
//
// In normal usage like the above, this all should "just work". However, if you
// do something unusual, you may get a runtime crash due to deadlock. Here are
// the ways that the returned Callback must be used to avoid a deadlock:
// (1) copied to another Callback. After that, the original callback can be
// destroyed with or without the proxy lock acquired, while the newly assigned
// callback has to conform to these same restrictions. Or
// (2) run without proxy lock acquired (e.g., being posted to a MessageLoop
// and run there). The callback must be destroyed on the same thread where it
// was run (but can be destroyed with or without the proxy lock acquired). Or
// (3) destroyed without the proxy lock acquired.
template <class FunctionType>
inline base::Callback<FunctionType> RunWhileLocked(
    const base::Callback<FunctionType>& callback) {
  internal::RunWhileLockedHelper<FunctionType>* helper =
      new internal::RunWhileLockedHelper<FunctionType>(callback);
  return base::Bind(
      &internal::RunWhileLockedHelper<FunctionType>::CallWhileLocked,
      base::Owned(helper));
}

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PROXY_LOCK_H_
