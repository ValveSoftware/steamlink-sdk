// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_PARAM_TRAITS_SIZE_MACROS_H_
#define IPC_PARAM_TRAITS_SIZE_MACROS_H_

// Null out all the macros that need nulling.
#include "ipc/ipc_message_null_macros.h"

// STRUCT declarations cause corresponding STRUCT_TRAITS declarations to occur.
#undef IPC_STRUCT_BEGIN_WITH_PARENT
#undef IPC_STRUCT_MEMBER
#undef IPC_STRUCT_END
#define IPC_STRUCT_BEGIN_WITH_PARENT(struct_name, parent) \
  IPC_STRUCT_TRAITS_BEGIN(struct_name)
#define IPC_STRUCT_MEMBER(type, name, ...) IPC_STRUCT_TRAITS_MEMBER(name)
#define IPC_STRUCT_END() IPC_STRUCT_TRAITS_END()

// Set up so next include will generate size methods.
#undef IPC_STRUCT_TRAITS_BEGIN
#undef IPC_STRUCT_TRAITS_MEMBER
#undef IPC_STRUCT_TRAITS_PARENT
#undef IPC_STRUCT_TRAITS_END
#define IPC_STRUCT_TRAITS_BEGIN(struct_name) \
  void ParamTraits<struct_name>::GetSize(base::PickleSizer* sizer, \
                                         const param_type& p) {
#define IPC_STRUCT_TRAITS_MEMBER(name) GetParamSize(sizer, p.name);
#define IPC_STRUCT_TRAITS_PARENT(type) ParamTraits<type>::GetSize(sizer, p);
#define IPC_STRUCT_TRAITS_END() }

#undef IPC_ENUM_TRAITS_VALIDATE
#define IPC_ENUM_TRAITS_VALIDATE(enum_name, validation_expression) \
  void ParamTraits<enum_name>::GetSize(base::PickleSizer* sizer,   \
                                       const param_type& value) {  \
    sizer->AddInt();                                               \
  }

#endif  // IPC_PARAM_TRAITS_SIZE_MACROS_H_

