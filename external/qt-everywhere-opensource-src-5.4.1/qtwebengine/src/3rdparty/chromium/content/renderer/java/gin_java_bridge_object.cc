// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/java/gin_java_bridge_object.h"

#include "base/strings/utf_string_conversions.h"
#include "content/common/android/gin_java_bridge_value.h"
#include "content/public/renderer/v8_value_converter.h"
#include "content/renderer/java/gin_java_bridge_value_converter.h"
#include "gin/function_template.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebKit.h"

namespace content {

namespace {

const char kMethodInvocationErrorMessage[] =
    "Java bridge method invocation error";

}  // namespace


// static
GinJavaBridgeObject* GinJavaBridgeObject::InjectNamed(
    blink::WebFrame* frame,
    const base::WeakPtr<GinJavaBridgeDispatcher>& dispatcher,
    const std::string& object_name,
    GinJavaBridgeDispatcher::ObjectID object_id) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Handle<v8::Context> context = frame->mainWorldScriptContext();
  if (context.IsEmpty())
    return NULL;

  GinJavaBridgeObject* object =
      new GinJavaBridgeObject(isolate, dispatcher, object_id);

  v8::Context::Scope context_scope(context);
  v8::Handle<v8::Object> global = context->Global();
  gin::Handle<GinJavaBridgeObject> controller =
      gin::CreateHandle(isolate, object);
  // WrappableBase instance deletes itself in case of a wrapper
  // creation failure, thus there is no need to delete |object|.
  if (controller.IsEmpty())
    return NULL;

  global->Set(gin::StringToV8(isolate, object_name), controller.ToV8());
  return object;
}

// static
GinJavaBridgeObject* GinJavaBridgeObject::InjectAnonymous(
    const base::WeakPtr<GinJavaBridgeDispatcher>& dispatcher,
    GinJavaBridgeDispatcher::ObjectID object_id) {
  return new GinJavaBridgeObject(
      blink::mainThreadIsolate(), dispatcher, object_id);
}

GinJavaBridgeObject::GinJavaBridgeObject(
    v8::Isolate* isolate,
    const base::WeakPtr<GinJavaBridgeDispatcher>& dispatcher,
    GinJavaBridgeDispatcher::ObjectID object_id)
    : gin::NamedPropertyInterceptor(isolate, this),
      dispatcher_(dispatcher),
      object_id_(object_id),
      converter_(new GinJavaBridgeValueConverter()) {
}

GinJavaBridgeObject::~GinJavaBridgeObject() {
  if (dispatcher_)
    dispatcher_->OnGinJavaBridgeObjectDeleted(object_id_);
}

gin::ObjectTemplateBuilder GinJavaBridgeObject::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<GinJavaBridgeObject>::GetObjectTemplateBuilder(isolate)
      .AddNamedPropertyInterceptor();
}

v8::Local<v8::Value> GinJavaBridgeObject::GetNamedProperty(
    v8::Isolate* isolate,
    const std::string& property) {
  if (dispatcher_ && dispatcher_->HasJavaMethod(object_id_, property)) {
    return gin::CreateFunctionTemplate(
               isolate,
               base::Bind(&GinJavaBridgeObject::InvokeMethod,
                          base::Unretained(this),
                          property))->GetFunction();
  } else {
    return v8::Local<v8::Value>();
  }
}

std::vector<std::string> GinJavaBridgeObject::EnumerateNamedProperties(
    v8::Isolate* isolate) {
  std::set<std::string> method_names;
  if (dispatcher_)
    dispatcher_->GetJavaMethods(object_id_, &method_names);
  return std::vector<std::string> (method_names.begin(), method_names.end());
}

v8::Handle<v8::Value> GinJavaBridgeObject::InvokeMethod(
    const std::string& name,
    gin::Arguments* args) {
  if (!dispatcher_) {
    args->isolate()->ThrowException(v8::Exception::Error(gin::StringToV8(
        args->isolate(), kMethodInvocationErrorMessage)));
    return v8::Undefined(args->isolate());
  }

  base::ListValue arguments;
  {
    v8::HandleScope handle_scope(args->isolate());
    v8::Handle<v8::Context> context = args->isolate()->GetCurrentContext();
    v8::Handle<v8::Value> val;
    while (args->GetNext(&val)) {
      scoped_ptr<base::Value> arg(converter_->FromV8Value(val, context));
      if (arg.get()) {
        arguments.Append(arg.release());
      } else {
        arguments.Append(base::Value::CreateNullValue());
      }
    }
  }

  scoped_ptr<base::Value> result =
      dispatcher_->InvokeJavaMethod(object_id_, name, arguments);
  if (!result.get()) {
    args->isolate()->ThrowException(v8::Exception::Error(gin::StringToV8(
        args->isolate(), kMethodInvocationErrorMessage)));
    return v8::Undefined(args->isolate());
  }
  if (!result->IsType(base::Value::TYPE_BINARY)) {
    return converter_->ToV8Value(result.get(),
                                 args->isolate()->GetCurrentContext());
  }

  scoped_ptr<const GinJavaBridgeValue> gin_value =
      GinJavaBridgeValue::FromValue(result.get());
  if (gin_value->IsType(GinJavaBridgeValue::TYPE_OBJECT_ID)) {
    GinJavaBridgeObject* result = NULL;
    GinJavaBridgeDispatcher::ObjectID object_id;
    if (gin_value->GetAsObjectID(&object_id)) {
      result = dispatcher_->GetObject(object_id);
    }
    if (result) {
      gin::Handle<GinJavaBridgeObject> controller =
          gin::CreateHandle(args->isolate(), result);
      if (controller.IsEmpty())
        return v8::Undefined(args->isolate());
      return controller.ToV8();
    }
  } else if (gin_value->IsType(GinJavaBridgeValue::TYPE_NONFINITE)) {
    float float_value;
    gin_value->GetAsNonFinite(&float_value);
    return v8::Number::New(args->isolate(), float_value);
  }
  return v8::Undefined(args->isolate());
}

gin::WrapperInfo GinJavaBridgeObject::kWrapperInfo = {gin::kEmbedderNativeGin};

}  // namespace content
