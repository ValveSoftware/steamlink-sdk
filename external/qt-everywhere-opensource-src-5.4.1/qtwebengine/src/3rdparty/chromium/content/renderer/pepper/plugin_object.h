// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PLUGIN_OBJECT_H_
#define CONTENT_RENDERER_PEPPER_PLUGIN_OBJECT_H_

#include <string>

#include "base/basictypes.h"

struct PP_Var;
struct PPP_Class_Deprecated;
typedef struct NPObject NPObject;
typedef struct _NPVariant NPVariant;

namespace content {

class PepperPluginInstanceImpl;

// A PluginObject is a JS-accessible object implemented by the plugin.
//
// In contrast, a var of type PP_VARTYPE_OBJECT is a reference to a JS object,
// which might be implemented by the plugin (here) or by the JS engine.
class PluginObject {
 public:
  virtual ~PluginObject();

  // Allocates a new PluginObject and returns it as a PP_Var with a
  // refcount of 1.
  static PP_Var Create(PepperPluginInstanceImpl* instance,
                       const PPP_Class_Deprecated* ppp_class,
                       void* ppp_class_data);

  PepperPluginInstanceImpl* instance() const { return instance_; }

  const PPP_Class_Deprecated* ppp_class() { return ppp_class_; }
  void* ppp_class_data() {
    return ppp_class_data_;
  };

  NPObject* GetNPObject() const;

  // Returns true if the given var is an object implemented by the same plugin
  // that owns the var object, and that the class matches. If it matches,
  // returns true and places the class data into |*ppp_class_data| (which can
  // optionally be NULL if no class data is desired).
  static bool IsInstanceOf(NPObject* np_object,
                           const PPP_Class_Deprecated* ppp_class,
                           void** ppp_class_data);

  // Converts the given NPObject to the corresponding ObjectVar.
  //
  // The given NPObject must be one corresponding to a PluginObject or this
  // will crash. If the object is a PluginObject but the plugin has gone
  // away (the object could still be alive because of a reference from JS),
  // then the return value will be NULL.
  static PluginObject* FromNPObject(NPObject* object);

  // Allocates a plugin wrapper object and returns it as an NPObject. This is
  // used internally only.
  static NPObject* AllocateObjectWrapper();

 private:
  struct NPObjectWrapper;

  // This object must be created using the CreateObject function of the which
  // will set up the correct NPObject.
  //
  // The NPObjectWrapper (an NPObject) should already have the reference
  // incremented on it, and this class will take ownership of that reference.
  PluginObject(PepperPluginInstanceImpl* instance,
               NPObjectWrapper* object_wrapper,
               const PPP_Class_Deprecated* ppp_class,
               void* ppp_class_data);

  PepperPluginInstanceImpl* instance_;

  // Holds a pointer to the NPObject wrapper backing the var. This class
  // derives from NPObject and we hold a reference to it, so it must be
  // refcounted. When the type is not an object, this value will be NULL.
  //
  // We don't actually own this pointer, it's the NPObject that actually
  // owns us.
  NPObjectWrapper* object_wrapper_;

  const PPP_Class_Deprecated* ppp_class_;
  void* ppp_class_data_;

  DISALLOW_COPY_AND_ASSIGN(PluginObject);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PLUGIN_OBJECT_H_
