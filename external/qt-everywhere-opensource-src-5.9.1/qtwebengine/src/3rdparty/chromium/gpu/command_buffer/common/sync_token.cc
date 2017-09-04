// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/sync_token.h"

namespace gpu {

SyncToken::SyncToken()
    : verified_flush_(false),
      namespace_id_(CommandBufferNamespace::INVALID),
      extra_data_field_(0),
      release_count_(0) {}

SyncToken::SyncToken(CommandBufferNamespace namespace_id,
                     int32_t extra_data_field,
                     CommandBufferId command_buffer_id,
                     uint64_t release_count)
    : verified_flush_(false),
      namespace_id_(namespace_id),
      extra_data_field_(extra_data_field),
      command_buffer_id_(command_buffer_id),
      release_count_(release_count) {}

SyncToken::SyncToken(const SyncToken& other) = default;

}  // namespace gpu
