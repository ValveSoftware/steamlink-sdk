// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_NO_INTERFACE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_NO_INTERFACE_H_

#include <assert.h>

#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/message_filter.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {

// NoInterface is for use in cases when a non-existent or empty interface is
// needed (e.g., when the Mojom "Peer" attribute is not present).

class NoInterfaceProxy;
class NoInterfaceStub;

class NoInterface {
 public:
  static const char* Name_;
  typedef NoInterfaceProxy Proxy_;
  typedef NoInterfaceStub Stub_;
  typedef PassThroughFilter RequestValidator_;
  typedef PassThroughFilter ResponseValidator_;
  typedef NoInterface Client;
  virtual ~NoInterface() {}
};

class NoInterfaceProxy : public NoInterface {
 public:
  explicit NoInterfaceProxy(MessageReceiver* receiver) {}
};

class NoInterfaceStub : public MessageReceiverWithResponder {
 public:
  NoInterfaceStub() {}
  void set_sink(NoInterface* sink) {}
  NoInterface* sink() { return NULL; }
  virtual bool Accept(Message* message) MOJO_OVERRIDE;
  virtual bool AcceptWithResponder(Message* message, MessageReceiver* responder)
      MOJO_OVERRIDE;
};


// AnyInterface is for use in cases where any interface would do (e.g., see the
// Shell::Connect method).

typedef NoInterface AnyInterface;

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_NO_INTERFACE_H_
