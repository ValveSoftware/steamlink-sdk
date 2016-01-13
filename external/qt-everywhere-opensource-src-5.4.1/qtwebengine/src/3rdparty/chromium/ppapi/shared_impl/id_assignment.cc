// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/id_assignment.h"

#include "base/basictypes.h"

namespace ppapi {

const unsigned int kPPIdTypeBits = 2;

const int32 kMaxPPId = kint32max >> kPPIdTypeBits;

COMPILE_ASSERT(PP_ID_TYPE_COUNT <= (1 << kPPIdTypeBits),
               kPPIdTypeBits_is_too_small_for_all_id_types);

}  // namespace ppapi
