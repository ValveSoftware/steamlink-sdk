// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_JS_WAITING_CALLBACK_H_
#define MOJO_EDK_JS_WAITING_CALLBACK_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "gin/handle.h"
#include "gin/runner.h"
#include "gin/wrappable.h"
#include "mojo/edk/js/handle.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/cpp/system/watcher.h"

namespace mojo {
namespace edk {
namespace js {

class WaitingCallback : public gin::Wrappable<WaitingCallback> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  // Creates a new WaitingCallback.
  //
  // If |one_shot| is true, the callback will only ever be called at most once.
  // If false, the callback may be called any number of times until the
  // WaitingCallback is explicitly cancelled.
  static gin::Handle<WaitingCallback> Create(
      v8::Isolate* isolate,
      v8::Handle<v8::Function> callback,
      gin::Handle<HandleWrapper> handle_wrapper,
      MojoHandleSignals signals,
      bool one_shot);

  // Cancels the callback. Does nothing if a callback is not pending. This is
  // implicitly invoked from the destructor but can be explicitly invoked as
  // necessary.
  void Cancel();

 private:
  WaitingCallback(v8::Isolate* isolate,
                  v8::Handle<v8::Function> callback,
                  bool one_shot);
  ~WaitingCallback() override;

  // Callback from the Watcher.
  void OnHandleReady(MojoResult result);

  // Indicates whether this is a one-shot callback or not. If so, it uses the
  // deprecated HandleWatcher to wait for signals; otherwise it uses the new
  // system Watcher API.
  const bool one_shot_;

  base::WeakPtr<gin::Runner> runner_;
  Watcher watcher_;
  base::WeakPtrFactory<WaitingCallback> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WaitingCallback);
};

}  // namespace js
}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_JS_WAITING_CALLBACK_H_
