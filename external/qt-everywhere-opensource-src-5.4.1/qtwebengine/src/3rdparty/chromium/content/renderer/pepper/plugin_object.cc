// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/plugin_object.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "content/renderer/pepper/npapi_glue.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/plugin_module.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/dev/ppp_class_deprecated.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "third_party/WebKit/public/web/WebBindings.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"

using ppapi::PpapiGlobals;
using ppapi::StringVar;
using ppapi::Var;
using blink::WebBindings;

namespace content {

namespace {

const char kInvalidValueException[] = "Error: Invalid value";

// NPObject implementation in terms of PPP_Class_Deprecated --------------------

NPObject* WrapperClass_Allocate(NPP npp, NPClass* unused) {
  return PluginObject::AllocateObjectWrapper();
}

void WrapperClass_Deallocate(NPObject* np_object) {
  PluginObject* plugin_object = PluginObject::FromNPObject(np_object);
  if (plugin_object) {
    plugin_object->ppp_class()->Deallocate(plugin_object->ppp_class_data());
    delete plugin_object;
  }
  delete np_object;
}

void WrapperClass_Invalidate(NPObject* object) {}

bool WrapperClass_HasMethod(NPObject* object, NPIdentifier method_name) {
  NPObjectAccessorWithIdentifier accessor(object, method_name, false);
  if (!accessor.is_valid())
    return false;

  PPResultAndExceptionToNPResult result_converter(
      accessor.object()->GetNPObject(), NULL);
  bool rv = accessor.object()->ppp_class()->HasMethod(
      accessor.object()->ppp_class_data(),
      accessor.identifier(),
      result_converter.exception());
  result_converter.CheckExceptionForNoResult();
  return rv;
}

bool WrapperClass_Invoke(NPObject* object,
                         NPIdentifier method_name,
                         const NPVariant* argv,
                         uint32_t argc,
                         NPVariant* result) {
  NPObjectAccessorWithIdentifier accessor(object, method_name, false);
  if (!accessor.is_valid())
    return false;

  PPResultAndExceptionToNPResult result_converter(
      accessor.object()->GetNPObject(), result);
  PPVarArrayFromNPVariantArray args(accessor.object()->instance(), argc, argv);

  // For the OOP plugin case we need to grab a reference on the plugin module
  // object to ensure that it is not destroyed courtsey an incoming
  // ExecuteScript call which destroys the plugin module and in turn the
  // dispatcher.
  scoped_refptr<PluginModule> ref(accessor.object()->instance()->module());

  return result_converter.SetResult(
      accessor.object()->ppp_class()->Call(accessor.object()->ppp_class_data(),
                                           accessor.identifier(),
                                           argc,
                                           args.array(),
                                           result_converter.exception()));
}

bool WrapperClass_InvokeDefault(NPObject* np_object,
                                const NPVariant* argv,
                                uint32_t argc,
                                NPVariant* result) {
  PluginObject* obj = PluginObject::FromNPObject(np_object);
  if (!obj)
    return false;

  PPVarArrayFromNPVariantArray args(obj->instance(), argc, argv);
  PPResultAndExceptionToNPResult result_converter(obj->GetNPObject(), result);

  // For the OOP plugin case we need to grab a reference on the plugin module
  // object to ensure that it is not destroyed courtsey an incoming
  // ExecuteScript call which destroys the plugin module and in turn the
  // dispatcher.
  scoped_refptr<PluginModule> ref(obj->instance()->module());

  result_converter.SetResult(
      obj->ppp_class()->Call(obj->ppp_class_data(),
                             PP_MakeUndefined(),
                             argc,
                             args.array(),
                             result_converter.exception()));
  return result_converter.success();
}

bool WrapperClass_HasProperty(NPObject* object, NPIdentifier property_name) {
  NPObjectAccessorWithIdentifier accessor(object, property_name, true);
  if (!accessor.is_valid())
    return false;

  PPResultAndExceptionToNPResult result_converter(
      accessor.object()->GetNPObject(), NULL);
  bool rv = accessor.object()->ppp_class()->HasProperty(
      accessor.object()->ppp_class_data(),
      accessor.identifier(),
      result_converter.exception());
  result_converter.CheckExceptionForNoResult();
  return rv;
}

bool WrapperClass_GetProperty(NPObject* object,
                              NPIdentifier property_name,
                              NPVariant* result) {
  NPObjectAccessorWithIdentifier accessor(object, property_name, true);
  if (!accessor.is_valid())
    return false;

  PPResultAndExceptionToNPResult result_converter(
      accessor.object()->GetNPObject(), result);
  return result_converter.SetResult(accessor.object()->ppp_class()->GetProperty(
      accessor.object()->ppp_class_data(),
      accessor.identifier(),
      result_converter.exception()));
}

bool WrapperClass_SetProperty(NPObject* object,
                              NPIdentifier property_name,
                              const NPVariant* value) {
  NPObjectAccessorWithIdentifier accessor(object, property_name, true);
  if (!accessor.is_valid())
    return false;

  PPResultAndExceptionToNPResult result_converter(
      accessor.object()->GetNPObject(), NULL);
  PP_Var value_var = NPVariantToPPVar(accessor.object()->instance(), value);
  accessor.object()->ppp_class()->SetProperty(
      accessor.object()->ppp_class_data(),
      accessor.identifier(),
      value_var,
      result_converter.exception());
  PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(value_var);
  return result_converter.CheckExceptionForNoResult();
}

bool WrapperClass_RemoveProperty(NPObject* object, NPIdentifier property_name) {
  NPObjectAccessorWithIdentifier accessor(object, property_name, true);
  if (!accessor.is_valid())
    return false;

  PPResultAndExceptionToNPResult result_converter(
      accessor.object()->GetNPObject(), NULL);
  accessor.object()->ppp_class()->RemoveProperty(
      accessor.object()->ppp_class_data(),
      accessor.identifier(),
      result_converter.exception());
  return result_converter.CheckExceptionForNoResult();
}

bool WrapperClass_Enumerate(NPObject* object,
                            NPIdentifier** values,
                            uint32_t* count) {
  *values = NULL;
  *count = 0;
  PluginObject* obj = PluginObject::FromNPObject(object);
  if (!obj)
    return false;

  uint32_t property_count = 0;
  PP_Var* properties = NULL;  // Must be freed!
  PPResultAndExceptionToNPResult result_converter(obj->GetNPObject(), NULL);
  obj->ppp_class()->GetAllPropertyNames(obj->ppp_class_data(),
                                        &property_count,
                                        &properties,
                                        result_converter.exception());

  // Convert the array of PP_Var to an array of NPIdentifiers. If any
  // conversions fail, we will set the exception.
  if (!result_converter.has_exception()) {
    if (property_count > 0) {
      *values = static_cast<NPIdentifier*>(
          calloc(property_count, sizeof(NPIdentifier)));
      *count = 0;  // Will be the number of items successfully converted.
      for (uint32_t i = 0; i < property_count; ++i) {
        if (!((*values)[i] = PPVarToNPIdentifier(properties[i]))) {
          // Throw an exception for the failed convertion.
          *result_converter.exception() =
              StringVar::StringToPPVar(kInvalidValueException);
          break;
        }
        (*count)++;
      }

      if (result_converter.has_exception()) {
        // We don't actually have to free the identifiers we converted since
        // all identifiers leak anyway :( .
        free(*values);
        *values = NULL;
        *count = 0;
      }
    }
  }

  // This will actually throw the exception, either from GetAllPropertyNames,
  // or if anything was set during the conversion process.
  result_converter.CheckExceptionForNoResult();

  // Release the PP_Var that the plugin allocated. On success, they will all
  // be converted to NPVariants, and on failure, we want them to just go away.
  ppapi::VarTracker* var_tracker = PpapiGlobals::Get()->GetVarTracker();
  for (uint32_t i = 0; i < property_count; ++i)
    var_tracker->ReleaseVar(properties[i]);
  free(properties);
  return result_converter.success();
}

bool WrapperClass_Construct(NPObject* object,
                            const NPVariant* argv,
                            uint32_t argc,
                            NPVariant* result) {
  PluginObject* obj = PluginObject::FromNPObject(object);
  if (!obj)
    return false;

  PPVarArrayFromNPVariantArray args(obj->instance(), argc, argv);
  PPResultAndExceptionToNPResult result_converter(obj->GetNPObject(), result);
  return result_converter.SetResult(obj->ppp_class()->Construct(
      obj->ppp_class_data(), argc, args.array(), result_converter.exception()));
}

const NPClass wrapper_class = {
    NP_CLASS_STRUCT_VERSION,     WrapperClass_Allocate,
    WrapperClass_Deallocate,     WrapperClass_Invalidate,
    WrapperClass_HasMethod,      WrapperClass_Invoke,
    WrapperClass_InvokeDefault,  WrapperClass_HasProperty,
    WrapperClass_GetProperty,    WrapperClass_SetProperty,
    WrapperClass_RemoveProperty, WrapperClass_Enumerate,
    WrapperClass_Construct};

}  // namespace

// PluginObject ----------------------------------------------------------------

struct PluginObject::NPObjectWrapper : public NPObject {
  // Points to the var object that owns this wrapper. This value may be NULL
  // if there is no var owning this wrapper. This can happen if the plugin
  // releases all references to the var, but a reference to the underlying
  // NPObject is still held by script on the page.
  PluginObject* obj;
};

PluginObject::PluginObject(PepperPluginInstanceImpl* instance,
                           NPObjectWrapper* object_wrapper,
                           const PPP_Class_Deprecated* ppp_class,
                           void* ppp_class_data)
    : instance_(instance),
      object_wrapper_(object_wrapper),
      ppp_class_(ppp_class),
      ppp_class_data_(ppp_class_data) {
  // Make the object wrapper refer back to this class so our NPObject
  // implementation can call back into the Pepper layer.
  object_wrapper_->obj = this;
  instance_->AddPluginObject(this);
}

PluginObject::~PluginObject() {
  // The wrapper we made for this NPObject may still have a reference to it
  // from JavaScript, so we clear out its ObjectVar back pointer which will
  // cause all calls "up" to the plugin to become NOPs. Our ObjectVar base
  // class will release our reference to the object, which may or may not
  // delete the NPObject.
  DCHECK(object_wrapper_->obj == this);
  object_wrapper_->obj = NULL;
  instance_->RemovePluginObject(this);
}

PP_Var PluginObject::Create(PepperPluginInstanceImpl* instance,
                            const PPP_Class_Deprecated* ppp_class,
                            void* ppp_class_data) {
  // This will internally end up calling our AllocateObjectWrapper via the
  // WrapperClass_Allocated function which will have created an object wrapper
  // appropriate for this class (derived from NPObject).
  NPObjectWrapper* wrapper =
      static_cast<NPObjectWrapper*>(WebBindings::createObject(
          instance->instanceNPP(), const_cast<NPClass*>(&wrapper_class)));

  // This object will register itself both with the NPObject and with the
  // PluginModule. The NPObject will normally handle its lifetime, and it
  // will get deleted in the destroy method. It may also get deleted when the
  // plugin module is deallocated.
  new PluginObject(instance, wrapper, ppp_class, ppp_class_data);

  // We can just use a normal ObjectVar to refer to this object from the
  // plugin. It will hold a ref to the underlying NPObject which will in turn
  // hold our pluginObject.
  PP_Var obj_var(NPObjectToPPVar(instance, wrapper));

  // Note that the ObjectVar constructor incremented the reference count, and so
  // did WebBindings::createObject above. Now that the PP_Var has taken
  // ownership, we need to release to balance out the createObject reference
  // count bump.
  WebBindings::releaseObject(wrapper);
  return obj_var;
}

NPObject* PluginObject::GetNPObject() const { return object_wrapper_; }

// static
bool PluginObject::IsInstanceOf(NPObject* np_object,
                                const PPP_Class_Deprecated* ppp_class,
                                void** ppp_class_data) {
  // Validate that this object is implemented by our wrapper class before
  // trying to get the PluginObject.
  if (np_object->_class != &wrapper_class)
    return false;

  PluginObject* plugin_object = FromNPObject(np_object);
  if (!plugin_object)
    return false;  // Object is no longer alive.

  if (plugin_object->ppp_class() != ppp_class)
    return false;
  if (ppp_class_data)
    *ppp_class_data = plugin_object->ppp_class_data();
  return true;
}

// static
PluginObject* PluginObject::FromNPObject(NPObject* object) {
  return static_cast<NPObjectWrapper*>(object)->obj;
}

// static
NPObject* PluginObject::AllocateObjectWrapper() {
  NPObjectWrapper* wrapper = new NPObjectWrapper;
  memset(wrapper, 0, sizeof(NPObjectWrapper));
  return wrapper;
}

}  // namespace content
