// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_binding.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "extensions/common/extension_api.h"
#include "extensions/renderer/api_event_handler.h"
#include "extensions/renderer/v8_helpers.h"
#include "gin/arguments.h"
#include "gin/per_context_data.h"

namespace extensions {

namespace {

const char kExtensionAPIPerContextKey[] = "extension_api_binding";

using APISignature = std::vector<std::unique_ptr<ArgumentSpec>>;

// Returns the expected APISignature for a dictionary describing an API method.
std::unique_ptr<APISignature> GetAPISignature(
    const base::DictionaryValue& dict) {
  std::unique_ptr<APISignature> signature = base::MakeUnique<APISignature>();
  const base::ListValue* params = nullptr;
  CHECK(dict.GetList("parameters", &params));
  signature->reserve(params->GetSize());
  for (const auto& value : *params) {
    const base::DictionaryValue* param = nullptr;
    CHECK(value->GetAsDictionary(&param));
    std::string name;
    CHECK(param->GetString("name", &name));
    signature->push_back(base::MakeUnique<ArgumentSpec>(*param));
  }
  return signature;
}

// Attempts to match an argument from |arguments| to the given |spec|.
// If the next argmument does not match and |spec| is optional, a null
// base::Value is returned.
// If the argument matches, |arguments| is advanced and the converted value is
// returned.
// If the argument does not match and it is not optional, returns null and
// populates error.
std::unique_ptr<base::Value> ParseArgument(
    const ArgumentSpec& spec,
    v8::Local<v8::Context> context,
    gin::Arguments* arguments,
    const ArgumentSpec::RefMap& type_refs,
    std::string* error) {
  v8::Local<v8::Value> value = arguments->PeekNext();
  if (value.IsEmpty() || value->IsNull() || value->IsUndefined()) {
    if (!spec.optional()) {
      *error = "Missing required argument: " + spec.name();
      return nullptr;
    }
    // This is safe to call even if |arguments| is at the end (which can happen
    // if n optional arguments are omitted at the end of the signature).
    arguments->Skip();
    return base::Value::CreateNullValue();
  }

  std::unique_ptr<base::Value> result =
      spec.ConvertArgument(context, value, type_refs, error);
  if (!result) {
    if (!spec.optional()) {
      *error = "Missing required argument: " + spec.name();
      return nullptr;
    }
    return base::Value::CreateNullValue();
  }

  arguments->Skip();
  return result;
}

// Parses the callback from |arguments| according to |callback_spec|. Since the
// callback isn't converted into a base::Value, this is different from
// ParseArgument() above.
bool ParseCallback(gin::Arguments* arguments,
                   const ArgumentSpec& callback_spec,
                   std::string* error,
                   v8::Local<v8::Function>* callback_out) {
  v8::Local<v8::Value> value = arguments->PeekNext();
  if (value.IsEmpty() || value->IsNull() || value->IsUndefined()) {
    if (!callback_spec.optional()) {
      *error = "Missing required argument: " + callback_spec.name();
      return false;
    }
    arguments->Skip();
    return true;
  }

  if (!value->IsFunction()) {
    *error = "Argument is wrong type: " + callback_spec.name();
    return false;
  }

  *callback_out = value.As<v8::Function>();
  arguments->Skip();
  return true;
}

// Parses |args| against |signature| and populates error with any errors.
std::unique_ptr<base::ListValue> ParseArguments(
    const APISignature* signature,
    gin::Arguments* arguments,
    const ArgumentSpec::RefMap& type_refs,
    v8::Local<v8::Function>* callback,
    std::string* error) {
  auto results = base::MakeUnique<base::ListValue>();

  // TODO(devlin): This is how extension APIs have always determined if a
  // function has a callback, but it seems a little silly. In the long run (once
  // signatures are generated), it probably makes sense to indicate this
  // differently.
  bool signature_has_callback =
      !signature->empty() &&
      signature->back()->type() == ArgumentType::FUNCTION;

  v8::Local<v8::Context> context = arguments->isolate()->GetCurrentContext();

  size_t end_size =
      signature_has_callback ? signature->size() - 1 : signature->size();
  for (size_t i = 0; i < end_size; ++i) {
    std::unique_ptr<base::Value> parsed =
        ParseArgument(*signature->at(i), context, arguments, type_refs, error);
    if (!parsed)
      return nullptr;
    results->Append(std::move(parsed));
  }

  v8::Local<v8::Function> callback_value;
  if (signature_has_callback &&
      !ParseCallback(arguments, *signature->back(), error, &callback_value)) {
    return nullptr;
  }

  if (!arguments->PeekNext().IsEmpty())
    return nullptr;  // Extra arguments aren't allowed.

  *callback = callback_value;
  return results;
}

void CallbackHelper(const v8::FunctionCallbackInfo<v8::Value>& info) {
  gin::Arguments args(info);
  v8::Local<v8::External> external;
  CHECK(args.GetData(&external));
  auto callback = static_cast<APIBinding::HandlerCallback*>(external->Value());
  callback->Run(&args);
}

}  // namespace

APIBinding::APIPerContextData::APIPerContextData() {}
APIBinding::APIPerContextData::~APIPerContextData() {}

APIBinding::APIBinding(const std::string& api_name,
                       const base::ListValue& function_definitions,
                       const base::ListValue* type_definitions,
                       const base::ListValue* event_definitions,
                       const APIMethodCallback& callback,
                       ArgumentSpec::RefMap* type_refs)
    : api_name_(api_name),
      method_callback_(callback),
      type_refs_(type_refs),
      weak_factory_(this) {
  DCHECK(!method_callback_.is_null());
  for (const auto& func : function_definitions) {
    const base::DictionaryValue* func_dict = nullptr;
    CHECK(func->GetAsDictionary(&func_dict));
    std::string name;
    CHECK(func_dict->GetString("name", &name));
    std::unique_ptr<APISignature> spec = GetAPISignature(*func_dict);
    signatures_[name] = std::move(spec);
  }
  if (type_definitions) {
    for (const auto& type : *type_definitions) {
      const base::DictionaryValue* type_dict = nullptr;
      CHECK(type->GetAsDictionary(&type_dict));
      std::string id;
      CHECK(type_dict->GetString("id", &id));
      DCHECK(type_refs->find(id) == type_refs->end());
      // TODO(devlin): refs are sometimes preceeded by the API namespace; we
      // might need to take that into account.
      (*type_refs)[id] = base::MakeUnique<ArgumentSpec>(*type_dict);
    }
  }
  if (event_definitions) {
    event_names_.reserve(event_definitions->GetSize());
    for (const auto& event : *event_definitions) {
      const base::DictionaryValue* event_dict = nullptr;
      CHECK(event->GetAsDictionary(&event_dict));
      std::string name;
      CHECK(event_dict->GetString("name", &name));
      event_names_.push_back(std::move(name));
    }
  }
}

APIBinding::~APIBinding() {}

v8::Local<v8::Object> APIBinding::CreateInstance(
    v8::Local<v8::Context> context,
    v8::Isolate* isolate,
    APIEventHandler* event_handler,
    const AvailabilityCallback& is_available) {
  // TODO(devlin): APIs may change depending on which features are available,
  // but we should be able to cache the unconditional methods on an object
  // template, create the object, and then add any conditional methods. Ideally,
  // this information should be available on the generated API specification.
  v8::Local<v8::Object> object = v8::Object::New(isolate);
  gin::PerContextData* per_context_data = gin::PerContextData::From(context);
  DCHECK(per_context_data);
  APIPerContextData* data = static_cast<APIPerContextData*>(
      per_context_data->GetUserData(kExtensionAPIPerContextKey));
  if (!data) {
    auto api_data = base::MakeUnique<APIPerContextData>();
    data = api_data.get();
    per_context_data->SetUserData(kExtensionAPIPerContextKey,
                                  api_data.release());
  }
  for (const auto& sig : signatures_) {
    std::string full_method_name =
        base::StringPrintf("%s.%s", api_name_.c_str(), sig.first.c_str());

    if (!is_available.Run(full_method_name))
      continue;

    auto handler_callback = base::MakeUnique<HandlerCallback>(
        base::Bind(&APIBinding::HandleCall, weak_factory_.GetWeakPtr(),
                   full_method_name, sig.second.get()));
    // TODO(devlin): We should be able to cache these in a function template.
    v8::MaybeLocal<v8::Function> maybe_function =
        v8::Function::New(context, &CallbackHelper,
                          v8::External::New(isolate, handler_callback.get()),
                          0, v8::ConstructorBehavior::kThrow);
    data->context_callbacks.push_back(std::move(handler_callback));
    v8::Maybe<bool> success = object->CreateDataProperty(
        context, gin::StringToSymbol(isolate, sig.first),
        maybe_function.ToLocalChecked());
    DCHECK(success.IsJust());
    DCHECK(success.FromJust());
  }

  for (const std::string& event_name : event_names_) {
    std::string full_event_name =
        base::StringPrintf("%s.%s", api_name_.c_str(), event_name.c_str());
    v8::Local<v8::Object> event =
        event_handler->CreateEventInstance(full_event_name, context);
    DCHECK(!event.IsEmpty());
    v8::Maybe<bool> success = object->CreateDataProperty(
        context, gin::StringToSymbol(isolate, event_name), event);
    DCHECK(success.IsJust());
    DCHECK(success.FromJust());
  }

  return object;
}

void APIBinding::HandleCall(const std::string& name,
                            const APISignature* signature,
                            gin::Arguments* arguments) {
  std::string error;
  v8::Isolate* isolate = arguments->isolate();
  v8::HandleScope handle_scope(isolate);
  std::unique_ptr<base::ListValue> parsed_arguments;
  v8::Local<v8::Function> callback;
  {
    v8::TryCatch try_catch(isolate);
    parsed_arguments =
        ParseArguments(signature, arguments, *type_refs_, &callback, &error);
    if (try_catch.HasCaught()) {
      DCHECK(!parsed_arguments);
      try_catch.ReThrow();
      return;
    }
  }
  if (!parsed_arguments) {
    arguments->ThrowTypeError("Invalid invocation");
    return;
  }

  // Since this is called synchronously from the JS entry point,
  // GetCurrentContext() should always be correct.
  method_callback_.Run(name, std::move(parsed_arguments), isolate,
                       isolate->GetCurrentContext(), callback);
}

}  // namespace extensions
