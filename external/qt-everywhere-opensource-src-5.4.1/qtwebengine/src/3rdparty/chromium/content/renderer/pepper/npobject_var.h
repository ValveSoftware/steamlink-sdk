// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_NPOBJECT_VAR_H_
#define CONTENT_RENDERER_PEPPER_NPOBJECT_VAR_H_

#include <string>

#include "base/compiler_specific.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/shared_impl/var.h"
#include "content/common/content_export.h"

typedef struct NPObject NPObject;
typedef struct _NPVariant NPVariant;
typedef void* NPIdentifier;

namespace ppapi {

// NPObjectVar -----------------------------------------------------------------

// Represents a JavaScript object Var. By itself, this represents random
// NPObjects that a given plugin (identified by the resource's module) wants to
// reference. If two different modules reference the same NPObject (like the
// "window" object), then there will be different NPObjectVar's (and hence
// PP_Var IDs) for each module. This allows us to track all references owned by
// a given module and free them when the plugin exits independently of other
// plugins that may be running at the same time.
class NPObjectVar : public Var {
 public:
  // You should always use FromNPObject to create an NPObjectVar. This function
  // guarantees that we maintain the 1:1 mapping between NPObject and
  // NPObjectVar.
  NPObjectVar(PP_Instance instance, NPObject* np_object);

  // Var overrides.
  virtual NPObjectVar* AsNPObjectVar() OVERRIDE;
  virtual PP_VarType GetType() const OVERRIDE;

  // Returns the underlying NPObject corresponding to this NPObjectVar.
  // Guaranteed non-NULL.
  NPObject* np_object() const { return np_object_; }

  // Notification that the instance was deleted, the internal reference will be
  // zeroed out.
  void InstanceDeleted();

  // Possibly 0 if the object has outlived its instance.
  PP_Instance pp_instance() const { return pp_instance_; }

  // Helper function that converts a PP_Var to an object. This will return NULL
  // if the PP_Var is not of object type or the object is invalid.
  CONTENT_EXPORT static scoped_refptr<NPObjectVar> FromPPVar(PP_Var var);

 private:
  virtual ~NPObjectVar();

  // Possibly 0 if the object has outlived its instance.
  PP_Instance pp_instance_;

  // Guaranteed non-NULL, this is the underlying object used by WebKit. We
  // hold a reference to this object.
  NPObject* np_object_;

  DISALLOW_COPY_AND_ASSIGN(NPObjectVar);
};

}  // ppapi

#endif  // CONTENT_RENDERER_PEPPER_NPOBJECT_VAR_H_
