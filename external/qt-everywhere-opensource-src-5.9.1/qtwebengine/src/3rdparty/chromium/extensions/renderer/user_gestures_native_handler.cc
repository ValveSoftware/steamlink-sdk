// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/user_gestures_native_handler.h"

#include "base/bind.h"
#include "extensions/renderer/script_context.h"
#include "third_party/WebKit/public/web/WebScopedUserGesture.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"

namespace extensions {

UserGesturesNativeHandler::UserGesturesNativeHandler(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction("IsProcessingUserGesture",
                "test",
                base::Bind(&UserGesturesNativeHandler::IsProcessingUserGesture,
                           base::Unretained(this)));
  RouteFunction("RunWithUserGesture",
                "test",
                base::Bind(&UserGesturesNativeHandler::RunWithUserGesture,
                           base::Unretained(this)));
  RouteFunction("RunWithoutUserGesture",
                "test",
                base::Bind(&UserGesturesNativeHandler::RunWithoutUserGesture,
                           base::Unretained(this)));
}

void UserGesturesNativeHandler::IsProcessingUserGesture(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().Set(v8::Boolean::New(
      args.GetIsolate(),
      blink::WebUserGestureIndicator::isProcessingUserGesture()));
}

void UserGesturesNativeHandler::RunWithUserGesture(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  blink::WebScopedUserGesture user_gesture(context()->web_frame());
  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsFunction());
  context()->SafeCallFunction(v8::Local<v8::Function>::Cast(args[0]), 0,
                              nullptr);
}

void UserGesturesNativeHandler::RunWithoutUserGesture(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  blink::WebUserGestureIndicator::consumeUserGesture();
  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsFunction());
  v8::Local<v8::Value> no_args;
  context()->SafeCallFunction(v8::Local<v8::Function>::Cast(args[0]), 0,
                              nullptr);
}

}  // namespace extensions
