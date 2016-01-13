// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_SIMPLE_DISPATCHER_H_
#define MOJO_SYSTEM_SIMPLE_DISPATCHER_H_

#include <list>

#include "base/basictypes.h"
#include "mojo/system/dispatcher.h"
#include "mojo/system/handle_signals_state.h"
#include "mojo/system/system_impl_export.h"
#include "mojo/system/waiter_list.h"

namespace mojo {
namespace system {

// A base class for simple dispatchers. "Simple" means that there's a one-to-one
// correspondence between handles and dispatchers (see the explanatory comment
// in core.cc). This class implements the standard waiter-signalling mechanism
// in that case.
class MOJO_SYSTEM_IMPL_EXPORT SimpleDispatcher : public Dispatcher {
 protected:
  SimpleDispatcher();
  virtual ~SimpleDispatcher();

  // To be called by subclasses when the state changes (so
  // |GetHandleSignalsStateNoLock()| should be checked again). Must be called
  // under lock.
  void HandleSignalsStateChangedNoLock();

  // Never called after the dispatcher has been closed; called under |lock_|.
  virtual HandleSignalsState GetHandleSignalsStateNoLock() const = 0;

  // |Dispatcher| protected methods:
  virtual void CancelAllWaitersNoLock() OVERRIDE;
  virtual MojoResult AddWaiterImplNoLock(Waiter* waiter,
                                         MojoHandleSignals signals,
                                         uint32_t context) OVERRIDE;
  virtual void RemoveWaiterImplNoLock(Waiter* waiter) OVERRIDE;

 private:
  // Protected by |lock()|:
  WaiterList waiter_list_;

  DISALLOW_COPY_AND_ASSIGN(SimpleDispatcher);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_SIMPLE_DISPATCHER_H_
