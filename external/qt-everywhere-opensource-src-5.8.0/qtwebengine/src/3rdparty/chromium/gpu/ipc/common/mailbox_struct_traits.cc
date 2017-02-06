// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/mailbox_struct_traits.h"

namespace mojo {

// static
MailboxName StructTraits<gpu::mojom::Mailbox, gpu::Mailbox>::name(
    const gpu::Mailbox& mailbox) {
  return {GL_MAILBOX_SIZE_CHROMIUM, GL_MAILBOX_SIZE_CHROMIUM,
          const_cast<int8_t*>(&mailbox.name[0])};
}

// static
bool StructTraits<gpu::mojom::Mailbox, gpu::Mailbox>::Read(
    gpu::mojom::MailboxDataView data,
    gpu::Mailbox* out) {
  MailboxName mailbox_name = {0, GL_MAILBOX_SIZE_CHROMIUM, &out->name[0]};
  return data.ReadName(&mailbox_name);
}

}  // namespace mojo
