// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_BINDINGS_JS_WAITING_CALLBACK_H_
#define MOJO_BINDINGS_JS_WAITING_CALLBACK_H_

#include "gin/handle.h"
#include "gin/runner.h"
#include "gin/wrappable.h"
#include "mojo/public/c/environment/async_waiter.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {
namespace js {

class WaitingCallback : public gin::Wrappable<WaitingCallback> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  // Creates a new WaitingCallback.
  static gin::Handle<WaitingCallback> Create(
      v8::Isolate* isolate,
      v8::Handle<v8::Function> callback,
      mojo::Handle handle,
      MojoHandleSignals signals);

  // Cancels the callback. Does nothing if a callback is not pending. This is
  // implicitly invoked from the destructor but can be explicitly invoked as
  // necessary.
  void Cancel();

 private:
  WaitingCallback(v8::Isolate* isolate, v8::Handle<v8::Function> callback);
  virtual ~WaitingCallback();

  // Callback from MojoAsyncWaiter. |closure| is the WaitingCallback.
  static void CallOnHandleReady(void* closure, MojoResult result);

  // Invoked from CallOnHandleReady() (CallOnHandleReady() must be static).
  void OnHandleReady(MojoResult result);

  base::WeakPtr<gin::Runner> runner_;
  MojoAsyncWaitID wait_id_;

  DISALLOW_COPY_AND_ASSIGN(WaitingCallback);
};

}  // namespace js
}  // namespace mojo

#endif  // MOJO_BINDINGS_JS_WAITING_CALLBACK_H_
