// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_PTR_INTERNAL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_PTR_INTERNAL_H_

#include <assert.h>
#include <stdio.h>

#include "mojo/public/cpp/bindings/lib/filter_chain.h"
#include "mojo/public/cpp/bindings/lib/message_header_validator.h"
#include "mojo/public/cpp/bindings/lib/router.h"
#include "mojo/public/cpp/environment/environment.h"

namespace mojo {
namespace internal {

template <typename Interface>
class InterfacePtrState {
 public:
  InterfacePtrState() : proxy_(NULL), router_(NULL) {}

  ~InterfacePtrState() {
    // Destruction order matters here. We delete |proxy_| first, even though
    // |router_| may have a reference to it, so that |~Interface| may have a
    // shot at generating new outbound messages (ie, invoking client methods).
    delete proxy_;
    delete router_;
  }

  Interface* instance() const { return proxy_; }

  Router* router() const { return router_; }

  void Swap(InterfacePtrState* other) {
    std::swap(other->proxy_, proxy_);
    std::swap(other->router_, router_);
  }

  void ConfigureProxy(
      ScopedMessagePipeHandle handle,
      const MojoAsyncWaiter* waiter = Environment::GetDefaultAsyncWaiter()) {
    assert(!proxy_);
    assert(!router_);

    FilterChain filters;
    filters.Append<MessageHeaderValidator>();
    filters.Append<typename Interface::Client::RequestValidator_>();
    filters.Append<typename Interface::ResponseValidator_>();

    router_ = new Router(handle.Pass(), filters.Pass(), waiter);
    ProxyWithStub* proxy = new ProxyWithStub(router_);
    router_->set_incoming_receiver(&proxy->stub);

    proxy_ = proxy;
  }

  void set_client(typename Interface::Client* client) {
    assert(proxy_);
    proxy_->stub.set_sink(client);
  }

 private:
  class ProxyWithStub : public Interface::Proxy_ {
   public:
    explicit ProxyWithStub(MessageReceiverWithResponder* receiver)
        : Interface::Proxy_(receiver) {
    }
    typename Interface::Client::Stub_ stub;
   private:
    MOJO_DISALLOW_COPY_AND_ASSIGN(ProxyWithStub);
  };

  ProxyWithStub* proxy_;
  Router* router_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(InterfacePtrState);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_PTR_INTERNAL_H_
