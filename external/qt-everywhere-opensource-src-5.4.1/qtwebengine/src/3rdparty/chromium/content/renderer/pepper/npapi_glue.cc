// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/npapi_glue.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "content/renderer/pepper/host_array_buffer_var.h"
#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/pepper/host_var_tracker.h"
#include "content/renderer/pepper/npobject_var.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/plugin_module.h"
#include "content/renderer/pepper/plugin_object.h"
#include "ppapi/c/pp_var.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"
#include "third_party/WebKit/public/web/WebBindings.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "v8/include/v8.h"

using ppapi::NPObjectVar;
using ppapi::PpapiGlobals;
using ppapi::StringVar;
using ppapi::Var;
using blink::WebArrayBuffer;
using blink::WebBindings;
using blink::WebLocalFrame;
using blink::WebPluginContainer;

namespace content {

namespace {

const char kInvalidPluginValue[] = "Error: Plugin returned invalid value.";

PP_Var NPObjectToPPVarImpl(PepperPluginInstanceImpl* instance,
                           NPObject* object,
                           v8::Local<v8::Context> context) {
  DCHECK(object);
  if (context.IsEmpty())
    return PP_MakeUndefined();
  v8::Context::Scope context_scope(context);

  WebArrayBuffer buffer;
  // TODO(dmichael): Should I protect against duplicate Vars representing the
  // same array buffer? It's probably not worth the trouble, since it will only
  // affect in-process plugins.
  if (WebBindings::getArrayBuffer(object, &buffer)) {
    scoped_refptr<HostArrayBufferVar> buffer_var(
        new HostArrayBufferVar(buffer));
    return buffer_var->GetPPVar();
  }
  scoped_refptr<NPObjectVar> object_var(
      HostGlobals::Get()->host_var_tracker()->NPObjectVarForNPObject(
          instance->pp_instance(), object));
  if (!object_var.get()) {  // No object for this module yet, make a new one.
    object_var = new NPObjectVar(instance->pp_instance(), object);
  }
  return object_var->GetPPVar();
}

}  // namespace

// Utilities -------------------------------------------------------------------

bool PPVarToNPVariant(PP_Var var, NPVariant* result) {
  switch (var.type) {
    case PP_VARTYPE_UNDEFINED:
      VOID_TO_NPVARIANT(*result);
      break;
    case PP_VARTYPE_NULL:
      NULL_TO_NPVARIANT(*result);
      break;
    case PP_VARTYPE_BOOL:
      BOOLEAN_TO_NPVARIANT(var.value.as_bool, *result);
      break;
    case PP_VARTYPE_INT32:
      INT32_TO_NPVARIANT(var.value.as_int, *result);
      break;
    case PP_VARTYPE_DOUBLE:
      DOUBLE_TO_NPVARIANT(var.value.as_double, *result);
      break;
    case PP_VARTYPE_STRING: {
      StringVar* string = StringVar::FromPPVar(var);
      if (!string) {
        VOID_TO_NPVARIANT(*result);
        return false;
      }
      const std::string& value = string->value();
      char* c_string = static_cast<char*>(malloc(value.size()));
      memcpy(c_string, value.data(), value.size());
      STRINGN_TO_NPVARIANT(c_string, value.size(), *result);
      break;
    }
    case PP_VARTYPE_OBJECT: {
      scoped_refptr<NPObjectVar> object(NPObjectVar::FromPPVar(var));
      if (!object.get()) {
        VOID_TO_NPVARIANT(*result);
        return false;
      }
      OBJECT_TO_NPVARIANT(WebBindings::retainObject(object->np_object()),
                          *result);
      break;
    }
    // The following types are not supported for use with PPB_Var_Deprecated,
    // because PPB_Var_Deprecated is only for trusted plugins, and the trusted
    // plugins we have don't need these types. We can add support in the future
    // if it becomes necessary.
    case PP_VARTYPE_ARRAY:
    case PP_VARTYPE_DICTIONARY:
    case PP_VARTYPE_ARRAY_BUFFER:
    case PP_VARTYPE_RESOURCE:
      VOID_TO_NPVARIANT(*result);
      break;
  }
  return true;
}

PP_Var NPVariantToPPVar(PepperPluginInstanceImpl* instance,
                        const NPVariant* variant) {
  switch (variant->type) {
    case NPVariantType_Void:
      return PP_MakeUndefined();
    case NPVariantType_Null:
      return PP_MakeNull();
    case NPVariantType_Bool:
      return PP_MakeBool(PP_FromBool(NPVARIANT_TO_BOOLEAN(*variant)));
    case NPVariantType_Int32:
      return PP_MakeInt32(NPVARIANT_TO_INT32(*variant));
    case NPVariantType_Double:
      return PP_MakeDouble(NPVARIANT_TO_DOUBLE(*variant));
    case NPVariantType_String:
      return StringVar::StringToPPVar(
          NPVARIANT_TO_STRING(*variant).UTF8Characters,
          NPVARIANT_TO_STRING(*variant).UTF8Length);
    case NPVariantType_Object:
      return NPObjectToPPVar(instance, NPVARIANT_TO_OBJECT(*variant));
  }
  NOTREACHED();
  return PP_MakeUndefined();
}

NPIdentifier PPVarToNPIdentifier(PP_Var var) {
  switch (var.type) {
    case PP_VARTYPE_STRING: {
      StringVar* string = StringVar::FromPPVar(var);
      if (!string)
        return NULL;
      return WebBindings::getStringIdentifier(string->value().c_str());
    }
    case PP_VARTYPE_INT32:
      return WebBindings::getIntIdentifier(var.value.as_int);
    default:
      return NULL;
  }
}

PP_Var NPIdentifierToPPVar(NPIdentifier id) {
  const NPUTF8* string_value = NULL;
  int32_t int_value = 0;
  bool is_string = false;
  WebBindings::extractIdentifierData(id, string_value, int_value, is_string);
  if (is_string)
    return StringVar::StringToPPVar(string_value);

  return PP_MakeInt32(int_value);
}

PP_Var NPObjectToPPVar(PepperPluginInstanceImpl* instance, NPObject* object) {
  WebPluginContainer* container = instance->container();
  // It's possible that container() is NULL if the plugin has been removed from
  // the DOM (but the PluginInstance is not destroyed yet).
  if (!container)
    return PP_MakeUndefined();
  WebLocalFrame* frame = container->element().document().frame();
  if (!frame)
    return PP_MakeUndefined();

  v8::HandleScope scope(instance->GetIsolate());
  v8::Local<v8::Context> context = frame->mainWorldScriptContext();
  return NPObjectToPPVarImpl(instance, object, context);
}

PP_Var NPObjectToPPVarForTest(PepperPluginInstanceImpl* instance,
                              NPObject* object) {
  v8::Isolate* test_isolate = v8::Isolate::New();
  PP_Var result = PP_MakeUndefined();
  {
    v8::HandleScope scope(test_isolate);
    v8::Isolate::Scope isolate_scope(test_isolate);
    v8::Local<v8::Context> context = v8::Context::New(test_isolate);
    result = NPObjectToPPVarImpl(instance, object, context);
  }
  test_isolate->Dispose();
  return result;
}

// PPResultAndExceptionToNPResult ----------------------------------------------

PPResultAndExceptionToNPResult::PPResultAndExceptionToNPResult(
    NPObject* object_var,
    NPVariant* np_result)
    : object_var_(object_var),
      np_result_(np_result),
      exception_(PP_MakeUndefined()),
      success_(false),
      checked_exception_(false) {}

PPResultAndExceptionToNPResult::~PPResultAndExceptionToNPResult() {
  // The user should have called SetResult or CheckExceptionForNoResult
  // before letting this class go out of scope, or the exception will have
  // been lost.
  DCHECK(checked_exception_);

  PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(exception_);
}

// Call this with the return value of the PPAPI function. It will convert
// the result to the NPVariant output parameter and pass any exception on to
// the JS engine. It will update the success flag and return it.
bool PPResultAndExceptionToNPResult::SetResult(PP_Var result) {
  DCHECK(!checked_exception_);  // Don't call more than once.
  DCHECK(np_result_);           // Should be expecting a result.

  checked_exception_ = true;

  if (has_exception()) {
    ThrowException();
    success_ = false;
  } else if (!PPVarToNPVariant(result, np_result_)) {
    WebBindings::setException(object_var_, kInvalidPluginValue);
    success_ = false;
  } else {
    success_ = true;
  }

  // No matter what happened, we need to release the reference to the
  // value passed in. On success, a reference to this value will be in
  // the np_result_.
  PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(result);
  return success_;
}

// Call this after calling a PPAPI function that could have set the
// exception. It will pass the exception on to the JS engine and update
// the success flag.
//
// The success flag will be returned.
bool PPResultAndExceptionToNPResult::CheckExceptionForNoResult() {
  DCHECK(!checked_exception_);  // Don't call more than once.
  DCHECK(!np_result_);          // Can't have a result when doing this.

  checked_exception_ = true;

  if (has_exception()) {
    ThrowException();
    success_ = false;
    return false;
  }
  success_ = true;
  return true;
}

// Call this to ignore any exception. This prevents the DCHECK from failing
// in the destructor.
void PPResultAndExceptionToNPResult::IgnoreException() {
  checked_exception_ = true;
}

// Throws the current exception to JS. The exception must be set.
void PPResultAndExceptionToNPResult::ThrowException() {
  StringVar* string = StringVar::FromPPVar(exception_);
  if (string)
    WebBindings::setException(object_var_, string->value().c_str());
}

// PPVarArrayFromNPVariantArray ------------------------------------------------

PPVarArrayFromNPVariantArray::PPVarArrayFromNPVariantArray(
    PepperPluginInstanceImpl* instance,
    size_t size,
    const NPVariant* variants)
    : size_(size) {
  if (size_ > 0) {
    array_.reset(new PP_Var[size_]);
    for (size_t i = 0; i < size_; i++)
      array_[i] = NPVariantToPPVar(instance, &variants[i]);
  }
}

PPVarArrayFromNPVariantArray::~PPVarArrayFromNPVariantArray() {
  ppapi::VarTracker* var_tracker = PpapiGlobals::Get()->GetVarTracker();
  for (size_t i = 0; i < size_; i++)
    var_tracker->ReleaseVar(array_[i]);
}

// PPVarFromNPObject -----------------------------------------------------------

PPVarFromNPObject::PPVarFromNPObject(PepperPluginInstanceImpl* instance,
                                     NPObject* object)
    : var_(NPObjectToPPVar(instance, object)) {}

PPVarFromNPObject::~PPVarFromNPObject() {
  PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(var_);
}

// NPObjectAccessorWithIdentifier ----------------------------------------------

NPObjectAccessorWithIdentifier::NPObjectAccessorWithIdentifier(
    NPObject* object,
    NPIdentifier identifier,
    bool allow_integer_identifier)
    : object_(PluginObject::FromNPObject(object)),
      identifier_(PP_MakeUndefined()) {
  if (object_) {
    identifier_ = NPIdentifierToPPVar(identifier);
    if (identifier_.type == PP_VARTYPE_INT32 && !allow_integer_identifier)
      identifier_.type = PP_VARTYPE_UNDEFINED;  // Mark it invalid.
  }
}

NPObjectAccessorWithIdentifier::~NPObjectAccessorWithIdentifier() {
  PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(identifier_);
}

// TryCatch --------------------------------------------------------------------

TryCatch::TryCatch(PP_Var* exception)
    : has_exception_(exception && exception->type != PP_VARTYPE_UNDEFINED),
      exception_(exception) {
  WebBindings::pushExceptionHandler(&TryCatch::Catch, this);
}

TryCatch::~TryCatch() { WebBindings::popExceptionHandler(); }

void TryCatch::SetException(const char* message) {
  if (!has_exception()) {
    has_exception_ = true;
    if (exception_) {
      if (!message)
        message = "Unknown exception.";
      *exception_ = ppapi::StringVar::StringToPPVar(message, strlen(message));
    }
  }
}

// static
void TryCatch::Catch(void* self, const char* message) {
  static_cast<TryCatch*>(self)->SetException(message);
}

}  // namespace content
