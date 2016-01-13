// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/gles2/command_buffer_type_conversions.h"

#include "mojo/services/gles2/command_buffer.mojom.h"

namespace mojo {

CommandBufferStatePtr
TypeConverter<CommandBufferStatePtr, gpu::CommandBuffer::State>::ConvertFrom(
    const gpu::CommandBuffer::State& input) {
  CommandBufferStatePtr result(CommandBufferState::New());
  result->num_entries = input.num_entries;
  result->get_offset = input.get_offset;
  result->put_offset = input.put_offset;
  result->token = input.token;
  result->error = input.error;
  result->context_lost_reason = input.context_lost_reason;
  result->generation = input.generation;
  return result.Pass();
}

gpu::CommandBuffer::State
TypeConverter<CommandBufferStatePtr, gpu::CommandBuffer::State>::ConvertTo(
    const CommandBufferStatePtr& input) {
  gpu::CommandBuffer::State state;
  state.num_entries = input->num_entries;
  state.get_offset = input->get_offset;
  state.put_offset = input->put_offset;
  state.token = input->token;
  state.error = static_cast<gpu::error::Error>(input->error);
  state.context_lost_reason =
      static_cast<gpu::error::ContextLostReason>(input->context_lost_reason);
  state.generation = input->generation;
  return state;
}

}  // namespace mojo
