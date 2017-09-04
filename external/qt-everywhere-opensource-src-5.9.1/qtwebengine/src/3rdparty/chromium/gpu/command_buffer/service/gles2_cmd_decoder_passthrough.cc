// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder_passthrough.h"

#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "ui/gl/gl_version_info.h"

namespace gpu {
namespace gles2 {

namespace {
template <typename ClientType, typename ServiceType, typename DeleteFunction>
void DeleteServiceObjects(ClientServiceMap<ClientType, ServiceType>* id_map,
                          bool have_context,
                          DeleteFunction delete_function) {
  if (have_context) {
    for (auto client_service_id_pair : *id_map) {
      delete_function(client_service_id_pair.second);
    }
  }

  id_map->Clear();
}

}  // anonymous namespace

PassthroughResources::PassthroughResources() {}

PassthroughResources::~PassthroughResources() {}

void PassthroughResources::Destroy(bool have_context) {
  DeleteServiceObjects(&texture_id_map, have_context,
                       [](GLuint texture) { glDeleteTextures(1, &texture); });
  DeleteServiceObjects(&buffer_id_map, have_context,
                       [](GLuint buffer) { glDeleteBuffersARB(1, &buffer); });
  DeleteServiceObjects(
      &renderbuffer_id_map, have_context,
      [](GLuint renderbuffer) { glDeleteRenderbuffersEXT(1, &renderbuffer); });
  DeleteServiceObjects(&sampler_id_map, have_context,
                       [](GLuint sampler) { glDeleteSamplers(1, &sampler); });
  DeleteServiceObjects(&program_id_map, have_context,
                       [](GLuint program) { glDeleteProgram(program); });
  DeleteServiceObjects(&shader_id_map, have_context,
                       [](GLuint shader) { glDeleteShader(shader); });
  DeleteServiceObjects(&sync_id_map, have_context, [](uintptr_t sync) {
    glDeleteSync(reinterpret_cast<GLsync>(sync));
  });

  if (!have_context) {
    for (auto passthrough_texture : texture_object_map) {
      passthrough_texture.second->MarkContextLost();
    }
  }
  texture_object_map.clear();
}

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
    const volatile void* buffer,
    int num_entries,
    int* entries_processed) {
  commands_to_process_ = num_commands;
  error::Error result = error::kNoError;
  const volatile CommandBufferEntry* cmd_data =
      static_cast<const volatile CommandBufferEntry*>(buffer);
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

  // Check for required extensions
  if (!feature_info_->feature_flags().angle_robust_client_memory ||
      !feature_info_->feature_flags().chromium_bind_generates_resource ||
      (feature_info_->IsWebGLContext() !=
       feature_info_->feature_flags().angle_webgl_compatibility)) {
    Destroy(true);
    return false;
  }

  image_manager_.reset(new ImageManager());

  bind_generates_resource_ = group_->bind_generates_resource();

  resources_ = group_->passthrough_resources();

  mailbox_manager_ = group_->mailbox_manager();

  // Query information about the texture units
  GLint num_texture_units = 0;
  glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &num_texture_units);

  active_texture_unit_ = 0;
  bound_textures_.resize(num_texture_units, 0);

  if (group_->gpu_preferences().enable_gpu_driver_debug_logging &&
      feature_info_->feature_flags().khr_debug) {
    InitializeGLDebugLogging();
  }

  emulated_extensions_.push_back("GL_CHROMIUM_lose_context");
  emulated_extensions_.push_back("GL_CHROMIUM_pixel_transfer_buffer_object");
  emulated_extensions_.push_back("GL_CHROMIUM_resource_safe");
  emulated_extensions_.push_back("GL_CHROMIUM_strict_attribs");
  emulated_extensions_.push_back("GL_CHROMIUM_texture_mailbox");
  emulated_extensions_.push_back("GL_CHROMIUM_trace_marker");
  BuildExtensionsString();

  set_initialized();
  return true;
}

void GLES2DecoderPassthroughImpl::Destroy(bool have_context) {
  image_manager_.reset();

  DeleteServiceObjects(
      &framebuffer_id_map_, have_context,
      [](GLuint framebuffer) { glDeleteFramebuffersEXT(1, &framebuffer); });
  DeleteServiceObjects(&transform_feedback_id_map_, have_context,
                       [](GLuint transform_feedback) {
                         glDeleteTransformFeedbacks(1, &transform_feedback);
                       });
  DeleteServiceObjects(&query_id_map_, have_context,
                       [](GLuint query) { glDeleteQueries(1, &query); });
  DeleteServiceObjects(
      &vertex_array_id_map_, have_context,
      [](GLuint vertex_array) { glDeleteVertexArraysOES(1, &vertex_array); });

  if (group_) {
    group_->Destroy(this, have_context);
    group_ = nullptr;
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

bool GLES2DecoderPassthroughImpl::ResizeOffscreenFramebuffer(
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
  return group_.get();
}

const FeatureInfo* GLES2DecoderPassthroughImpl::GetFeatureInfo() const {
  return group_->feature_info();
}

gpu::Capabilities GLES2DecoderPassthroughImpl::GetCapabilities() {
  DCHECK(initialized());
  Capabilities caps;

  PopulateNumericCapabilities(&caps, feature_info_.get());

  glGetIntegerv(GL_BIND_GENERATES_RESOURCE_CHROMIUM,
                &caps.bind_generates_resource_chromium);
  DCHECK_EQ(caps.bind_generates_resource_chromium != GL_FALSE,
            group_->bind_generates_resource());

  caps.egl_image_external =
      feature_info_->feature_flags().oes_egl_image_external;
  caps.texture_format_astc =
      feature_info_->feature_flags().ext_texture_format_astc;
  caps.texture_format_atc =
      feature_info_->feature_flags().ext_texture_format_atc;
  caps.texture_format_bgra8888 =
      feature_info_->feature_flags().ext_texture_format_bgra8888;
  caps.texture_format_dxt1 =
      feature_info_->feature_flags().ext_texture_format_dxt1;
  caps.texture_format_dxt5 =
      feature_info_->feature_flags().ext_texture_format_dxt5;
  caps.texture_format_etc1 =
      feature_info_->feature_flags().oes_compressed_etc1_rgb8_texture;
  caps.texture_format_etc1_npot = caps.texture_format_etc1;
  caps.texture_rectangle = feature_info_->feature_flags().arb_texture_rectangle;
  caps.texture_usage = feature_info_->feature_flags().angle_texture_usage;
  caps.texture_storage = feature_info_->feature_flags().ext_texture_storage;
  caps.discard_framebuffer =
      feature_info_->feature_flags().ext_discard_framebuffer;
  caps.sync_query = feature_info_->feature_flags().chromium_sync_query;
#if defined(OS_MACOSX)
  // This is unconditionally true on mac, no need to test for it at runtime.
  caps.iosurface = true;
#endif
  caps.flips_vertically = surface_->FlipsVertically();
  caps.blend_equation_advanced =
      feature_info_->feature_flags().blend_equation_advanced;
  caps.blend_equation_advanced_coherent =
      feature_info_->feature_flags().blend_equation_advanced_coherent;
  caps.texture_rg = feature_info_->feature_flags().ext_texture_rg;
  caps.texture_half_float_linear =
      feature_info_->feature_flags().enable_texture_half_float_linear;
  caps.image_ycbcr_422 =
      feature_info_->feature_flags().chromium_image_ycbcr_422;
  caps.image_ycbcr_420v =
      feature_info_->feature_flags().chromium_image_ycbcr_420v;
  caps.max_copy_texture_chromium_size =
      feature_info_->workarounds().max_copy_texture_chromium_size;
  caps.render_buffer_format_bgra8888 =
      feature_info_->feature_flags().ext_render_buffer_format_bgra8888;
  caps.occlusion_query_boolean =
      feature_info_->feature_flags().occlusion_query_boolean;

  // TODO:
  // caps.timer_queries
  // caps.post_sub_buffer
  // caps.commit_overlay_planes
  // caps.surfaceless
  // caps.is_offscreen
  // caps.flips_vertically

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
    const FenceSyncReleaseCallback& callback) {
  fence_sync_release_callback_ = callback;
}

void GLES2DecoderPassthroughImpl::SetWaitFenceSyncCallback(
    const WaitFenceSyncCallback& callback) {
  wait_fence_sync_callback_ = callback;
}

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
  return resources_->texture_id_map.GetServiceID(client_texture_id,
                                                 service_texture_id);
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

scoped_refptr<ShaderTranslatorInterface>
GLES2DecoderPassthroughImpl::GetTranslator(GLenum type) {
  return nullptr;
}

void GLES2DecoderPassthroughImpl::BuildExtensionsString() {
  std::ostringstream combined_string_stream;
  combined_string_stream << reinterpret_cast<const char*>(
                                glGetString(GL_EXTENSIONS))
                         << " ";
  std::copy(emulated_extensions_.begin(), emulated_extensions_.end(),
            std::ostream_iterator<std::string>(combined_string_stream, " "));
  extension_string_ = combined_string_stream.str();
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
