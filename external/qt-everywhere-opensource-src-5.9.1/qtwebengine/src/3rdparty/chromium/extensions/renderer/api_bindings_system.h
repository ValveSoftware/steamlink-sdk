// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_BINDINGS_SYSTEM_H_
#define EXTENSIONS_RENDERER_API_BINDINGS_SYSTEM_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "extensions/renderer/api_binding.h"
#include "extensions/renderer/api_binding_types.h"
#include "extensions/renderer/api_event_handler.h"
#include "extensions/renderer/api_request_handler.h"
#include "extensions/renderer/argument_spec.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace extensions {
class APIRequestHandler;

// A class encompassing the necessary pieces to construct the JS entry points
// for Extension APIs. Designed to be used on a single thread, but safe between
// multiple v8::Contexts.
class APIBindingsSystem {
 public:
  // TODO(devlin): We will probably want to coalesce this with the
  // ExtensionHostMsg_Request_Params IPC struct.
  struct Request {
    Request();
    ~Request();

    std::string request_id;
    std::string method_name;
    std::unique_ptr<base::ListValue> arguments;
  };

  using GetAPISchemaMethod =
      base::Callback<const base::DictionaryValue&(const std::string&)>;
  using SendRequestMethod = base::Callback<void(std::unique_ptr<Request>)>;

  APIBindingsSystem(const binding::RunJSFunction& call_js,
                    const GetAPISchemaMethod& get_api_schema,
                    const SendRequestMethod& send_request);
  ~APIBindingsSystem();

  // Returns a new v8::Object representing the api specified by |api_name|.
  v8::Local<v8::Object> CreateAPIInstance(
      const std::string& api_name,
      v8::Local<v8::Context> context,
      v8::Isolate* isolate,
      const APIBinding::AvailabilityCallback& is_available);

  // Responds to the request with the given |request_id|, calling the callback
  // with |response|.
  void CompleteRequest(const std::string& request_id,
                       const base::ListValue& response);

  // Notifies the APIEventHandler to fire the corresponding event, notifying
  // listeners.
  void FireEventInContext(const std::string& event_name,
                          v8::Local<v8::Context> context,
                          const base::ListValue& response);

 private:
  // Creates a new APIBinding for the given |api_name|.
  std::unique_ptr<APIBinding> CreateNewAPIBinding(const std::string& api_name);

  // Handles a call into an API, adds a pending request to the
  // |request_handler_|, and calls |send_request_|.
  void OnAPICall(const std::string& name,
                 std::unique_ptr<base::ListValue> arguments,
                 v8::Isolate* isolate,
                 v8::Local<v8::Context> context,
                 v8::Local<v8::Function> callback);

  // The map of cached API reference types.
  ArgumentSpec::RefMap type_reference_map_;

  // The request handler associated with the system.
  APIRequestHandler request_handler_;

  // The event handler associated with the system.
  APIEventHandler event_handler_;

  // A map from api_name -> APIBinding for constructed APIs. APIBindings are
  // created lazily.
  std::map<std::string, std::unique_ptr<APIBinding>> api_bindings_;

  // The method to retrieve the DictionaryValue describing a given extension
  // API. Curried in for testing purposes so we can use fake APIs.
  GetAPISchemaMethod get_api_schema_;

  // The method to call when a new API call is triggered. Curried in for testing
  // purposes. Typically, this would send an IPC to the browser to begin the
  // function work.
  SendRequestMethod send_request_;

  DISALLOW_COPY_AND_ASSIGN(APIBindingsSystem);
};

}  // namespace

#endif  // EXTENSIONS_RENDERER_API_BINDINGS_SYSTEM_H_
