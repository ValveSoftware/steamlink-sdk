// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_REQUEST_HANDLER_H_
#define EXTENSIONS_RENDERER_API_REQUEST_HANDLER_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "v8/include/v8.h"

namespace base {
class ListValue;
}

namespace extensions {

// A wrapper around a map for extension API calls. Contains all pending requests
// and the associated context and callback. Designed to be used on a single
// thread, but amongst multiple contexts.
class APIRequestHandler {
 public:
  using CallJSFunction = base::Callback<void(v8::Local<v8::Function>,
                                             v8::Local<v8::Context>,
                                             int argc,
                                             v8::Local<v8::Value>[])>;

  explicit APIRequestHandler(const CallJSFunction& call_js);
  ~APIRequestHandler();

  // Adds a pending request to the map. Returns a unique identifier for that
  // request.
  std::string AddPendingRequest(v8::Isolate* isolate,
                                v8::Local<v8::Function> callback,
                                v8::Local<v8::Context> context);

  // Responds to the request with the given |request_id|, calling the callback
  // with the given |response| arguments.
  // Invalid ids are ignored.
  void CompleteRequest(const std::string& request_id,
                       const base::ListValue& response);

  // Invalidates any requests that are associated with |context|.
  void InvalidateContext(v8::Local<v8::Context> context);

  std::set<std::string> GetPendingRequestIdsForTesting() const;

 private:
  struct PendingRequest {
    PendingRequest(v8::Isolate* isolate,
                   v8::Local<v8::Function> callback,
                   v8::Local<v8::Context> context);
    ~PendingRequest();
    PendingRequest(PendingRequest&&);
    PendingRequest& operator=(PendingRequest&&);

    v8::Isolate* isolate;
    v8::Global<v8::Function> callback;
    v8::Global<v8::Context> context;
  };

  // A map of all pending requests.
  std::map<std::string, PendingRequest> pending_requests_;

  // The method to call into a JS with specific arguments. We curry this in
  // because the manner we want to do this is a unittest (e.g.
  // v8::Function::Call) can be significantly different than in production
  // (where we have to deal with e.g. blocking javascript).
  CallJSFunction call_js_;

  DISALLOW_COPY_AND_ASSIGN(APIRequestHandler);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_REQUEST_HANDLER_H_
