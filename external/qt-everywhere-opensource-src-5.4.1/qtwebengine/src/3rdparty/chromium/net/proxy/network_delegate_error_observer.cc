// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/network_delegate_error_observer.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "net/base/net_errors.h"
#include "net/base/network_delegate.h"

namespace net {

// NetworkDelegateErrorObserver::Core -----------------------------------------

class NetworkDelegateErrorObserver::Core
    : public base::RefCountedThreadSafe<NetworkDelegateErrorObserver::Core> {
 public:
  Core(NetworkDelegate* network_delegate, base::MessageLoopProxy* origin_loop);

  void NotifyPACScriptError(int line_number, const base::string16& error);

  void Shutdown();

 private:
  friend class base::RefCountedThreadSafe<NetworkDelegateErrorObserver::Core>;

  virtual ~Core();

  NetworkDelegate* network_delegate_;
  scoped_refptr<base::MessageLoopProxy> origin_loop_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

NetworkDelegateErrorObserver::Core::Core(NetworkDelegate* network_delegate,
                                         base::MessageLoopProxy* origin_loop)
  : network_delegate_(network_delegate),
    origin_loop_(origin_loop) {
  DCHECK(origin_loop);
}

NetworkDelegateErrorObserver::Core::~Core() {}


void NetworkDelegateErrorObserver::Core::NotifyPACScriptError(
    int line_number,
    const base::string16& error) {
  if (!origin_loop_->BelongsToCurrentThread()) {
    origin_loop_->PostTask(
        FROM_HERE,
        base::Bind(&Core::NotifyPACScriptError, this, line_number, error));
    return;
  }
  if (network_delegate_)
    network_delegate_->NotifyPACScriptError(line_number, error);
}

void NetworkDelegateErrorObserver::Core::Shutdown() {
  CHECK(origin_loop_->BelongsToCurrentThread());
  network_delegate_ = NULL;
}

// NetworkDelegateErrorObserver -----------------------------------------------

NetworkDelegateErrorObserver::NetworkDelegateErrorObserver(
    NetworkDelegate* network_delegate,
    base::MessageLoopProxy* origin_loop)
    : core_(new Core(network_delegate, origin_loop)) {}

NetworkDelegateErrorObserver::~NetworkDelegateErrorObserver() {
  core_->Shutdown();
}

void NetworkDelegateErrorObserver::OnPACScriptError(
    int line_number,
    const base::string16& error) {
  core_->NotifyPACScriptError(line_number, error);
}

}  // namespace net
