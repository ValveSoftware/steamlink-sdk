/* -*- c++ -*- */
/*
 * Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_NACL_PPAPI_UTIL_NACL_PPAPI_UTIL_H_
#define NATIVE_CLIENT_SRC_UNTRUSTED_NACL_PPAPI_UTIL_NACL_PPAPI_UTIL_H_

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/platform/nacl_sync_raii.h"

// TODO(bsy): move weak_ref module to the shared directory
#include "native_client/src/trusted/weak_ref/weak_ref.h"
#include "ppapi/native_client/src/trusted/weak_ref/call_on_main_thread.h"

// The nomenclature used in this file is intended to clarify thinking
// about the Pepper "main thread".  The "main thread" is really an
// interrupt service thread, or an event handler thread, since it is
// bad to do blocking operations or execute code that runs for a long
// time on it.  Event handlers should complete quickly -- possibly
// just enqueuing the event for processing by some worker thread --
// and return, so that additional event dispatch can occur.

// Code that does real work (and tests) should run in a separate,
// worker thread.  The main event handler thread is where the
// post-message handler runs via a pp::Module virtual member function
// (HandleMessage), which represents the plugin instance.  This plugin
// instance can go away at any time, e.g., due to surf-away.  Thus,
// other threads should not use pointers to the pp::Module, since the
// object isn't reference counted (and probably shouldn't be, since
// it really have to shut down / clean up when Pepper tells it to),
// and main thread-only operations such as PostMessage or
// GetOSFileDescriptor on the FileIO_Dev PP_Resource must not be done
// from the worker threads.

// Our solution to this is as follows:
//
// The plugin instance object holds a reference to a WeakRefAnchor
// object, and the plugin instance object's dtor invokes the Abandon
// method on the anchor.  Since the nacl::WeakRefAnchor object is
// thread-safe and is reference counted, the anchor pointer may be
// passed to worker threads.  Worker threads are responsible for
// maintaining the anchor refcount: each thread would hold a
// reference, and must Unref prior to thread exit.  The worker threads
// can use plugin::WeakRefCallOnMainThread to enqueue continuation
// callbacks to run on the main thread -- these will get cancelled if
// the WeakRefAnchor was abandoned, which only occurs in the module
// object's dtor, which can only run in the main thread.  Since the
// continuation won't run if the plugin instance is still valid, the
// continuation can safely use pointers to the instance to perform
// main-thread operations or to run member functions in the test
// object or in the module object.  The worker thread may hold a
// pointer to the plugin instance object in order to schedule
// callbacks via plugin::WeakRefCallOnMainThread using a method
// pointer, but should not otherwise use the pointer.
//
// So, an operation (test) running on a worker thread must be broken
// into separate computation phases according to which thread is
// appropriate for invoking which operations.  For compute-only phases
// or manifest RPCs (which block and also invoke CallOnMainThread),
// the computation should occur on the worker thread.  When the worker
// thread needs to invoke a main-thread-only operation such as
// PostMessage, it should use its WeakRefAnchor objecct to schedule a
// main thread callback and then wait on a condition variable for the
// operation to complete.  The main thread callback can invoke the
// main-thread-only operation, then signal the condition variable to
// wake up the worker thread prior to returning.  After the worker
// thread wakes up, it can use other synchronization methods to
// determine if the worker thread should continue to run or exit
// (e.g., if the worker thread is associated with the plugin instance,
// then if the main thread work result is NULL, the worker thread
// should probably Unref its anchor (and do other cleanup) and exit.

namespace nacl_ppapi {

template <typename R> class EventThreadWorkStateWrapper;  // fwd

// the worker thread should own the EventThreadWorkState<R> object
template <typename R>
class EventThreadWorkState {
 public:
  EventThreadWorkState()
      : done_(false),
        result_(NULL) {
    NaClXMutexCtor(&mu_);
    NaClXCondVarCtor(&cv_);
  }

  virtual ~EventThreadWorkState() {
    NaClMutexDtor(&mu_);
    NaClCondVarDtor(&cv_);
  }

  // Pass ownership of result into the EventThreadWorkState.  The value
  // of result should be non-NULL to distinguish between
  // completion/abandonment.
  void SetResult(R *result) {
    nacl::MutexLocker take(&mu_);
    result_.reset(result);
  }

  // Returns result if callback completed, NULL if abandoned
  R *WaitForCompletion() {
    nacl::MutexLocker take(&mu_);
    while (!done_) {
      NaClXCondVarWait(&cv_, &mu_);
    }
    return result_.release();
  }

 private:
  friend class EventThreadWorkStateWrapper<R>;
  void EventThreadWorkDone() {
    nacl::MutexLocker take(&mu_);
    done_ = true;
    NaClXCondVarBroadcast(&cv_);
  }

  NaClMutex mu_;
  NaClCondVar cv_;
  bool done_;
  nacl::scoped_ptr<R> result_;

  DISALLOW_COPY_AND_ASSIGN(EventThreadWorkState);
};


// Wrapper around EventThreadWorkState<R> or subclass thereof.  The
// wrapper object should be created by a worker thread and the
// ownership passed into the COMT machinery, to be used and deleted on
// the main thread.  This object is automatically deleted by the
// WeakRef machinery when the callback fires, and the dtor will just
// signal completion.  If the anchor corresponding to the callback had
// not been abandoned, then the callback function should invoke
// SetResult before returning to pass ownership of a result object (R)
// from the main thread to the worker thread.
//
// Subclasses of EventThreadWorkStateWrapper may be used, so that
// contained input arguments are automatically deleted when the
// callback fires, or input arguments may be stashed in subclasses of
// EventThreadWorkState<R>.
template <typename R>
class EventThreadWorkStateWrapper {
 public:
  explicit EventThreadWorkStateWrapper(EventThreadWorkState<R> *ws):
      ws_(ws) {}
  virtual ~EventThreadWorkStateWrapper() {
    ws_->EventThreadWorkDone();
  };

  void SetResult(R *result) {
    ws_->SetResult(result);
  }
 private:
  EventThreadWorkState<R> *ws_;

  DISALLOW_COPY_AND_ASSIGN(EventThreadWorkStateWrapper);
};

class VoidResult;

extern VoidResult *const g_void_result;

class VoidResult {
 public:
  VoidResult() {}
  void *operator new(size_t size) { return g_void_result; }
  void operator delete(void *p) {}
 private:
  DISALLOW_COPY_AND_ASSIGN(VoidResult);
};

// Canonical pointer return value used with SetResult when the main
// thread operation does not return a result.  The class declaration
// is private, so the compiler should refuse to allow the use of the
// delete operator.

// A plugin instance object should be referred to only from the main
// thread.  Pointers to the anchor object can be given to worker
// thread so they can schedule work on the main thread via COMT.
class NaClPpapiPluginInstance : public pp::Instance {
 public:
  explicit NaClPpapiPluginInstance(PP_Instance instance);
  virtual ~NaClPpapiPluginInstance();
  nacl::WeakRefAnchor* anchor() const { return anchor_; }
 protected:
  nacl::WeakRefAnchor* anchor_;
  DISALLOW_COPY_AND_ASSIGN(NaClPpapiPluginInstance);
};

}  // namespace nacl_ppapi

#endif
