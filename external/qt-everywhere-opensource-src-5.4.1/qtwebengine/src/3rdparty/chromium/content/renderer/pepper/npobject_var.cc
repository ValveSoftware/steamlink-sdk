// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/npobject_var.h"

#include "base/logging.h"
#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/pepper/host_var_tracker.h"
#include "ppapi/c/pp_var.h"
#include "third_party/WebKit/public/web/WebBindings.h"

using blink::WebBindings;

namespace ppapi {

// NPObjectVar -----------------------------------------------------------------

NPObjectVar::NPObjectVar(PP_Instance instance, NPObject* np_object)
    : pp_instance_(instance), np_object_(np_object) {
  WebBindings::retainObject(np_object_);
  content::HostGlobals::Get()->host_var_tracker()->AddNPObjectVar(this);
}

NPObjectVar::~NPObjectVar() {
  if (pp_instance())
    content::HostGlobals::Get()->host_var_tracker()->RemoveNPObjectVar(this);
  WebBindings::releaseObject(np_object_);
}

NPObjectVar* NPObjectVar::AsNPObjectVar() { return this; }

PP_VarType NPObjectVar::GetType() const { return PP_VARTYPE_OBJECT; }

void NPObjectVar::InstanceDeleted() {
  DCHECK(pp_instance_);
  content::HostGlobals::Get()->host_var_tracker()->RemoveNPObjectVar(this);
  pp_instance_ = 0;
}

// static
scoped_refptr<NPObjectVar> NPObjectVar::FromPPVar(PP_Var var) {
  if (var.type != PP_VARTYPE_OBJECT)
    return scoped_refptr<NPObjectVar>(NULL);
  scoped_refptr<Var> var_object(
      PpapiGlobals::Get()->GetVarTracker()->GetVar(var));
  if (!var_object.get())
    return scoped_refptr<NPObjectVar>();
  return scoped_refptr<NPObjectVar>(var_object->AsNPObjectVar());
}

}  // namespace ppapi
