// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/scoped_pp_var.h"

#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/var_tracker.h"

namespace ppapi {

namespace {

void CallAddRef(const PP_Var& v) {
  PpapiGlobals::Get()->GetVarTracker()->AddRefVar(v);
}

void CallRelease(const PP_Var& v) {
  PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(v);
}

}  // namespace

ScopedPPVar::ScopedPPVar() : var_(PP_MakeUndefined()) {}

ScopedPPVar::ScopedPPVar(const PP_Var& v) : var_(v) { CallAddRef(var_); }

ScopedPPVar::ScopedPPVar(const PassRef&, const PP_Var& v) : var_(v) {}

ScopedPPVar::ScopedPPVar(const ScopedPPVar& other) : var_(other.var_) {
  CallAddRef(var_);
}

ScopedPPVar::~ScopedPPVar() { CallRelease(var_); }

ScopedPPVar& ScopedPPVar::operator=(const PP_Var& v) {
  CallAddRef(v);
  CallRelease(var_);
  var_ = v;
  return *this;
}

PP_Var ScopedPPVar::Release() {
  PP_Var result = var_;
  var_ = PP_MakeUndefined();
  return result;
}

}  // namespace ppapi
