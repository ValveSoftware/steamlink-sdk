// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Get basic type definitions.
#define IPC_MESSAGE_IMPL
#include "components/autofill/content/common/autofill_param_traits_macros.h"

// Generate constructors.
#include "ipc/struct_constructor_macros.h"
#include "components/autofill/content/common/autofill_param_traits_macros.h"

// Generate destructors.
#include "ipc/struct_destructor_macros.h"
#include "components/autofill/content/common/autofill_param_traits_macros.h"

// Generate param traits size methods.
#include "ipc/param_traits_size_macros.h"
namespace IPC {
#undef COMPONENTS_AUTOFILL_CONTENT_COMMON_AUTOFILL_PARAM_TRAITS_MACROS_H_
#include "components/autofill/content/common/autofill_param_traits_macros.h"
}  // namespace IPC

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#undef COMPONENTS_AUTOFILL_CONTENT_COMMON_AUTOFILL_PARAM_TRAITS_MACROS_H_
#include "components/autofill/content/common/autofill_param_traits_macros.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef COMPONENTS_AUTOFILL_CONTENT_COMMON_AUTOFILL_PARAM_TRAITS_MACROS_H_
#include "components/autofill/content/common/autofill_param_traits_macros.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#undef COMPONENTS_AUTOFILL_CONTENT_COMMON_AUTOFILL_PARAM_TRAITS_MACROS_H_
#include "components/autofill/content/common/autofill_param_traits_macros.h"
}  // namespace IPC
