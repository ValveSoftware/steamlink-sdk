// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/ipc/transferable_resource_struct_traits.h"
#include "gpu/ipc/common/mailbox_holder_struct_traits.h"
#include "gpu/ipc/common/mailbox_struct_traits.h"
#include "gpu/ipc/common/sync_token_struct_traits.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"

namespace mojo {

// static
bool StructTraits<cc::mojom::TransferableResource, cc::TransferableResource>::
    Read(cc::mojom::TransferableResourceDataView data,
         cc::TransferableResource* out) {
  if (!data.ReadSize(&out->size) ||
      !data.ReadMailboxHolder(&out->mailbox_holder))
    return false;
  out->id = data.id();
  out->format = static_cast<cc::ResourceFormat>(data.format());
  out->filter = data.filter();
  out->read_lock_fences_enabled = data.read_lock_fences_enabled();
  out->is_software = data.is_software();
  out->is_overlay_candidate = data.is_overlay_candidate();
  return true;
}

}  // namespace mojo
