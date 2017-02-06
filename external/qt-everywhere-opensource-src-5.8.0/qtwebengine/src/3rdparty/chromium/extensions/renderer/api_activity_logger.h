// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_ACTIVITY_LOGGER_H_
#define EXTENSIONS_RENDERER_API_ACTIVITY_LOGGER_H_

#include <string>

#include "base/macros.h"
#include "extensions/common/features/feature.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "v8/include/v8.h"

namespace extensions {
class Dispatcher;

// Used to log extension API calls and events that are implemented with custom
// bindings.The actions are sent via IPC to extensions::ActivityLog for
// recording and display.
class APIActivityLogger : public ObjectBackedNativeHandler {
 public:
  APIActivityLogger(ScriptContext* context, Dispatcher* dispatcher);
  ~APIActivityLogger() override;

 private:
  // Used to distinguish API calls & events from each other in LogInternal.
  enum CallType { APICALL, EVENT };

  // This is ultimately invoked in bindings.js with JavaScript arguments.
  //    arg0 - extension ID as a string
  //    arg1 - API call name as a string
  //    arg2 - arguments to the API call
  //    arg3 - any extra logging info as a string (optional)
  void LogAPICall(const v8::FunctionCallbackInfo<v8::Value>& args);

  // This is ultimately invoked in bindings.js with JavaScript arguments.
  //    arg0 - extension ID as a string
  //    arg1 - Event name as a string
  //    arg2 - Event arguments
  //    arg3 - any extra logging info as a string (optional)
  void LogEvent(const v8::FunctionCallbackInfo<v8::Value>& args);

  // LogAPICall and LogEvent are really the same underneath except for
  // how they are ultimately dispatched to the log.
  void LogInternal(const CallType call_type,
                   const v8::FunctionCallbackInfo<v8::Value>& args);

  Dispatcher* dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(APIActivityLogger);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_ACTIVITY_LOGGER_H_
