// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_MAILBOX_STRUCT_TRAITS_H_
#define GPU_IPC_COMMON_MAILBOX_STRUCT_TRAITS_H_

#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/ipc/common/mailbox.mojom.h"
#include "mojo/public/cpp/bindings/array_traits.h"

namespace mojo {

// A buffer used to read bytes directly from MailboxDataView to gpu::Mailbox
// name.
using MailboxName = CArray<int8_t>;

template <>
struct StructTraits<gpu::mojom::Mailbox, gpu::Mailbox> {
  static MailboxName name(const gpu::Mailbox& mailbox);
  static bool Read(gpu::mojom::MailboxDataView data, gpu::Mailbox* out);
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_MAILBOX_STRUCT_TRAITS_H_
