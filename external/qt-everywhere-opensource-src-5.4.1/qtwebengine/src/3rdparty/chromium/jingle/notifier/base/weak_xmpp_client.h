// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A thin wrapper around buzz::XmppClient that exposes weak pointers
// so that users know when the buzz::XmppClient becomes invalid to use
// (not necessarily only at destruction time).

#ifndef JINGLE_NOTIFIER_BASE_WEAK_XMPP_CLIENT_H_
#define JINGLE_NOTIFIER_BASE_WEAK_XMPP_CLIENT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "talk/xmpp/xmppclient.h"

namespace talk_base {
class TaskParent;
}  // namespace

namespace notifier {

// buzz::XmppClient's destructor isn't marked virtual, but it inherits
// from talk_base::Task, whose destructor *is* marked virtual, so we
// can safely inherit from it.
class WeakXmppClient : public buzz::XmppClient, public base::NonThreadSafe {
 public:
  explicit WeakXmppClient(talk_base::TaskParent* parent);

  virtual ~WeakXmppClient();

  // Returns a weak pointer that is invalidated when the XmppClient
  // becomes invalid to use.
  base::WeakPtr<WeakXmppClient> AsWeakPtr();

  // Invalidates all weak pointers to this object.  (This method is
  // necessary as calling Abort() does not always lead to Stop() being
  // called, so it's not a reliable way to cause an invalidation.)
  void Invalidate();

 protected:
  virtual void Stop() OVERRIDE;

 private:
  // We use our own WeakPtrFactory instead of inheriting from
  // SupportsWeakPtr since we want to invalidate in other places
  // besides the destructor.
  base::WeakPtrFactory<WeakXmppClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WeakXmppClient);
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_BASE_WEAK_XMPP_CLIENT_H_
