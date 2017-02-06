// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder_passthrough.h"

namespace gpu {
namespace gles2 {

GLES2DecoderPassthroughImpl::GLES2DecoderPassthroughImpl(ContextGroup* group)
    : commands_to_process_(0),
      debug_marker_manager_(),
      logger_(&debug_marker_manager_),
      surface_(),
      context_(),
      group_(group),
      feature_info_(group->feature_info()) {
  DCHECK(group);
}

GLES2DecoderPassthroughImpl::~GLES2DecoderPassthroughImpl() {}

GLES2Decoder::Error GLES2DecoderPassthroughImpl::DoCommands(
    unsigned int num_commands,
    const void* buffer,
    int num_entries,
    int* entries_processed) {
  commands_to_process_ = num_commands;
  error::Error result = error::kNoError;
  const CommandBufferEntry* cmd_data =
      static_cast<const CommandBufferEntry*>(buffer);
  int process_pos = 0;
  unsigned int command = 0;

  while (process_pos < num_entries && result == error::kNoError &&
         commands_to_process_--) {
    const unsigned int size = cmd_data->value_header.size;
    command = cmd_data->value_header.command;

    if (size == 0) {
      result = error::kInvalidSize;
      break;
    }

    // size can't overflow because it is 21 bits.
    if (static_cast<int>(size) + process_pos > num_entries) {
      result = error::kOutOfBounds;
      break;
    }

    const unsigned int arg_count = size - 1;
    unsigned int command_index = command - kFirstGLES2Command;
    if (command_index < arraysize(command_info)) {
      const CommandInfo& info = command_info[command_index];
      unsigned int info_arg_count = static_cast<unsigned int>(info.arg_count);
      if ((info.arg_flags == cmd::kFixed && arg_count == info_arg_count) ||
          (info.arg_flags == cmd::kAtLeastN && arg_count >= info_arg_count)) {
        uint32_t immediate_data_size = (arg_count - info_arg_count) *
                                       sizeof(CommandBufferEntry);  // NOLINT
        if (info.cmd_handler) {
          result = (this->*info.cmd_handler)(immediate_data_size, cmd_data);
        } else {
          result = error::kUnknownCommand;
        }
      } else {
        result = error::kInvalidArguments;
      }
    } else {
      result = DoCommonCommand(command, arg_count, cmd_data);
    }

    if (result != error::kDeferCommandUntilLater) {
      process_pos += size;
      cmd_data += size;
    }
  }

  if (entries_processed)
    *entries_processed = process_pos;

  return result;
}

const char* GLES2DecoderPassthroughImpl::GetCommandName(
    unsigned int command_id) const {
  if (command_id >= kFirstGLES2Command && command_id < kNumCommands) {
    return gles2::GetCommandName(static_cast<CommandId>(command_id));
  }
  return GetCommonCommandName(static_cast<cmd::CommandId>(command_id));
}

bool GLES2DecoderPassthroughImpl::Initialize(
    const scoped_refptr<gl::GLSurface>& surface,
    const scoped_refptr<gl::GLContext>& context,
    bool offscreen,
    const DisallowedFeatures& disallowed_features,
    const ContextCreationAttribHelper& attrib_helper) {
  // Take ownership of the context and surface. The surface can be replaced
  // with SetSurface.
  context_ = context;
  surface_ = surface;

  if (!group_->Initialize(this, attrib_helper.context_type,
                          disallowed_features)) {
    group_ = NULL;  // Must not destroy ContextGroup if it is not initialized.
    Destroy(true);
    return false;
  }

  image_manager_.reset(new ImageManager());

  set_initialized();
  return true;
}

void GLES2DecoderPassthroughImpl::Destroy(bool have_context) {
  if (image_manager_.get()) {
    image_manager_->Destroy(have_context);
    image_manager_.reset();
  }
}

void GLES2DecoderPassthroughImpl::SetSurface(
    const scoped_refptr<gl::GLSurface>& surface) {
  DCHECK(context_->IsCurrent(nullptr));
  DCHECK(surface_.get());
  surface_ = surface;
}

void GLES2DecoderPassthroughImpl::ReleaseSurface() {
  if (!context_.get())
    return;
  if (WasContextLost()) {
    DLOG(ERROR) << "  GLES2DecoderImpl: Trying to release lost context.";
    return;
  }
  context_->ReleaseCurrent(surface_.get());
  surface_ = nullptr;
}

void GLES2DecoderPassthroughImpl::TakeFrontBuffer(const Mailbox& mailbox) {}

void GLES2DecoderPassthroughImpl::ReturnFrontBuffer(const Mailbox& mailbox,
                                                    bool is_lost) {}

bool GLES2DecoderPassthroughImpl::ResizeOffscreenFrameBuffer(
    const gfx::Size& size) {
  return true;
}

bool GLES2DecoderPassthroughImpl::MakeCurrent() {
  if (!context_.get())
    return false;

  if (!context_->MakeCurrent(surface_.get())) {
    LOG(ERROR) << "  GLES2DecoderImpl: Context lost during MakeCurrent.";
    MarkContextLost(error::kMakeCurrentFailed);
    group_->LoseContexts(error::kUnknown);
    return false;
  }

  return true;
}

gpu::gles2::GLES2Util* GLES2DecoderPassthroughImpl::GetGLES2Util() {
  return nullptr;
}

gl::GLContext* GLES2DecoderPassthroughImpl::GetGLContext() {
  return nullptr;
}

gpu::gles2::ContextGroup* GLES2DecoderPassthroughImpl::GetContextGroup() {
  return nullptr;
}

gpu::Capabilities GLES2DecoderPassthroughImpl::GetCapabilities() {
  DCHECK(initialized());
  Capabilities caps;
  return caps;
}

void GLES2DecoderPassthroughImpl::RestoreState(const ContextState* prev_state) {

}

void GLES2DecoderPassthroughImpl::RestoreActiveTexture() const {}

void GLES2DecoderPassthroughImpl::RestoreAllTextureUnitBindings(
    const ContextState* prev_state) const {}

void GLES2DecoderPassthroughImpl::RestoreActiveTextureUnitBinding(
    unsigned int target) const {}

void GLES2DecoderPassthroughImpl::RestoreBufferBindings() const {}

void GLES2DecoderPassthroughImpl::RestoreFramebufferBindings() const {}

void GLES2DecoderPassthroughImpl::RestoreRenderbufferBindings() {}

void GLES2DecoderPassthroughImpl::RestoreGlobalState() const {}

void GLES2DecoderPassthroughImpl::RestoreProgramBindings() const {}

void GLES2DecoderPassthroughImpl::RestoreTextureState(
    unsigned service_id) const {}

void GLES2DecoderPassthroughImpl::RestoreTextureUnitBindings(
    unsigned unit) const {}

void GLES2DecoderPassthroughImpl::RestoreAllExternalTextureBindingsIfNeeded() {}

void GLES2DecoderPassthroughImpl::ClearAllAttributes() const {}

void GLES2DecoderPassthroughImpl::RestoreAllAttributes() const {}

void GLES2DecoderPassthroughImpl::SetIgnoreCachedStateForTest(bool ignore) {}

void GLES2DecoderPassthroughImpl::SetForceShaderNameHashingForTest(bool force) {

}

size_t GLES2DecoderPassthroughImpl::GetSavedBackTextureCountForTest() {
  return 0;
}

size_t GLES2DecoderPassthroughImpl::GetCreatedBackTextureCountForTest() {
  return 0;
}

void GLES2DecoderPassthroughImpl::SetFenceSyncReleaseCallback(
    const FenceSyncReleaseCallback& callback) {}

void GLES2DecoderPassthroughImpl::SetWaitFenceSyncCallback(
    const WaitFenceSyncCallback& callback) {}

void GLES2DecoderPassthroughImpl::SetDescheduleUntilFinishedCallback(
    const NoParamCallback& callback) {}

void GLES2DecoderPassthroughImpl::SetRescheduleAfterFinishedCallback(
    const NoParamCallback& callback) {}

gpu::gles2::QueryManager* GLES2DecoderPassthroughImpl::GetQueryManager() {
  return nullptr;
}

gpu::gles2::TransformFeedbackManager*
GLES2DecoderPassthroughImpl::GetTransformFeedbackManager() {
  return nullptr;
}

gpu::gles2::VertexArrayManager*
GLES2DecoderPassthroughImpl::GetVertexArrayManager() {
  return nullptr;
}

gpu::gles2::ImageManager* GLES2DecoderPassthroughImpl::GetImageManager() {
  return image_manager_.get();
}

bool GLES2DecoderPassthroughImpl::HasPendingQueries() const {
  return false;
}

void GLES2DecoderPassthroughImpl::ProcessPendingQueries(bool did_finish) {}

bool GLES2DecoderPassthroughImpl::HasMoreIdleWork() const {
  return false;
}

void GLES2DecoderPassthroughImpl::PerformIdleWork() {}

bool GLES2DecoderPassthroughImpl::HasPollingWork() const {
  return false;
}

void GLES2DecoderPassthroughImpl::PerformPollingWork() {}

bool GLES2DecoderPassthroughImpl::GetServiceTextureId(
    uint32_t client_texture_id,
    uint32_t* service_texture_id) {
  return false;
}

gpu::error::ContextLostReason
GLES2DecoderPassthroughImpl::GetContextLostReason() {
  return error::kUnknown;
}

bool GLES2DecoderPassthroughImpl::ClearLevel(Texture* texture,
                                             unsigned target,
                                             int level,
                                             unsigned format,
                                             unsigned type,
                                             int xoffset,
                                             int yoffset,
                                             int width,
                                             int height) {
  return true;
}

bool GLES2DecoderPassthroughImpl::ClearCompressedTextureLevel(Texture* texture,
                                                              unsigned target,
                                                              int level,
                                                              unsigned format,
                                                              int width,
                                                              int height) {
  return true;
}

bool GLES2DecoderPassthroughImpl::IsCompressedTextureFormat(unsigned format) {
  return false;
}

bool GLES2DecoderPassthroughImpl::ClearLevel3D(Texture* texture,
                                               unsigned target,
                                               int level,
                                               unsigned format,
                                               unsigned type,
                                               int width,
                                               int height,
                                               int depth) {
  return true;
}

gpu::gles2::ErrorState* GLES2DecoderPassthroughImpl::GetErrorState() {
  return nullptr;
}

void GLES2DecoderPassthroughImpl::SetShaderCacheCallback(
    const ShaderCacheCallback& callback) {}

void GLES2DecoderPassthroughImpl::WaitForReadPixels(base::Closure callback) {}

uint32_t GLES2DecoderPassthroughImpl::GetTextureUploadCount() {
  return 0;
}

base::TimeDelta GLES2DecoderPassthroughImpl::GetTotalTextureUploadTime() {
  return base::TimeDelta();
}

base::TimeDelta GLES2DecoderPassthroughImpl::GetTotalProcessingCommandsTime() {
  return base::TimeDelta();
}

void GLES2DecoderPassthroughImpl::AddProcessingCommandsTime(base::TimeDelta) {}

bool GLES2DecoderPassthroughImpl::WasContextLost() const {
  return false;
}

bool GLES2DecoderPassthroughImpl::WasContextLostByRobustnessExtension() const {
  return false;
}

void GLES2DecoderPassthroughImpl::MarkContextLost(
    error::ContextLostReason reason) {}

gpu::gles2::Logger* GLES2DecoderPassthroughImpl::GetLogger() {
  return &logger_;
}

const gpu::gles2::ContextState* GLES2DecoderPassthroughImpl::GetContextState() {
  return nullptr;
}

#define GLES2_CMD_OP(name)                                               \
  {                                                                      \
      &GLES2DecoderPassthroughImpl::Handle##name, cmds::name::kArgFlags, \
      cmds::name::cmd_flags,                                             \
      sizeof(cmds::name) / sizeof(CommandBufferEntry) - 1,               \
  }, /* NOLINT */

const GLES2DecoderPassthroughImpl::CommandInfo
    GLES2DecoderPassthroughImpl::command_info[] = {
        GLES2_COMMAND_LIST(GLES2_CMD_OP)};

#undef GLES2_CMD_OP

}  // namespace gles2
}  // namespace gpu
