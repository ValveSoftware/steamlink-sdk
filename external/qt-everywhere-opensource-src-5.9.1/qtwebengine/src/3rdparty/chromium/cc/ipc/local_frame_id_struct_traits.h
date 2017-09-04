// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_LOCAL_FRAME_ID_STRUCT_TRAITS_H_
#define CC_IPC_LOCAL_FRAME_ID_STRUCT_TRAITS_H_

#include "cc/ipc/local_frame_id.mojom-shared.h"
#include "cc/surfaces/local_frame_id.h"
#include "mojo/common/common_custom_types_struct_traits.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::LocalFrameIdDataView, cc::LocalFrameId> {
  static uint32_t local_id(const cc::LocalFrameId& local_frame_id) {
    return local_frame_id.local_id();
  }

  static const base::UnguessableToken& nonce(
      const cc::LocalFrameId& local_frame_id) {
    return local_frame_id.nonce();
  }

  static bool Read(cc::mojom::LocalFrameIdDataView data,
                   cc::LocalFrameId* out) {
    base::UnguessableToken nonce;
    if (!data.ReadNonce(&nonce))
      return false;

    *out = cc::LocalFrameId(data.local_id(), nonce);
    return true;
  }
};

}  // namespace mojo

#endif  // CC_IPC_LOCAL_FRAME_ID_STRUCT_TRAITS_H_
