// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_SYNC_TOKEN_STRUCT_TRAITS_H_
#define GPU_IPC_COMMON_SYNC_TOKEN_STRUCT_TRAITS_H_

#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/ipc/common/sync_token.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<gpu::mojom::SyncTokenDataView, gpu::SyncToken> {
  static bool verified_flush(const gpu::SyncToken& token) {
    return token.verified_flush();
  }

  static gpu::mojom::CommandBufferNamespace namespace_id(
      const gpu::SyncToken& token) {
    return static_cast<gpu::mojom::CommandBufferNamespace>(
        token.namespace_id());
  }

  static int32_t extra_data_field(const gpu::SyncToken& token) {
    return token.extra_data_field();
  }

  static uint64_t command_buffer_id(const gpu::SyncToken& token) {
    return token.command_buffer_id().GetUnsafeValue();
  }

  static uint64_t release_count(const gpu::SyncToken& token) {
    return token.release_count();
  }

  static bool Read(gpu::mojom::SyncTokenDataView data, gpu::SyncToken* out) {
    *out = gpu::SyncToken(
        static_cast<gpu::CommandBufferNamespace>(data.namespace_id()),
        data.extra_data_field(),
        gpu::CommandBufferId::FromUnsafeValue(data.command_buffer_id()),
        data.release_count());
    if (data.verified_flush())
      out->SetVerifyFlush();
    return true;
  }
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_SYNC_TOKEN_STRUCT_TRAITS_H_
