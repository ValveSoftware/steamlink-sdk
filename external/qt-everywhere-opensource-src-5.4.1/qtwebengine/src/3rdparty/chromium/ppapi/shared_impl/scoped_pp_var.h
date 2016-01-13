// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_SCOPED_PP_VAR_H_
#define PPAPI_SHARED_IMPL_SCOPED_PP_VAR_H_

#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

class PPAPI_SHARED_EXPORT ScopedPPVar {
 public:
  struct PassRef {};

  ScopedPPVar();

  // Takes one reference to the given var.
  explicit ScopedPPVar(const PP_Var& v);

  // Assumes responsibility for one ref that the var already has.
  ScopedPPVar(const PassRef&, const PP_Var& v);

  // Implicit copy constructor allowed.
  ScopedPPVar(const ScopedPPVar& other);

  ~ScopedPPVar();

  ScopedPPVar& operator=(const PP_Var& r);
  ScopedPPVar& operator=(const ScopedPPVar& other) {
    return operator=(other.var_);
  }

  const PP_Var& get() const { return var_; }

  // Returns the PP_Var, passing the reference to the caller. This class
  // will no longer hold the var.
  PP_Var Release();

 private:
  PP_Var var_;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_SCOPED_PP_VAR_H_
