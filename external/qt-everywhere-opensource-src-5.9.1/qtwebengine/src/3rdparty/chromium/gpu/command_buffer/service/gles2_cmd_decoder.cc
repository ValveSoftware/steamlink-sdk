// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <algorithm>
#include <cmath>
#include <list>
#include <map>
#include <memory>
#include <queue>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_synthetic_delay.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/debug_marker_manager.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/error_state.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "gpu/command_buffer/service/gl_stream_texture_image.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/gles2_cmd_apply_framebuffer_attachment_cmaa_intel.h"
#include "gpu/command_buffer/service/gles2_cmd_clear_framebuffer.h"
#include "gpu/command_buffer/service/gles2_cmd_copy_tex_image.h"
#include "gpu/command_buffer/service/gles2_cmd_copy_texture_chromium.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_passthrough.h"
#include "gpu/command_buffer/service/gles2_cmd_srgb_converter.h"
#include "gpu/command_buffer/service/gles2_cmd_validation.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "gpu/command_buffer/service/gpu_state_tracer.h"
#include "gpu/command_buffer/service/gpu_tracer.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/logger.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/path_manager.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/query_manager.h"
#include "gpu/command_buffer/service/renderbuffer_manager.h"
#include "gpu/command_buffer/service/sampler_manager.h"
#include "gpu/command_buffer/service/shader_manager.h"
#include "gpu/command_buffer/service/shader_translator.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/command_buffer/service/transform_feedback_manager.h"
#include "gpu/command_buffer/service/vertex_array_manager.h"
#include "gpu/command_buffer/service/vertex_attrib_manager.h"
#include "third_party/angle/src/image_util/loadimage.h"
#include "third_party/smhasher/src/City.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gfx/transform.h"
#include "ui/gl/ca_renderer_layer_params.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/gpu_timing.h"

#if defined(OS_MACOSX)
#include <IOSurface/IOSurface.h>
// Note that this must be included after gl_bindings.h to avoid conflicts.
#include <OpenGL/CGLIOSurface.h>
#endif

namespace gpu {
namespace gles2 {

namespace {

const char kOESDerivativeExtension[] = "GL_OES_standard_derivatives";
const char kEXTFragDepthExtension[] = "GL_EXT_frag_depth";
const char kEXTDrawBuffersExtension[] = "GL_EXT_draw_buffers";
const char kEXTShaderTextureLodExtension[] = "GL_EXT_shader_texture_lod";

const GLfloat kIdentityMatrix[16] = {1.0f, 0.0f, 0.0f, 0.0f,
                                     0.0f, 1.0f, 0.0f, 0.0f,
                                     0.0f, 0.0f, 1.0f, 0.0f,
                                     0.0f, 0.0f, 0.0f, 1.0f};

gfx::OverlayTransform GetGFXOverlayTransform(GLenum plane_transform) {
  switch (plane_transform) {
    case GL_OVERLAY_TRANSFORM_NONE_CHROMIUM:
      return gfx::OVERLAY_TRANSFORM_NONE;
    case GL_OVERLAY_TRANSFORM_FLIP_HORIZONTAL_CHROMIUM:
      return gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL;
    case GL_OVERLAY_TRANSFORM_FLIP_VERTICAL_CHROMIUM:
      return gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL;
    case GL_OVERLAY_TRANSFORM_ROTATE_90_CHROMIUM:
      return gfx::OVERLAY_TRANSFORM_ROTATE_90;
    case GL_OVERLAY_TRANSFORM_ROTATE_180_CHROMIUM:
      return gfx::OVERLAY_TRANSFORM_ROTATE_180;
    case GL_OVERLAY_TRANSFORM_ROTATE_270_CHROMIUM:
      return gfx::OVERLAY_TRANSFORM_ROTATE_270;
    default:
      return gfx::OVERLAY_TRANSFORM_INVALID;
  }
}

template <typename MANAGER_TYPE, typename OBJECT_TYPE>
GLuint GetClientId(const MANAGER_TYPE* manager, const OBJECT_TYPE* object) {
  DCHECK(manager);
  GLuint client_id = 0;
  if (object) {
    manager->GetClientId(object->service_id(), &client_id);
  }
  return client_id;
}

template <typename OBJECT_TYPE>
GLuint GetServiceId(const OBJECT_TYPE* object) {
  return object ? object->service_id() : 0;
}

struct Vec4f {
  explicit Vec4f(const Vec4& data) {
    data.GetValues(v);
  }

  GLfloat v[4];
};

struct TexSubCoord3D {
  TexSubCoord3D(int _xoffset, int _yoffset, int _zoffset,
                int _width, int _height, int _depth)
      : xoffset(_xoffset),
        yoffset(_yoffset),
        zoffset(_zoffset),
        width(_width),
        height(_height),
        depth(_depth) {}

  int xoffset;
  int yoffset;
  int zoffset;
  int width;
  int height;
  int depth;
};

// Check if all |ref| bits are set in |bits|.
bool AllBitsSet(GLbitfield bits, GLbitfield ref) {
  DCHECK_NE(0u, ref);
  return ((bits & ref) == ref);
}

// Check if any of |ref| bits are set in |bits|.
bool AnyBitsSet(GLbitfield bits, GLbitfield ref) {
  DCHECK_NE(0u, ref);
  return ((bits & ref) != 0);
}

// Check if any bits are set in |bits| other than the bits in |ref|.
bool AnyOtherBitsSet(GLbitfield bits, GLbitfield ref) {
  DCHECK_NE(0u, ref);
  GLbitfield mask = ~ref;
  return ((bits & mask) != 0);
}

}  // namespace

class GLES2DecoderImpl;

// Local versions of the SET_GL_ERROR macros
#define LOCAL_SET_GL_ERROR(error, function_name, msg) \
    ERRORSTATE_SET_GL_ERROR(state_.GetErrorState(), error, function_name, msg)
#define LOCAL_SET_GL_ERROR_INVALID_ENUM(function_name, value, label) \
    ERRORSTATE_SET_GL_ERROR_INVALID_ENUM(state_.GetErrorState(), \
                                         function_name, value, label)
#define LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER(function_name) \
    ERRORSTATE_COPY_REAL_GL_ERRORS_TO_WRAPPER(state_.GetErrorState(), \
                                              function_name)
#define LOCAL_PEEK_GL_ERROR(function_name) \
    ERRORSTATE_PEEK_GL_ERROR(state_.GetErrorState(), function_name)
#define LOCAL_CLEAR_REAL_GL_ERRORS(function_name) \
    ERRORSTATE_CLEAR_REAL_GL_ERRORS(state_.GetErrorState(), function_name)
#define LOCAL_PERFORMANCE_WARNING(msg) \
    PerformanceWarning(__FILE__, __LINE__, msg)
#define LOCAL_RENDER_WARNING(msg) \
    RenderWarning(__FILE__, __LINE__, msg)

// Check that certain assumptions the code makes are true. There are places in
// the code where shared memory is passed direclty to GL. Example, glUniformiv,
// glShaderSource. The command buffer code assumes GLint and GLsizei (and maybe
// a few others) are 32bits. If they are not 32bits the code will have to change
// to call those GL functions with service side memory and then copy the results
// to shared memory, converting the sizes.
static_assert(sizeof(GLint) == sizeof(uint32_t),  // NOLINT
              "GLint should be the same size as uint32_t");
static_assert(sizeof(GLsizei) == sizeof(uint32_t),  // NOLINT
              "GLsizei should be the same size as uint32_t");
static_assert(sizeof(GLfloat) == sizeof(float),  // NOLINT
              "GLfloat should be the same size as float");

// TODO(kbr): the use of this anonymous namespace core dumps the
// linker on Mac OS X 10.6 when the symbol ordering file is used
// namespace {

// Return true if a character belongs to the ASCII subset as defined in
// GLSL ES 1.0 spec section 3.1.
static bool CharacterIsValidForGLES(unsigned char c) {
  // Printing characters are valid except " $ ` @ \ ' DEL.
  if (c >= 32 && c <= 126 &&
      c != '"' &&
      c != '$' &&
      c != '`' &&
      c != '@' &&
      c != '\\' &&
      c != '\'') {
    return true;
  }
  // Horizontal tab, line feed, vertical tab, form feed, carriage return
  // are also valid.
  if (c >= 9 && c <= 13) {
    return true;
  }

  return false;
}

static bool StringIsValidForGLES(const std::string& str) {
  return str.length() == 0 ||
         std::find_if_not(str.begin(), str.end(), CharacterIsValidForGLES) ==
             str.end();
}

// This class prevents any GL errors that occur when it is in scope from
// being reported to the client.
class ScopedGLErrorSuppressor {
 public:
  explicit ScopedGLErrorSuppressor(
      const char* function_name, ErrorState* error_state);
  ~ScopedGLErrorSuppressor();
 private:
  const char* function_name_;
  ErrorState* error_state_;
  DISALLOW_COPY_AND_ASSIGN(ScopedGLErrorSuppressor);
};

// Temporarily changes a decoder's bound texture and restore it when this
// object goes out of scope. Also temporarily switches to using active texture
// unit zero in case the client has changed that to something invalid.
class ScopedTextureBinder {
 public:
  explicit ScopedTextureBinder(ContextState* state, GLuint id, GLenum target);
  ~ScopedTextureBinder();

 private:
  ContextState* state_;
  GLenum target_;
  DISALLOW_COPY_AND_ASSIGN(ScopedTextureBinder);
};

// Temporarily changes a decoder's bound render buffer and restore it when this
// object goes out of scope.
class ScopedRenderBufferBinder {
 public:
  explicit ScopedRenderBufferBinder(ContextState* state, GLuint id);
  ~ScopedRenderBufferBinder();

 private:
  ContextState* state_;
  DISALLOW_COPY_AND_ASSIGN(ScopedRenderBufferBinder);
};

// Temporarily changes a decoder's bound frame buffer and restore it when this
// object goes out of scope.
class ScopedFramebufferBinder {
 public:
  explicit ScopedFramebufferBinder(GLES2DecoderImpl* decoder, GLuint id);
  ~ScopedFramebufferBinder();

 private:
  GLES2DecoderImpl* decoder_;
  DISALLOW_COPY_AND_ASSIGN(ScopedFramebufferBinder);
};

// Temporarily changes a decoder's bound frame buffer to a resolved version of
// the multisampled offscreen render buffer if that buffer is multisampled, and,
// if it is bound or enforce_internal_framebuffer is true. If internal is
// true, the resolved framebuffer is not visible to the parent.
class ScopedResolvedFramebufferBinder {
 public:
  explicit ScopedResolvedFramebufferBinder(GLES2DecoderImpl* decoder,
                                           bool enforce_internal_framebuffer,
                                           bool internal);
  ~ScopedResolvedFramebufferBinder();

 private:
  GLES2DecoderImpl* decoder_;
  bool resolve_and_bind_;
  DISALLOW_COPY_AND_ASSIGN(ScopedResolvedFramebufferBinder);
};

// Encapsulates an OpenGL texture.
class BackTexture {
 public:
  explicit BackTexture(GLES2DecoderImpl* decoder);
  ~BackTexture();

  // Create a new render texture.
  void Create();

  // Set the initial size and format of a render texture or resize it.
  bool AllocateStorage(const gfx::Size& size, GLenum format, bool zero);

  // Copy the contents of the currently bound frame buffer.
  void Copy();

  // Destroy the render texture. This must be explicitly called before
  // destroying this object.
  void Destroy();

  // Invalidate the texture. This can be used when a context is lost and it is
  // not possible to make it current in order to free the resource.
  void Invalidate();

  // The bind point for the texture.
  GLenum Target();

  scoped_refptr<TextureRef> texture_ref() { return texture_ref_; }

  GLuint id() const {
    return texture_ref_ ? texture_ref_->service_id() : 0;
  }

  gfx::Size size() const {
    return size_;
  }

 private:
  // The texture must be bound to Target() before calling this method.
  // Returns whether the operation was successful.
  bool AllocateNativeGpuMemoryBuffer(const gfx::Size& size,
                                     GLenum format,
                                     bool zero);

  // The texture must be bound to Target() before calling this method.
  void DestroyNativeGpuMemoryBuffer(bool have_context);

  MemoryTypeTracker memory_tracker_;
  size_t bytes_allocated_;
  gfx::Size size_;
  GLES2DecoderImpl* decoder_;

  scoped_refptr<TextureRef> texture_ref_;

  // The image that backs the texture, if its backed by a native
  // GpuMemoryBuffer.
  scoped_refptr<gl::GLImage> image_;

  DISALLOW_COPY_AND_ASSIGN(BackTexture);
};

// Encapsulates an OpenGL render buffer of any format.
class BackRenderbuffer {
 public:
  explicit BackRenderbuffer(GLES2DecoderImpl* decoder);
  ~BackRenderbuffer();

  // Create a new render buffer.
  void Create();

  // Set the initial size and format of a render buffer or resize it.
  bool AllocateStorage(const FeatureInfo* feature_info,
                       const gfx::Size& size,
                       GLenum format,
                       GLsizei samples);

  // Destroy the render buffer. This must be explicitly called before destroying
  // this object.
  void Destroy();

  // Invalidate the render buffer. This can be used when a context is lost and
  // it is not possible to make it current in order to free the resource.
  void Invalidate();

  GLuint id() const {
    return id_;
  }

 private:
  GLES2DecoderImpl* decoder_;
  MemoryTypeTracker memory_tracker_;
  size_t bytes_allocated_;
  GLuint id_;
  DISALLOW_COPY_AND_ASSIGN(BackRenderbuffer);
};

// Encapsulates an OpenGL frame buffer.
class BackFramebuffer {
 public:
  explicit BackFramebuffer(GLES2DecoderImpl* decoder);
  ~BackFramebuffer();

  // Create a new frame buffer.
  void Create();

  // Attach a color render buffer to a frame buffer.
  void AttachRenderTexture(BackTexture* texture);

  // Attach a render buffer to a frame buffer. Note that this unbinds any
  // currently bound frame buffer.
  void AttachRenderBuffer(GLenum target, BackRenderbuffer* render_buffer);

  // Destroy the frame buffer. This must be explicitly called before destroying
  // this object.
  void Destroy();

  // Invalidate the frame buffer. This can be used when a context is lost and it
  // is not possible to make it current in order to free the resource.
  void Invalidate();

  // See glCheckFramebufferStatusEXT.
  GLenum CheckStatus();

  GLuint id() const {
    return id_;
  }

 private:
  GLES2DecoderImpl* decoder_;
  GLuint id_;
  DISALLOW_COPY_AND_ASSIGN(BackFramebuffer);
};

struct FenceCallback {
  FenceCallback() : fence(gl::GLFence::Create()) { DCHECK(fence); }
  FenceCallback(FenceCallback&&) = default;
  FenceCallback& operator=(FenceCallback&&) = default;
  std::vector<base::Closure> callbacks;
  std::unique_ptr<gl::GLFence> fence;
};

// }  // anonymous namespace.

// static
const unsigned int GLES2Decoder::kDefaultStencilMask =
    static_cast<unsigned int>(-1);

bool GLES2Decoder::GetServiceTextureId(uint32_t client_texture_id,
                                       uint32_t* service_texture_id) {
  return false;
}

uint32_t GLES2Decoder::GetAndClearBackbufferClearBitsForTest() {
  return 0;
}

GLES2Decoder::GLES2Decoder()
    : initialized_(false),
      debug_(false),
      log_commands_(false) {
}

GLES2Decoder::~GLES2Decoder() {
}

void GLES2Decoder::BeginDecoding() {}

void GLES2Decoder::EndDecoding() {}

error::Error GLES2Decoder::DoCommand(unsigned int command,
                                     unsigned int arg_count,
                                     const volatile void* cmd_data) {
  return DoCommands(1, cmd_data, arg_count + 1, 0);
}

// This class implements GLES2Decoder so we don't have to expose all the GLES2
// cmd stuff to outside this class.
class GLES2DecoderImpl : public GLES2Decoder, public ErrorStateClient {
 public:
  explicit GLES2DecoderImpl(ContextGroup* group);
  ~GLES2DecoderImpl() override;

  error::Error DoCommands(unsigned int num_commands,
                          const volatile void* buffer,
                          int num_entries,
                          int* entries_processed) override;

  template <bool DebugImpl>
  error::Error DoCommandsImpl(unsigned int num_commands,
                              const volatile void* buffer,
                              int num_entries,
                              int* entries_processed);

  // Overridden from AsyncAPIInterface.
  const char* GetCommandName(unsigned int command_id) const override;

  // Overridden from GLES2Decoder.
  bool Initialize(const scoped_refptr<gl::GLSurface>& surface,
                  const scoped_refptr<gl::GLContext>& context,
                  bool offscreen,
                  const DisallowedFeatures& disallowed_features,
                  const ContextCreationAttribHelper& attrib_helper) override;
  void Destroy(bool have_context) override;
  void SetSurface(const scoped_refptr<gl::GLSurface>& surface) override;
  void ReleaseSurface() override;
  void TakeFrontBuffer(const Mailbox& mailbox) override;
  void ReturnFrontBuffer(const Mailbox& mailbox, bool is_lost) override;
  bool ResizeOffscreenFramebuffer(const gfx::Size& size) override;
  bool MakeCurrent() override;
  GLES2Util* GetGLES2Util() override { return &util_; }
  gl::GLContext* GetGLContext() override { return context_.get(); }
  ContextGroup* GetContextGroup() override { return group_.get(); }
  const FeatureInfo* GetFeatureInfo() const override {
    return feature_info_.get();
  }
  Capabilities GetCapabilities() override;
  void RestoreState(const ContextState* prev_state) override;

  void RestoreActiveTexture() const override { state_.RestoreActiveTexture(); }
  void RestoreAllTextureUnitBindings(
      const ContextState* prev_state) const override {
    state_.RestoreAllTextureUnitBindings(prev_state);
  }
  void RestoreActiveTextureUnitBinding(unsigned int target) const override {
    state_.RestoreActiveTextureUnitBinding(target);
  }
  void RestoreBufferBindings() const override {
    state_.RestoreBufferBindings();
  }
  void RestoreGlobalState() const override { state_.RestoreGlobalState(NULL); }
  void RestoreProgramBindings() const override {
    state_.RestoreProgramSettings(nullptr, false);
  }
  void RestoreTextureUnitBindings(unsigned unit) const override {
    state_.RestoreTextureUnitBindings(unit, NULL);
  }
  void RestoreFramebufferBindings() const override;
  void RestoreRenderbufferBindings() override;
  void RestoreTextureState(unsigned service_id) const override;

  void ClearAllAttributes() const override;
  void RestoreAllAttributes() const override;

  QueryManager* GetQueryManager() override { return query_manager_.get(); }
  TransformFeedbackManager* GetTransformFeedbackManager() override {
    return transform_feedback_manager_.get();
  }
  VertexArrayManager* GetVertexArrayManager() override {
    return vertex_array_manager_.get();
  }
  ImageManager* GetImageManager() override { return image_manager_.get(); }

  bool HasPendingQueries() const override;
  void ProcessPendingQueries(bool did_finish) override;

  bool HasMoreIdleWork() const override;
  void PerformIdleWork() override;

  bool HasPollingWork() const override;
  void PerformPollingWork() override;

  void WaitForReadPixels(base::Closure callback) override;

  Logger* GetLogger() override;

  void BeginDecoding() override;
  void EndDecoding() override;

  ErrorState* GetErrorState() override;
  const ContextState* GetContextState() override { return &state_; }
  scoped_refptr<ShaderTranslatorInterface> GetTranslator(GLenum type) override;

  void SetShaderCacheCallback(const ShaderCacheCallback& callback) override;
  void SetFenceSyncReleaseCallback(
      const FenceSyncReleaseCallback& callback) override;
  void SetWaitFenceSyncCallback(const WaitFenceSyncCallback& callback) override;

  void SetDescheduleUntilFinishedCallback(
      const NoParamCallback& callback) override;
  void SetRescheduleAfterFinishedCallback(
      const NoParamCallback& callback) override;

  void SetIgnoreCachedStateForTest(bool ignore) override;
  void SetForceShaderNameHashingForTest(bool force) override;
  uint32_t GetAndClearBackbufferClearBitsForTest() override;
  void ProcessFinishedAsyncTransfers();

  bool GetServiceTextureId(uint32_t client_texture_id,
                           uint32_t* service_texture_id) override;

  uint32_t GetTextureUploadCount() override;
  base::TimeDelta GetTotalTextureUploadTime() override;
  base::TimeDelta GetTotalProcessingCommandsTime() override;
  void AddProcessingCommandsTime(base::TimeDelta) override;

  // Restores the current state to the user's settings.
  void RestoreCurrentFramebufferBindings();

  // Sets DEPTH_TEST, STENCIL_TEST and color mask for the current framebuffer.
  void ApplyDirtyState();

  // These check the state of the currently bound framebuffer or the
  // backbuffer if no framebuffer is bound.
  // Check with all attached and enabled color attachments.
  bool BoundFramebufferAllowsChangesToAlphaChannel();
  bool BoundFramebufferHasDepthAttachment();
  bool BoundFramebufferHasStencilAttachment();

  error::ContextLostReason GetContextLostReason() override;

  // Overriden from ErrorStateClient.
  void OnContextLostError() override;
  void OnOutOfMemoryError() override;

  // Ensure Renderbuffer corresponding to last DoBindRenderbuffer() is bound.
  void EnsureRenderbufferBound();

  // Helpers to facilitate calling into compatible extensions.
  static void RenderbufferStorageMultisampleHelper(
      const FeatureInfo* feature_info,
      GLenum target,
      GLsizei samples,
      GLenum internal_format,
      GLsizei width,
      GLsizei height);

  void BlitFramebufferHelper(GLint srcX0,
                             GLint srcY0,
                             GLint srcX1,
                             GLint srcY1,
                             GLint dstX0,
                             GLint dstY0,
                             GLint dstX1,
                             GLint dstY1,
                             GLbitfield mask,
                             GLenum filter);

  PathManager* path_manager() { return group_->path_manager(); }

 private:
  friend class ScopedFramebufferBinder;
  friend class ScopedResolvedFramebufferBinder;
  friend class BackFramebuffer;
  friend class BackRenderbuffer;
  friend class BackTexture;

  enum FramebufferOperation {
    kFramebufferDiscard,
    kFramebufferInvalidate,
    kFramebufferInvalidateSub
  };

  enum BindIndexedBufferFunctionType {
    kBindBufferBase,
    kBindBufferRange
  };

  // Initialize or re-initialize the shader translator.
  bool InitializeShaderTranslator();

  void UpdateCapabilities();

  // Helpers for the glGen and glDelete functions.
  bool GenTexturesHelper(GLsizei n, const GLuint* client_ids);
  void DeleteTexturesHelper(GLsizei n, const volatile GLuint* client_ids);
  bool GenBuffersHelper(GLsizei n, const GLuint* client_ids);
  void DeleteBuffersHelper(GLsizei n, const volatile GLuint* client_ids);
  bool GenFramebuffersHelper(GLsizei n, const GLuint* client_ids);
  void DeleteFramebuffersHelper(GLsizei n, const volatile GLuint* client_ids);
  bool GenRenderbuffersHelper(GLsizei n, const GLuint* client_ids);
  void DeleteRenderbuffersHelper(GLsizei n, const volatile GLuint* client_ids);
  bool GenQueriesEXTHelper(GLsizei n, const GLuint* client_ids);
  void DeleteQueriesEXTHelper(GLsizei n, const volatile GLuint* client_ids);
  bool GenVertexArraysOESHelper(GLsizei n, const GLuint* client_ids);
  void DeleteVertexArraysOESHelper(GLsizei n,
                                   const volatile GLuint* client_ids);
  bool GenPathsCHROMIUMHelper(GLuint first_client_id, GLsizei range);
  bool DeletePathsCHROMIUMHelper(GLuint first_client_id, GLsizei range);
  bool GenSamplersHelper(GLsizei n, const GLuint* client_ids);
  void DeleteSamplersHelper(GLsizei n, const volatile GLuint* client_ids);
  bool GenTransformFeedbacksHelper(GLsizei n, const GLuint* client_ids);
  void DeleteTransformFeedbacksHelper(GLsizei n,
                                      const volatile GLuint* client_ids);
  void DeleteSyncHelper(GLuint sync);

  // Workarounds
  void OnFboChanged() const;
  void OnUseFramebuffer() const;

  error::ContextLostReason GetContextLostReasonFromResetStatus(
      GLenum reset_status) const;

  // TODO(gman): Cache these pointers?
  BufferManager* buffer_manager() {
    return group_->buffer_manager();
  }

  RenderbufferManager* renderbuffer_manager() {
    return group_->renderbuffer_manager();
  }

  FramebufferManager* framebuffer_manager() {
    return group_->framebuffer_manager();
  }

  ProgramManager* program_manager() {
    return group_->program_manager();
  }

  SamplerManager* sampler_manager() {
    return group_->sampler_manager();
  }

  ShaderManager* shader_manager() {
    return group_->shader_manager();
  }

  ShaderTranslatorCache* shader_translator_cache() {
    return group_->shader_translator_cache();
  }

  const TextureManager* texture_manager() const {
    return group_->texture_manager();
  }

  TextureManager* texture_manager() {
    return group_->texture_manager();
  }

  MailboxManager* mailbox_manager() {
    return group_->mailbox_manager();
  }

  ImageManager* image_manager() { return image_manager_.get(); }

  VertexArrayManager* vertex_array_manager() {
    return vertex_array_manager_.get();
  }

  MemoryTracker* memory_tracker() {
    return group_->memory_tracker();
  }

  const gl::GLVersionInfo& gl_version_info() {
    return feature_info_->gl_version_info();
  }

  bool EnsureGPUMemoryAvailable(size_t estimated_size) {
    MemoryTracker* tracker = memory_tracker();
    if (tracker) {
      return tracker->EnsureGPUMemoryAvailable(estimated_size);
    }
    return true;
  }

  bool IsOffscreenBufferMultisampled() const {
    return offscreen_target_samples_ > 1;
  }

  // Creates a Texture for the given texture.
  TextureRef* CreateTexture(
      GLuint client_id, GLuint service_id) {
    return texture_manager()->CreateTexture(client_id, service_id);
  }

  // Gets the texture info for the given texture. Returns NULL if none exists.
  TextureRef* GetTexture(GLuint client_id) const {
    return texture_manager()->GetTexture(client_id);
  }

  // Deletes the texture info for the given texture.
  void RemoveTexture(GLuint client_id) {
    texture_manager()->RemoveTexture(client_id);
  }

  // Creates a Sampler for the given sampler.
  Sampler* CreateSampler(
      GLuint client_id, GLuint service_id) {
    return sampler_manager()->CreateSampler(client_id, service_id);
  }

  // Gets the sampler info for the given sampler. Returns NULL if none exists.
  Sampler* GetSampler(GLuint client_id) {
    return sampler_manager()->GetSampler(client_id);
  }

  // Deletes the sampler info for the given sampler.
  void RemoveSampler(GLuint client_id) {
    sampler_manager()->RemoveSampler(client_id);
  }

  // Creates a TransformFeedback for the given transformfeedback.
  TransformFeedback* CreateTransformFeedback(
      GLuint client_id, GLuint service_id) {
    return transform_feedback_manager_->CreateTransformFeedback(
        client_id, service_id);
  }

  // Gets the TransformFeedback info for the given transformfeedback.
  // Returns nullptr if none exists.
  TransformFeedback* GetTransformFeedback(GLuint client_id) {
    return transform_feedback_manager_->GetTransformFeedback(client_id);
  }

  // Deletes the TransformFeedback info for the given transformfeedback.
  void RemoveTransformFeedback(GLuint client_id) {
    transform_feedback_manager_->RemoveTransformFeedback(client_id);
  }

  // Get the size (in pixels) of the currently bound frame buffer (either FBO
  // or regular back buffer).
  gfx::Size GetBoundReadFramebufferSize();

  // Get the service side ID for the bound read framebuffer.
  // If it's back buffer, 0 is returned.
  GLuint GetBoundReadFramebufferServiceId();

  // Get the service side ID for the bound draw framebuffer.
  // If it's back buffer, 0 is returned.
  GLuint GetBoundDrawFramebufferServiceId();

  // Get the format/type of the currently bound frame buffer (either FBO or
  // regular back buffer).
  // If the color image is a renderbuffer, returns 0 for type.
  GLenum GetBoundReadFramebufferTextureType();
  GLenum GetBoundReadFramebufferInternalFormat();

  // Get the i-th draw buffer's internal format/type from the bound framebuffer.
  // If no framebuffer is bound, or no image is attached, or the DrawBuffers
  // setting for that image is GL_NONE, return 0.
  GLenum GetBoundColorDrawBufferType(GLint drawbuffer_i);
  GLenum GetBoundColorDrawBufferInternalFormat(GLint drawbuffer_i);

  GLsizei GetBoundFramebufferSamples(GLenum target);

  // Return 0 if no depth attachment.
  GLenum GetBoundFramebufferDepthFormat(GLenum target);
  // Return 0 if no stencil attachment.
  GLenum GetBoundFramebufferStencilFormat(GLenum target);

  void MarkDrawBufferAsCleared(GLenum buffer, GLint drawbuffer_i);

  // Wrapper for CompressedTexImage{2|3}D commands.
  error::Error DoCompressedTexImage(
      GLenum target,
      GLint level,
      GLenum internal_format,
      GLsizei width,
      GLsizei height,
      GLsizei depth,
      GLint border,
      GLsizei image_size,
      const void* data,
      ContextState::Dimension dimension);

  // Wrapper for CompressedTexSubImage{2|3}D.
  error::Error DoCompressedTexSubImage(
      GLenum target,
      GLint level,
      GLint xoffset,
      GLint yoffset,
      GLint zoffset,
      GLsizei width,
      GLsizei height,
      GLsizei depth,
      GLenum format,
      GLsizei imageSize,
      const void* data,
      ContextState::Dimension dimension);

  // Validate if |format| is valid for CopyTex{Sub}Image functions.
  // If not, generate a GL error and return false.
  bool ValidateCopyTexFormat(const char* func_name, GLenum internal_format,
                             GLenum read_format, GLenum read_type);

  // Wrapper for CopyTexImage2D.
  void DoCopyTexImage2D(
      GLenum target,
      GLint level,
      GLenum internal_format,
      GLint x,
      GLint y,
      GLsizei width,
      GLsizei height,
      GLint border);

  // Wrapper for SwapBuffers.
  void DoSwapBuffers();

  // Callback for async SwapBuffers.
  void FinishSwapBuffers(gfx::SwapResult result);

  void DoCommitOverlayPlanes();

  // Wrapper for SwapInterval.
  void DoSwapInterval(int interval);

  // Wrapper for CopyTexSubImage2D.
  void DoCopyTexSubImage2D(
      GLenum target,
      GLint level,
      GLint xoffset,
      GLint yoffset,
      GLint x,
      GLint y,
      GLsizei width,
      GLsizei height);

  // Wrapper for CopyTexSubImage3D.
  void DoCopyTexSubImage3D(
      GLenum target,
      GLint level,
      GLint xoffset,
      GLint yoffset,
      GLint zoffset,
      GLint x,
      GLint y,
      GLsizei width,
      GLsizei height);

  // Wrapper for glCopyBufferSubData.
  void DoCopyBufferSubData(GLenum readtarget,
                           GLenum writetarget,
                           GLintptr readoffset,
                           GLintptr writeoffset,
                           GLsizeiptr size);

  void DoCopyTextureCHROMIUM(GLuint source_id,
                             GLuint dest_id,
                             GLenum internal_format,
                             GLenum dest_type,
                             GLboolean unpack_flip_y,
                             GLboolean unpack_premultiply_alpha,
                             GLboolean unpack_unmultiply_alpha);

  void DoCopySubTextureCHROMIUM(GLuint source_id,
                                GLuint dest_id,
                                GLint xoffset,
                                GLint yoffset,
                                GLint x,
                                GLint y,
                                GLsizei width,
                                GLsizei height,
                                GLboolean unpack_flip_y,
                                GLboolean unpack_premultiply_alpha,
                                GLboolean unpack_unmultiply_alpha);

  void DoCompressedCopyTextureCHROMIUM(GLuint source_id, GLuint dest_id);

  // Helper for DoTexStorage2DEXT and DoTexStorage3D.
  void TexStorageImpl(GLenum target,
                      GLsizei levels,
                      GLenum internal_format,
                      GLsizei width,
                      GLsizei height,
                      GLsizei depth,
                      ContextState::Dimension dimension,
                      const char* function_name);

  // Wrapper for TexStorage2DEXT.
  void DoTexStorage2DEXT(GLenum target,
                         GLsizei levels,
                         GLenum internal_format,
                         GLsizei width,
                         GLsizei height);

  // Wrapper for TexStorage3D.
  void DoTexStorage3D(GLenum target,
                      GLsizei levels,
                      GLenum internal_format,
                      GLsizei width,
                      GLsizei height,
                      GLsizei depth);

  void DoProduceTextureCHROMIUM(GLenum target, const volatile GLbyte* key);
  void DoProduceTextureDirectCHROMIUM(GLuint texture,
                                      GLenum target,
                                      const volatile GLbyte* key);
  void ProduceTextureRef(const char* func_name,
                         bool clear,
                         TextureRef* texture_ref,
                         GLenum target,
                         const volatile GLbyte* data);

  void EnsureTextureForClientId(GLenum target, GLuint client_id);
  void DoConsumeTextureCHROMIUM(GLenum target, const volatile GLbyte* key);
  void DoCreateAndConsumeTextureINTERNAL(GLenum target,
                                         GLuint client_id,
                                         const volatile GLbyte* key);
  void DoApplyScreenSpaceAntialiasingCHROMIUM();

  void DoBindTexImage2DCHROMIUM(
      GLenum target,
      GLint image_id);
  void DoReleaseTexImage2DCHROMIUM(
      GLenum target,
      GLint image_id);

  void DoTraceEndCHROMIUM(void);

  void DoDrawBuffersEXT(GLsizei count, const volatile GLenum* bufs);

  void DoLoseContextCHROMIUM(GLenum current, GLenum other);

  void DoFlushDriverCachesCHROMIUM(void);

  void DoMatrixLoadfCHROMIUM(GLenum matrix_mode,
                             const volatile GLfloat* matrix);
  void DoMatrixLoadIdentityCHROMIUM(GLenum matrix_mode);
  void DoScheduleCALayerInUseQueryCHROMIUM(GLsizei count,
                                           const volatile GLuint* textures);

  void DoFlushMappedBufferRange(
      GLenum target, GLintptr offset, GLsizeiptr size);

  // Creates a Program for the given program.
  Program* CreateProgram(GLuint client_id, GLuint service_id) {
    return program_manager()->CreateProgram(client_id, service_id);
  }

  // Gets the program info for the given program. Returns NULL if none exists.
  Program* GetProgram(GLuint client_id) {
    return program_manager()->GetProgram(client_id);
  }

#if defined(NDEBUG)
  void LogClientServiceMapping(
      const char* /* function_name */,
      GLuint /* client_id */,
      GLuint /* service_id */) {
  }
  template<typename T>
  void LogClientServiceForInfo(
      T* /* info */, GLuint /* client_id */, const char* /* function_name */) {
  }
#else
  void LogClientServiceMapping(
      const char* function_name, GLuint client_id, GLuint service_id) {
    if (service_logging_) {
      VLOG(1) << "[" << logger_.GetLogPrefix() << "] " << function_name
              << ": client_id = " << client_id
              << ", service_id = " << service_id;
    }
  }
  template<typename T>
  void LogClientServiceForInfo(
      T* info, GLuint client_id, const char* function_name) {
    if (info) {
      LogClientServiceMapping(function_name, client_id, info->service_id());
    }
  }
#endif

  // Gets the program info for the given program. If it's not a program
  // generates a GL error. Returns NULL if not program.
  Program* GetProgramInfoNotShader(
      GLuint client_id, const char* function_name) {
    Program* program = GetProgram(client_id);
    if (!program) {
      if (GetShader(client_id)) {
        LOCAL_SET_GL_ERROR(
            GL_INVALID_OPERATION, function_name, "shader passed for program");
      } else {
        LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "unknown program");
      }
    }
    LogClientServiceForInfo(program, client_id, function_name);
    return program;
  }


  // Creates a Shader for the given shader.
  Shader* CreateShader(
      GLuint client_id,
      GLuint service_id,
      GLenum shader_type) {
    return shader_manager()->CreateShader(
        client_id, service_id, shader_type);
  }

  // Gets the shader info for the given shader. Returns NULL if none exists.
  Shader* GetShader(GLuint client_id) {
    return shader_manager()->GetShader(client_id);
  }

  // Gets the shader info for the given shader. If it's not a shader generates a
  // GL error. Returns NULL if not shader.
  Shader* GetShaderInfoNotProgram(
      GLuint client_id, const char* function_name) {
    Shader* shader = GetShader(client_id);
    if (!shader) {
      if (GetProgram(client_id)) {
        LOCAL_SET_GL_ERROR(
            GL_INVALID_OPERATION, function_name, "program passed for shader");
      } else {
        LOCAL_SET_GL_ERROR(
            GL_INVALID_VALUE, function_name, "unknown shader");
      }
    }
    LogClientServiceForInfo(shader, client_id, function_name);
    return shader;
  }

  // Creates a buffer info for the given buffer.
  void CreateBuffer(GLuint client_id, GLuint service_id) {
    return buffer_manager()->CreateBuffer(client_id, service_id);
  }

  // Gets the buffer info for the given buffer.
  Buffer* GetBuffer(GLuint client_id) {
    Buffer* buffer = buffer_manager()->GetBuffer(client_id);
    return buffer;
  }

  // Removes any buffers in the VertexAtrribInfos and BufferInfos. This is used
  // on glDeleteBuffers so we can make sure the user does not try to render
  // with deleted buffers.
  void RemoveBuffer(GLuint client_id);

  // Creates a framebuffer info for the given framebuffer.
  void CreateFramebuffer(GLuint client_id, GLuint service_id) {
    return framebuffer_manager()->CreateFramebuffer(client_id, service_id);
  }

  // Gets the framebuffer info for the given framebuffer.
  Framebuffer* GetFramebuffer(GLuint client_id) {
    return framebuffer_manager()->GetFramebuffer(client_id);
  }

  // Removes the framebuffer info for the given framebuffer.
  void RemoveFramebuffer(GLuint client_id) {
    framebuffer_manager()->RemoveFramebuffer(client_id);
  }

  // Creates a renderbuffer info for the given renderbuffer.
  void CreateRenderbuffer(GLuint client_id, GLuint service_id) {
    return renderbuffer_manager()->CreateRenderbuffer(
        client_id, service_id);
  }

  // Gets the renderbuffer info for the given renderbuffer.
  Renderbuffer* GetRenderbuffer(GLuint client_id) {
    return renderbuffer_manager()->GetRenderbuffer(client_id);
  }

  // Removes the renderbuffer info for the given renderbuffer.
  void RemoveRenderbuffer(GLuint client_id) {
    renderbuffer_manager()->RemoveRenderbuffer(client_id);
  }

  // Gets the vertex attrib manager for the given vertex array.
  VertexAttribManager* GetVertexAttribManager(GLuint client_id) {
    VertexAttribManager* info =
        vertex_array_manager()->GetVertexAttribManager(client_id);
    return info;
  }

  // Removes the vertex attrib manager for the given vertex array.
  void RemoveVertexAttribManager(GLuint client_id) {
    vertex_array_manager()->RemoveVertexAttribManager(client_id);
  }

  // Creates a vertex attrib manager for the given vertex array.
  scoped_refptr<VertexAttribManager> CreateVertexAttribManager(
      GLuint client_id,
      GLuint service_id,
      bool client_visible) {
    return vertex_array_manager()->CreateVertexAttribManager(
        client_id, service_id, group_->max_vertex_attribs(), client_visible);
  }

  void DoBindAttribLocation(GLuint client_id,
                            GLuint index,
                            const std::string& name);

  error::Error DoBindFragDataLocation(GLuint program_id,
                                      GLuint colorName,
                                      const std::string& name);

  error::Error DoBindFragDataLocationIndexed(GLuint program_id,
                                             GLuint colorName,
                                             GLuint index,
                                             const std::string& name);

  void DoBindUniformLocationCHROMIUM(GLuint client_id,
                                     GLint location,
                                     const std::string& name);

  error::Error GetAttribLocationHelper(GLuint client_id,
                                       uint32_t location_shm_id,
                                       uint32_t location_shm_offset,
                                       const std::string& name_str);

  error::Error GetUniformLocationHelper(GLuint client_id,
                                        uint32_t location_shm_id,
                                        uint32_t location_shm_offset,
                                        const std::string& name_str);

  error::Error GetFragDataLocationHelper(GLuint client_id,
                                         uint32_t location_shm_id,
                                         uint32_t location_shm_offset,
                                         const std::string& name_str);

  error::Error GetFragDataIndexHelper(GLuint program_id,
                                      uint32_t index_shm_id,
                                      uint32_t index_shm_offset,
                                      const std::string& name_str);

  // Wrapper for glShaderSource.
  void DoShaderSource(
      GLuint client_id, GLsizei count, const char** data, const GLint* length);

  // Wrapper for glTransformFeedbackVaryings.
  void DoTransformFeedbackVaryings(
      GLuint client_program_id, GLsizei count, const char* const* varyings,
      GLenum buffer_mode);

  // Clear any textures used by the current program.
  bool ClearUnclearedTextures();

  // Clears any uncleared attachments attached to the given frame buffer.
  // Returns false if there was a generated GL error.
  void ClearUnclearedAttachments(GLenum target, Framebuffer* framebuffer);

  // overridden from GLES2Decoder
  bool ClearLevel(Texture* texture,
                  unsigned target,
                  int level,
                  unsigned format,
                  unsigned type,
                  int xoffset,
                  int yoffset,
                  int width,
                  int height) override;

  // overridden from GLES2Decoder
  bool ClearCompressedTextureLevel(Texture* texture,
                                   unsigned target,
                                   int level,
                                   unsigned format,
                                   int width,
                                   int height) override;
  bool IsCompressedTextureFormat(unsigned format) override;

  // overridden from GLES2Decoder
  bool ClearLevel3D(Texture* texture,
                    unsigned target,
                    int level,
                    unsigned format,
                    unsigned type,
                    int width,
                    int height,
                    int depth) override;

  // Restore all GL state that affects clearing.
  void RestoreClearState();

  // Remembers the state of some capabilities.
  // Returns: true if glEnable/glDisable should actually be called.
  bool SetCapabilityState(GLenum cap, bool enabled);

  // Infer color encoding from internalformat
  static GLint GetColorEncodingFromInternalFormat(GLenum internalformat);

  // Check that the currently bound read framebuffer's color image
  // isn't the target texture of the glCopyTex{Sub}Image{2D|3D}.
  bool FormsTextureCopyingFeedbackLoop(
      TextureRef* texture,
      GLint level,
      GLint layer);

  // Check if a framebuffer meets our requirements.
  // Generates |gl_error| if the framebuffer is incomplete.
  bool CheckFramebufferValid(
      Framebuffer* framebuffer,
      GLenum target,
      GLenum gl_error,
      const char* func_name);

  bool CheckBoundDrawFramebufferValid(const char* func_name);
  // Generates |gl_error| if the bound read fbo is incomplete.
  bool CheckBoundReadFramebufferValid(const char* func_name, GLenum gl_error);
  // This is only used by DoBlitFramebufferCHROMIUM which operates read/draw
  // framebuffer at the same time.
  bool CheckBoundFramebufferValid(const char* func_name);

  // Checks if the current program exists and is valid. If not generates the
  // appropriate GL error.  Returns true if the current program is in a usable
  // state.
  bool CheckCurrentProgram(const char* function_name);

  // Checks if the current program exists and is valid and that location is not
  // -1. If the current program is not valid generates the appropriate GL
  // error. Returns true if the current program is in a usable state and
  // location is not -1.
  bool CheckCurrentProgramForUniform(GLint location, const char* function_name);

  // Checks if the current program samples a texture that is also the color
  // image of the current bound framebuffer, i.e., the source and destination
  // of the draw operation are the same.
  bool CheckDrawingFeedbackLoops();

  bool SupportsDrawBuffers() const;

  // Checks if a draw buffer's format and its corresponding fragment shader
  // output's type are compatible, i.e., a signed integer typed variable is
  // incompatible with a float or unsigned integer buffer.
  // If incompaticle, generates an INVALID_OPERATION to avoid undefined buffer
  // contents and return false.
  // Otherwise, filter out the draw buffers that are not written to but are not
  // NONE through DrawBuffers, to be on the safe side. Return true.
  bool ValidateAndAdjustDrawBuffers(const char* function_name);

  // Checks if all active uniform blocks in the current program are backed by
  // a buffer of sufficient size.
  // If not, generates an INVALID_OPERATION to avoid undefined behavior in
  // shader execution and return false.
  bool ValidateUniformBlockBackings(const char* function_name);

  // Checks if |api_type| is valid for the given uniform
  // If the api type is not valid generates the appropriate GL
  // error. Returns true if |api_type| is valid for the uniform
  bool CheckUniformForApiType(const Program::UniformInfo* info,
                              const char* function_name,
                              Program::UniformApiType api_type);

  // Gets the type of a uniform for a location in the current program. Sets GL
  // errors if the current program is not valid. Returns true if the current
  // program is valid and the location exists. Adjusts count so it
  // does not overflow the uniform.
  bool PrepForSetUniformByLocation(GLint fake_location,
                                   const char* function_name,
                                   Program::UniformApiType api_type,
                                   GLint* real_location,
                                   GLenum* type,
                                   GLsizei* count);

  // Gets the service id for any simulated backbuffer fbo.
  GLuint GetBackbufferServiceId() const;

  // Helper for glGetBooleanv, glGetFloatv and glGetIntegerv.  Returns
  // false if pname is unhandled.
  bool GetHelper(GLenum pname, GLint* params, GLsizei* num_written);

  // Helper for glGetVertexAttrib
  void GetVertexAttribHelper(
    const VertexAttrib* attrib, GLenum pname, GLint* param);

  // Wrapper for glActiveTexture
  void DoActiveTexture(GLenum texture_unit);

  // Wrapper for glAttachShader
  void DoAttachShader(GLuint client_program_id, GLint client_shader_id);

  // Wrapper for glBindBuffer since we need to track the current targets.
  void DoBindBuffer(GLenum target, GLuint buffer);

  // Wrapper for glBindBufferBase since we need to track the current targets.
  void DoBindBufferBase(GLenum target, GLuint index, GLuint buffer);

  // Wrapper for glBindBufferRange since we need to track the current targets.
  void DoBindBufferRange(GLenum target, GLuint index, GLuint buffer,
      GLintptr offset, GLsizeiptr size);

  // Helper for DoBindBufferBase and DoBindBufferRange.
  void BindIndexedBufferImpl(GLenum target, GLuint index, GLuint buffer,
                             GLintptr offset, GLsizeiptr size,
                             BindIndexedBufferFunctionType function_type,
                             const char* function_name);

  // Wrapper for glBindFramebuffer since we need to track the current targets.
  void DoBindFramebuffer(GLenum target, GLuint framebuffer);

  // Wrapper for glBindRenderbuffer since we need to track the current targets.
  void DoBindRenderbuffer(GLenum target, GLuint renderbuffer);

  // Wrapper for glBindTexture since we need to track the current targets.
  void DoBindTexture(GLenum target, GLuint texture);

  // Wrapper for glBindSampler since we need to track the current targets.
  void DoBindSampler(GLuint unit, GLuint sampler);

  // Wrapper for glBindTransformFeedback since we need to emulate ES3 behaviors
  // for BindBufferRange on Desktop GL lower than 4.2.
  void DoBindTransformFeedback(GLenum target, GLuint transform_feedback);

  // Wrapper for glBeginTransformFeedback.
  void DoBeginTransformFeedback(GLenum primitive_mode);

  // Wrapper for glEndTransformFeedback.
  void DoEndTransformFeedback();

  // Wrapper for glPauseTransformFeedback.
  void DoPauseTransformFeedback();

  // Wrapper for glResumeTransformFeedback.
  void DoResumeTransformFeedback();

  // Wrapper for glBindVertexArrayOES
  void DoBindVertexArrayOES(GLuint array);
  void EmulateVertexArrayState();

  // Wrapper for glBlitFramebufferCHROMIUM.
  void DoBlitFramebufferCHROMIUM(
      GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
      GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
      GLbitfield mask, GLenum filter);

  // Wrapper for glBufferSubData.
  void DoBufferSubData(
    GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid * data);

  // Wrapper for glCheckFramebufferStatus
  GLenum DoCheckFramebufferStatus(GLenum target);

  // Wrapper for glClear*()
  error::Error DoClear(GLbitfield mask);
  void DoClearBufferiv(GLenum buffer,
                       GLint drawbuffer,
                       const volatile GLint* value);
  void DoClearBufferuiv(GLenum buffer,
                        GLint drawbuffer,
                        const volatile GLuint* value);
  void DoClearBufferfv(GLenum buffer,
                       GLint drawbuffer,
                       const volatile GLfloat* value);
  void DoClearBufferfi(
      GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil);

  // Wrappers for various state.
  void DoDepthRangef(GLclampf znear, GLclampf zfar);
  void DoSampleCoverage(GLclampf value, GLboolean invert);

  // Wrapper for glCompileShader.
  void DoCompileShader(GLuint shader);

  // Wrapper for glDetachShader
  void DoDetachShader(GLuint client_program_id, GLint client_shader_id);

  // Wrapper for glDisable
  void DoDisable(GLenum cap);

  // Wrapper for glDisableVertexAttribArray.
  void DoDisableVertexAttribArray(GLuint index);

  // Wrapper for glDiscardFramebufferEXT, since we need to track undefined
  // attachments.
  void DoDiscardFramebufferEXT(GLenum target,
                               GLsizei count,
                               const volatile GLenum* attachments);

  void DoInvalidateFramebuffer(GLenum target,
                               GLsizei count,
                               const volatile GLenum* attachments);
  void DoInvalidateSubFramebuffer(GLenum target,
                                  GLsizei count,
                                  const volatile GLenum* attachments,
                                  GLint x,
                                  GLint y,
                                  GLsizei width,
                                  GLsizei height);

  // Helper for DoDiscardFramebufferEXT, DoInvalidate{Sub}Framebuffer.
  void InvalidateFramebufferImpl(GLenum target,
                                 GLsizei count,
                                 const volatile GLenum* attachments,
                                 GLint x,
                                 GLint y,
                                 GLsizei width,
                                 GLsizei height,
                                 const char* function_name,
                                 FramebufferOperation op);

  // Wrapper for glEnable
  void DoEnable(GLenum cap);

  // Wrapper for glEnableVertexAttribArray.
  void DoEnableVertexAttribArray(GLuint index);

  // Wrapper for glFinish.
  void DoFinish();

  // Wrapper for glFlush.
  void DoFlush();

  // Wrapper for glFramebufferRenderbufffer.
  void DoFramebufferRenderbuffer(
      GLenum target, GLenum attachment, GLenum renderbuffertarget,
      GLuint renderbuffer);

  // Wrapper for glFramebufferTexture2D.
  void DoFramebufferTexture2D(
      GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
      GLint level);

  // Wrapper for glFramebufferTexture2DMultisampleEXT.
  void DoFramebufferTexture2DMultisample(
      GLenum target, GLenum attachment, GLenum textarget,
      GLuint texture, GLint level, GLsizei samples);

  // Common implementation for both DoFramebufferTexture2D wrappers.
  void DoFramebufferTexture2DCommon(const char* name,
      GLenum target, GLenum attachment, GLenum textarget,
      GLuint texture, GLint level, GLsizei samples);

  // Wrapper for glFramebufferTextureLayer.
  void DoFramebufferTextureLayer(
      GLenum target, GLenum attachment, GLuint texture, GLint level,
      GLint layer);

  // Wrapper for glGenerateMipmap
  void DoGenerateMipmap(GLenum target);

  // Helper for DoGetBooleanv, Floatv, and Intergerv to adjust pname
  // to account for different pname values defined in different extension
  // variants.
  GLenum AdjustGetPname(GLenum pname);

  // Wrapper for DoGetBooleanv.
  void DoGetBooleanv(GLenum pname, GLboolean* params, GLsizei params_size);

  // Wrapper for DoGetFloatv.
  void DoGetFloatv(GLenum pname, GLfloat* params, GLsizei params_size);

  // Wrapper for glGetFramebufferAttachmentParameteriv.
  void DoGetFramebufferAttachmentParameteriv(GLenum target,
                                             GLenum attachment,
                                             GLenum pname,
                                             GLint* params,
                                             GLsizei params_size);

  // Wrapper for glGetInteger64v.
  void DoGetInteger64v(GLenum pname, GLint64* params, GLsizei params_size);

  // Wrapper for glGetIntegerv.
  void DoGetIntegerv(GLenum pname, GLint* params, GLsizei params_size);

  // Helper for DoGetIntegeri_v and DoGetInteger64i_v.
  template <typename TYPE>
  void GetIndexedIntegerImpl(
      const char* function_name, GLenum target, GLuint index, TYPE* data);

  // Wrapper for glGetIntegeri_v.
  void DoGetIntegeri_v(GLenum target,
                       GLuint index,
                       GLint* params,
                       GLsizei params_size);

  // Wrapper for glGetInteger64i_v.
  void DoGetInteger64i_v(GLenum target,
                         GLuint index,
                         GLint64* params,
                         GLsizei params_size);

  // Gets the max value in a range in a buffer.
  GLuint DoGetMaxValueInBufferCHROMIUM(
      GLuint buffer_id, GLsizei count, GLenum type, GLuint offset);

  // Wrapper for glGetBufferParameteri64v.
  void DoGetBufferParameteri64v(GLenum target,
                                GLenum pname,
                                GLint64* params,
                                GLsizei params_size);

  // Wrapper for glGetBufferParameteriv.
  void DoGetBufferParameteriv(GLenum target,
                              GLenum pname,
                              GLint* params,
                              GLsizei params_size);

  // Wrapper for glGetProgramiv.
  void DoGetProgramiv(GLuint program_id,
                      GLenum pname,
                      GLint* params,
                      GLsizei params_size);

  // Wrapper for glRenderbufferParameteriv.
  void DoGetRenderbufferParameteriv(GLenum target,
                                    GLenum pname,
                                    GLint* params,
                                    GLsizei params_size);

  // Wrappers for glGetSamplerParameter.
  void DoGetSamplerParameterfv(GLuint client_id,
                               GLenum pname,
                               GLfloat* params,
                               GLsizei params_size);
  void DoGetSamplerParameteriv(GLuint client_id,
                               GLenum pname,
                               GLint* params,
                               GLsizei params_size);

  // Wrapper for glGetShaderiv
  void DoGetShaderiv(GLuint shader,
                     GLenum pname,
                     GLint* params,
                     GLsizei params_size);

  // Helper for DoGetTexParameter{f|i}v.
  void GetTexParameterImpl(
      GLenum target, GLenum pname, GLfloat* fparams, GLint* iparams,
      const char* function_name);

  // Wrappers for glGetTexParameter.
  void DoGetTexParameterfv(GLenum target,
                           GLenum pname,
                           GLfloat* params,
                           GLsizei params_size);
  void DoGetTexParameteriv(GLenum target,
                           GLenum pname,
                           GLint* params,
                           GLsizei params_size);

  // Wrappers for glGetVertexAttrib.
  template <typename T>
  void DoGetVertexAttribImpl(GLuint index, GLenum pname, T* params);
  void DoGetVertexAttribfv(GLuint index,
                           GLenum pname,
                           GLfloat* params,
                           GLsizei params_size);
  void DoGetVertexAttribiv(GLuint index,
                           GLenum pname,
                           GLint* params,
                           GLsizei params_size);
  void DoGetVertexAttribIiv(GLuint index,
                            GLenum pname,
                            GLint* params,
                            GLsizei params_size);
  void DoGetVertexAttribIuiv(GLuint index,
                             GLenum pname,
                             GLuint* params,
                             GLsizei params_size);

  // Wrappers for glIsXXX functions.
  bool DoIsEnabled(GLenum cap);
  bool DoIsBuffer(GLuint client_id);
  bool DoIsFramebuffer(GLuint client_id);
  bool DoIsProgram(GLuint client_id);
  bool DoIsRenderbuffer(GLuint client_id);
  bool DoIsShader(GLuint client_id);
  bool DoIsTexture(GLuint client_id);
  bool DoIsSampler(GLuint client_id);
  bool DoIsTransformFeedback(GLuint client_id);
  bool DoIsVertexArrayOES(GLuint client_id);
  bool DoIsPathCHROMIUM(GLuint client_id);
  bool DoIsSync(GLuint client_id);

  void DoLineWidth(GLfloat width);

  // Wrapper for glLinkProgram
  void DoLinkProgram(GLuint program);

  // Wrapper for glReadBuffer
  void DoReadBuffer(GLenum src);

  // Wrapper for glRenderbufferStorage.
  void DoRenderbufferStorage(
      GLenum target, GLenum internalformat, GLsizei width, GLsizei height);

  // Handler for glRenderbufferStorageMultisampleCHROMIUM.
  void DoRenderbufferStorageMultisampleCHROMIUM(
      GLenum target, GLsizei samples, GLenum internalformat,
      GLsizei width, GLsizei height);

  // Handler for glRenderbufferStorageMultisampleEXT
  // (multisampled_render_to_texture).
  void DoRenderbufferStorageMultisampleEXT(
      GLenum target, GLsizei samples, GLenum internalformat,
      GLsizei width, GLsizei height);

  // Wrapper for glFenceSync.
  GLsync DoFenceSync(GLenum condition, GLbitfield flags);

  // Common validation for multisample extensions.
  bool ValidateRenderbufferStorageMultisample(GLsizei samples,
                                              GLenum internalformat,
                                              GLsizei width,
                                              GLsizei height);

  // Verifies that the currently bound multisample renderbuffer is valid
  // Very slow! Only done on platforms with driver bugs that return invalid
  // buffers under memory pressure
  bool VerifyMultisampleRenderbufferIntegrity(
      GLuint renderbuffer, GLenum format);

  // Wrapper for glReleaseShaderCompiler.
  void DoReleaseShaderCompiler() { }

  // Wrappers for glSamplerParameter functions.
  void DoSamplerParameterf(GLuint client_id, GLenum pname, GLfloat param);
  void DoSamplerParameteri(GLuint client_id, GLenum pname, GLint param);
  void DoSamplerParameterfv(GLuint client_id,
                            GLenum pname,
                            const volatile GLfloat* params);
  void DoSamplerParameteriv(GLuint client_id,
                            GLenum pname,
                            const volatile GLint* params);

  // Wrappers for glTexParameter functions.
  void DoTexParameterf(GLenum target, GLenum pname, GLfloat param);
  void DoTexParameteri(GLenum target, GLenum pname, GLint param);
  void DoTexParameterfv(GLenum target,
                        GLenum pname,
                        const volatile GLfloat* params);
  void DoTexParameteriv(GLenum target,
                        GLenum pname,
                        const volatile GLint* params);

  // Wrappers for glUniform1i and glUniform1iv as according to the GLES2
  // spec only these 2 functions can be used to set sampler uniforms.
  void DoUniform1i(GLint fake_location, GLint v0);
  void DoUniform1iv(GLint fake_location,
                    GLsizei count,
                    const volatile GLint* value);
  void DoUniform2iv(GLint fake_location,
                    GLsizei count,
                    const volatile GLint* value);
  void DoUniform3iv(GLint fake_location,
                    GLsizei count,
                    const volatile GLint* value);
  void DoUniform4iv(GLint fake_location,
                    GLsizei count,
                    const volatile GLint* value);

  void DoUniform1ui(GLint fake_location, GLuint v0);
  void DoUniform1uiv(GLint fake_location,
                     GLsizei count,
                     const volatile GLuint* value);
  void DoUniform2uiv(GLint fake_location,
                     GLsizei count,
                     const volatile GLuint* value);
  void DoUniform3uiv(GLint fake_location,
                     GLsizei count,
                     const volatile GLuint* value);
  void DoUniform4uiv(GLint fake_location,
                     GLsizei count,
                     const volatile GLuint* value);

  // Wrappers for glUniformfv because some drivers don't correctly accept
  // bool uniforms.
  void DoUniform1fv(GLint fake_location,
                    GLsizei count,
                    const volatile GLfloat* value);
  void DoUniform2fv(GLint fake_location,
                    GLsizei count,
                    const volatile GLfloat* value);
  void DoUniform3fv(GLint fake_location,
                    GLsizei count,
                    const volatile GLfloat* value);
  void DoUniform4fv(GLint fake_location,
                    GLsizei count,
                    const volatile GLfloat* value);

  void DoUniformMatrix2fv(GLint fake_location,
                          GLsizei count,
                          GLboolean transpose,
                          const volatile GLfloat* value);
  void DoUniformMatrix3fv(GLint fake_location,
                          GLsizei count,
                          GLboolean transpose,
                          const volatile GLfloat* value);
  void DoUniformMatrix4fv(GLint fake_location,
                          GLsizei count,
                          GLboolean transpose,
                          const volatile GLfloat* value);
  void DoUniformMatrix4fvStreamTextureMatrixCHROMIUM(
      GLint fake_location,
      GLboolean transpose,
      const volatile GLfloat* default_value);
  void DoUniformMatrix2x3fv(GLint fake_location,
                            GLsizei count,
                            GLboolean transpose,
                            const volatile GLfloat* value);
  void DoUniformMatrix2x4fv(GLint fake_location,
                            GLsizei count,
                            GLboolean transpose,
                            const volatile GLfloat* value);
  void DoUniformMatrix3x2fv(GLint fake_location,
                            GLsizei count,
                            GLboolean transpose,
                            const volatile GLfloat* value);
  void DoUniformMatrix3x4fv(GLint fake_location,
                            GLsizei count,
                            GLboolean transpose,
                            const volatile GLfloat* value);
  void DoUniformMatrix4x2fv(GLint fake_location,
                            GLsizei count,
                            GLboolean transpose,
                            const volatile GLfloat* value);
  void DoUniformMatrix4x3fv(GLint fake_location,
                            GLsizei count,
                            GLboolean transpose,
                            const volatile GLfloat* value);

  template <typename T>
  bool SetVertexAttribValue(
    const char* function_name, GLuint index, const T* value);

  // Wrappers for glVertexAttrib??
  void DoVertexAttrib1f(GLuint index, GLfloat v0);
  void DoVertexAttrib2f(GLuint index, GLfloat v0, GLfloat v1);
  void DoVertexAttrib3f(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2);
  void DoVertexAttrib4f(
      GLuint index, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
  void DoVertexAttrib1fv(GLuint index, const volatile GLfloat* v);
  void DoVertexAttrib2fv(GLuint index, const volatile GLfloat* v);
  void DoVertexAttrib3fv(GLuint index, const volatile GLfloat* v);
  void DoVertexAttrib4fv(GLuint index, const volatile GLfloat* v);
  void DoVertexAttribI4i(GLuint index, GLint v0, GLint v1, GLint v2, GLint v3);
  void DoVertexAttribI4iv(GLuint index, const volatile GLint* v);
  void DoVertexAttribI4ui(
      GLuint index, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
  void DoVertexAttribI4uiv(GLuint index, const volatile GLuint* v);

  // Wrapper for glViewport
  void DoViewport(GLint x, GLint y, GLsizei width, GLsizei height);

  // Wrapper for glUseProgram
  void DoUseProgram(GLuint program);

  // Wrapper for glValidateProgram.
  void DoValidateProgram(GLuint program_client_id);

  void DoInsertEventMarkerEXT(GLsizei length, const GLchar* marker);
  void DoPushGroupMarkerEXT(GLsizei length, const GLchar* group);
  void DoPopGroupMarkerEXT(void);

  // Gets the number of values that will be returned by glGetXXX. Returns
  // false if pname is unknown.
  bool GetNumValuesReturnedForGLGet(GLenum pname, GLsizei* num_values);

  // Checks if every attribute's type set by vertexAttrib API match
  // the type of corresponding attribute in vertex shader.
  bool AttribsTypeMatch();

  // Checks if the current program and vertex attributes are valid for drawing.
  bool IsDrawValid(
      const char* function_name, GLuint max_vertex_accessed, bool instanced,
      GLsizei primcount);

  // Returns true if successful, simulated will be true if attrib0 was
  // simulated.
  bool SimulateAttrib0(
      const char* function_name, GLuint max_vertex_accessed, bool* simulated);
  void RestoreStateForAttrib(GLuint attrib, bool restore_array_binding);

  // Copies the image to the texture currently bound to |textarget|. The image
  // state of |texture| is updated to reflect the new state.
  void DoCopyTexImage(Texture* texture, GLenum textarget, gl::GLImage* image);

  // This will call DoCopyTexImage if texture has an image but that image is
  // not bound or copied to the texture.
  void DoCopyTexImageIfNeeded(Texture* texture, GLenum textarget);

  // Returns false if textures were replaced.
  bool PrepareTexturesForRender();
  void RestoreStateForTextures();

  // Returns true if GL_FIXED attribs were simulated.
  bool SimulateFixedAttribs(
      const char* function_name,
      GLuint max_vertex_accessed, bool* simulated, GLsizei primcount);
  void RestoreStateForSimulatedFixedAttribs();

  // Handle DrawArrays and DrawElements for both instanced and non-instanced
  // cases (primcount is always 1 for non-instanced).
  error::Error DoDrawArrays(
      const char* function_name,
      bool instanced, GLenum mode, GLint first, GLsizei count,
      GLsizei primcount);
  error::Error DoDrawElements(const char* function_name,
                              bool instanced,
                              GLenum mode,
                              GLsizei count,
                              GLenum type,
                              int32_t offset,
                              GLsizei primcount);

  GLenum GetBindTargetForSamplerType(GLenum type) {
    switch (type) {
      case GL_SAMPLER_2D:
      case GL_SAMPLER_2D_SHADOW:
      case GL_INT_SAMPLER_2D:
      case GL_UNSIGNED_INT_SAMPLER_2D:
        return GL_TEXTURE_2D;
      case GL_SAMPLER_CUBE:
      case GL_SAMPLER_CUBE_SHADOW:
      case GL_INT_SAMPLER_CUBE:
      case GL_UNSIGNED_INT_SAMPLER_CUBE:
        return GL_TEXTURE_CUBE_MAP;
      case GL_SAMPLER_EXTERNAL_OES:
        return GL_TEXTURE_EXTERNAL_OES;
      case GL_SAMPLER_2D_RECT_ARB:
        return GL_TEXTURE_RECTANGLE_ARB;
      case GL_SAMPLER_3D:
      case GL_INT_SAMPLER_3D:
      case GL_UNSIGNED_INT_SAMPLER_3D:
        return GL_TEXTURE_3D;
      case GL_SAMPLER_2D_ARRAY:
      case GL_SAMPLER_2D_ARRAY_SHADOW:
      case GL_INT_SAMPLER_2D_ARRAY:
      case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
        return GL_TEXTURE_2D_ARRAY;
      default:
        NOTREACHED();
        return 0;
    }
  }

  // Gets the framebuffer info for a particular target.
  Framebuffer* GetFramebufferInfoForTarget(GLenum target) const {
    Framebuffer* framebuffer = nullptr;
    switch (target) {
      case GL_FRAMEBUFFER:
      case GL_DRAW_FRAMEBUFFER_EXT:
        framebuffer = framebuffer_state_.bound_draw_framebuffer.get();
        break;
      case GL_READ_FRAMEBUFFER_EXT:
        framebuffer = framebuffer_state_.bound_read_framebuffer.get();
        break;
      default:
        NOTREACHED();
        break;
    }
    return framebuffer;
  }

  Renderbuffer* GetRenderbufferInfoForTarget(
      GLenum target) {
    Renderbuffer* renderbuffer = NULL;
    switch (target) {
      case GL_RENDERBUFFER:
        renderbuffer = state_.bound_renderbuffer.get();
        break;
      default:
        NOTREACHED();
        break;
    }
    return renderbuffer;
  }

  // Validates the program and location for a glGetUniform call and returns
  // a SizeResult setup to receive the result. Returns true if glGetUniform
  // should be called.
  template <class T>
  bool GetUniformSetup(GLuint program,
                       GLint fake_location,
                       uint32_t shm_id,
                       uint32_t shm_offset,
                       error::Error* error,
                       GLint* real_location,
                       GLuint* service_id,
                       SizedResult<T>** result,
                       GLenum* result_type,
                       GLsizei* result_size);

  bool WasContextLost() const override;
  bool WasContextLostByRobustnessExtension() const override;
  void MarkContextLost(error::ContextLostReason reason) override;
  bool CheckResetStatus();

  bool GetCompressedTexSizeInBytes(
      const char* function_name, GLsizei width, GLsizei height, GLsizei depth,
      GLenum format, GLsizei* size_in_bytes);

  bool ValidateCompressedTexDimensions(
      const char* function_name, GLenum target, GLint level,
      GLsizei width, GLsizei height, GLsizei depth, GLenum format);
  bool ValidateCompressedTexFuncData(const char* function_name,
                                     GLsizei width,
                                     GLsizei height,
                                     GLsizei depth,
                                     GLenum format,
                                     GLsizei size,
                                     const GLvoid* data);
  bool ValidateCompressedTexSubDimensions(
    const char* function_name,
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
    GLsizei width, GLsizei height, GLsizei depth, GLenum format,
    Texture* texture);
  bool ValidateCopyTextureCHROMIUMTextures(const char* function_name,
                                           TextureRef* source_texture_ref,
                                           TextureRef* dest_texture_ref);
  bool ValidateCopyTextureCHROMIUMInternalFormats(
      const char* function_name,
      TextureRef* source_texture_ref,
      GLenum dest_internal_format);
  bool ValidateCompressedCopyTextureCHROMIUM(const char* function_name,
                                             TextureRef* source_texture_ref,
                                             TextureRef* dest_texture_ref);

  void RenderWarning(const char* filename, int line, const std::string& msg);
  void PerformanceWarning(
      const char* filename, int line, const std::string& msg);

  const FeatureInfo::FeatureFlags& features() const {
    return feature_info_->feature_flags();
  }

  const GpuDriverBugWorkarounds& workarounds() const {
    return feature_info_->workarounds();
  }

  bool ShouldDeferDraws() {
    return !offscreen_target_frame_buffer_.get() &&
           framebuffer_state_.bound_draw_framebuffer.get() == NULL &&
           surface_->DeferDraws();
  }

  bool ShouldDeferReads() {
    return !offscreen_target_frame_buffer_.get() &&
           framebuffer_state_.bound_read_framebuffer.get() == NULL &&
           surface_->DeferDraws();
  }

  bool IsRobustnessSupported() {
    return has_robustness_extension_ &&
           context_->WasAllocatedUsingRobustnessExtension();
  }

  error::Error WillAccessBoundFramebufferForDraw() {
    if (ShouldDeferDraws())
      return error::kDeferCommandUntilLater;
    if (!offscreen_target_frame_buffer_.get() &&
        !framebuffer_state_.bound_draw_framebuffer.get() &&
        !surface_->SetBackbufferAllocation(true))
      return error::kLostContext;
    return error::kNoError;
  }

  error::Error WillAccessBoundFramebufferForRead() {
    if (ShouldDeferReads())
      return error::kDeferCommandUntilLater;
    if (!offscreen_target_frame_buffer_.get() &&
        !framebuffer_state_.bound_read_framebuffer.get() &&
        !surface_->SetBackbufferAllocation(true))
      return error::kLostContext;
    return error::kNoError;
  }

  // Whether the back buffer exposed to the client has an alpha channel. Note
  // that this is potentially different from whether the implementation of the
  // back buffer has an alpha channel.
  bool ClientExposedBackBufferHasAlpha() const {
    if (back_buffer_draw_buffer_ == GL_NONE)
      return false;
    if (offscreen_target_frame_buffer_.get()) {
      return offscreen_buffer_should_have_alpha_;
    }
    return (back_buffer_color_format_ == GL_RGBA ||
            back_buffer_color_format_ == GL_RGBA8);
  }

  // If the back buffer has a non-emulated alpha channel, the clear color should
  // be 0. Otherwise, the clear color should be 1.
  GLfloat BackBufferAlphaClearColor() const {
    return offscreen_buffer_should_have_alpha_ ? 0.f : 1.f;
  }

  // Set remaining commands to process to 0 to force DoCommands to return
  // and allow context preemption and GPU watchdog checks in CommandExecutor().
  void ExitCommandProcessingEarly() { commands_to_process_ = 0; }

  void ProcessPendingReadPixels(bool did_finish);
  void FinishReadPixels(GLsizei width,
                        GLsizei height,
                        GLsizei format,
                        GLsizei type,
                        uint32_t pixels_shm_id,
                        uint32_t pixels_shm_offset,
                        uint32_t result_shm_id,
                        uint32_t result_shm_offset,
                        GLint pack_alignment,
                        GLenum read_format,
                        GLuint buffer);

  // Checks to see if the inserted fence has completed.
  void ProcessDescheduleUntilFinished();

  void DoBindFragmentInputLocationCHROMIUM(GLuint program_id,
                                           GLint location,
                                           const std::string& name);

  // If |texture_manager_version_| doesn't match the current version, then this
  // will rebind all external textures to match their current service_id.
  void RestoreAllExternalTextureBindingsIfNeeded() override;

  const SamplerState& GetSamplerStateForTextureUnit(GLenum target, GLuint unit);

  // copyTexImage2D doesn't work on OSX under very specific conditions.
  // Returns whether those conditions have been met. If this method returns
  // true, |source_texture_service_id| and |source_texture_target| are also
  // populated, since they are needed to implement the workaround.
  bool NeedsCopyTextureImageWorkaround(GLenum internal_format,
                                       int32_t channels_exist,
                                       GLuint* source_texture_service_id,
                                       GLenum* source_texture_target);

  // Whether a texture backed by a Chromium Image needs to emulate GL_RGB format
  // using GL_RGBA and glColorMask.
  bool ChromiumImageNeedsRGBEmulation();

  // The GL_CHROMIUM_schedule_ca_layer extension requires that SwapBuffers and
  // equivalent functions reset shared state.
  void ClearScheduleCALayerState();

  // Helper method to call glClear workaround.
  void ClearFramebufferForWorkaround(GLbitfield mask);

  bool SupportsSeparateFramebufferBinds() const {
    return (feature_info_->feature_flags().chromium_framebuffer_multisample ||
            feature_info_->IsWebGL2OrES3Context());
  }

  GLenum GetDrawFramebufferTarget() const {
    return SupportsSeparateFramebufferBinds() ?
        GL_DRAW_FRAMEBUFFER : GL_FRAMEBUFFER;
  }

  GLenum GetReadFramebufferTarget() const {
    return SupportsSeparateFramebufferBinds() ?
        GL_READ_FRAMEBUFFER : GL_FRAMEBUFFER;
  }

  Framebuffer* GetBoundDrawFramebuffer() const {
    return framebuffer_state_.bound_draw_framebuffer.get();
  }

  Framebuffer* GetBoundReadFramebuffer() const {
    GLenum target = GetReadFramebufferTarget();
    return GetFramebufferInfoForTarget(target);
  }

  bool InitializeCopyTexImageBlitter(const char* function_name);
  bool InitializeCopyTextureCHROMIUM(const char* function_name);
  bool InitializeSRGBConverter(const char* function_name);
  // Generate a member function prototype for each command in an automated and
  // typesafe way.
#define GLES2_CMD_OP(name) \
  Error Handle##name(uint32_t immediate_data_size, const volatile void* data);

  GLES2_COMMAND_LIST(GLES2_CMD_OP)

  #undef GLES2_CMD_OP

  // The GL context this decoder renders to on behalf of the client.
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;

  // The ContextGroup for this decoder uses to track resources.
  scoped_refptr<ContextGroup> group_;

  DebugMarkerManager debug_marker_manager_;
  Logger logger_;

  // All the state for this context.
  ContextState state_;

  std::unique_ptr<TransformFeedbackManager> transform_feedback_manager_;

  // Current width and height of the offscreen frame buffer.
  gfx::Size offscreen_size_;

  // Util to help with GL.
  GLES2Util util_;

  // The buffer we bind to attrib 0 since OpenGL requires it (ES does not).
  GLuint attrib_0_buffer_id_;

  // The value currently in attrib_0.
  Vec4 attrib_0_value_;

  // Whether or not the attrib_0 buffer holds the attrib_0_value.
  bool attrib_0_buffer_matches_value_;

  // The size of attrib 0.
  GLsizei attrib_0_size_;

  // The buffer used to simulate GL_FIXED attribs.
  GLuint fixed_attrib_buffer_id_;

  // The size of fiixed attrib buffer.
  GLsizei fixed_attrib_buffer_size_;

  // The offscreen frame buffer that the client renders to. With EGL, the
  // depth and stencil buffers are separate. With regular GL there is a single
  // packed depth stencil buffer in offscreen_target_depth_render_buffer_.
  // offscreen_target_stencil_render_buffer_ is unused.
  std::unique_ptr<BackFramebuffer> offscreen_target_frame_buffer_;
  std::unique_ptr<BackTexture> offscreen_target_color_texture_;
  std::unique_ptr<BackRenderbuffer> offscreen_target_color_render_buffer_;
  std::unique_ptr<BackRenderbuffer> offscreen_target_depth_render_buffer_;
  std::unique_ptr<BackRenderbuffer> offscreen_target_stencil_render_buffer_;

  // The format of the texture or renderbuffer backing the offscreen
  // framebuffer. Also the format of the texture backing the saved offscreen
  // framebuffer.
  GLenum offscreen_target_color_format_;

  GLenum offscreen_target_depth_format_;
  GLenum offscreen_target_stencil_format_;
  GLsizei offscreen_target_samples_;
  GLboolean offscreen_target_buffer_preserved_;

  // The saved copy of the backbuffer after a call to SwapBuffers.
  std::unique_ptr<BackTexture> offscreen_saved_color_texture_;

  // For simplicity, |offscreen_saved_color_texture_| is always bound to
  // |offscreen_saved_frame_buffer_|.
  std::unique_ptr<BackFramebuffer> offscreen_saved_frame_buffer_;

  // When a client requests ownership of the swapped front buffer, all
  // information is saved in this structure, and |in_use| is set to true. When a
  // client releases ownership, |in_use| is set to false.
  //
  // An instance of this struct, with |in_use| = false may be reused instead of
  // making a new BackTexture.
  struct SavedBackTexture {
    std::unique_ptr<BackTexture> back_texture;
    bool in_use;
  };
  std::vector<SavedBackTexture> saved_back_textures_;

  // If there's a SavedBackTexture that's not in use, takes that. Otherwise,
  // generates a new back texture.
  void CreateBackTexture();
  size_t create_back_texture_count_for_test_ = 0;

  // Releases all saved BackTextures that are not in use by a client.
  void ReleaseNotInUseBackTextures();

  // Releases all saved BackTextures.
  void ReleaseAllBackTextures(bool have_context);

  size_t GetSavedBackTextureCountForTest() override;
  size_t GetCreatedBackTextureCountForTest() override;

  // The copy that is used as the destination for multi-sample resolves.
  std::unique_ptr<BackFramebuffer> offscreen_resolved_frame_buffer_;
  std::unique_ptr<BackTexture> offscreen_resolved_color_texture_;
  GLenum offscreen_saved_color_format_;

  // Whether the client requested an offscreen buffer with an alpha channel.
  bool offscreen_buffer_should_have_alpha_;

  std::unique_ptr<QueryManager> query_manager_;

  std::unique_ptr<VertexArrayManager> vertex_array_manager_;

  std::unique_ptr<ImageManager> image_manager_;

  FenceSyncReleaseCallback fence_sync_release_callback_;
  WaitFenceSyncCallback wait_fence_sync_callback_;
  NoParamCallback deschedule_until_finished_callback_;
  NoParamCallback reschedule_after_finished_callback_;

  ShaderCacheCallback shader_cache_callback_;

  // The format of the back buffer_
  GLenum back_buffer_color_format_;
  bool back_buffer_has_depth_;
  bool back_buffer_has_stencil_;

  // Tracks read buffer and draw buffer for backbuffer, whether it's onscreen
  // or offscreen.
  // TODO(zmo): when ES3 APIs are exposed to Nacl, make sure read_buffer_
  // setting is set correctly when SwapBuffers().
  GLenum back_buffer_read_buffer_;
  GLenum back_buffer_draw_buffer_;

  bool surfaceless_;

  // Backbuffer attachments that are currently undefined.
  uint32_t backbuffer_needs_clear_bits_;

  uint64_t swaps_since_resize_;

  // The current decoder error communicates the decoder error through command
  // processing functions that do not return the error value. Should be set only
  // if not returning an error.
  error::Error current_decoder_error_;

  scoped_refptr<ShaderTranslatorInterface> vertex_translator_;
  scoped_refptr<ShaderTranslatorInterface> fragment_translator_;

  // Cached from ContextGroup
  const Validators* validators_;
  scoped_refptr<FeatureInfo> feature_info_;

  int frame_number_;

  // Number of commands remaining to be processed in DoCommands().
  int commands_to_process_;

  bool has_robustness_extension_;
  error::ContextLostReason context_lost_reason_;
  bool context_was_lost_;
  bool reset_by_robustness_extension_;
  bool supports_post_sub_buffer_;
  bool supports_swap_buffers_with_damage_;
  bool supports_commit_overlay_planes_;
  bool supports_async_swap_;

  // These flags are used to override the state of the shared feature_info_
  // member.  Because the same FeatureInfo instance may be shared among many
  // contexts, the assumptions on the availablity of extensions in WebGL
  // contexts may be broken.  These flags override the shared state to preserve
  // WebGL semantics.
  bool derivatives_explicitly_enabled_;
  bool frag_depth_explicitly_enabled_;
  bool draw_buffers_explicitly_enabled_;
  bool shader_texture_lod_explicitly_enabled_;

  bool compile_shader_always_succeeds_;

  // An optional behaviour to lose the context and group when OOM.
  bool lose_context_when_out_of_memory_;

  // Forces the backbuffer to use native GMBs rather than a TEXTURE_2D texture.
  bool should_use_native_gmb_for_backbuffer_;

  // Log extra info.
  bool service_logging_;

  std::unique_ptr<ApplyFramebufferAttachmentCMAAINTELResourceManager>
      apply_framebuffer_attachment_cmaa_intel_;
  std::unique_ptr<CopyTexImageResourceManager> copy_tex_image_blit_;
  std::unique_ptr<CopyTextureCHROMIUMResourceManager> copy_texture_CHROMIUM_;
  std::unique_ptr<SRGBConverter> srgb_converter_;
  std::unique_ptr<ClearFramebufferResourceManager> clear_framebuffer_blit_;

  // Cached values of the currently assigned viewport dimensions.
  GLsizei viewport_max_width_;
  GLsizei viewport_max_height_;

  // Command buffer stats.
  base::TimeDelta total_processing_commands_time_;

  // States related to each manager.
  DecoderTextureState texture_state_;
  DecoderFramebufferState framebuffer_state_;

  std::unique_ptr<GPUTracer> gpu_tracer_;
  std::unique_ptr<GPUStateTracer> gpu_state_tracer_;
  const unsigned char* gpu_decoder_category_;
  int gpu_trace_level_;
  bool gpu_trace_commands_;
  bool gpu_debug_commands_;

  std::queue<FenceCallback> pending_readpixel_fences_;

  // After a second fence is inserted, both the GpuChannelMessageQueue and
  // CommandExecutor are descheduled. Once the first fence has completed, both
  // get rescheduled.
  std::vector<std::unique_ptr<gl::GLFence>> deschedule_until_finished_fences_;

  // Used to validate multisample renderbuffers if needed
  typedef base::hash_map<GLenum, GLuint> TextureMap;
  TextureMap validation_textures_;
  GLuint validation_fbo_multisample_;
  GLuint validation_fbo_;

  typedef gpu::gles2::GLES2Decoder::Error (GLES2DecoderImpl::*CmdHandler)(
      uint32_t immediate_data_size,
      const volatile void* data);

  // A struct to hold info about each command.
  struct CommandInfo {
    CmdHandler cmd_handler;
    uint8_t arg_flags;   // How to handle the arguments for this command
    uint8_t cmd_flags;   // How to handle this command
    uint16_t arg_count;  // How many arguments are expected for this command.
  };

  // A table of CommandInfo for all the commands.
  static const CommandInfo command_info[kNumCommands - kFirstGLES2Command];

  // Most recent generation of the TextureManager.  If this no longer matches
  // the current generation when our context becomes current, then we'll rebind
  // all the textures to stay up to date with Texture::service_id() changes.
  uint32_t texture_manager_service_id_generation_;

  bool force_shader_name_hashing_for_test;

  GLfloat line_width_range_[2];

  SamplerState default_sampler_state_;

  struct CALayerSharedState{
    float opacity;
    bool is_clipped;
    gfx::Rect clip_rect;
    int sorting_context_id;
    gfx::Transform transform;
  };

  std::unique_ptr<CALayerSharedState> ca_layer_shared_state_;

  DISALLOW_COPY_AND_ASSIGN(GLES2DecoderImpl);
};

const GLES2DecoderImpl::CommandInfo GLES2DecoderImpl::command_info[] = {
#define GLES2_CMD_OP(name)                                   \
  {                                                          \
    &GLES2DecoderImpl::Handle##name, cmds::name::kArgFlags,  \
        cmds::name::cmd_flags,                               \
        sizeof(cmds::name) / sizeof(CommandBufferEntry) - 1, \
  }                                                          \
  , /* NOLINT */
    GLES2_COMMAND_LIST(GLES2_CMD_OP)
#undef GLES2_CMD_OP
};

ScopedGLErrorSuppressor::ScopedGLErrorSuppressor(
    const char* function_name, ErrorState* error_state)
    : function_name_(function_name),
      error_state_(error_state) {
  ERRORSTATE_COPY_REAL_GL_ERRORS_TO_WRAPPER(error_state_, function_name_);
}

ScopedGLErrorSuppressor::~ScopedGLErrorSuppressor() {
  ERRORSTATE_CLEAR_REAL_GL_ERRORS(error_state_, function_name_);
}

static void RestoreCurrentTextureBindings(ContextState* state, GLenum target) {
  DCHECK(!state->texture_units.empty());
  TextureUnit& info = state->texture_units[0];
  GLuint last_id;
  scoped_refptr<TextureRef>& texture_ref = info.GetInfoForTarget(target);
  if (texture_ref.get()) {
    last_id = texture_ref->service_id();
  } else {
    last_id = 0;
  }

  glBindTexture(target, last_id);
  glActiveTexture(GL_TEXTURE0 + state->active_texture_unit);
}

ScopedTextureBinder::ScopedTextureBinder(ContextState* state,
                                         GLuint id,
                                         GLenum target)
    : state_(state),
      target_(target) {
  ScopedGLErrorSuppressor suppressor(
      "ScopedTextureBinder::ctor", state_->GetErrorState());

  // TODO(apatrick): Check if there are any other states that need to be reset
  // before binding a new texture.
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(target, id);
}

ScopedTextureBinder::~ScopedTextureBinder() {
  ScopedGLErrorSuppressor suppressor(
      "ScopedTextureBinder::dtor", state_->GetErrorState());
  RestoreCurrentTextureBindings(state_, target_);
}

ScopedRenderBufferBinder::ScopedRenderBufferBinder(ContextState* state,
                                                   GLuint id)
    : state_(state) {
  ScopedGLErrorSuppressor suppressor(
      "ScopedRenderBufferBinder::ctor", state_->GetErrorState());
  glBindRenderbufferEXT(GL_RENDERBUFFER, id);
}

ScopedRenderBufferBinder::~ScopedRenderBufferBinder() {
  ScopedGLErrorSuppressor suppressor(
      "ScopedRenderBufferBinder::dtor", state_->GetErrorState());
  state_->RestoreRenderbufferBindings();
}

ScopedFramebufferBinder::ScopedFramebufferBinder(GLES2DecoderImpl* decoder,
                                                 GLuint id)
    : decoder_(decoder) {
  ScopedGLErrorSuppressor suppressor(
      "ScopedFramebufferBinder::ctor", decoder_->GetErrorState());
  glBindFramebufferEXT(GL_FRAMEBUFFER, id);
  decoder->OnFboChanged();
}

ScopedFramebufferBinder::~ScopedFramebufferBinder() {
  ScopedGLErrorSuppressor suppressor(
      "ScopedFramebufferBinder::dtor", decoder_->GetErrorState());
  decoder_->RestoreCurrentFramebufferBindings();
}

ScopedResolvedFramebufferBinder::ScopedResolvedFramebufferBinder(
    GLES2DecoderImpl* decoder, bool enforce_internal_framebuffer, bool internal)
    : decoder_(decoder) {
  resolve_and_bind_ = (
      decoder_->offscreen_target_frame_buffer_.get() &&
      decoder_->IsOffscreenBufferMultisampled() &&
      (!decoder_->framebuffer_state_.bound_read_framebuffer.get() ||
       enforce_internal_framebuffer));
  if (!resolve_and_bind_)
    return;
  ScopedGLErrorSuppressor suppressor(
      "ScopedResolvedFramebufferBinder::ctor", decoder_->GetErrorState());

  // On old AMD GPUs on macOS, glColorMask doesn't work correctly for
  // multisampled renderbuffers and the alpha channel can be overwritten. This
  // workaround clears the alpha channel before resolving.
  bool alpha_channel_needs_clear =
      decoder_->should_use_native_gmb_for_backbuffer_ &&
      !decoder_->offscreen_buffer_should_have_alpha_ &&
      decoder_->ChromiumImageNeedsRGBEmulation() &&
      decoder_->workarounds()
          .disable_multisampling_color_mask_usage;
  if (alpha_channel_needs_clear) {
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT,
                         decoder_->offscreen_target_frame_buffer_->id());
    decoder_->state_.SetDeviceColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
    decoder->state_.SetDeviceCapabilityState(GL_SCISSOR_TEST, false);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    decoder_->RestoreClearState();
  }

  glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT,
                       decoder_->offscreen_target_frame_buffer_->id());
  GLuint targetid;
  if (internal) {
    if (!decoder_->offscreen_resolved_frame_buffer_.get()) {
      decoder_->offscreen_resolved_frame_buffer_.reset(
          new BackFramebuffer(decoder_));
      decoder_->offscreen_resolved_frame_buffer_->Create();
      decoder_->offscreen_resolved_color_texture_.reset(
          new BackTexture(decoder));
      decoder_->offscreen_resolved_color_texture_->Create();

      DCHECK(decoder_->offscreen_saved_color_format_);
      decoder_->offscreen_resolved_color_texture_->AllocateStorage(
          decoder_->offscreen_size_, decoder_->offscreen_saved_color_format_,
          false);
      decoder_->offscreen_resolved_frame_buffer_->AttachRenderTexture(
          decoder_->offscreen_resolved_color_texture_.get());
      if (decoder_->offscreen_resolved_frame_buffer_->CheckStatus() !=
          GL_FRAMEBUFFER_COMPLETE) {
        LOG(ERROR) << "ScopedResolvedFramebufferBinder failed "
                   << "because offscreen resolved FBO was incomplete.";
        return;
      }
    }
    targetid = decoder_->offscreen_resolved_frame_buffer_->id();
  } else {
    targetid = decoder_->offscreen_saved_frame_buffer_->id();
  }
  glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, targetid);
  const int width = decoder_->offscreen_size_.width();
  const int height = decoder_->offscreen_size_.height();
  decoder->state_.SetDeviceCapabilityState(GL_SCISSOR_TEST, false);
  decoder->BlitFramebufferHelper(0,
                                 0,
                                 width,
                                 height,
                                 0,
                                 0,
                                 width,
                                 height,
                                 GL_COLOR_BUFFER_BIT,
                                 GL_NEAREST);
  glBindFramebufferEXT(GL_FRAMEBUFFER, targetid);
}

ScopedResolvedFramebufferBinder::~ScopedResolvedFramebufferBinder() {
  if (!resolve_and_bind_)
    return;

  ScopedGLErrorSuppressor suppressor(
      "ScopedResolvedFramebufferBinder::dtor", decoder_->GetErrorState());
  decoder_->RestoreCurrentFramebufferBindings();
  if (decoder_->state_.enable_flags.scissor_test) {
    decoder_->state_.SetDeviceCapabilityState(GL_SCISSOR_TEST, true);
  }
}

BackTexture::BackTexture(GLES2DecoderImpl* decoder)
    : memory_tracker_(decoder->memory_tracker()),
      bytes_allocated_(0),
      decoder_(decoder) {
  DCHECK(!decoder_->should_use_native_gmb_for_backbuffer_ ||
         decoder_->GetContextGroup()->image_factory());
}

BackTexture::~BackTexture() {
  // This does not destroy the render texture because that would require that
  // the associated GL context was current. Just check that it was explicitly
  // destroyed.
  DCHECK_EQ(id(), 0u);
  DCHECK(!image_.get());
}

void BackTexture::Create() {
  DCHECK_EQ(id(), 0u);
  ScopedGLErrorSuppressor suppressor("BackTexture::Create",
                                     decoder_->state_.GetErrorState());
  GLuint id;
  glGenTextures(1, &id);

  GLenum target = Target();
  ScopedTextureBinder binder(&decoder_->state_, id, target);

  // No client id is necessary because this texture will never be directly
  // accessed by a client, only indirectly via a mailbox.
  texture_ref_ = TextureRef::Create(decoder_->texture_manager(), 0, id);
  decoder_->texture_manager()->SetTarget(texture_ref_.get(), target);
  decoder_->texture_manager()->SetParameteri(
      "BackTexture::Create",
      decoder_->GetErrorState(),
      texture_ref_.get(),
      GL_TEXTURE_MAG_FILTER,
      GL_LINEAR);
  decoder_->texture_manager()->SetParameteri(
      "BackTexture::Create",
      decoder_->GetErrorState(),
      texture_ref_.get(),
      GL_TEXTURE_MIN_FILTER,
      GL_LINEAR);
  decoder_->texture_manager()->SetParameteri(
      "BackTexture::Create",
      decoder_->GetErrorState(),
      texture_ref_.get(),
      GL_TEXTURE_WRAP_S,
      GL_CLAMP_TO_EDGE);
  decoder_->texture_manager()->SetParameteri(
      "BackTexture::Create",
      decoder_->GetErrorState(),
      texture_ref_.get(),
      GL_TEXTURE_WRAP_T,
      GL_CLAMP_TO_EDGE);
}

bool BackTexture::AllocateStorage(
    const gfx::Size& size, GLenum format, bool zero) {
  DCHECK_NE(id(), 0u);
  ScopedGLErrorSuppressor suppressor("BackTexture::AllocateStorage",
                                     decoder_->state_.GetErrorState());
  ScopedTextureBinder binder(&decoder_->state_, id(), Target());
  uint32_t image_size = 0;
  GLES2Util::ComputeImageDataSizes(
      size.width(), size.height(), 1, format, GL_UNSIGNED_BYTE, 8, &image_size,
      NULL, NULL);

  if (!memory_tracker_.EnsureGPUMemoryAvailable(image_size)) {
    return false;
  }

  bool success = false;
  size_ = size;
  if (decoder_->should_use_native_gmb_for_backbuffer_) {
    DestroyNativeGpuMemoryBuffer(true);
    success = AllocateNativeGpuMemoryBuffer(size, format, zero);
  } else {
    {
      // Add extra scope to destroy zero_data and the object it owns right
      // after its usage.
      std::unique_ptr<char[]> zero_data;
      if (zero) {
        zero_data.reset(new char[image_size]);
        memset(zero_data.get(), 0, image_size);
      }

      glTexImage2D(Target(),
        0,  // mip level
        format, size.width(), size.height(),
        0,  // border
        format, GL_UNSIGNED_BYTE, zero_data.get());
    }

    decoder_->texture_manager()->SetLevelInfo(
        texture_ref_.get(), Target(),
        0,  // level
        GL_RGBA, size.width(), size.height(),
        1,  // depth
        0,  // border
        GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(size));
    success = glGetError() == GL_NO_ERROR;
  }

  if (success) {
    memory_tracker_.TrackMemFree(bytes_allocated_);
    bytes_allocated_ = image_size;
    memory_tracker_.TrackMemAlloc(bytes_allocated_);
  }
  return success;
}

void BackTexture::Copy() {
  DCHECK_NE(id(), 0u);
  ScopedGLErrorSuppressor suppressor("BackTexture::Copy",
                                     decoder_->state_.GetErrorState());
  ScopedTextureBinder binder(&decoder_->state_, id(), Target());
  glCopyTexSubImage2D(Target(),
                      0,  // level
                      0, 0, 0, 0, size_.width(), size_.height());
}

void BackTexture::Destroy() {
  if (image_) {
    DCHECK(texture_ref_);
    ScopedTextureBinder binder(&decoder_->state_, id(), Target());
    DestroyNativeGpuMemoryBuffer(true);
  }

  if (texture_ref_) {
    ScopedGLErrorSuppressor suppressor("BackTexture::Destroy",
                                       decoder_->state_.GetErrorState());
    texture_ref_ = nullptr;
  }
  memory_tracker_.TrackMemFree(bytes_allocated_);
  bytes_allocated_ = 0;
}

void BackTexture::Invalidate() {
  if (image_) {
    DestroyNativeGpuMemoryBuffer(false);
    image_ = nullptr;
  }
  if (texture_ref_) {
    texture_ref_->ForceContextLost();
    texture_ref_ = nullptr;
  }
}

GLenum BackTexture::Target() {
  return decoder_->should_use_native_gmb_for_backbuffer_
             ? decoder_->GetContextGroup()
                   ->image_factory()
                   ->RequiredTextureType()
             : GL_TEXTURE_2D;
}

bool BackTexture::AllocateNativeGpuMemoryBuffer(const gfx::Size& size,
                                                GLenum format,
                                                bool zero) {
  gfx::BufferFormat buffer_format = gfx::BufferFormat::RGBA_8888;
  scoped_refptr<gl::GLImage> image =
      decoder_->GetContextGroup()->image_factory()->CreateAnonymousImage(
          size, buffer_format, format);
  if (!image || !image->BindTexImage(Target()))
    return false;

  image_ = image;
  decoder_->texture_manager()->SetLevelInfo(
      texture_ref_.get(), Target(), 0, image_->GetInternalFormat(),
      size.width(), size.height(), 1, 0, image_->GetInternalFormat(),
      GL_UNSIGNED_BYTE, gfx::Rect(size));
  decoder_->texture_manager()->SetLevelImage(texture_ref_.get(), Target(), 0,
                                             image_.get(), Texture::BOUND);

  // Ignore the zero flag if the alpha channel needs to be cleared for RGB
  // emulation.
  bool needs_clear_for_rgb_emulation =
      !decoder_->offscreen_buffer_should_have_alpha_ &&
      decoder_->ChromiumImageNeedsRGBEmulation();
  if (zero || needs_clear_for_rgb_emulation) {
    GLuint fbo;
    glGenFramebuffersEXT(1, &fbo);
    {
      ScopedFramebufferBinder binder(decoder_, fbo);
      glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, Target(),
                                id(), 0);
      glClearColor(0, 0, 0, decoder_->BackBufferAlphaClearColor());
      decoder_->state_.SetDeviceColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      decoder_->state_.SetDeviceCapabilityState(GL_SCISSOR_TEST, false);
      glClear(GL_COLOR_BUFFER_BIT);
      decoder_->RestoreClearState();
    }
    glDeleteFramebuffersEXT(1, &fbo);
  }
  return true;
}

void BackTexture::DestroyNativeGpuMemoryBuffer(bool have_context) {
  if (image_) {
    ScopedGLErrorSuppressor suppressor(
        "BackTexture::DestroyNativeGpuMemoryBuffer",
        decoder_->state_.GetErrorState());

    image_->ReleaseTexImage(Target());

    decoder_->texture_manager()->SetLevelImage(texture_ref_.get(), Target(), 0,
                                               nullptr, Texture::UNBOUND);
    image_ = nullptr;
  }
}

BackRenderbuffer::BackRenderbuffer(GLES2DecoderImpl* decoder)
    : decoder_(decoder),
      memory_tracker_(decoder->memory_tracker()),
      bytes_allocated_(0),
      id_(0) {}

BackRenderbuffer::~BackRenderbuffer() {
  // This does not destroy the render buffer because that would require that
  // the associated GL context was current. Just check that it was explicitly
  // destroyed.
  DCHECK_EQ(id_, 0u);
}

void BackRenderbuffer::Create() {
  ScopedGLErrorSuppressor suppressor("BackRenderbuffer::Create",
                                     decoder_->state_.GetErrorState());
  Destroy();
  glGenRenderbuffersEXT(1, &id_);
}

bool BackRenderbuffer::AllocateStorage(const FeatureInfo* feature_info,
                                       const gfx::Size& size,
                                       GLenum format,
                                       GLsizei samples) {
  ScopedGLErrorSuppressor suppressor("BackRenderbuffer::AllocateStorage",
                                     decoder_->state_.GetErrorState());
  ScopedRenderBufferBinder binder(&decoder_->state_, id_);

  uint32_t estimated_size = 0;
  if (!decoder_->renderbuffer_manager()->ComputeEstimatedRenderbufferSize(
          size.width(), size.height(), samples, format, &estimated_size)) {
    return false;
  }

  if (!memory_tracker_.EnsureGPUMemoryAvailable(estimated_size)) {
    return false;
  }

  if (samples <= 1) {
    glRenderbufferStorageEXT(GL_RENDERBUFFER,
                             format,
                             size.width(),
                             size.height());
  } else {
    GLES2DecoderImpl::RenderbufferStorageMultisampleHelper(feature_info,
                                                           GL_RENDERBUFFER,
                                                           samples,
                                                           format,
                                                           size.width(),
                                                           size.height());
  }

  bool alpha_channel_needs_clear =
      (format == GL_RGBA || format == GL_RGBA8) &&
      !decoder_->offscreen_buffer_should_have_alpha_;
  if (alpha_channel_needs_clear) {
    GLuint fbo;
    glGenFramebuffersEXT(1, &fbo);
    {
      ScopedFramebufferBinder binder(decoder_, fbo);
      glFramebufferRenderbufferEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_RENDERBUFFER, id_);
      glClearColor(0, 0, 0, decoder_->BackBufferAlphaClearColor());
      decoder_->state_.SetDeviceColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      decoder_->state_.SetDeviceCapabilityState(GL_SCISSOR_TEST, false);
      glClear(GL_COLOR_BUFFER_BIT);
      decoder_->RestoreClearState();
    }
    glDeleteFramebuffersEXT(1, &fbo);
  }

  bool success = glGetError() == GL_NO_ERROR;
  if (success) {
    // Mark the previously allocated bytes as free.
    memory_tracker_.TrackMemFree(bytes_allocated_);
    bytes_allocated_ = estimated_size;
    // Track the newly allocated bytes.
    memory_tracker_.TrackMemAlloc(bytes_allocated_);
  }
  return success;
}

void BackRenderbuffer::Destroy() {
  if (id_ != 0) {
    ScopedGLErrorSuppressor suppressor("BackRenderbuffer::Destroy",
                                       decoder_->state_.GetErrorState());
    glDeleteRenderbuffersEXT(1, &id_);
    id_ = 0;
  }
  memory_tracker_.TrackMemFree(bytes_allocated_);
  bytes_allocated_ = 0;
}

void BackRenderbuffer::Invalidate() {
  id_ = 0;
}

BackFramebuffer::BackFramebuffer(GLES2DecoderImpl* decoder)
    : decoder_(decoder),
      id_(0) {
}

BackFramebuffer::~BackFramebuffer() {
  // This does not destroy the frame buffer because that would require that
  // the associated GL context was current. Just check that it was explicitly
  // destroyed.
  DCHECK_EQ(id_, 0u);
}

void BackFramebuffer::Create() {
  ScopedGLErrorSuppressor suppressor("BackFramebuffer::Create",
                                     decoder_->GetErrorState());
  Destroy();
  glGenFramebuffersEXT(1, &id_);
}

void BackFramebuffer::AttachRenderTexture(BackTexture* texture) {
  DCHECK_NE(id_, 0u);
  ScopedGLErrorSuppressor suppressor(
      "BackFramebuffer::AttachRenderTexture", decoder_->GetErrorState());
  ScopedFramebufferBinder binder(decoder_, id_);
  GLuint attach_id = texture ? texture->id() : 0;
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            texture->Target(), attach_id, 0);
}

void BackFramebuffer::AttachRenderBuffer(GLenum target,
                                         BackRenderbuffer* render_buffer) {
  DCHECK_NE(id_, 0u);
  ScopedGLErrorSuppressor suppressor(
      "BackFramebuffer::AttachRenderBuffer", decoder_->GetErrorState());
  ScopedFramebufferBinder binder(decoder_, id_);
  GLuint attach_id = render_buffer ? render_buffer->id() : 0;
  glFramebufferRenderbufferEXT(GL_FRAMEBUFFER,
                               target,
                               GL_RENDERBUFFER,
                               attach_id);
}

void BackFramebuffer::Destroy() {
  if (id_ != 0) {
    ScopedGLErrorSuppressor suppressor("BackFramebuffer::Destroy",
                                       decoder_->GetErrorState());
    glDeleteFramebuffersEXT(1, &id_);
    id_ = 0;
  }
}

void BackFramebuffer::Invalidate() {
  id_ = 0;
}

GLenum BackFramebuffer::CheckStatus() {
  DCHECK_NE(id_, 0u);
  ScopedGLErrorSuppressor suppressor("BackFramebuffer::CheckStatus",
                                     decoder_->GetErrorState());
  ScopedFramebufferBinder binder(decoder_, id_);
  return glCheckFramebufferStatusEXT(GL_FRAMEBUFFER);
}

GLES2Decoder* GLES2Decoder::Create(ContextGroup* group) {
  if (group->gpu_preferences().use_passthrough_cmd_decoder) {
    return new GLES2DecoderPassthroughImpl(group);
  }
  return new GLES2DecoderImpl(group);
}

GLES2DecoderImpl::GLES2DecoderImpl(ContextGroup* group)
    : GLES2Decoder(),
      group_(group),
      logger_(&debug_marker_manager_),
      state_(group_->feature_info(), this, &logger_),
      attrib_0_buffer_id_(0),
      attrib_0_buffer_matches_value_(true),
      attrib_0_size_(0),
      fixed_attrib_buffer_id_(0),
      fixed_attrib_buffer_size_(0),
      offscreen_target_color_format_(0),
      offscreen_target_depth_format_(0),
      offscreen_target_stencil_format_(0),
      offscreen_target_samples_(0),
      offscreen_target_buffer_preserved_(true),
      offscreen_saved_color_format_(0),
      offscreen_buffer_should_have_alpha_(false),
      back_buffer_color_format_(0),
      back_buffer_has_depth_(false),
      back_buffer_has_stencil_(false),
      back_buffer_read_buffer_(GL_BACK),
      back_buffer_draw_buffer_(GL_BACK),
      surfaceless_(false),
      backbuffer_needs_clear_bits_(0),
      swaps_since_resize_(0),
      current_decoder_error_(error::kNoError),
      validators_(group_->feature_info()->validators()),
      feature_info_(group_->feature_info()),
      frame_number_(0),
      has_robustness_extension_(false),
      context_lost_reason_(error::kUnknown),
      context_was_lost_(false),
      reset_by_robustness_extension_(false),
      supports_post_sub_buffer_(false),
      supports_swap_buffers_with_damage_(false),
      supports_commit_overlay_planes_(false),
      supports_async_swap_(false),
      derivatives_explicitly_enabled_(false),
      frag_depth_explicitly_enabled_(false),
      draw_buffers_explicitly_enabled_(false),
      shader_texture_lod_explicitly_enabled_(false),
      compile_shader_always_succeeds_(false),
      lose_context_when_out_of_memory_(false),
      should_use_native_gmb_for_backbuffer_(false),
      service_logging_(
          group_->gpu_preferences().enable_gpu_service_logging_gpu),
      viewport_max_width_(0),
      viewport_max_height_(0),
      texture_state_(group_->feature_info()->workarounds()),
      gpu_decoder_category_(TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
          TRACE_DISABLED_BY_DEFAULT("gpu_decoder"))),
      gpu_trace_level_(2),
      gpu_trace_commands_(false),
      gpu_debug_commands_(false),
      validation_fbo_multisample_(0),
      validation_fbo_(0),
      texture_manager_service_id_generation_(0),
      force_shader_name_hashing_for_test(false) {
  DCHECK(group);
}

GLES2DecoderImpl::~GLES2DecoderImpl() {
}

bool GLES2DecoderImpl::Initialize(
    const scoped_refptr<gl::GLSurface>& surface,
    const scoped_refptr<gl::GLContext>& context,
    bool offscreen,
    const DisallowedFeatures& disallowed_features,
    const ContextCreationAttribHelper& attrib_helper) {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::Initialize");
  DCHECK(context->IsCurrent(surface.get()));
  DCHECK(!context_.get());

  surfaceless_ = surface->IsSurfaceless() && !offscreen;

  set_initialized();
  gpu_state_tracer_ = GPUStateTracer::Create(&state_);

  if (group_->gpu_preferences().enable_gpu_debugging)
    set_debug(true);

  if (group_->gpu_preferences().enable_gpu_command_logging)
    set_log_commands(true);

  compile_shader_always_succeeds_ =
      group_->gpu_preferences().compile_shader_always_succeeds;

  // Take ownership of the context and surface. The surface can be replaced with
  // SetSurface.
  context_ = context;
  surface_ = surface;

  // Create GPU Tracer for timing values.
  gpu_tracer_.reset(new GPUTracer(this));

  if (workarounds().disable_timestamp_queries) {
    // Forcing time elapsed query for any GPU Timing Client forces it for all
    // clients in the context.
    GetGLContext()->CreateGPUTimingClient()->ForceTimeElapsedQuery();
  }

  // Save the loseContextWhenOutOfMemory context creation attribute.
  lose_context_when_out_of_memory_ =
      attrib_helper.lose_context_when_out_of_memory;

  // If the failIfMajorPerformanceCaveat context creation attribute was true
  // and we are using a software renderer, fail.
  if (attrib_helper.fail_if_major_perf_caveat &&
      feature_info_->feature_flags().is_swiftshader) {
    group_ = NULL;  // Must not destroy ContextGroup if it is not initialized.
    Destroy(true);
    return false;
  }

  if (!group_->Initialize(this, attrib_helper.context_type,
                          disallowed_features)) {
    group_ = NULL;  // Must not destroy ContextGroup if it is not initialized.
    Destroy(true);
    return false;
  }
  CHECK_GL_ERROR();

  should_use_native_gmb_for_backbuffer_ =
      attrib_helper.should_use_native_gmb_for_backbuffer;
  if (should_use_native_gmb_for_backbuffer_) {
    gpu::ImageFactory* image_factory = group_->image_factory();
    bool supported = false;
    if (image_factory) {
      switch (image_factory->RequiredTextureType()) {
        case GL_TEXTURE_RECTANGLE_ARB:
          supported = feature_info_->feature_flags().arb_texture_rectangle;
          break;
        case GL_TEXTURE_2D:
          supported = true;
          break;
        default:
          break;
      }
    }

    if (!supported) {
      Destroy(true);
      return false;
    }
  }

  // In theory |needs_emulation| needs to be true on Desktop GL 4.1 or lower.
  // However, we set it to true everywhere, not to trust drivers to handle
  // out-of-bounds buffer accesses.
  bool needs_emulation = true;
  transform_feedback_manager_.reset(new TransformFeedbackManager(
      group_->max_transform_feedback_separate_attribs(), needs_emulation));

  if (feature_info_->IsWebGL2OrES3Context()) {
    if (!feature_info_->IsES3Capable()) {
      LOG(ERROR) << "ES3 is blacklisted/disabled/unsupported by driver.";
      Destroy(true);
      return false;
    }
    feature_info_->EnableES3Validators();

    frag_depth_explicitly_enabled_ = true;
    draw_buffers_explicitly_enabled_ = true;
    // TODO(zmo): Look into shader_texture_lod_explicitly_enabled_ situation.

    // Create a fake default transform feedback and bind to it.
    GLuint default_transform_feedback = 0;
    glGenTransformFeedbacks(1, &default_transform_feedback);
    state_.default_transform_feedback =
        transform_feedback_manager_->CreateTransformFeedback(
            0, default_transform_feedback);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, default_transform_feedback);
    state_.bound_transform_feedback = state_.default_transform_feedback.get();
  }
  state_.indexed_uniform_buffer_bindings = new IndexedBufferBindingHost(
      group_->max_uniform_buffer_bindings(), needs_emulation);

  state_.InitGenericAttribs(group_->max_vertex_attribs());
  vertex_array_manager_.reset(new VertexArrayManager());

  GLuint default_vertex_attrib_service_id = 0;
  if (features().native_vertex_array_object) {
    glGenVertexArraysOES(1, &default_vertex_attrib_service_id);
    glBindVertexArrayOES(default_vertex_attrib_service_id);
  }

  state_.default_vertex_attrib_manager =
      CreateVertexAttribManager(0, default_vertex_attrib_service_id, false);

  state_.default_vertex_attrib_manager->Initialize(
      group_->max_vertex_attribs(),
      workarounds().init_vertex_attributes);

  // vertex_attrib_manager is set to default_vertex_attrib_manager by this call
  DoBindVertexArrayOES(0);

  query_manager_.reset(new QueryManager(this, feature_info_.get()));

  image_manager_.reset(new ImageManager);

  util_.set_num_compressed_texture_formats(
      validators_->compressed_texture_format.GetValues().size());

  if (!gl_version_info().BehavesLikeGLES()) {
    // We have to enable vertex array 0 on GL with compatibility profile or it
    // won't render. Note that ES or GL with core profile does not have this
    // issue.
    glEnableVertexAttribArray(0);
  }
  glGenBuffersARB(1, &attrib_0_buffer_id_);
  glBindBuffer(GL_ARRAY_BUFFER, attrib_0_buffer_id_);
  glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, 0, NULL);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glGenBuffersARB(1, &fixed_attrib_buffer_id_);

  state_.texture_units.resize(group_->max_texture_units());
  state_.sampler_units.resize(group_->max_texture_units());
  for (uint32_t tt = 0; tt < state_.texture_units.size(); ++tt) {
    glActiveTexture(GL_TEXTURE0 + tt);
    // We want the last bind to be 2D.
    TextureRef* ref;
    if (features().oes_egl_image_external ||
        features().nv_egl_stream_consumer_external) {
      ref = texture_manager()->GetDefaultTextureInfo(
          GL_TEXTURE_EXTERNAL_OES);
      state_.texture_units[tt].bound_texture_external_oes = ref;
      glBindTexture(GL_TEXTURE_EXTERNAL_OES, ref ? ref->service_id() : 0);
    }
    if (features().arb_texture_rectangle) {
      ref = texture_manager()->GetDefaultTextureInfo(
          GL_TEXTURE_RECTANGLE_ARB);
      state_.texture_units[tt].bound_texture_rectangle_arb = ref;
      glBindTexture(GL_TEXTURE_RECTANGLE_ARB, ref ? ref->service_id() : 0);
    }
    ref = texture_manager()->GetDefaultTextureInfo(GL_TEXTURE_CUBE_MAP);
    state_.texture_units[tt].bound_texture_cube_map = ref;
    glBindTexture(GL_TEXTURE_CUBE_MAP, ref ? ref->service_id() : 0);
    ref = texture_manager()->GetDefaultTextureInfo(GL_TEXTURE_2D);
    state_.texture_units[tt].bound_texture_2d = ref;
    glBindTexture(GL_TEXTURE_2D, ref ? ref->service_id() : 0);
  }
  glActiveTexture(GL_TEXTURE0);
  CHECK_GL_ERROR();

  // cache ALPHA_BITS result for re-use with clear behaviour
  GLint alpha_bits = 0;

  if (offscreen) {
    offscreen_buffer_should_have_alpha_ = attrib_helper.alpha_size > 0;

    // Whether the offscreen buffer texture should have an alpha channel. Does
    // not include logic from workarounds.
    bool offscreen_buffer_texture_needs_alpha =
        offscreen_buffer_should_have_alpha_ ||
        (ChromiumImageNeedsRGBEmulation() &&
         attrib_helper.should_use_native_gmb_for_backbuffer);

    if (attrib_helper.samples > 0 && attrib_helper.sample_buffers > 0 &&
        features().chromium_framebuffer_multisample) {
      // Per ext_framebuffer_multisample spec, need max bound on sample count.
      // max_sample_count must be initialized to a sane value.  If
      // glGetIntegerv() throws a GL error, it leaves its argument unchanged.
      GLint max_sample_count = 1;
      glGetIntegerv(GL_MAX_SAMPLES_EXT, &max_sample_count);
      offscreen_target_samples_ = std::min(attrib_helper.samples,
                                           max_sample_count);
    } else {
      offscreen_target_samples_ = 1;
    }
    offscreen_target_buffer_preserved_ = attrib_helper.buffer_preserved;

    if (gl_version_info().is_es) {
      const bool rgb8_supported = context_->HasExtension("GL_OES_rgb8_rgba8");
      // The only available default render buffer formats in GLES2 have very
      // little precision.  Don't enable multisampling unless 8-bit render
      // buffer formats are available--instead fall back to 8-bit textures.
      if (rgb8_supported && offscreen_target_samples_ > 1) {
        offscreen_target_color_format_ =
            offscreen_buffer_texture_needs_alpha ? GL_RGBA8 : GL_RGB8;
      } else {
        offscreen_target_samples_ = 1;
        offscreen_target_color_format_ =
            offscreen_buffer_texture_needs_alpha ||
                    workarounds().disable_gl_rgb_format
                ? GL_RGBA
                : GL_RGB;
      }

      // ANGLE only supports packed depth/stencil formats, so use it if it is
      // available.
      const bool depth24_stencil8_supported =
          feature_info_->feature_flags().packed_depth24_stencil8;
      VLOG(1) << "GL_OES_packed_depth_stencil "
              << (depth24_stencil8_supported ? "" : "not ") << "supported.";
      if ((attrib_helper.depth_size > 0 || attrib_helper.stencil_size > 0) &&
          depth24_stencil8_supported) {
        offscreen_target_depth_format_ = GL_DEPTH24_STENCIL8;
        offscreen_target_stencil_format_ = 0;
      } else {
        // It may be the case that this depth/stencil combination is not
        // supported, but this will be checked later by CheckFramebufferStatus.
        offscreen_target_depth_format_ = attrib_helper.depth_size > 0 ?
            GL_DEPTH_COMPONENT16 : 0;
        offscreen_target_stencil_format_ = attrib_helper.stencil_size > 0 ?
            GL_STENCIL_INDEX8 : 0;
      }
    } else {
      offscreen_target_color_format_ =
          offscreen_buffer_texture_needs_alpha ||
                  workarounds().disable_gl_rgb_format
              ? GL_RGBA
              : GL_RGB;

      // If depth is requested at all, use the packed depth stencil format if
      // it's available, as some desktop GL drivers don't support any non-packed
      // formats for depth attachments.
      const bool depth24_stencil8_supported =
          feature_info_->feature_flags().packed_depth24_stencil8;
      VLOG(1) << "GL_EXT_packed_depth_stencil "
              << (depth24_stencil8_supported ? "" : "not ") << "supported.";

      if ((attrib_helper.depth_size > 0 || attrib_helper.stencil_size > 0) &&
          depth24_stencil8_supported) {
        offscreen_target_depth_format_ = GL_DEPTH24_STENCIL8;
        offscreen_target_stencil_format_ = 0;
      } else {
        offscreen_target_depth_format_ = attrib_helper.depth_size > 0 ?
            GL_DEPTH_COMPONENT : 0;
        offscreen_target_stencil_format_ = attrib_helper.stencil_size > 0 ?
            GL_STENCIL_INDEX : 0;
      }
    }

    offscreen_saved_color_format_ = offscreen_buffer_texture_needs_alpha ||
                                            workarounds().disable_gl_rgb_format
                                        ? GL_RGBA
                                        : GL_RGB;

    gfx::Size initial_size = attrib_helper.offscreen_framebuffer_size;
    if (initial_size.IsEmpty()) {
      // If we're an offscreen surface with zero width and/or height, set to a
      // non-zero size so that we have a complete framebuffer for operations
      // like glClear.
      // TODO(piman): allow empty framebuffers, similar to
      // EGL_KHR_surfaceless_context / GL_OES_surfaceless_context.
      initial_size = gfx::Size(1, 1);
    }

    state_.viewport_width = initial_size.width();
    state_.viewport_height = initial_size.height();
  } else {
    glBindFramebufferEXT(GL_FRAMEBUFFER, GetBackbufferServiceId());
    // These are NOT if the back buffer has these proprorties. They are
    // if we want the command buffer to enforce them regardless of what
    // the real backbuffer is assuming the real back buffer gives us more than
    // we ask for. In other words, if we ask for RGB and we get RGBA then we'll
    // make it appear RGB. If on the other hand we ask for RGBA nd get RGB we
    // can't do anything about that.

    if (!surfaceless_) {
      GLint depth_bits = 0;
      GLint stencil_bits = 0;

      bool default_fb = (GetBackbufferServiceId() == 0);

      if (gl_version_info().is_desktop_core_profile) {
        glGetFramebufferAttachmentParameterivEXT(
            GL_FRAMEBUFFER,
            default_fb ? GL_BACK_LEFT : GL_COLOR_ATTACHMENT0,
            GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE, &alpha_bits);
        glGetFramebufferAttachmentParameterivEXT(
            GL_FRAMEBUFFER,
            default_fb ? GL_DEPTH : GL_DEPTH_ATTACHMENT,
            GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &depth_bits);
        glGetFramebufferAttachmentParameterivEXT(
            GL_FRAMEBUFFER,
            default_fb ? GL_STENCIL : GL_STENCIL_ATTACHMENT,
            GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &stencil_bits);
      } else {
        glGetIntegerv(GL_ALPHA_BITS, &alpha_bits);
        glGetIntegerv(GL_DEPTH_BITS, &depth_bits);
        glGetIntegerv(GL_STENCIL_BITS, &stencil_bits);
      }

      // This checks if the user requested RGBA and we have RGBA then RGBA. If
      // the user requested RGB then RGB. If the user did not specify a
      // preference than use whatever we were given. Same for DEPTH and STENCIL.
      back_buffer_color_format_ =
          (attrib_helper.alpha_size != 0 && alpha_bits > 0) ? GL_RGBA : GL_RGB;
      back_buffer_has_depth_ = attrib_helper.depth_size != 0 && depth_bits > 0;
      back_buffer_has_stencil_ =
          attrib_helper.stencil_size != 0 && stencil_bits > 0;
    }

    state_.viewport_width = surface->GetSize().width();
    state_.viewport_height = surface->GetSize().height();
  }

  // OpenGL ES 2.0 implicitly enables the desktop GL capability
  // VERTEX_PROGRAM_POINT_SIZE and doesn't expose this enum. This fact
  // isn't well documented; it was discovered in the Khronos OpenGL ES
  // mailing list archives. It also implicitly enables the desktop GL
  // capability GL_POINT_SPRITE to provide access to the gl_PointCoord
  // variable in fragment shaders.
  if (!gl_version_info().BehavesLikeGLES()) {
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    glEnable(GL_POINT_SPRITE);
  } else if (gl_version_info().is_desktop_core_profile) {
    // The desktop core profile changed how program point size mode is
    // enabled.
    glEnable(GL_PROGRAM_POINT_SIZE);
  }

  // ES3 requires seamless cubemap. ES2 does not.
  // However, when ES2 is implemented on top of DX11, seamless cubemap is
  // always enabled and there is no way to disable it.
  // Therefore, it seems OK to also always enable it on top of Desktop GL for
  // both ES2 and ES3 contexts.
  if (!workarounds().disable_texture_cube_map_seamless &&
      gl_version_info().IsAtLeastGL(3, 2)) {
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
  }

  has_robustness_extension_ =
      context->HasExtension("GL_ARB_robustness") ||
      context->HasExtension("GL_KHR_robustness") ||
      context->HasExtension("GL_EXT_robustness");

  if (!InitializeShaderTranslator()) {
    return false;
  }

  GLint viewport_params[4] = { 0 };
  glGetIntegerv(GL_MAX_VIEWPORT_DIMS, viewport_params);
  viewport_max_width_ = viewport_params[0];
  viewport_max_height_ = viewport_params[1];

  glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, line_width_range_);
  state_.SetLineWidthBounds(line_width_range_[0], line_width_range_[1]);

  state_.scissor_width = state_.viewport_width;
  state_.scissor_height = state_.viewport_height;

  // Set all the default state because some GL drivers get it wrong.
  state_.InitCapabilities(NULL);
  state_.InitState(NULL);

  // Default state must be set before offscreen resources can be created.
  if (offscreen) {
    // Create the target frame buffer. This is the one that the client renders
    // directly to.
    offscreen_target_frame_buffer_.reset(new BackFramebuffer(this));
    offscreen_target_frame_buffer_->Create();
    // Due to GLES2 format limitations, either the color texture (for
    // non-multisampling) or the color render buffer (for multisampling) will be
    // attached to the offscreen frame buffer.  The render buffer has more
    // limited formats available to it, but the texture can't do multisampling.
    if (IsOffscreenBufferMultisampled()) {
      offscreen_target_color_render_buffer_.reset(new BackRenderbuffer(this));
      offscreen_target_color_render_buffer_->Create();
    } else {
      offscreen_target_color_texture_.reset(new BackTexture(this));
      offscreen_target_color_texture_->Create();
    }
    offscreen_target_depth_render_buffer_.reset(new BackRenderbuffer(this));
    offscreen_target_depth_render_buffer_->Create();
    offscreen_target_stencil_render_buffer_.reset(new BackRenderbuffer(this));
    offscreen_target_stencil_render_buffer_->Create();

    // Create the saved offscreen texture. The target frame buffer is copied
    // here when SwapBuffers is called.
    offscreen_saved_frame_buffer_.reset(new BackFramebuffer(this));
    offscreen_saved_frame_buffer_->Create();
    //
    offscreen_saved_color_texture_.reset(new BackTexture(this));
    offscreen_saved_color_texture_->Create();

    // Allocate the render buffers at their initial size and check the status
    // of the frame buffers is okay.
    if (!ResizeOffscreenFramebuffer(
            gfx::Size(state_.viewport_width, state_.viewport_height))) {
      LOG(ERROR) << "Could not allocate offscreen buffer storage.";
      Destroy(true);
      return false;
    }
    // Allocate the offscreen saved color texture.
    DCHECK(offscreen_saved_color_format_);
    offscreen_saved_color_texture_->AllocateStorage(
        gfx::Size(1, 1), offscreen_saved_color_format_, true);

    offscreen_saved_frame_buffer_->AttachRenderTexture(
        offscreen_saved_color_texture_.get());
    if (offscreen_saved_frame_buffer_->CheckStatus() !=
        GL_FRAMEBUFFER_COMPLETE) {
      LOG(ERROR) << "Offscreen saved FBO was incomplete.";
      Destroy(true);
      return false;
    }
  }

  glActiveTexture(GL_TEXTURE0 + state_.active_texture_unit);

  DoBindBuffer(GL_ARRAY_BUFFER, 0);
  DoBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  DoBindFramebuffer(GL_FRAMEBUFFER, 0);
  DoBindRenderbuffer(GL_RENDERBUFFER, 0);

  bool call_gl_clear = !surfaceless_ && !offscreen;
#if defined(OS_ANDROID)
  // Temporary workaround for Android WebView because this clear ignores the
  // clip and corrupts that external UI of the App. Not calling glClear is ok
  // because the system already clears the buffer before each draw. Proper
  // fix might be setting the scissor clip properly before initialize. See
  // crbug.com/259023 for details.
  call_gl_clear = surface_->GetHandle();
#endif
  if (call_gl_clear) {
    // On configs where we report no alpha, if the underlying surface has
    // alpha, clear the surface alpha to 1.0 to be correct on ReadPixels/etc.
    bool clear_alpha = back_buffer_color_format_ == GL_RGB && alpha_bits > 0;
    if (clear_alpha) {
      glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    }

    // Clear the backbuffer.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Restore alpha clear value if we changed it.
    if (clear_alpha) {
      glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    }
  }

  supports_post_sub_buffer_ = surface->SupportsPostSubBuffer();
  if (workarounds()
          .disable_post_sub_buffers_for_onscreen_surfaces &&
      !surface->IsOffscreen())
    supports_post_sub_buffer_ = false;

  supports_swap_buffers_with_damage_ = surface->SupportsSwapBuffersWithDamage();

  supports_commit_overlay_planes_ = surface->SupportsCommitOverlayPlanes();

  supports_async_swap_ = surface->SupportsAsyncSwap();

  if (workarounds().reverse_point_sprite_coord_origin) {
    glPointParameteri(GL_POINT_SPRITE_COORD_ORIGIN, GL_LOWER_LEFT);
  }

  if (workarounds().unbind_fbo_on_context_switch) {
    context_->SetUnbindFboOnMakeCurrent();
  }

  if (workarounds().gl_clear_broken) {
    DCHECK(!clear_framebuffer_blit_.get());
    LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("glClearWorkaroundInit");
    clear_framebuffer_blit_.reset(new ClearFramebufferResourceManager(this));
    if (LOCAL_PEEK_GL_ERROR("glClearWorkaroundInit") != GL_NO_ERROR)
      return false;
  }

  if (group_->gpu_preferences().enable_gpu_driver_debug_logging &&
      feature_info_->feature_flags().khr_debug) {
    InitializeGLDebugLogging();
  }

  return true;
}

Capabilities GLES2DecoderImpl::GetCapabilities() {
  DCHECK(initialized());
  Capabilities caps;
  const gl::GLVersionInfo& version_info = gl_version_info();
  caps.VisitPrecisions([&version_info](
      GLenum shader, GLenum type,
      Capabilities::ShaderPrecision* shader_precision) {
    GLint range[2] = {0, 0};
    GLint precision = 0;
    QueryShaderPrecisionFormat(version_info, shader, type, range, &precision);
    shader_precision->min_range = range[0];
    shader_precision->max_range = range[1];
    shader_precision->precision = precision;
  });
  DoGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,
                &caps.max_combined_texture_image_units, 1);
  DoGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &caps.max_cube_map_texture_size,
                1);
  DoGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS,
                &caps.max_fragment_uniform_vectors, 1);
  DoGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &caps.max_renderbuffer_size, 1);
  DoGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &caps.max_texture_image_units, 1);
  DoGetIntegerv(GL_MAX_TEXTURE_SIZE, &caps.max_texture_size, 1);
  DoGetIntegerv(GL_MAX_VARYING_VECTORS, &caps.max_varying_vectors, 1);
  DoGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &caps.max_vertex_attribs, 1);
  DoGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS,
                &caps.max_vertex_texture_image_units, 1);
  DoGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &caps.max_vertex_uniform_vectors,
                1);
  DoGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS,
                &caps.num_compressed_texture_formats, 1);
  DoGetIntegerv(GL_NUM_SHADER_BINARY_FORMATS, &caps.num_shader_binary_formats,
                1);
  DoGetIntegerv(GL_BIND_GENERATES_RESOURCE_CHROMIUM,
                &caps.bind_generates_resource_chromium, 1);
  if (feature_info_->IsWebGL2OrES3Context()) {
    // TODO(zmo): Note that some parameter values could be more than 32-bit,
    // but for now we clamp them to 32-bit max.
    DoGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &caps.max_3d_texture_size, 1);
    DoGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &caps.max_array_texture_layers,
                  1);
    DoGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &caps.max_color_attachments, 1);
    DoGetInteger64v(GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS,
                    &caps.max_combined_fragment_uniform_components, 1);
    DoGetIntegerv(GL_MAX_COMBINED_UNIFORM_BLOCKS,
                  &caps.max_combined_uniform_blocks, 1);
    DoGetInteger64v(GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS,
                    &caps.max_combined_vertex_uniform_components, 1);
    DoGetIntegerv(GL_MAX_DRAW_BUFFERS, &caps.max_draw_buffers, 1);
    DoGetInteger64v(GL_MAX_ELEMENT_INDEX, &caps.max_element_index, 1);
    DoGetIntegerv(GL_MAX_ELEMENTS_INDICES, &caps.max_elements_indices, 1);
    DoGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &caps.max_elements_vertices, 1);
    DoGetIntegerv(GL_MAX_FRAGMENT_INPUT_COMPONENTS,
                  &caps.max_fragment_input_components, 1);
    DoGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_BLOCKS,
                  &caps.max_fragment_uniform_blocks, 1);
    DoGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS,
                  &caps.max_fragment_uniform_components, 1);
    DoGetIntegerv(GL_MAX_PROGRAM_TEXEL_OFFSET, &caps.max_program_texel_offset,
                  1);
    DoGetInteger64v(GL_MAX_SERVER_WAIT_TIMEOUT, &caps.max_server_wait_timeout,
                    1);
    // Work around Linux NVIDIA driver bug where GL_TIMEOUT_IGNORED is
    // returned.
    if (caps.max_server_wait_timeout < 0)
      caps.max_server_wait_timeout = 0;
    DoGetFloatv(GL_MAX_TEXTURE_LOD_BIAS, &caps.max_texture_lod_bias, 1);
    DoGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS,
                  &caps.max_transform_feedback_interleaved_components, 1);
    DoGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS,
                  &caps.max_transform_feedback_separate_attribs, 1);
    DoGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS,
                  &caps.max_transform_feedback_separate_components, 1);
    DoGetInteger64v(GL_MAX_UNIFORM_BLOCK_SIZE, &caps.max_uniform_block_size, 1);
    DoGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS,
                  &caps.max_uniform_buffer_bindings, 1);
    DoGetIntegerv(GL_MAX_VARYING_COMPONENTS, &caps.max_varying_components, 1);
    DoGetIntegerv(GL_MAX_VERTEX_OUTPUT_COMPONENTS,
                  &caps.max_vertex_output_components, 1);
    DoGetIntegerv(GL_MAX_VERTEX_UNIFORM_BLOCKS, &caps.max_vertex_uniform_blocks,
                  1);
    DoGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS,
                  &caps.max_vertex_uniform_components, 1);
    DoGetIntegerv(GL_MIN_PROGRAM_TEXEL_OFFSET, &caps.min_program_texel_offset,
                  1);
    DoGetIntegerv(GL_NUM_EXTENSIONS, &caps.num_extensions, 1);
    // TODO(vmiura): Remove GL_NUM_PROGRAM_BINARY_FORMATS.
    DoGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS,
                  &caps.num_program_binary_formats, 1);
    DoGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,
                  &caps.uniform_buffer_offset_alignment, 1);
    // TODO(zmo): once we switch to MANGLE, we should query version numbers.
    caps.major_version = 3;
    caps.minor_version = 0;
  }
  if (feature_info_->feature_flags().multisampled_render_to_texture ||
      feature_info_->feature_flags().chromium_framebuffer_multisample ||
      feature_info_->IsWebGL2OrES3Context()) {
    DoGetIntegerv(GL_MAX_SAMPLES, &caps.max_samples, 1);
  }

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
  caps.texture_format_etc1_npot =
      caps.texture_format_etc1 && !workarounds().etc1_power_of_two_only;
  // Whether or not a texture will be bound to an EGLImage is
  // dependent on whether we are using the sync mailbox manager.
  caps.disable_one_component_textures =
      mailbox_manager()->UsesSync() &&
      workarounds().avoid_one_component_egl_images;
  caps.texture_rectangle = feature_info_->feature_flags().arb_texture_rectangle;
  caps.texture_usage = feature_info_->feature_flags().angle_texture_usage;
  caps.texture_storage = feature_info_->feature_flags().ext_texture_storage;
  caps.discard_framebuffer =
      feature_info_->feature_flags().ext_discard_framebuffer;
  caps.sync_query = feature_info_->feature_flags().chromium_sync_query;

  caps.chromium_image_rgb_emulation = ChromiumImageNeedsRGBEmulation();
#if defined(OS_MACOSX)
  // This is unconditionally true on mac, no need to test for it at runtime.
  caps.iosurface = true;
#endif

  caps.post_sub_buffer = supports_post_sub_buffer_;
  caps.swap_buffers_with_damage = supports_swap_buffers_with_damage_;
  caps.commit_overlay_planes = supports_commit_overlay_planes_;
  caps.surfaceless = surfaceless_;
  bool is_offscreen = !!offscreen_target_frame_buffer_.get();
  caps.flips_vertically = !is_offscreen && surface_->FlipsVertically();
  caps.msaa_is_slow = workarounds().msaa_is_slow;

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
      workarounds().max_copy_texture_chromium_size;
  caps.render_buffer_format_bgra8888 =
      feature_info_->feature_flags().ext_render_buffer_format_bgra8888;
  caps.occlusion_query_boolean =
      feature_info_->feature_flags().occlusion_query_boolean;
  caps.timer_queries =
      query_manager_->GPUTimingAvailable();
  caps.disable_multisampling_color_mask_usage =
      workarounds().disable_multisampling_color_mask_usage;
  caps.disable_webgl_rgb_multisampling_usage =
      workarounds().disable_webgl_rgb_multisampling_usage;
  caps.emulate_rgb_buffer_with_rgba =
      workarounds().disable_gl_rgb_format;

  return caps;
}

void GLES2DecoderImpl::UpdateCapabilities() {
  util_.set_num_compressed_texture_formats(
      validators_->compressed_texture_format.GetValues().size());
  util_.set_num_shader_binary_formats(
      validators_->shader_binary_format.GetValues().size());
}

bool GLES2DecoderImpl::InitializeShaderTranslator() {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::InitializeShaderTranslator");
  if (feature_info_->disable_shader_translator()) {
    return true;
  }
  ShBuiltInResources resources;
  ShInitBuiltInResources(&resources);
  resources.MaxVertexAttribs = group_->max_vertex_attribs();
  resources.MaxVertexUniformVectors =
      group_->max_vertex_uniform_vectors();
  resources.MaxVaryingVectors = group_->max_varying_vectors();
  resources.MaxVertexTextureImageUnits =
      group_->max_vertex_texture_image_units();
  resources.MaxCombinedTextureImageUnits = group_->max_texture_units();
  resources.MaxTextureImageUnits = group_->max_texture_image_units();
  resources.MaxFragmentUniformVectors =
      group_->max_fragment_uniform_vectors();
  resources.MaxDrawBuffers = group_->max_draw_buffers();
  resources.MaxExpressionComplexity = 256;
  resources.MaxCallStackDepth = 256;
  resources.MaxDualSourceDrawBuffers = group_->max_dual_source_draw_buffers();

  if(!feature_info_->IsWebGL1OrES2Context()) {
    resources.MaxVertexOutputVectors =
        group_->max_vertex_output_components() / 4;
    resources.MaxFragmentInputVectors =
        group_->max_fragment_input_components() / 4;
    resources.MaxProgramTexelOffset = group_->max_program_texel_offset();
    resources.MinProgramTexelOffset = group_->min_program_texel_offset();
  }

  GLint range[2] = { 0, 0 };
  GLint precision = 0;
  QueryShaderPrecisionFormat(gl_version_info(), GL_FRAGMENT_SHADER,
                             GL_HIGH_FLOAT, range, &precision);
  resources.FragmentPrecisionHigh =
      PrecisionMeetsSpecForHighpFloat(range[0], range[1], precision);

  ShShaderSpec shader_spec;
  switch (feature_info_->context_type()) {
    case CONTEXT_TYPE_WEBGL1:
      shader_spec = SH_WEBGL_SPEC;
      resources.OES_standard_derivatives = derivatives_explicitly_enabled_;
      resources.EXT_frag_depth = frag_depth_explicitly_enabled_;
      resources.EXT_draw_buffers = draw_buffers_explicitly_enabled_;
      if (!draw_buffers_explicitly_enabled_)
        resources.MaxDrawBuffers = 1;
      resources.EXT_shader_texture_lod = shader_texture_lod_explicitly_enabled_;
      resources.NV_draw_buffers =
          draw_buffers_explicitly_enabled_ && features().nv_draw_buffers;
      break;
    case CONTEXT_TYPE_WEBGL2:
      shader_spec = SH_WEBGL2_SPEC;
      break;
    case CONTEXT_TYPE_OPENGLES2:
      shader_spec = SH_GLES2_SPEC;
      resources.OES_standard_derivatives =
          features().oes_standard_derivatives ? 1 : 0;
      resources.ARB_texture_rectangle =
          features().arb_texture_rectangle ? 1 : 0;
      resources.OES_EGL_image_external =
          features().oes_egl_image_external ? 1 : 0;
      resources.NV_EGL_stream_consumer_external =
          features().nv_egl_stream_consumer_external ? 1 : 0;
      resources.EXT_draw_buffers =
          features().ext_draw_buffers ? 1 : 0;
      resources.EXT_frag_depth =
          features().ext_frag_depth ? 1 : 0;
      resources.EXT_shader_texture_lod =
          features().ext_shader_texture_lod ? 1 : 0;
      resources.NV_draw_buffers =
          features().nv_draw_buffers ? 1 : 0;
      resources.EXT_blend_func_extended =
          features().ext_blend_func_extended ? 1 : 0;
      break;
    case CONTEXT_TYPE_OPENGLES3:
      shader_spec = SH_GLES3_SPEC;
      resources.ARB_texture_rectangle =
          features().arb_texture_rectangle ? 1 : 0;
      resources.OES_EGL_image_external =
          features().oes_egl_image_external ? 1 : 0;
      resources.NV_EGL_stream_consumer_external =
          features().nv_egl_stream_consumer_external ? 1 : 0;
      resources.EXT_blend_func_extended =
          features().ext_blend_func_extended ? 1 : 0;
      break;
    default:
      NOTREACHED();
      shader_spec = SH_GLES2_SPEC;
      break;
  }

  if (((shader_spec == SH_WEBGL_SPEC || shader_spec == SH_WEBGL2_SPEC) &&
       features().enable_shader_name_hashing) ||
      force_shader_name_hashing_for_test)
    resources.HashFunction = &CityHash64;
  else
    resources.HashFunction = NULL;

  ShCompileOptions driver_bug_workarounds = 0;
  if (workarounds().init_gl_position_in_vertex_shader)
    driver_bug_workarounds |= SH_INIT_GL_POSITION;
  if (workarounds().unfold_short_circuit_as_ternary_operation)
    driver_bug_workarounds |= SH_UNFOLD_SHORT_CIRCUIT;
  if (workarounds().scalarize_vec_and_mat_constructor_args)
    driver_bug_workarounds |= SH_SCALARIZE_VEC_AND_MAT_CONSTRUCTOR_ARGS;
  if (workarounds().regenerate_struct_names)
    driver_bug_workarounds |= SH_REGENERATE_STRUCT_NAMES;
  if (workarounds().remove_pow_with_constant_exponent)
    driver_bug_workarounds |= SH_REMOVE_POW_WITH_CONSTANT_EXPONENT;
  if (workarounds().emulate_abs_int_function)
    driver_bug_workarounds |= SH_EMULATE_ABS_INT_FUNCTION;
  if (workarounds().rewrite_texelfetchoffset_to_texelfetch)
    driver_bug_workarounds |= SH_REWRITE_TEXELFETCHOFFSET_TO_TEXELFETCH;
  if (workarounds().add_and_true_to_loop_condition)
    driver_bug_workarounds |= SH_ADD_AND_TRUE_TO_LOOP_CONDITION;
  if (workarounds().rewrite_do_while_loops)
    driver_bug_workarounds |= SH_REWRITE_DO_WHILE_LOOPS;
  if (workarounds().emulate_isnan_on_float)
    driver_bug_workarounds |= SH_EMULATE_ISNAN_FLOAT_FUNCTION;
  if (workarounds().use_unused_standard_shared_blocks)
    driver_bug_workarounds |= SH_USE_UNUSED_STANDARD_SHARED_BLOCKS;
  if (workarounds().dont_remove_invariant_for_fragment_input)
    driver_bug_workarounds |= SH_DONT_REMOVE_INVARIANT_FOR_FRAGMENT_INPUT;
  if (workarounds().remove_invariant_and_centroid_for_essl3)
    driver_bug_workarounds |= SH_REMOVE_INVARIANT_AND_CENTROID_FOR_ESSL3;

  resources.WEBGL_debug_shader_precision =
      group_->gpu_preferences().emulate_shader_precision;

  ShShaderOutput shader_output_language =
      ShaderTranslator::GetShaderOutputLanguageForContext(gl_version_info());

  vertex_translator_ = shader_translator_cache()->GetTranslator(
      GL_VERTEX_SHADER, shader_spec, &resources, shader_output_language,
      driver_bug_workarounds);
  if (!vertex_translator_.get()) {
    LOG(ERROR) << "Could not initialize vertex shader translator.";
    Destroy(true);
    return false;
  }

  fragment_translator_ = shader_translator_cache()->GetTranslator(
      GL_FRAGMENT_SHADER, shader_spec, &resources, shader_output_language,
      driver_bug_workarounds);
  if (!fragment_translator_.get()) {
    LOG(ERROR) << "Could not initialize fragment shader translator.";
    Destroy(true);
    return false;
  }
  return true;
}

bool GLES2DecoderImpl::GenBuffersHelper(GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (GetBuffer(client_ids[ii])) {
      return false;
    }
  }
  std::unique_ptr<GLuint[]> service_ids(new GLuint[n]);
  glGenBuffersARB(n, service_ids.get());
  for (GLsizei ii = 0; ii < n; ++ii) {
    CreateBuffer(client_ids[ii], service_ids[ii]);
  }
  return true;
}

bool GLES2DecoderImpl::GenFramebuffersHelper(
    GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (GetFramebuffer(client_ids[ii])) {
      return false;
    }
  }
  std::unique_ptr<GLuint[]> service_ids(new GLuint[n]);
  glGenFramebuffersEXT(n, service_ids.get());
  for (GLsizei ii = 0; ii < n; ++ii) {
    CreateFramebuffer(client_ids[ii], service_ids[ii]);
  }
  return true;
}

bool GLES2DecoderImpl::GenRenderbuffersHelper(
    GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (GetRenderbuffer(client_ids[ii])) {
      return false;
    }
  }
  std::unique_ptr<GLuint[]> service_ids(new GLuint[n]);
  glGenRenderbuffersEXT(n, service_ids.get());
  for (GLsizei ii = 0; ii < n; ++ii) {
    CreateRenderbuffer(client_ids[ii], service_ids[ii]);
  }
  return true;
}

bool GLES2DecoderImpl::GenTexturesHelper(GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (GetTexture(client_ids[ii])) {
      return false;
    }
  }
  std::unique_ptr<GLuint[]> service_ids(new GLuint[n]);
  glGenTextures(n, service_ids.get());
  for (GLsizei ii = 0; ii < n; ++ii) {
    CreateTexture(client_ids[ii], service_ids[ii]);
  }
  return true;
}

bool GLES2DecoderImpl::GenSamplersHelper(GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (GetSampler(client_ids[ii])) {
      return false;
    }
  }
  std::unique_ptr<GLuint[]> service_ids(new GLuint[n]);
  glGenSamplers(n, service_ids.get());
  for (GLsizei ii = 0; ii < n; ++ii) {
    CreateSampler(client_ids[ii], service_ids[ii]);
  }
  return true;
}

bool GLES2DecoderImpl::GenTransformFeedbacksHelper(
    GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (GetTransformFeedback(client_ids[ii])) {
      return false;
    }
  }
  std::unique_ptr<GLuint[]> service_ids(new GLuint[n]);
  glGenTransformFeedbacks(n, service_ids.get());
  for (GLsizei ii = 0; ii < n; ++ii) {
    CreateTransformFeedback(client_ids[ii], service_ids[ii]);
  }
  return true;
}

bool GLES2DecoderImpl::GenPathsCHROMIUMHelper(GLuint first_client_id,
                                              GLsizei range) {
  GLuint last_client_id;
  if (!SafeAddUint32(first_client_id, range - 1, &last_client_id))
    return false;

  if (path_manager()->HasPathsInRange(first_client_id, last_client_id))
    return false;

  GLuint first_service_id = glGenPathsNV(range);
  if (first_service_id == 0) {
    // We have to fail the connection here, because client has already
    // succeeded in allocating the ids. This happens if we allocate
    // the whole path id space (two allocations of 0x7FFFFFFF paths, for
    // example).
    return false;
  }
  // GenPathsNV does not wrap.
  DCHECK(first_service_id + range - 1 >= first_service_id);

  path_manager()->CreatePathRange(first_client_id, last_client_id,
                                  first_service_id);

  return true;
}

bool GLES2DecoderImpl::DeletePathsCHROMIUMHelper(GLuint first_client_id,
                                                 GLsizei range) {
  GLuint last_client_id;
  if (!SafeAddUint32(first_client_id, range - 1, &last_client_id))
    return false;

  path_manager()->RemovePaths(first_client_id, last_client_id);
  return true;
}

void GLES2DecoderImpl::DeleteBuffersHelper(GLsizei n,
                                           const volatile GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    GLuint client_id = client_ids[ii];
    Buffer* buffer = GetBuffer(client_id);
    if (buffer && !buffer->IsDeleted()) {
      buffer->RemoveMappedRange();
      state_.RemoveBoundBuffer(buffer);
      RemoveBuffer(client_id);
    }
  }
}

void GLES2DecoderImpl::DeleteFramebuffersHelper(
    GLsizei n,
    const volatile GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    GLuint client_id = client_ids[ii];
    Framebuffer* framebuffer = GetFramebuffer(client_id);
    if (framebuffer && !framebuffer->IsDeleted()) {
      if (framebuffer == framebuffer_state_.bound_draw_framebuffer.get()) {
        GLenum target = GetDrawFramebufferTarget();

        // Unbind attachments on FBO before deletion.
        if (workarounds().unbind_attachments_on_bound_render_fbo_delete)
          framebuffer->DoUnbindGLAttachmentsForWorkaround(target);

        glBindFramebufferEXT(target, GetBackbufferServiceId());
        framebuffer_state_.bound_draw_framebuffer = NULL;
        framebuffer_state_.clear_state_dirty = true;
      }
      if (framebuffer == framebuffer_state_.bound_read_framebuffer.get()) {
        framebuffer_state_.bound_read_framebuffer = NULL;
        GLenum target = GetReadFramebufferTarget();
        glBindFramebufferEXT(target, GetBackbufferServiceId());
      }
      OnFboChanged();
      RemoveFramebuffer(client_id);
    }
  }
}

void GLES2DecoderImpl::DeleteRenderbuffersHelper(
    GLsizei n,
    const volatile GLuint* client_ids) {
  bool supports_separate_framebuffer_binds =
     features().chromium_framebuffer_multisample;
  for (GLsizei ii = 0; ii < n; ++ii) {
    GLuint client_id = client_ids[ii];
    Renderbuffer* renderbuffer = GetRenderbuffer(client_id);
    if (renderbuffer && !renderbuffer->IsDeleted()) {
      if (state_.bound_renderbuffer.get() == renderbuffer) {
        state_.bound_renderbuffer = NULL;
      }
      // Unbind from current framebuffers.
      if (supports_separate_framebuffer_binds) {
        if (framebuffer_state_.bound_read_framebuffer.get()) {
          framebuffer_state_.bound_read_framebuffer
              ->UnbindRenderbuffer(GL_READ_FRAMEBUFFER_EXT, renderbuffer);
        }
        if (framebuffer_state_.bound_draw_framebuffer.get()) {
          framebuffer_state_.bound_draw_framebuffer
              ->UnbindRenderbuffer(GL_DRAW_FRAMEBUFFER_EXT, renderbuffer);
        }
      } else {
        if (framebuffer_state_.bound_draw_framebuffer.get()) {
          framebuffer_state_.bound_draw_framebuffer
              ->UnbindRenderbuffer(GL_FRAMEBUFFER, renderbuffer);
        }
      }
      framebuffer_state_.clear_state_dirty = true;
      RemoveRenderbuffer(client_id);
    }
  }
}

void GLES2DecoderImpl::DeleteTexturesHelper(GLsizei n,
                                            const volatile GLuint* client_ids) {
  bool supports_separate_framebuffer_binds = SupportsSeparateFramebufferBinds();
  for (GLsizei ii = 0; ii < n; ++ii) {
    GLuint client_id = client_ids[ii];
    TextureRef* texture_ref = GetTexture(client_id);
    if (texture_ref) {
      Texture* texture = texture_ref->texture();
      if (texture->IsAttachedToFramebuffer()) {
        framebuffer_state_.clear_state_dirty = true;
      }
      // Unbind texture_ref from texture_ref units.
      state_.UnbindTexture(texture_ref);

      // Unbind from current framebuffers.
      if (supports_separate_framebuffer_binds) {
        if (framebuffer_state_.bound_read_framebuffer.get()) {
          framebuffer_state_.bound_read_framebuffer
              ->UnbindTexture(GL_READ_FRAMEBUFFER_EXT, texture_ref);
        }
        if (framebuffer_state_.bound_draw_framebuffer.get()) {
          framebuffer_state_.bound_draw_framebuffer
              ->UnbindTexture(GL_DRAW_FRAMEBUFFER_EXT, texture_ref);
        }
      } else {
        if (framebuffer_state_.bound_draw_framebuffer.get()) {
          framebuffer_state_.bound_draw_framebuffer
              ->UnbindTexture(GL_FRAMEBUFFER, texture_ref);
        }
      }
      RemoveTexture(client_id);
    }
  }
}

void GLES2DecoderImpl::DeleteSamplersHelper(GLsizei n,
                                            const volatile GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    GLuint client_id = client_ids[ii];
    Sampler* sampler = GetSampler(client_id);
    if (sampler && !sampler->IsDeleted()) {
      // Unbind from current sampler units.
      state_.UnbindSampler(sampler);

      RemoveSampler(client_id);
    }
  }
}

void GLES2DecoderImpl::DeleteTransformFeedbacksHelper(
    GLsizei n,
    const volatile GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    GLuint client_id = client_ids[ii];
    TransformFeedback* transform_feedback = GetTransformFeedback(client_id);
    if (transform_feedback) {
      if (transform_feedback->active()) {
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glDeleteTransformFeedbacks",
                           "Deleting transform feedback is active");
        return;
      }
      if (state_.bound_transform_feedback.get() == transform_feedback) {
        // Bind to the default transform feedback.
        DCHECK(state_.default_transform_feedback.get());
        state_.default_transform_feedback->DoBindTransformFeedback(
            GL_TRANSFORM_FEEDBACK);
        state_.bound_transform_feedback =
            state_.default_transform_feedback.get();
      }
      RemoveTransformFeedback(client_id);
    }
  }
}

void GLES2DecoderImpl::DeleteSyncHelper(GLuint sync) {
  GLsync service_id = 0;
  if (group_->GetSyncServiceId(sync, &service_id)) {
    glDeleteSync(service_id);
    group_->RemoveSyncId(sync);
  } else if (sync != 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glDeleteSync", "unknown sync");
  }
}

bool GLES2DecoderImpl::MakeCurrent() {
  DCHECK(surface_);
  if (!context_.get())
    return false;

  if (WasContextLost()) {
    LOG(ERROR) << "  GLES2DecoderImpl: Trying to make lost context current.";
    return false;
  }

  if (!context_->MakeCurrent(surface_.get())) {
    LOG(ERROR) << "  GLES2DecoderImpl: Context lost during MakeCurrent.";
    MarkContextLost(error::kMakeCurrentFailed);
    group_->LoseContexts(error::kUnknown);
    return false;
  }

  if (CheckResetStatus()) {
    LOG(ERROR)
        << "  GLES2DecoderImpl: Context reset detected after MakeCurrent.";
    group_->LoseContexts(error::kUnknown);
    return false;
  }

  ProcessFinishedAsyncTransfers();

  // Rebind the FBO if it was unbound by the context.
  if (workarounds().unbind_fbo_on_context_switch)
    RestoreFramebufferBindings();

  framebuffer_state_.clear_state_dirty = true;

  // Rebind textures if the service ids may have changed.
  RestoreAllExternalTextureBindingsIfNeeded();

  return true;
}

void GLES2DecoderImpl::ProcessFinishedAsyncTransfers() {
  ProcessPendingReadPixels(false);
  if (engine() && query_manager_.get())
    query_manager_->ProcessPendingTransferQueries();
}

static void RebindCurrentFramebuffer(
    GLenum target,
    Framebuffer* framebuffer,
    GLuint back_buffer_service_id) {
  GLuint framebuffer_id = framebuffer ? framebuffer->service_id() : 0;

  if (framebuffer_id == 0) {
    framebuffer_id = back_buffer_service_id;
  }

  glBindFramebufferEXT(target, framebuffer_id);
}

void GLES2DecoderImpl::RestoreCurrentFramebufferBindings() {
  framebuffer_state_.clear_state_dirty = true;

  if (!SupportsSeparateFramebufferBinds()) {
    RebindCurrentFramebuffer(
        GL_FRAMEBUFFER,
        framebuffer_state_.bound_draw_framebuffer.get(),
        GetBackbufferServiceId());
  } else {
    RebindCurrentFramebuffer(
        GL_READ_FRAMEBUFFER_EXT,
        framebuffer_state_.bound_read_framebuffer.get(),
        GetBackbufferServiceId());
    RebindCurrentFramebuffer(
        GL_DRAW_FRAMEBUFFER_EXT,
        framebuffer_state_.bound_draw_framebuffer.get(),
        GetBackbufferServiceId());
  }
  OnFboChanged();
}

bool GLES2DecoderImpl::CheckFramebufferValid(
    Framebuffer* framebuffer,
    GLenum target,
    GLenum gl_error,
    const char* func_name) {
  if (!framebuffer) {
    if (surfaceless_)
      return false;
    if (backbuffer_needs_clear_bits_) {
      glClearColor(0, 0, 0, BackBufferAlphaClearColor());
      state_.SetDeviceColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      glClearStencil(0);
      state_.SetDeviceStencilMaskSeparate(GL_FRONT, kDefaultStencilMask);
      state_.SetDeviceStencilMaskSeparate(GL_BACK, kDefaultStencilMask);
      glClearDepth(1.0f);
      state_.SetDeviceDepthMask(GL_TRUE);
      state_.SetDeviceCapabilityState(GL_SCISSOR_TEST, false);
      bool reset_draw_buffer = false;
      if ((backbuffer_needs_clear_bits_ & GL_COLOR_BUFFER_BIT) != 0 &&
          back_buffer_draw_buffer_ == GL_NONE) {
        reset_draw_buffer = true;
        GLenum buf = GL_BACK;
        if (GetBackbufferServiceId() != 0)  // emulated backbuffer
          buf = GL_COLOR_ATTACHMENT0;
        glDrawBuffersARB(1, &buf);
      }
      if (workarounds().gl_clear_broken) {
        ClearFramebufferForWorkaround(backbuffer_needs_clear_bits_);
      } else {
        glClear(backbuffer_needs_clear_bits_);
      }
      if (reset_draw_buffer) {
        GLenum buf = GL_NONE;
        glDrawBuffersARB(1, &buf);
      }
      backbuffer_needs_clear_bits_ = 0;
      RestoreClearState();
    }
    return true;
  }

  if (!framebuffer_manager()->IsComplete(framebuffer)) {
    GLenum completeness = framebuffer->IsPossiblyComplete(feature_info_.get());
    if (completeness != GL_FRAMEBUFFER_COMPLETE) {
      LOCAL_SET_GL_ERROR(gl_error, func_name, "framebuffer incomplete");
      return false;
    }

    if (framebuffer->GetStatus(texture_manager(), target) !=
        GL_FRAMEBUFFER_COMPLETE) {
      LOCAL_SET_GL_ERROR(
          gl_error, func_name, "framebuffer incomplete (check)");
      return false;
    }
    framebuffer_manager()->MarkAsComplete(framebuffer);
  }

  // Are all the attachments cleared?
  if (renderbuffer_manager()->HaveUnclearedRenderbuffers() ||
      texture_manager()->HaveUnclearedMips()) {
    if (!framebuffer->IsCleared()) {
      ClearUnclearedAttachments(target, framebuffer);
    }
  }
  return true;
}

bool GLES2DecoderImpl::CheckBoundDrawFramebufferValid(const char* func_name) {
  Framebuffer* framebuffer = GetBoundDrawFramebuffer();
  bool valid = CheckFramebufferValid(
      framebuffer, GetDrawFramebufferTarget(),
      GL_INVALID_FRAMEBUFFER_OPERATION, func_name);
  if (!valid)
    return false;

  if (!SupportsSeparateFramebufferBinds())
    OnUseFramebuffer();
  if (valid && feature_info_->feature_flags().desktop_srgb_support) {
    // If framebuffer contains sRGB images, then enable FRAMEBUFFER_SRGB.
    // Otherwise, disable FRAMEBUFFER_SRGB. Assume default fbo does not have
    // sRGB image.
    // In theory, we can just leave FRAMEBUFFER_SRGB on. However, many drivers
    // behave incorrectly when all images are linear encoding, they still apply
    // the sRGB conversion, but when at least one image is sRGB, then they
    // behave correctly.
    bool enable_framebuffer_srgb =
        framebuffer && framebuffer->HasSRGBAttachments();
    state_.EnableDisableFramebufferSRGB(enable_framebuffer_srgb);
  }
  return valid;
}

bool GLES2DecoderImpl::CheckBoundReadFramebufferValid(
    const char* func_name, GLenum gl_error) {
  Framebuffer* framebuffer = GetBoundReadFramebuffer();
  bool valid = CheckFramebufferValid(
      framebuffer, GetReadFramebufferTarget(), gl_error, func_name);
  return valid;
}

bool GLES2DecoderImpl::CheckBoundFramebufferValid(const char* func_name) {
  GLenum gl_error = GL_INVALID_FRAMEBUFFER_OPERATION;
  Framebuffer* draw_framebuffer = GetBoundDrawFramebuffer();
  bool valid = CheckFramebufferValid(
      draw_framebuffer, GetDrawFramebufferTarget(), gl_error, func_name);

  Framebuffer* read_framebuffer = GetBoundReadFramebuffer();
  valid = valid && CheckFramebufferValid(
      read_framebuffer, GetReadFramebufferTarget(), gl_error, func_name);
  return valid;
}

GLint GLES2DecoderImpl::GetColorEncodingFromInternalFormat(
    GLenum internalformat) {
  switch (internalformat) {
    case GL_SRGB_EXT:
    case GL_SRGB_ALPHA_EXT:
    case GL_SRGB8:
    case GL_SRGB8_ALPHA8:
      return GL_SRGB;
    default:
      return GL_LINEAR;
  }
}

bool GLES2DecoderImpl::FormsTextureCopyingFeedbackLoop(
    TextureRef* texture, GLint level, GLint layer) {
  Framebuffer* framebuffer = GetBoundReadFramebuffer();
  if (!framebuffer)
    return false;
  const Framebuffer::Attachment* attachment =
      framebuffer->GetReadBufferAttachment();
  if (!attachment)
    return false;
  return attachment->FormsFeedbackLoop(texture, level, layer);
}

gfx::Size GLES2DecoderImpl::GetBoundReadFramebufferSize() {
  Framebuffer* framebuffer = GetBoundReadFramebuffer();
  if (framebuffer) {
    const Framebuffer::Attachment* attachment =
        framebuffer->GetReadBufferAttachment();
    if (attachment) {
      return gfx::Size(attachment->width(), attachment->height());
    }
    return gfx::Size(0, 0);
  } else if (offscreen_target_frame_buffer_.get()) {
    return offscreen_size_;
  } else {
    return surface_->GetSize();
  }
}

GLuint GLES2DecoderImpl::GetBoundReadFramebufferServiceId() {
  Framebuffer* framebuffer = GetBoundReadFramebuffer();
  if (framebuffer) {
    return framebuffer->service_id();
  }
  if (offscreen_resolved_frame_buffer_.get()) {
    return offscreen_resolved_frame_buffer_->id();
  }
  if (offscreen_target_frame_buffer_.get()) {
    return offscreen_target_frame_buffer_->id();
  }
  if (surface_.get()) {
    return surface_->GetBackingFramebufferObject();
  }
  return 0;
}

GLuint GLES2DecoderImpl::GetBoundDrawFramebufferServiceId() {
  Framebuffer* framebuffer = GetBoundDrawFramebuffer();
  if (framebuffer) {
    return framebuffer->service_id();
  }
  if (offscreen_target_frame_buffer_.get()) {
    return offscreen_target_frame_buffer_->id();
  }
  if (surface_.get()) {
    return surface_->GetBackingFramebufferObject();
  }
  return 0;
}

GLenum GLES2DecoderImpl::GetBoundReadFramebufferTextureType() {
  Framebuffer* framebuffer = GetBoundReadFramebuffer();
  if (framebuffer) {
    return framebuffer->GetReadBufferTextureType();
  } else {  // Back buffer.
    if (back_buffer_read_buffer_ == GL_NONE)
      return 0;
    return GL_UNSIGNED_BYTE;
  }
}

GLenum GLES2DecoderImpl::GetBoundReadFramebufferInternalFormat() {
  Framebuffer* framebuffer = GetBoundReadFramebuffer();
  if (framebuffer) {
    return framebuffer->GetReadBufferInternalFormat();
  } else {  // Back buffer.
    if (back_buffer_read_buffer_ == GL_NONE)
      return 0;
    if (offscreen_target_frame_buffer_.get()) {
      return offscreen_target_color_format_;
    }
    return back_buffer_color_format_;
  }
}

GLenum GLES2DecoderImpl::GetBoundColorDrawBufferType(GLint drawbuffer_i) {
  DCHECK(drawbuffer_i >= 0 &&
         drawbuffer_i < static_cast<GLint>(group_->max_draw_buffers()));
  Framebuffer* framebuffer = GetBoundDrawFramebuffer();
  if (!framebuffer) {
    return 0;
  }
  GLenum drawbuffer = static_cast<GLenum>(GL_DRAW_BUFFER0 + drawbuffer_i);
  if (framebuffer->GetDrawBuffer(drawbuffer) == GL_NONE) {
    return 0;
  }
  GLenum attachment = static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + drawbuffer_i);
  const Framebuffer::Attachment* buffer =
      framebuffer->GetAttachment(attachment);
  if (!buffer) {
    return 0;
  }
  return buffer->texture_type();
}

GLenum GLES2DecoderImpl::GetBoundColorDrawBufferInternalFormat(
    GLint drawbuffer_i) {
  DCHECK(drawbuffer_i >= 0 &&
         drawbuffer_i < static_cast<GLint>(group_->max_draw_buffers()));
  Framebuffer* framebuffer = GetBoundDrawFramebuffer();
  if (!framebuffer) {
    return 0;
  }
  GLenum drawbuffer = static_cast<GLenum>(GL_DRAW_BUFFER0 + drawbuffer_i);
  if (framebuffer->GetDrawBuffer(drawbuffer) == GL_NONE) {
    return 0;
  }
  GLenum attachment = static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + drawbuffer_i);
  const Framebuffer::Attachment* buffer =
      framebuffer->GetAttachment(attachment);
  if (!buffer) {
    return 0;
  }
  return buffer->internal_format();
}

GLsizei GLES2DecoderImpl::GetBoundFramebufferSamples(GLenum target) {
  DCHECK(target == GL_DRAW_FRAMEBUFFER || target == GL_READ_FRAMEBUFFER ||
         target == GL_FRAMEBUFFER);
  Framebuffer* framebuffer = GetFramebufferInfoForTarget(target);
  if (framebuffer) {
    return framebuffer->GetSamples();
  } else {  // Back buffer.
    if (offscreen_target_frame_buffer_.get()) {
      return offscreen_target_samples_;
    }
    return 0;
  }
}

GLenum GLES2DecoderImpl::GetBoundFramebufferDepthFormat(
    GLenum target) {
  DCHECK(target == GL_DRAW_FRAMEBUFFER || target == GL_READ_FRAMEBUFFER ||
         target == GL_FRAMEBUFFER);
  Framebuffer* framebuffer = GetFramebufferInfoForTarget(target);
  if (framebuffer) {
    return framebuffer->GetDepthFormat();
  } else {  // Back buffer.
    if (offscreen_target_frame_buffer_.get()) {
      return offscreen_target_depth_format_;
    }
    if (back_buffer_has_depth_)
      return GL_DEPTH;
    return 0;
  }
}

GLenum GLES2DecoderImpl::GetBoundFramebufferStencilFormat(
    GLenum target) {
  DCHECK(target == GL_DRAW_FRAMEBUFFER || target == GL_READ_FRAMEBUFFER ||
         target == GL_FRAMEBUFFER);
  Framebuffer* framebuffer = GetFramebufferInfoForTarget(target);
  if (framebuffer) {
    return framebuffer->GetStencilFormat();
  } else {  // Back buffer.
    if (offscreen_target_frame_buffer_.get()) {
      return offscreen_target_stencil_format_;
    }
    if (back_buffer_has_stencil_)
      return GL_STENCIL;
    return 0;
  }
}

void GLES2DecoderImpl::MarkDrawBufferAsCleared(
    GLenum buffer, GLint drawbuffer_i) {
  Framebuffer* framebuffer = GetBoundDrawFramebuffer();
  if (!framebuffer)
    return;
  GLenum attachment  = 0;
  switch (buffer) {
    case GL_COLOR:
      DCHECK(drawbuffer_i >= 0 &&
             drawbuffer_i < static_cast<GLint>(group_->max_draw_buffers()));
      attachment = static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + drawbuffer_i);
      break;
    case GL_DEPTH:
      attachment = GL_DEPTH_ATTACHMENT;
      break;
    case GL_STENCIL:
      attachment = GL_STENCIL_ATTACHMENT;
      break;
    default:
      // Caller is responsible for breaking GL_DEPTH_STENCIL into GL_DEPTH and
      // GL_STENCIL.
      NOTREACHED();
  }
  framebuffer->MarkAttachmentAsCleared(
      renderbuffer_manager(), texture_manager(), attachment, true);
}

Logger* GLES2DecoderImpl::GetLogger() {
  return &logger_;
}

void GLES2DecoderImpl::BeginDecoding() {
  gpu_tracer_->BeginDecoding();
  gpu_trace_commands_ = gpu_tracer_->IsTracing() && *gpu_decoder_category_;
  gpu_debug_commands_ = log_commands() || debug() || gpu_trace_commands_;
  query_manager_->ProcessFrameBeginUpdates();
}

void GLES2DecoderImpl::EndDecoding() {
  gpu_tracer_->EndDecoding();
}

ErrorState* GLES2DecoderImpl::GetErrorState() {
  return state_.GetErrorState();
}

void GLES2DecoderImpl::SetShaderCacheCallback(
    const ShaderCacheCallback& callback) {
  shader_cache_callback_ = callback;
}

void GLES2DecoderImpl::SetFenceSyncReleaseCallback(
    const FenceSyncReleaseCallback& callback) {
  fence_sync_release_callback_ = callback;
}

void GLES2DecoderImpl::SetWaitFenceSyncCallback(
    const WaitFenceSyncCallback& callback) {
  wait_fence_sync_callback_ = callback;
}

void GLES2DecoderImpl::SetDescheduleUntilFinishedCallback(
    const NoParamCallback& callback) {
  deschedule_until_finished_callback_ = callback;
}

void GLES2DecoderImpl::SetRescheduleAfterFinishedCallback(
    const NoParamCallback& callback) {
  reschedule_after_finished_callback_ = callback;
}

bool GLES2DecoderImpl::GetServiceTextureId(uint32_t client_texture_id,
                                           uint32_t* service_texture_id) {
  TextureRef* texture_ref = texture_manager()->GetTexture(client_texture_id);
  if (texture_ref) {
    *service_texture_id = texture_ref->service_id();
    return true;
  }
  return false;
}

uint32_t GLES2DecoderImpl::GetTextureUploadCount() {
  return texture_state_.texture_upload_count;
}

base::TimeDelta GLES2DecoderImpl::GetTotalTextureUploadTime() {
  return texture_state_.total_texture_upload_time;
}

base::TimeDelta GLES2DecoderImpl::GetTotalProcessingCommandsTime() {
  return total_processing_commands_time_;
}

void GLES2DecoderImpl::AddProcessingCommandsTime(base::TimeDelta time) {
  total_processing_commands_time_ += time;
}

void GLES2DecoderImpl::Destroy(bool have_context) {
  if (!initialized())
    return;

  DCHECK(!have_context || context_->IsCurrent(nullptr));

  ReleaseAllBackTextures(have_context);
  if (have_context) {
    if (apply_framebuffer_attachment_cmaa_intel_.get()) {
      apply_framebuffer_attachment_cmaa_intel_->Destroy();
      apply_framebuffer_attachment_cmaa_intel_.reset();
    }

    if (copy_tex_image_blit_.get()) {
      copy_tex_image_blit_->Destroy();
      copy_tex_image_blit_.reset();
    }

    if (copy_texture_CHROMIUM_.get()) {
      copy_texture_CHROMIUM_->Destroy();
      copy_texture_CHROMIUM_.reset();
    }

    if (srgb_converter_.get()) {
      srgb_converter_->Destroy();
      srgb_converter_.reset();
    }

    clear_framebuffer_blit_.reset();

    if (state_.current_program.get()) {
      program_manager()->UnuseProgram(shader_manager(),
                                      state_.current_program.get());
    }

    if (attrib_0_buffer_id_) {
      glDeleteBuffersARB(1, &attrib_0_buffer_id_);
    }
    if (fixed_attrib_buffer_id_) {
      glDeleteBuffersARB(1, &fixed_attrib_buffer_id_);
    }

    if (validation_fbo_) {
      glDeleteFramebuffersEXT(1, &validation_fbo_multisample_);
      glDeleteFramebuffersEXT(1, &validation_fbo_);
    }
    while (!validation_textures_.empty()) {
      GLuint tex;
      tex = validation_textures_.begin()->second;
      glDeleteTextures(1, &tex);
      validation_textures_.erase(validation_textures_.begin());
    }

    if (offscreen_target_frame_buffer_.get())
      offscreen_target_frame_buffer_->Destroy();
    if (offscreen_target_color_texture_.get())
      offscreen_target_color_texture_->Destroy();
    if (offscreen_target_color_render_buffer_.get())
      offscreen_target_color_render_buffer_->Destroy();
    if (offscreen_target_depth_render_buffer_.get())
      offscreen_target_depth_render_buffer_->Destroy();
    if (offscreen_target_stencil_render_buffer_.get())
      offscreen_target_stencil_render_buffer_->Destroy();
    if (offscreen_saved_frame_buffer_.get())
      offscreen_saved_frame_buffer_->Destroy();
    if (offscreen_saved_color_texture_.get())
      offscreen_saved_color_texture_->Destroy();
    if (offscreen_resolved_frame_buffer_.get())
      offscreen_resolved_frame_buffer_->Destroy();
    if (offscreen_resolved_color_texture_.get())
      offscreen_resolved_color_texture_->Destroy();
  } else {
    if (offscreen_target_frame_buffer_.get())
      offscreen_target_frame_buffer_->Invalidate();
    if (offscreen_target_color_texture_.get())
      offscreen_target_color_texture_->Invalidate();
    if (offscreen_target_color_render_buffer_.get())
      offscreen_target_color_render_buffer_->Invalidate();
    if (offscreen_target_depth_render_buffer_.get())
      offscreen_target_depth_render_buffer_->Invalidate();
    if (offscreen_target_stencil_render_buffer_.get())
      offscreen_target_stencil_render_buffer_->Invalidate();
    if (offscreen_saved_frame_buffer_.get())
      offscreen_saved_frame_buffer_->Invalidate();
    if (offscreen_saved_color_texture_.get())
      offscreen_saved_color_texture_->Invalidate();
    if (offscreen_resolved_frame_buffer_.get())
      offscreen_resolved_frame_buffer_->Invalidate();
    if (offscreen_resolved_color_texture_.get())
      offscreen_resolved_color_texture_->Invalidate();
    for (auto& fence : deschedule_until_finished_fences_) {
      fence->Invalidate();
    }
  }
  deschedule_until_finished_fences_.clear();

  // Unbind everything.
  state_.vertex_attrib_manager = nullptr;
  state_.default_vertex_attrib_manager = nullptr;
  state_.texture_units.clear();
  state_.sampler_units.clear();
  state_.bound_array_buffer = nullptr;
  state_.bound_copy_read_buffer = nullptr;
  state_.bound_copy_write_buffer = nullptr;
  state_.bound_pixel_pack_buffer = nullptr;
  state_.bound_pixel_unpack_buffer = nullptr;
  state_.bound_transform_feedback_buffer = nullptr;
  state_.bound_uniform_buffer = nullptr;
  framebuffer_state_.bound_read_framebuffer = nullptr;
  framebuffer_state_.bound_draw_framebuffer = nullptr;
  state_.bound_renderbuffer = nullptr;
  state_.bound_transform_feedback = nullptr;
  state_.default_transform_feedback = nullptr;
  state_.indexed_uniform_buffer_bindings = nullptr;

  // Current program must be cleared after calling ProgramManager::UnuseProgram.
  // Otherwise, we can leak objects. http://crbug.com/258772.
  // state_.current_program must be reset before group_ is reset because
  // the later deletes the ProgramManager object that referred by
  // state_.current_program object.
  state_.current_program = NULL;

  apply_framebuffer_attachment_cmaa_intel_.reset();
  copy_tex_image_blit_.reset();
  copy_texture_CHROMIUM_.reset();
  srgb_converter_.reset();
  clear_framebuffer_blit_.reset();

  if (query_manager_.get()) {
    query_manager_->Destroy(have_context);
    query_manager_.reset();
  }

  if (vertex_array_manager_ .get()) {
    vertex_array_manager_->Destroy(have_context);
    vertex_array_manager_.reset();
  }

  if (transform_feedback_manager_.get()) {
    if (!have_context) {
      transform_feedback_manager_->MarkContextLost();
    }
    transform_feedback_manager_->Destroy();
    transform_feedback_manager_.reset();
  }

  image_manager_.reset();

  offscreen_target_frame_buffer_.reset();
  offscreen_target_color_texture_.reset();
  offscreen_target_color_render_buffer_.reset();
  offscreen_target_depth_render_buffer_.reset();
  offscreen_target_stencil_render_buffer_.reset();
  offscreen_saved_frame_buffer_.reset();
  offscreen_saved_color_texture_.reset();
  offscreen_resolved_frame_buffer_.reset();
  offscreen_resolved_color_texture_.reset();

  // Need to release these before releasing |group_| which may own the
  // ShaderTranslatorCache.
  fragment_translator_ = NULL;
  vertex_translator_ = NULL;

  // Destroy the GPU Tracer which may own some in process GPU Timings.
  if (gpu_tracer_) {
    gpu_tracer_->Destroy(have_context);
    gpu_tracer_.reset();
  }

  if (group_.get()) {
    group_->Destroy(this, have_context);
    group_ = NULL;
  }

  if (context_.get()) {
    context_->ReleaseCurrent(NULL);
    context_ = NULL;
  }
}

void GLES2DecoderImpl::SetSurface(const scoped_refptr<gl::GLSurface>& surface) {
  DCHECK(context_->IsCurrent(NULL));
  DCHECK(surface);
  surface_ = surface;
  RestoreCurrentFramebufferBindings();
}

void GLES2DecoderImpl::ReleaseSurface() {
  if (!context_.get())
    return;
  if (WasContextLost()) {
    DLOG(ERROR) << "  GLES2DecoderImpl: Trying to release lost context.";
    return;
  }
  context_->ReleaseCurrent(surface_.get());
  surface_ = nullptr;
}

void GLES2DecoderImpl::TakeFrontBuffer(const Mailbox& mailbox) {
  if (!offscreen_saved_color_texture_.get()) {
    DLOG(ERROR) << "Called TakeFrontBuffer on a non-offscreen context";
    return;
  }

  mailbox_manager()->ProduceTexture(
      mailbox, offscreen_saved_color_texture_->texture_ref()->texture());

  // Save the BackTexture and TextureRef. There's no need to update
  // |offscreen_saved_frame_buffer_| since CreateBackTexture() will take care of
  // that.
  SavedBackTexture save;
  save.back_texture.swap(offscreen_saved_color_texture_);
  save.in_use = true;
  saved_back_textures_.push_back(std::move(save));

  CreateBackTexture();
}

void GLES2DecoderImpl::ReturnFrontBuffer(const Mailbox& mailbox, bool is_lost) {
  Texture* texture =
      static_cast<Texture*>(group_->mailbox_manager()->ConsumeTexture(mailbox));

  for (auto it = saved_back_textures_.begin(); it != saved_back_textures_.end();
       ++it) {
    if (texture != it->back_texture->texture_ref()->texture())
      continue;

    if (is_lost || it->back_texture->size() != offscreen_size_) {
      it->back_texture->Invalidate();
      saved_back_textures_.erase(it);
      return;
    }

    it->in_use = false;
    return;
  }

  DLOG(ERROR) << "Attempting to return a frontbuffer that was not saved.";
}

void GLES2DecoderImpl::CreateBackTexture() {
  for (auto it = saved_back_textures_.begin(); it != saved_back_textures_.end();
       ++it) {
    if (it->in_use)
      continue;

    if (it->back_texture->size() != offscreen_size_)
      continue;
    offscreen_saved_color_texture_ = std::move(it->back_texture);
    offscreen_saved_frame_buffer_->AttachRenderTexture(
        offscreen_saved_color_texture_.get());
    saved_back_textures_.erase(it);
    return;
  }

  ++create_back_texture_count_for_test_;
  offscreen_saved_color_texture_.reset(new BackTexture(this));
  offscreen_saved_color_texture_->Create();
  offscreen_saved_color_texture_->AllocateStorage(
      offscreen_size_, offscreen_saved_color_format_, false);
  offscreen_saved_frame_buffer_->AttachRenderTexture(
      offscreen_saved_color_texture_.get());
}

void GLES2DecoderImpl::ReleaseNotInUseBackTextures() {
  for (auto& saved_back_texture : saved_back_textures_) {
    if (!saved_back_texture.in_use)
      saved_back_texture.back_texture->Destroy();
  }
  auto to_remove =
      std::remove_if(saved_back_textures_.begin(), saved_back_textures_.end(),
                     [](const SavedBackTexture& saved_back_texture) {
                       return !saved_back_texture.in_use;
                     });
  saved_back_textures_.erase(to_remove, saved_back_textures_.end());
}

void GLES2DecoderImpl::ReleaseAllBackTextures(bool have_context) {
  for (auto& saved_back_texture : saved_back_textures_) {
    if (have_context)
      saved_back_texture.back_texture->Destroy();
    else
      saved_back_texture.back_texture->Invalidate();
  }
  saved_back_textures_.clear();
}

size_t GLES2DecoderImpl::GetSavedBackTextureCountForTest() {
  return saved_back_textures_.size();
}

size_t GLES2DecoderImpl::GetCreatedBackTextureCountForTest() {
  return create_back_texture_count_for_test_;
}

bool GLES2DecoderImpl::ResizeOffscreenFramebuffer(const gfx::Size& size) {
  bool is_offscreen = !!offscreen_target_frame_buffer_.get();
  if (!is_offscreen) {
    LOG(ERROR) << "GLES2DecoderImpl::ResizeOffscreenFramebuffer called "
               << " with an onscreen framebuffer.";
    return false;
  }

  if (offscreen_size_ == size)
    return true;

  offscreen_size_ = size;
  int w = offscreen_size_.width();
  int h = offscreen_size_.height();
  if (w < 0 || h < 0 || h >= (INT_MAX / 4) / (w ? w : 1)) {
    LOG(ERROR) << "GLES2DecoderImpl::ResizeOffscreenFramebuffer failed "
               << "to allocate storage due to excessive dimensions.";
    return false;
  }

  // Reallocate the offscreen target buffers.
  DCHECK(offscreen_target_color_format_);
  if (IsOffscreenBufferMultisampled()) {
    if (!offscreen_target_color_render_buffer_->AllocateStorage(
            feature_info_.get(), offscreen_size_,
            offscreen_target_color_format_, offscreen_target_samples_)) {
      LOG(ERROR) << "GLES2DecoderImpl::ResizeOffscreenFramebuffer failed "
                 << "to allocate storage for offscreen target color buffer.";
      return false;
    }
  } else {
    if (!offscreen_target_color_texture_->AllocateStorage(
        offscreen_size_, offscreen_target_color_format_, false)) {
      LOG(ERROR) << "GLES2DecoderImpl::ResizeOffscreenFramebuffer failed "
                 << "to allocate storage for offscreen target color texture.";
      return false;
    }
  }
  if (offscreen_target_depth_format_ &&
      !offscreen_target_depth_render_buffer_->AllocateStorage(
          feature_info_.get(),
          offscreen_size_,
          offscreen_target_depth_format_,
          offscreen_target_samples_)) {
    LOG(ERROR) << "GLES2DecoderImpl::ResizeOffscreenFramebuffer failed "
               << "to allocate storage for offscreen target depth buffer.";
    return false;
  }
  if (offscreen_target_stencil_format_ &&
      !offscreen_target_stencil_render_buffer_->AllocateStorage(
          feature_info_.get(),
          offscreen_size_,
          offscreen_target_stencil_format_,
          offscreen_target_samples_)) {
    LOG(ERROR) << "GLES2DecoderImpl::ResizeOffscreenFramebuffer failed "
               << "to allocate storage for offscreen target stencil buffer.";
    return false;
  }

  // Attach the offscreen target buffers to the target frame buffer.
  if (IsOffscreenBufferMultisampled()) {
    offscreen_target_frame_buffer_->AttachRenderBuffer(
        GL_COLOR_ATTACHMENT0,
        offscreen_target_color_render_buffer_.get());
  } else {
    offscreen_target_frame_buffer_->AttachRenderTexture(
        offscreen_target_color_texture_.get());
  }
  if (offscreen_target_depth_format_) {
    offscreen_target_frame_buffer_->AttachRenderBuffer(
        GL_DEPTH_ATTACHMENT,
        offscreen_target_depth_render_buffer_.get());
  }
  const bool packed_depth_stencil =
      offscreen_target_depth_format_ == GL_DEPTH24_STENCIL8;
  if (packed_depth_stencil) {
    offscreen_target_frame_buffer_->AttachRenderBuffer(
        GL_STENCIL_ATTACHMENT,
        offscreen_target_depth_render_buffer_.get());
  } else if (offscreen_target_stencil_format_) {
    offscreen_target_frame_buffer_->AttachRenderBuffer(
        GL_STENCIL_ATTACHMENT,
        offscreen_target_stencil_render_buffer_.get());
  }

  if (offscreen_target_frame_buffer_->CheckStatus() !=
      GL_FRAMEBUFFER_COMPLETE) {
      LOG(ERROR) << "GLES2DecoderImpl::ResizeOffscreenFramebuffer failed "
                 << "because offscreen FBO was incomplete.";
    return false;
  }

  // Clear the target frame buffer.
  {
    ScopedFramebufferBinder binder(this, offscreen_target_frame_buffer_->id());
    glClearColor(0, 0, 0, BackBufferAlphaClearColor());
    state_.SetDeviceColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearStencil(0);
    state_.SetDeviceStencilMaskSeparate(GL_FRONT, kDefaultStencilMask);
    state_.SetDeviceStencilMaskSeparate(GL_BACK, kDefaultStencilMask);
    glClearDepth(0);
    state_.SetDeviceDepthMask(GL_TRUE);
    state_.SetDeviceCapabilityState(GL_SCISSOR_TEST, false);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    RestoreClearState();
  }

  // Destroy the offscreen resolved framebuffers.
  if (offscreen_resolved_frame_buffer_.get())
    offscreen_resolved_frame_buffer_->Destroy();
  if (offscreen_resolved_color_texture_.get())
    offscreen_resolved_color_texture_->Destroy();
  offscreen_resolved_color_texture_.reset();
  offscreen_resolved_frame_buffer_.reset();

  return true;
}

error::Error GLES2DecoderImpl::HandleResizeCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ResizeCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::ResizeCHROMIUM*>(cmd_data);
  if (!offscreen_target_frame_buffer_.get() && surface_->DeferDraws())
    return error::kDeferCommandUntilLater;

  GLuint width = static_cast<GLuint>(c.width);
  GLuint height = static_cast<GLuint>(c.height);
  GLfloat scale_factor = c.scale_factor;
  GLboolean has_alpha = c.alpha;
  TRACE_EVENT2("gpu", "glResizeChromium", "width", width, "height", height);

  width = std::max(1U, width);
  height = std::max(1U, height);

#if defined(OS_POSIX) && !defined(OS_MACOSX) && \
    !defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
  // Make sure that we are done drawing to the back buffer before resizing.
  glFinish();
#endif
  bool is_offscreen = !!offscreen_target_frame_buffer_.get();
  if (is_offscreen) {
    if (!ResizeOffscreenFramebuffer(gfx::Size(width, height))) {
      LOG(ERROR) << "GLES2DecoderImpl: Context lost because "
                 << "ResizeOffscreenFramebuffer failed.";
      return error::kLostContext;
    }
  } else {
    if (!surface_->Resize(gfx::Size(width, height), scale_factor,
                          !!has_alpha)) {
      LOG(ERROR) << "GLES2DecoderImpl: Context lost because resize failed.";
      return error::kLostContext;
    }
    DCHECK(context_->IsCurrent(surface_.get()));
    if (!context_->IsCurrent(surface_.get())) {
      LOG(ERROR) << "GLES2DecoderImpl: Context lost because context no longer "
                 << "current after resize callback.";
      return error::kLostContext;
    }
    if (surface_->BuffersFlipped()) {
      backbuffer_needs_clear_bits_ |= GL_COLOR_BUFFER_BIT;
    }
  }

  swaps_since_resize_ = 0;

  return error::kNoError;
}

const char* GLES2DecoderImpl::GetCommandName(unsigned int command_id) const {
  if (command_id >= kFirstGLES2Command && command_id < kNumCommands) {
    return gles2::GetCommandName(static_cast<CommandId>(command_id));
  }
  return GetCommonCommandName(static_cast<cmd::CommandId>(command_id));
}

// Decode multiple commands, and call the corresponding GL functions.
// NOTE: 'buffer' is a pointer to the command buffer. As such, it could be
// changed by a (malicious) client at any time, so if validation has to happen,
// it should operate on a copy of them.
// NOTE: This is duplicating code from AsyncAPIInterface::DoCommands() in the
// interest of performance in this critical execution loop.
template <bool DebugImpl>
error::Error GLES2DecoderImpl::DoCommandsImpl(unsigned int num_commands,
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

    if (static_cast<int>(size) + process_pos > num_entries) {
      result = error::kOutOfBounds;
      break;
    }

    if (DebugImpl && log_commands()) {
      LOG(ERROR) << "[" << logger_.GetLogPrefix() << "]"
                 << "cmd: " << GetCommandName(command);
    }

    const unsigned int arg_count = size - 1;
    unsigned int command_index = command - kFirstGLES2Command;
    if (command_index < arraysize(command_info)) {
      const CommandInfo& info = command_info[command_index];
      unsigned int info_arg_count = static_cast<unsigned int>(info.arg_count);
      if ((info.arg_flags == cmd::kFixed && arg_count == info_arg_count) ||
          (info.arg_flags == cmd::kAtLeastN && arg_count >= info_arg_count)) {
        bool doing_gpu_trace = false;
        if (DebugImpl && gpu_trace_commands_) {
          if (CMD_FLAG_GET_TRACE_LEVEL(info.cmd_flags) <= gpu_trace_level_) {
            doing_gpu_trace = true;
            gpu_tracer_->Begin(TRACE_DISABLED_BY_DEFAULT("gpu_decoder"),
                               GetCommandName(command),
                               kTraceDecoder);
          }
        }

        uint32_t immediate_data_size = (arg_count - info_arg_count) *
                                       sizeof(CommandBufferEntry);  // NOLINT

        result = (this->*info.cmd_handler)(immediate_data_size, cmd_data);

        if (DebugImpl && doing_gpu_trace)
          gpu_tracer_->End(kTraceDecoder);

        if (DebugImpl && debug() && !WasContextLost()) {
          GLenum error;
          while ((error = glGetError()) != GL_NO_ERROR) {
            LOG(ERROR) << "[" << logger_.GetLogPrefix() << "] "
                       << "GL ERROR: " << GLES2Util::GetStringEnum(error)
                       << " : " << GetCommandName(command);
            LOCAL_SET_GL_ERROR(error, "DoCommand", "GL error from driver");
          }
        }
      } else {
        result = error::kInvalidArguments;
      }
    } else {
      result = DoCommonCommand(command, arg_count, cmd_data);
    }

    if (result == error::kNoError &&
        current_decoder_error_ != error::kNoError) {
      result = current_decoder_error_;
      current_decoder_error_ = error::kNoError;
    }

    if (result != error::kDeferCommandUntilLater) {
      process_pos += size;
      cmd_data += size;
    }
  }

  if (entries_processed)
    *entries_processed = process_pos;

  if (error::IsError(result)) {
    LOG(ERROR) << "Error: " << result << " for Command "
               << GetCommandName(command);
  }

  return result;
}

error::Error GLES2DecoderImpl::DoCommands(unsigned int num_commands,
                                          const volatile void* buffer,
                                          int num_entries,
                                          int* entries_processed) {
  if (gpu_debug_commands_) {
    return DoCommandsImpl<true>(
        num_commands, buffer, num_entries, entries_processed);
  } else {
    return DoCommandsImpl<false>(
        num_commands, buffer, num_entries, entries_processed);
  }
}

void GLES2DecoderImpl::RemoveBuffer(GLuint client_id) {
  buffer_manager()->RemoveBuffer(client_id);
}

void GLES2DecoderImpl::DoFinish() {
  glFinish();
  ProcessPendingReadPixels(true);
  ProcessPendingQueries(true);
}

void GLES2DecoderImpl::DoFlush() {
  glFlush();
  ProcessPendingQueries(false);
}

void GLES2DecoderImpl::DoActiveTexture(GLenum texture_unit) {
  GLuint texture_index = texture_unit - GL_TEXTURE0;
  if (texture_index >= state_.texture_units.size()) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(
        "glActiveTexture", texture_unit, "texture_unit");
    return;
  }
  state_.active_texture_unit = texture_index;
  glActiveTexture(texture_unit);
}

void GLES2DecoderImpl::DoBindBuffer(GLenum target, GLuint client_id) {
  Buffer* buffer = NULL;
  GLuint service_id = 0;
  if (client_id != 0) {
    buffer = GetBuffer(client_id);
    if (!buffer) {
      if (!group_->bind_generates_resource()) {
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                           "glBindBuffer",
                           "id not generated by glGenBuffers");
        return;
      }

      // It's a new id so make a buffer buffer for it.
      glGenBuffersARB(1, &service_id);
      CreateBuffer(client_id, service_id);
      buffer = GetBuffer(client_id);
    }
  }
  LogClientServiceForInfo(buffer, client_id, "glBindBuffer");
  if (buffer) {
    if (!buffer_manager()->SetTarget(buffer, target)) {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_OPERATION,
          "glBindBuffer", "buffer bound to more than 1 target");
      return;
    }
    service_id = buffer->service_id();
  }
  state_.SetBoundBuffer(target, buffer);
  glBindBuffer(target, service_id);
}

void GLES2DecoderImpl::BindIndexedBufferImpl(
    GLenum target, GLuint index, GLuint client_id,
    GLintptr offset, GLsizeiptr size,
    BindIndexedBufferFunctionType function_type, const char* function_name) {
  switch (target) {
    case GL_TRANSFORM_FEEDBACK_BUFFER: {
      if (index >= group_->max_transform_feedback_separate_attribs()) {
        LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name,
                           "index out of range");
        return;
      }
      DCHECK(state_.bound_transform_feedback.get());
      if (state_.bound_transform_feedback->active()) {
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                           "bound transform feedback is active");
        return;
      }
      break;
    }
    case GL_UNIFORM_BUFFER: {
      if (index >= group_->max_uniform_buffer_bindings()) {
        LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name,
                           "index out of range");
        return;
      }
      break;
    }
    default:
      NOTREACHED();
      break;
  }

  if (function_type == kBindBufferRange) {
    switch (target) {
      case GL_TRANSFORM_FEEDBACK_BUFFER:
        if ((size % 4 != 0) || (offset % 4 != 0)) {
          LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name,
                             "size or offset are not multiples of 4");
          return;
        }
        break;
      case GL_UNIFORM_BUFFER: {
        if (offset % group_->uniform_buffer_offset_alignment() != 0) {
          LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name,
              "offset is not a multiple of UNIFORM_BUFFER_OFFSET_ALIGNMENT");
          return;
        }
        break;
      }
      default:
        NOTREACHED();
        break;
    }

    if (client_id != 0) {
      if (size <= 0) {
        LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "size <= 0");
        return;
      }
      if (offset < 0) {
        LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "offset < 0");
        return;
      }
    }
  }

  Buffer* buffer = nullptr;
  GLuint service_id = 0;
  if (client_id != 0) {
    buffer = GetBuffer(client_id);
    if (!buffer) {
      if (!group_->bind_generates_resource()) {
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                           "id not generated by glGenBuffers");
        return;
      }

      // It's a new id so make a buffer for it.
      glGenBuffersARB(1, &service_id);
      CreateBuffer(client_id, service_id);
      buffer = GetBuffer(client_id);
      DCHECK(buffer);
    }
    if (!buffer_manager()->SetTarget(buffer, target)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                         "buffer bound to more than 1 target");
      return;
    }
    service_id = buffer->service_id();
  }
  LogClientServiceForInfo(buffer, client_id, function_name);

  scoped_refptr<IndexedBufferBindingHost> bindings;
  switch (target) {
    case GL_TRANSFORM_FEEDBACK_BUFFER:
      bindings = state_.bound_transform_feedback.get();
      break;
    case GL_UNIFORM_BUFFER:
      bindings = state_.indexed_uniform_buffer_bindings.get();
      break;
    default:
      NOTREACHED();
      break;
  }
  DCHECK(bindings);
  switch (function_type) {
    case kBindBufferBase:
      bindings->DoBindBufferBase(target, index, buffer);
      break;
    case kBindBufferRange:
      bindings->DoBindBufferRange(target, index, buffer, offset, size);
      break;
    default:
      NOTREACHED();
      break;
  }
  state_.SetBoundBuffer(target, buffer);
}

void GLES2DecoderImpl::DoBindBufferBase(GLenum target, GLuint index,
                                        GLuint client_id) {
  BindIndexedBufferImpl(target, index, client_id, 0, 0,
                        kBindBufferBase, "glBindBufferBase");
}

void GLES2DecoderImpl::DoBindBufferRange(GLenum target, GLuint index,
                                         GLuint client_id,
                                         GLintptr offset,
                                         GLsizeiptr size) {
  BindIndexedBufferImpl(target, index, client_id, offset, size,
                        kBindBufferRange, "glBindBufferRange");
}

bool GLES2DecoderImpl::BoundFramebufferAllowsChangesToAlphaChannel() {
  Framebuffer* framebuffer = GetBoundDrawFramebuffer();
  if (framebuffer)
    return framebuffer->HasAlphaMRT();
  if (back_buffer_draw_buffer_ == GL_NONE)
    return false;
  if (offscreen_target_frame_buffer_.get()) {
    GLenum format = offscreen_target_color_format_;
    return (format == GL_RGBA || format == GL_RGBA8) &&
           offscreen_buffer_should_have_alpha_;
  }
  return (back_buffer_color_format_ == GL_RGBA ||
          back_buffer_color_format_ == GL_RGBA8);
}

bool GLES2DecoderImpl::BoundFramebufferHasDepthAttachment() {
  Framebuffer* framebuffer = GetBoundDrawFramebuffer();
  if (framebuffer) {
    return framebuffer->HasDepthAttachment();
  }
  if (offscreen_target_frame_buffer_.get()) {
    return offscreen_target_depth_format_ != 0;
  }
  return back_buffer_has_depth_;
}

bool GLES2DecoderImpl::BoundFramebufferHasStencilAttachment() {
  Framebuffer* framebuffer = GetBoundDrawFramebuffer();
  if (framebuffer) {
    return framebuffer->HasStencilAttachment();
  }
  if (offscreen_target_frame_buffer_.get()) {
    return offscreen_target_stencil_format_ != 0 ||
           offscreen_target_depth_format_ == GL_DEPTH24_STENCIL8;
  }
  return back_buffer_has_stencil_;
}

void GLES2DecoderImpl::ApplyDirtyState() {
  if (framebuffer_state_.clear_state_dirty) {
    bool allows_alpha_change = BoundFramebufferAllowsChangesToAlphaChannel();
    state_.SetDeviceColorMask(state_.color_mask_red, state_.color_mask_green,
                              state_.color_mask_blue,
                              state_.color_mask_alpha && allows_alpha_change);

    bool have_depth = BoundFramebufferHasDepthAttachment();
    state_.SetDeviceDepthMask(state_.depth_mask && have_depth);

    bool have_stencil = BoundFramebufferHasStencilAttachment();
    state_.SetDeviceStencilMaskSeparate(
        GL_FRONT, have_stencil ? state_.stencil_front_writemask : 0);
    state_.SetDeviceStencilMaskSeparate(
        GL_BACK, have_stencil ? state_.stencil_back_writemask : 0);

    state_.SetDeviceCapabilityState(
        GL_DEPTH_TEST, state_.enable_flags.depth_test && have_depth);
    state_.SetDeviceCapabilityState(
        GL_STENCIL_TEST, state_.enable_flags.stencil_test && have_stencil);
    framebuffer_state_.clear_state_dirty = false;
  }
}

GLuint GLES2DecoderImpl::GetBackbufferServiceId() const {
  return (offscreen_target_frame_buffer_.get())
             ? offscreen_target_frame_buffer_->id()
             : (surface_.get() ? surface_->GetBackingFramebufferObject() : 0);
}

void GLES2DecoderImpl::RestoreState(const ContextState* prev_state) {
  TRACE_EVENT1("gpu", "GLES2DecoderImpl::RestoreState",
               "context", logger_.GetLogPrefix());
  // Restore the Framebuffer first because of bugs in Intel drivers.
  // Intel drivers incorrectly clip the viewport settings to
  // the size of the current framebuffer object.
  RestoreFramebufferBindings();
  state_.RestoreState(prev_state);
}

void GLES2DecoderImpl::RestoreFramebufferBindings() const {
  GLuint service_id =
      framebuffer_state_.bound_draw_framebuffer.get()
          ? framebuffer_state_.bound_draw_framebuffer->service_id()
          : GetBackbufferServiceId();
  if (!SupportsSeparateFramebufferBinds()) {
    glBindFramebufferEXT(GL_FRAMEBUFFER, service_id);
  } else {
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, service_id);
    service_id = framebuffer_state_.bound_read_framebuffer.get()
                     ? framebuffer_state_.bound_read_framebuffer->service_id()
                     : GetBackbufferServiceId();
    glBindFramebufferEXT(GL_READ_FRAMEBUFFER, service_id);
  }
  OnFboChanged();
}

void GLES2DecoderImpl::RestoreRenderbufferBindings() {
  state_.RestoreRenderbufferBindings();
}

void GLES2DecoderImpl::RestoreTextureState(unsigned service_id) const {
  Texture* texture = texture_manager()->GetTextureForServiceId(service_id);
  if (texture) {
    GLenum target = texture->target();
    glBindTexture(target, service_id);
    glTexParameteri(
        target, GL_TEXTURE_WRAP_S, texture->wrap_s());
    glTexParameteri(
        target, GL_TEXTURE_WRAP_T, texture->wrap_t());
    glTexParameteri(
        target, GL_TEXTURE_MIN_FILTER, texture->min_filter());
    glTexParameteri(
        target, GL_TEXTURE_MAG_FILTER, texture->mag_filter());
    RestoreTextureUnitBindings(state_.active_texture_unit);
  }
}

void GLES2DecoderImpl::ClearAllAttributes() const {
  // Must use native VAO 0, as RestoreAllAttributes can't fully restore
  // other VAOs.
  if (feature_info_->feature_flags().native_vertex_array_object)
    glBindVertexArrayOES(0);

  for (uint32_t i = 0; i < group_->max_vertex_attribs(); ++i) {
    if (i != 0)  // Never disable attribute 0
      glDisableVertexAttribArray(i);
    if (features().angle_instanced_arrays)
      glVertexAttribDivisorANGLE(i, 0);
  }
}

void GLES2DecoderImpl::RestoreAllAttributes() const {
  state_.RestoreVertexAttribs();
}

void GLES2DecoderImpl::SetIgnoreCachedStateForTest(bool ignore) {
  state_.SetIgnoreCachedStateForTest(ignore);
}

void GLES2DecoderImpl::SetForceShaderNameHashingForTest(bool force) {
  force_shader_name_hashing_for_test = force;
}

// Added specifically for testing backbuffer_needs_clear_bits unittests.
uint32_t GLES2DecoderImpl::GetAndClearBackbufferClearBitsForTest() {
  uint32_t clear_bits = backbuffer_needs_clear_bits_;
  backbuffer_needs_clear_bits_ = 0;
  return clear_bits;
}

void GLES2DecoderImpl::OnFboChanged() const {
  if (workarounds().restore_scissor_on_fbo_change)
    state_.fbo_binding_for_scissor_workaround_dirty = true;
}

// Called after the FBO is checked for completeness.
void GLES2DecoderImpl::OnUseFramebuffer() const {
  if (state_.fbo_binding_for_scissor_workaround_dirty) {
    state_.fbo_binding_for_scissor_workaround_dirty = false;
    // The driver forgets the correct scissor when modifying the FBO binding.
    glScissor(state_.scissor_x,
              state_.scissor_y,
              state_.scissor_width,
              state_.scissor_height);

    // crbug.com/222018 - Also on QualComm, the flush here avoids flicker,
    // it's unclear how this bug works.
    glFlush();
  }
}

void GLES2DecoderImpl::DoBindFramebuffer(GLenum target, GLuint client_id) {
  Framebuffer* framebuffer = NULL;
  GLuint service_id = 0;
  if (client_id != 0) {
    framebuffer = GetFramebuffer(client_id);
    if (!framebuffer) {
      if (!group_->bind_generates_resource()) {
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                           "glBindFramebuffer",
                           "id not generated by glGenFramebuffers");
        return;
      }

      // It's a new id so make a framebuffer framebuffer for it.
      glGenFramebuffersEXT(1, &service_id);
      CreateFramebuffer(client_id, service_id);
      framebuffer = GetFramebuffer(client_id);
    } else {
      service_id = framebuffer->service_id();
    }
    framebuffer->MarkAsValid();
  }
  LogClientServiceForInfo(framebuffer, client_id, "glBindFramebuffer");

  if (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER_EXT) {
    framebuffer_state_.bound_draw_framebuffer = framebuffer;
  }

  // vmiura: This looks like dup code
  if (target == GL_FRAMEBUFFER || target == GL_READ_FRAMEBUFFER_EXT) {
    framebuffer_state_.bound_read_framebuffer = framebuffer;
  }

  framebuffer_state_.clear_state_dirty = true;

  // If we are rendering to the backbuffer get the FBO id for any simulated
  // backbuffer.
  if (framebuffer == NULL) {
    service_id = GetBackbufferServiceId();
  }

  glBindFramebufferEXT(target, service_id);
  OnFboChanged();
}

void GLES2DecoderImpl::DoBindRenderbuffer(GLenum target, GLuint client_id) {
  Renderbuffer* renderbuffer = NULL;
  GLuint service_id = 0;
  if (client_id != 0) {
    renderbuffer = GetRenderbuffer(client_id);
    if (!renderbuffer) {
      if (!group_->bind_generates_resource()) {
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                           "glBindRenderbuffer",
                           "id not generated by glGenRenderbuffers");
        return;
      }

      // It's a new id so make a renderbuffer for it.
      glGenRenderbuffersEXT(1, &service_id);
      CreateRenderbuffer(client_id, service_id);
      renderbuffer = GetRenderbuffer(client_id);
    } else {
      service_id = renderbuffer->service_id();
    }
    renderbuffer->MarkAsValid();
  }
  LogClientServiceForInfo(renderbuffer, client_id, "glBindRenderbuffer");
  state_.bound_renderbuffer = renderbuffer;
  state_.bound_renderbuffer_valid = true;
  glBindRenderbufferEXT(GL_RENDERBUFFER, service_id);
}

void GLES2DecoderImpl::DoBindTexture(GLenum target, GLuint client_id) {
  TextureRef* texture_ref = NULL;
  GLuint service_id = 0;
  if (client_id != 0) {
    texture_ref = GetTexture(client_id);
    if (!texture_ref) {
      if (!group_->bind_generates_resource()) {
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                           "glBindTexture",
                           "id not generated by glGenTextures");
        return;
      }

      // It's a new id so make a texture texture for it.
      glGenTextures(1, &service_id);
      DCHECK_NE(0u, service_id);
      CreateTexture(client_id, service_id);
      texture_ref = GetTexture(client_id);
    }
  } else {
    texture_ref = texture_manager()->GetDefaultTextureInfo(target);
  }

  // Check the texture exists
  if (texture_ref) {
    Texture* texture = texture_ref->texture();
    // Check that we are not trying to bind it to a different target.
    if (texture->target() != 0 && texture->target() != target) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                         "glBindTexture",
                         "texture bound to more than 1 target.");
      return;
    }
    LogClientServiceForInfo(texture, client_id, "glBindTexture");
    glBindTexture(target, texture->service_id());
    if (texture->target() == 0) {
      texture_manager()->SetTarget(texture_ref, target);
      if (!gl_version_info().BehavesLikeGLES() &&
          gl_version_info().IsAtLeastGL(3, 2)) {
        // In Desktop GL core profile and GL ES, depth textures are always
        // sampled to the RED channel, whereas on Desktop GL compatibility
        // proifle, they are sampled to RED, LUMINANCE, INTENSITY, or ALPHA
        // channel, depending on the DEPTH_TEXTURE_MODE value.
        // In theory we only need to apply this for depth textures, but it is
        // simpler to apply to all textures.
        glTexParameteri(target, GL_DEPTH_TEXTURE_MODE, GL_RED);
      }
    }
  } else {
    glBindTexture(target, 0);
  }

  TextureUnit& unit = state_.texture_units[state_.active_texture_unit];
  unit.bind_target = target;
  unit.GetInfoForTarget(target) = texture_ref;
}

void GLES2DecoderImpl::DoBindSampler(GLuint unit, GLuint client_id) {
  if (unit >= group_->max_texture_units()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glBindSampler", "unit out of bounds");
    return;
  }
  Sampler* sampler = nullptr;
  if (client_id != 0) {
    sampler = GetSampler(client_id);
    if (!sampler) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                         "glBindSampler",
                         "id not generated by glGenSamplers");
      return;
    }
  }

  // Check the sampler exists
  if (sampler) {
    LogClientServiceForInfo(sampler, client_id, "glBindSampler");
    glBindSampler(unit, sampler->service_id());
  } else {
    glBindSampler(unit, 0);
  }

  state_.sampler_units[unit] = sampler;
}

void GLES2DecoderImpl::DoBindTransformFeedback(
    GLenum target, GLuint client_id) {
  const char* function_name = "glBindTransformFeedback";

  TransformFeedback* transform_feedback = nullptr;
  if (client_id != 0) {
    transform_feedback = GetTransformFeedback(client_id);
    if (!transform_feedback) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                         "id not generated by glGenTransformFeedbacks");
      return;
    }
  } else {
    transform_feedback = state_.default_transform_feedback.get();
  }
  DCHECK(transform_feedback);
  if (transform_feedback == state_.bound_transform_feedback.get())
    return;
  if (state_.bound_transform_feedback->active() &&
      !state_.bound_transform_feedback->paused()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                       "currently bound transform feedback is active");
    return;
  }
  LogClientServiceForInfo(transform_feedback, client_id, function_name);
  transform_feedback->DoBindTransformFeedback(target);
  state_.bound_transform_feedback = transform_feedback;
}

void GLES2DecoderImpl::DoBeginTransformFeedback(GLenum primitive_mode) {
  const char* function_name = "glBeginTransformFeedback";
  TransformFeedback* transform_feedback = state_.bound_transform_feedback.get();
  DCHECK(transform_feedback);
  if (transform_feedback->active()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                       "transform feedback is already active");
    return;
  }
  if (!CheckCurrentProgram(function_name)) {
    return;
  }
  Program* program = state_.current_program.get();
  DCHECK(program);
  size_t required_buffer_count =
      program->effective_transform_feedback_varyings().size();
  if (required_buffer_count == 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                       "no active transform feedback varyings");
    return;
  }
  if (required_buffer_count > 1 &&
      GL_INTERLEAVED_ATTRIBS ==
          program->effective_transform_feedback_buffer_mode()) {
    required_buffer_count = 1;
  }
  for (size_t ii = 0; ii < required_buffer_count; ++ii) {
    Buffer* buffer = transform_feedback->GetBufferBinding(ii);
    if (!buffer) {
      std::string msg = base::StringPrintf("missing buffer bound at index %i",
                                           static_cast<int>(ii));
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name, msg.c_str());
      return;
    }
    if (buffer->GetMappedRange()) {
      std::string msg = base::StringPrintf(
          "bound buffer bound at index %i is mapped", static_cast<int>(ii));
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name, msg.c_str());
      return;
    }
  }
  transform_feedback->DoBeginTransformFeedback(primitive_mode);
  DCHECK(transform_feedback->active());
}

void GLES2DecoderImpl::DoEndTransformFeedback() {
  const char* function_name = "glEndTransformFeedback";
  DCHECK(state_.bound_transform_feedback.get());
  if (!state_.bound_transform_feedback->active()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                       "transform feedback is not active");
    return;
  }
  // TODO(zmo): Validate binding points.
  state_.bound_transform_feedback->DoEndTransformFeedback();
}

void GLES2DecoderImpl::DoPauseTransformFeedback() {
  DCHECK(state_.bound_transform_feedback.get());
  if (!state_.bound_transform_feedback->active() ||
      state_.bound_transform_feedback->paused()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glPauseTransformFeedback",
                       "transform feedback is not active or already paused");
    return;
  }
  state_.bound_transform_feedback->DoPauseTransformFeedback();
}

void GLES2DecoderImpl::DoResumeTransformFeedback() {
  DCHECK(state_.bound_transform_feedback.get());
  if (!state_.bound_transform_feedback->active() ||
      !state_.bound_transform_feedback->paused()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glResumeTransformFeedback",
                       "transform feedback is not active or not paused");
    return;
  }
  if (workarounds().rebind_transform_feedback_before_resume) {
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK,
        state_.bound_transform_feedback->service_id());
  }
  state_.bound_transform_feedback->DoResumeTransformFeedback();
}

void GLES2DecoderImpl::DoDisableVertexAttribArray(GLuint index) {
  if (state_.vertex_attrib_manager->Enable(index, false)) {
    if (index != 0 || gl_version_info().BehavesLikeGLES()) {
      glDisableVertexAttribArray(index);
    }
  } else {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE,
        "glDisableVertexAttribArray", "index out of range");
  }
}

void GLES2DecoderImpl::InvalidateFramebufferImpl(
    GLenum target,
    GLsizei count,
    const volatile GLenum* attachments,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    const char* function_name,
    FramebufferOperation op) {
  Framebuffer* framebuffer = GetFramebufferInfoForTarget(GL_FRAMEBUFFER);

  // Because of performance issues, no-op if the format of the attachment is
  // DEPTH_STENCIL and only one part is intended to be invalidated.
  bool has_depth_stencil_format = framebuffer &&
      framebuffer->HasDepthStencilFormatAttachment();
  bool invalidate_depth = false;
  bool invalidate_stencil = false;
  std::unique_ptr<GLenum[]> validated_attachments(new GLenum[count]);
  GLsizei validated_count = 0;

  // Validates the attachments. If one of them fails, the whole command fails.
  GLenum thresh0 = GL_COLOR_ATTACHMENT0 + group_->max_color_attachments();
  GLenum thresh1 = GL_COLOR_ATTACHMENT15;
  for (GLsizei i = 0; i < count; ++i) {
    GLenum attachment = attachments[i];
    if (framebuffer) {
      if (attachment >= thresh0 && attachment <= thresh1) {
        LOCAL_SET_GL_ERROR(
            GL_INVALID_OPERATION, function_name, "invalid attachment");
        return;
      }
      if (!validators_->attachment.IsValid(attachment)) {
        LOCAL_SET_GL_ERROR_INVALID_ENUM(function_name, attachment,
                                        "attachments");
        return;
      }
      if (has_depth_stencil_format) {
        switch (attachment) {
          case GL_DEPTH_ATTACHMENT:
            invalidate_depth = true;
            continue;
          case GL_STENCIL_ATTACHMENT:
            invalidate_stencil = true;
            continue;
          case GL_DEPTH_STENCIL_ATTACHMENT:
            invalidate_depth = true;
            invalidate_stencil = true;
            continue;
        }
      }
    } else {
      if (!validators_->backbuffer_attachment.IsValid(attachment)) {
        LOCAL_SET_GL_ERROR_INVALID_ENUM(function_name, attachment,
                                        "attachments");
        return;
      }
    }
    validated_attachments[validated_count++] = attachment;
  }
  if (invalidate_depth && invalidate_stencil) {
    validated_attachments[validated_count++] = GL_DEPTH_STENCIL_ATTACHMENT;
  }

  // If the default framebuffer is bound but we are still rendering to an
  // FBO, translate attachment names that refer to default framebuffer
  // channels to corresponding framebuffer attachments.
  std::unique_ptr<GLenum[]> translated_attachments(new GLenum[validated_count]);
  for (GLsizei i = 0; i < validated_count; ++i) {
    GLenum attachment = validated_attachments[i];
    if (!framebuffer && GetBackbufferServiceId()) {
      switch (attachment) {
        case GL_COLOR_EXT:
          attachment = GL_COLOR_ATTACHMENT0;
          break;
        case GL_DEPTH_EXT:
          attachment = GL_DEPTH_ATTACHMENT;
          break;
        case GL_STENCIL_EXT:
          attachment = GL_STENCIL_ATTACHMENT;
          break;
        default:
          NOTREACHED();
          return;
      }
    }
    translated_attachments[i] = attachment;
  }

  bool dirty = false;
  switch (op) {
    case kFramebufferDiscard:
      if (gl_version_info().is_es3) {
        glInvalidateFramebuffer(
            target, validated_count, translated_attachments.get());
      } else {
        glDiscardFramebufferEXT(
            target, validated_count, translated_attachments.get());
      }
      dirty = true;
      break;
    case kFramebufferInvalidate:
      if (gl_version_info().IsLowerThanGL(4, 3)) {
        // no-op since the function isn't supported.
      } else {
        glInvalidateFramebuffer(
            target, validated_count, translated_attachments.get());
        dirty = true;
      }
      break;
    case kFramebufferInvalidateSub:
      // Make it an no-op because we don't have a mechanism to mark partial
      // pixels uncleared yet.
      // TODO(zmo): Revisit this.
      break;
  }

  if (!dirty)
    return;

  // Marks each one of them as not cleared.
  for (GLsizei i = 0; i < validated_count; ++i) {
    if (framebuffer) {
      if (validated_attachments[i] == GL_DEPTH_STENCIL_ATTACHMENT) {
        framebuffer->MarkAttachmentAsCleared(renderbuffer_manager(),
                                             texture_manager(),
                                             GL_DEPTH_ATTACHMENT,
                                             false);
        framebuffer->MarkAttachmentAsCleared(renderbuffer_manager(),
                                             texture_manager(),
                                             GL_STENCIL_ATTACHMENT,
                                             false);
      } else {
        framebuffer->MarkAttachmentAsCleared(renderbuffer_manager(),
                                             texture_manager(),
                                             validated_attachments[i],
                                             false);
      }
    } else {
      switch (validated_attachments[i]) {
        case GL_COLOR_EXT:
          backbuffer_needs_clear_bits_ |= GL_COLOR_BUFFER_BIT;
          break;
        case GL_DEPTH_EXT:
          backbuffer_needs_clear_bits_ |= GL_DEPTH_BUFFER_BIT;
          break;
        case GL_STENCIL_EXT:
          backbuffer_needs_clear_bits_ |= GL_STENCIL_BUFFER_BIT;
          break;
        default:
          NOTREACHED();
          break;
      }
    }
  }
}

void GLES2DecoderImpl::DoDiscardFramebufferEXT(
    GLenum target,
    GLsizei count,
    const volatile GLenum* attachments) {
  if (workarounds().disable_discard_framebuffer)
    return;

  const GLsizei kWidthNotUsed = 1;
  const GLsizei kHeightNotUsed = 1;
  InvalidateFramebufferImpl(
      target, count, attachments, 0, 0, kWidthNotUsed, kHeightNotUsed,
      "glDiscardFramebufferEXT", kFramebufferDiscard);
}

void GLES2DecoderImpl::DoInvalidateFramebuffer(
    GLenum target,
    GLsizei count,
    const volatile GLenum* attachments) {
  const GLsizei kWidthNotUsed = 1;
  const GLsizei kHeightNotUsed = 1;
  InvalidateFramebufferImpl(
      target, count, attachments, 0, 0, kWidthNotUsed, kHeightNotUsed,
      "glInvalidateFramebuffer", kFramebufferInvalidate);
}

void GLES2DecoderImpl::DoInvalidateSubFramebuffer(
    GLenum target,
    GLsizei count,
    const volatile GLenum* attachments,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height) {
  InvalidateFramebufferImpl(
      target, count, attachments, x, y, width, height,
      "glInvalidateSubFramebuffer", kFramebufferInvalidateSub);
}

void GLES2DecoderImpl::DoEnableVertexAttribArray(GLuint index) {
  if (state_.vertex_attrib_manager->Enable(index, true)) {
    glEnableVertexAttribArray(index);
  } else {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glEnableVertexAttribArray", "index out of range");
  }
}

void GLES2DecoderImpl::DoGenerateMipmap(GLenum target) {
  TextureRef* texture_ref = texture_manager()->GetTextureInfoForTarget(
      &state_, target);
  if (!texture_ref ||
      !texture_manager()->CanGenerateMipmaps(texture_ref)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, "glGenerateMipmap", "Can not generate mips");
    return;
  }
  Texture* tex = texture_ref->texture();
  GLint base_level = tex->base_level();

  if (target == GL_TEXTURE_CUBE_MAP) {
    for (int i = 0; i < 6; ++i) {
      GLenum face = GL_TEXTURE_CUBE_MAP_POSITIVE_X + i;
      if (!texture_manager()->ClearTextureLevel(this, texture_ref, face,
                                                base_level)) {
        LOCAL_SET_GL_ERROR(
            GL_OUT_OF_MEMORY, "glGenerateMipmap", "dimensions too big");
        return;
      }
    }
  } else {
    if (!texture_manager()->ClearTextureLevel(this, texture_ref, target,
                                              base_level)) {
      LOCAL_SET_GL_ERROR(
          GL_OUT_OF_MEMORY, "glGenerateMipmap", "dimensions too big");
      return;
    }
  }

  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("glGenerateMipmap");
  // Workaround for Mac driver bug. In the large scheme of things setting
  // glTexParamter twice for glGenerateMipmap is probably not a lage performance
  // hit so there's probably no need to make this conditional. The bug appears
  // to be that if the filtering mode is set to something that doesn't require
  // mipmaps for rendering, or is never set to something other than the default,
  // then glGenerateMipmap misbehaves.
  if (workarounds().set_texture_filter_before_generating_mipmap) {
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
  }

  // Workaround for Mac driver bug. If the base level is non-zero but the zero
  // level of a texture has not been set glGenerateMipmaps sets the entire mip
  // chain to opaque black. If the zero level is set at all, however, the mip
  // chain is properly generated from the base level.
  bool texture_zero_level_set = false;
  GLenum type = 0;
  GLenum internal_format = 0;
  GLenum format = 0;
  if (workarounds().set_zero_level_before_generating_mipmap &&
      target == GL_TEXTURE_2D) {
    if (base_level != 0 &&
        !tex->GetLevelType(target, 0, &type, &internal_format) &&
        tex->GetLevelType(target, tex->base_level(), &type, &internal_format)) {
      format = TextureManager::ExtractFormatFromStorageFormat(internal_format);
      glTexImage2D(target, 0, internal_format, 1, 1, 0, format, type, nullptr);
      texture_zero_level_set = true;
    }
  }

  bool enable_srgb = 0;
  if (target == GL_TEXTURE_2D) {
    tex->GetLevelType(target, tex->base_level(), &type, &internal_format);
    enable_srgb =
        GetColorEncodingFromInternalFormat(internal_format) == GL_SRGB;
  }
  if (!enable_srgb || !feature_info_->feature_flags().desktop_srgb_support ||
      !workarounds().decode_encode_srgb_for_generatemipmap) {
    if (feature_info_->feature_flags().desktop_srgb_support) {
      state_.EnableDisableFramebufferSRGB(enable_srgb);
    }
    glGenerateMipmapEXT(target);
  } else {
    if (target == GL_TEXTURE_2D) {
      state_.EnableDisableFramebufferSRGB(true);
      if (!InitializeSRGBConverter("generateMipmap")) {
        return;
      }
      srgb_converter_->GenerateMipmap(this, tex, target);
    } else {
      // TODO(yizhou): If the target is GL_TEXTURE_3D or GL_TEXTURE_2D_ARRAY,
      // this change can not generate correct mipmap.
      glGenerateMipmapEXT(target);
    }
  }

  if (texture_zero_level_set) {
    // This may have some unwanted side effects, but we expect command buffer
    // validation to prevent you from doing anything weird with the texture
    // after this, like calling texSubImage2D sucessfully.
    glTexImage2D(target, 0, internal_format, 0, 0, 0, format, type, nullptr);
  }

  if (workarounds().set_texture_filter_before_generating_mipmap) {
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER,
                    texture_ref->texture()->min_filter());
  }
  GLenum error = LOCAL_PEEK_GL_ERROR("glGenerateMipmap");
  if (error == GL_NO_ERROR) {
    texture_manager()->MarkMipmapsGenerated(texture_ref);
  }
}

bool GLES2DecoderImpl::GetHelper(
    GLenum pname, GLint* params, GLsizei* num_written) {
  DCHECK(num_written);
  switch (pname) {
    case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
    case GL_IMPLEMENTATION_COLOR_READ_TYPE:
      *num_written = 1;
      {
        Framebuffer* framebuffer = GetBoundReadFramebuffer();
        if (framebuffer &&
            framebuffer->IsPossiblyComplete(feature_info_.get()) !=
            GL_FRAMEBUFFER_COMPLETE) {
          // Here we avoid querying the driver framebuffer status because the
          // above should cover most cases. This is an effort to reduce crashes
          // on MacOSX. See crbug.com/662802.
          LOCAL_SET_GL_ERROR(
              GL_INVALID_OPERATION, "glGetIntegerv", "incomplete framebuffer");
          if (params) {
            *params = 0;
          }
          return true;
        }
      }
      if (params) {
        if (feature_info_->gl_version_info().is_es) {
          glGetIntegerv(pname, params);
        } else {
          // On Desktop GL where these two enums can be queried, instead of
          // returning the second pair of read format/type, the preferred pair
          // is returned. So this semantic is different from GL ES.
          if (pname == GL_IMPLEMENTATION_COLOR_READ_FORMAT) {
            *params = GLES2Util::GetGLReadPixelsImplementationFormat(
                GetBoundReadFramebufferInternalFormat(),
                GetBoundReadFramebufferTextureType(),
                feature_info_->feature_flags().ext_read_format_bgra);
          } else {
            *params = GLES2Util::GetGLReadPixelsImplementationType(
                GetBoundReadFramebufferInternalFormat(),
                GetBoundReadFramebufferTextureType());
          }
        }
        if (*params == GL_HALF_FLOAT && feature_info_->IsWebGL1OrES2Context()) {
          *params = GL_HALF_FLOAT_OES;
        }
        if (*params == GL_SRGB_ALPHA_EXT) {
          *params = GL_RGBA;
        }
        if (*params == GL_SRGB_EXT) {
          *params = GL_RGB;
        }
      }
      return true;
    default:
      break;
  }

  if (!gl_version_info().is_es) {
    switch (pname) {
      case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
        *num_written = 1;
        if (params) {
          *params = group_->max_fragment_uniform_vectors();
        }
        return true;
      case GL_MAX_VARYING_VECTORS:
        *num_written = 1;
        if (params) {
          *params = group_->max_varying_vectors();
        }
        return true;
      case GL_MAX_VERTEX_UNIFORM_VECTORS:
        *num_written = 1;
        if (params) {
          *params = group_->max_vertex_uniform_vectors();
        }
        return true;
      }
  }
  if (feature_info_->IsWebGL2OrES3Context()) {
    switch (pname) {
      case GL_MAX_VARYING_COMPONENTS: {
        if (gl_version_info().is_es) {
          // We can just delegate this query to the driver.
          *num_written = 1;
          break;
        }

        // GL_MAX_VARYING_COMPONENTS is deprecated in the desktop
        // OpenGL core profile, so for simplicity, just compute it
        // from GL_MAX_VARYING_VECTORS on non-OpenGL ES
        // configurations.
        GLint max_varying_vectors = 0;
        glGetIntegerv(GL_MAX_VARYING_VECTORS, &max_varying_vectors);
        *num_written = 1;
        if (params) {
          *params = max_varying_vectors * 4;
        }
        return true;
      }
      case GL_READ_BUFFER:
        *num_written = 1;
        if (params) {
          Framebuffer* framebuffer = GetBoundReadFramebuffer();
          GLenum read_buffer;
          if (framebuffer) {
            read_buffer = framebuffer->read_buffer();
          } else {
            read_buffer = back_buffer_read_buffer_;
          }
          *params = static_cast<GLint>(read_buffer);
        }
        return true;
      case GL_TRANSFORM_FEEDBACK_ACTIVE:
        *num_written = 1;
        if (params) {
          *params =
              static_cast<GLint>(state_.bound_transform_feedback->active());
        }
        return true;
      case GL_TRANSFORM_FEEDBACK_PAUSED:
        *num_written = 1;
        if (params) {
          *params =
              static_cast<GLint>(state_.bound_transform_feedback->paused());
        }
        return true;
    }
  }
  switch (pname) {
    case GL_MAX_VIEWPORT_DIMS:
      *num_written = 2;
      if (offscreen_target_frame_buffer_.get()) {
        if (params) {
          params[0] = renderbuffer_manager()->max_renderbuffer_size();
          params[1] = renderbuffer_manager()->max_renderbuffer_size();
        }
        return true;
      }
      break;
    case GL_MAX_SAMPLES:
      *num_written = 1;
      if (params) {
        params[0] = renderbuffer_manager()->max_samples();
      }
      return true;
    case GL_MAX_RENDERBUFFER_SIZE:
      *num_written = 1;
      if (params) {
        params[0] = renderbuffer_manager()->max_renderbuffer_size();
      }
      return true;
    case GL_MAX_TEXTURE_SIZE:
      *num_written = 1;
      if (params) {
        params[0] = texture_manager()->MaxSizeForTarget(GL_TEXTURE_2D);
      }
      return true;
    case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
      *num_written = 1;
      if (params) {
        params[0] = texture_manager()->MaxSizeForTarget(GL_TEXTURE_CUBE_MAP);
      }
      return true;
    case GL_MAX_COLOR_ATTACHMENTS_EXT:
      *num_written = 1;
      if (params) {
        params[0] = group_->max_color_attachments();
      }
      return true;
    case GL_MAX_DRAW_BUFFERS_ARB:
      *num_written = 1;
      if (params) {
        params[0] = group_->max_draw_buffers();
      }
      return true;
    case GL_ALPHA_BITS:
      *num_written = 1;
      if (params) {
        GLint v = 0;
        Framebuffer* framebuffer = GetBoundDrawFramebuffer();
        if (framebuffer) {
          if (framebuffer->HasAlphaMRT() &&
              framebuffer->HasSameInternalFormatsMRT()) {
            if (gl_version_info().is_desktop_core_profile) {
              for (uint32_t i = 0; i < group_->max_draw_buffers(); i++) {
                if (framebuffer->HasColorAttachment(i)) {
                  glGetFramebufferAttachmentParameterivEXT(
                      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                      GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE, &v);
                  break;
                }
              }
            } else {
              glGetIntegerv(GL_ALPHA_BITS, &v);
            }
          }
        } else {
          v = (ClientExposedBackBufferHasAlpha() ? 8 : 0);
        }
        params[0] = v;
      }
      return true;
    case GL_DEPTH_BITS:
      *num_written = 1;
      if (params) {
        GLint v = 0;
        if (gl_version_info().is_desktop_core_profile) {
          Framebuffer* framebuffer = GetBoundDrawFramebuffer();
          if (framebuffer) {
            if (framebuffer->HasDepthAttachment()) {
              glGetFramebufferAttachmentParameterivEXT(
                  GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                  GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &v);
            }
          } else {
            v = (back_buffer_has_depth_ ? 24 : 0);
          }
        } else {
          glGetIntegerv(GL_DEPTH_BITS, &v);
        }
        params[0] = BoundFramebufferHasDepthAttachment() ? v : 0;
      }
      return true;
    case GL_RED_BITS:
    case GL_GREEN_BITS:
    case GL_BLUE_BITS:
      *num_written = 1;
      if (params) {
        GLint v = 0;
        if (gl_version_info().is_desktop_core_profile) {
          Framebuffer* framebuffer = GetBoundDrawFramebuffer();
          if (framebuffer) {
            if (framebuffer->HasSameInternalFormatsMRT()) {
              GLenum framebuffer_enum = 0;
              switch (pname) {
                case GL_RED_BITS:
                  framebuffer_enum = GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE;
                  break;
                case GL_GREEN_BITS:
                  framebuffer_enum = GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE;
                  break;
                case GL_BLUE_BITS:
                  framebuffer_enum = GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE;
                  break;
              }
              for (uint32_t i = 0; i < group_->max_draw_buffers(); i++) {
                if (framebuffer->HasColorAttachment(i)) {
                  glGetFramebufferAttachmentParameterivEXT(
                      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                      framebuffer_enum, &v);
                  break;
                }
              }
            }
          } else {
            v = 8;
          }
        } else {
          glGetIntegerv(pname, &v);
        }
        params[0] = v;
      }
      return true;
    case GL_STENCIL_BITS:
      *num_written = 1;
      if (params) {
        GLint v = 0;
        if (gl_version_info().is_desktop_core_profile) {
          Framebuffer* framebuffer = GetBoundDrawFramebuffer();
          if (framebuffer) {
            if (framebuffer->HasStencilAttachment()) {
              glGetFramebufferAttachmentParameterivEXT(
                  GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                  GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &v);
            }
          } else {
            v = (back_buffer_has_stencil_ ? 8 : 0);
          }
        } else {
          glGetIntegerv(GL_STENCIL_BITS, &v);
        }
        params[0] = BoundFramebufferHasStencilAttachment() ? v : 0;
      }
      return true;
    case GL_COMPRESSED_TEXTURE_FORMATS:
      *num_written = validators_->compressed_texture_format.GetValues().size();
      if (params) {
        for (GLint ii = 0; ii < *num_written; ++ii) {
          params[ii] = validators_->compressed_texture_format.GetValues()[ii];
        }
      }
      return true;
    case GL_NUM_COMPRESSED_TEXTURE_FORMATS:
      *num_written = 1;
      if (params) {
        *params = validators_->compressed_texture_format.GetValues().size();
      }
      return true;
    case GL_NUM_SHADER_BINARY_FORMATS:
      *num_written = 1;
      if (params) {
        *params = validators_->shader_binary_format.GetValues().size();
      }
      return true;
    case GL_SHADER_BINARY_FORMATS:
      *num_written = validators_->shader_binary_format.GetValues().size();
      if (params) {
        for (GLint ii = 0; ii <  *num_written; ++ii) {
          params[ii] = validators_->shader_binary_format.GetValues()[ii];
        }
      }
      return true;
    case GL_SHADER_COMPILER:
      *num_written = 1;
      if (params) {
        *params = GL_TRUE;
      }
      return true;
    case GL_ARRAY_BUFFER_BINDING:
      *num_written = 1;
      if (params) {
        *params = GetClientId(
            buffer_manager(), state_.bound_array_buffer.get());
      }
      return true;
    case GL_ELEMENT_ARRAY_BUFFER_BINDING:
      *num_written = 1;
      if (params) {
        *params = GetClientId(
            buffer_manager(),
            state_.vertex_attrib_manager->element_array_buffer());
      }
      return true;
    case GL_COPY_READ_BUFFER_BINDING:
      *num_written = 1;
      if (params) {
        *params = GetClientId(
            buffer_manager(), state_.bound_copy_read_buffer.get());
      }
      return true;
    case GL_COPY_WRITE_BUFFER_BINDING:
      *num_written = 1;
      if (params) {
        *params = GetClientId(
            buffer_manager(), state_.bound_copy_write_buffer.get());
      }
      return true;
    case GL_PIXEL_PACK_BUFFER_BINDING:
      *num_written = 1;
      if (params) {
        *params = GetClientId(
            buffer_manager(), state_.bound_pixel_pack_buffer.get());
      }
      return true;
    case GL_PIXEL_UNPACK_BUFFER_BINDING:
      *num_written = 1;
      if (params) {
        *params = GetClientId(
            buffer_manager(), state_.bound_pixel_unpack_buffer.get());
      }
      return true;
    case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
      *num_written = 1;
      if (params) {
        *params = GetClientId(
            buffer_manager(), state_.bound_transform_feedback_buffer.get());
      }
      return true;
    case GL_UNIFORM_BUFFER_BINDING:
      *num_written = 1;
      if (params) {
        *params = GetClientId(
            buffer_manager(), state_.bound_uniform_buffer.get());
      }
      return true;
    case GL_FRAMEBUFFER_BINDING:
    // case GL_DRAW_FRAMEBUFFER_BINDING_EXT: (same as GL_FRAMEBUFFER_BINDING)
      *num_written = 1;
      if (params) {
        *params = GetClientId(
            framebuffer_manager(),
            GetFramebufferInfoForTarget(GL_FRAMEBUFFER));
      }
      return true;
    case GL_READ_FRAMEBUFFER_BINDING_EXT:
      *num_written = 1;
      if (params) {
        *params = GetClientId(
            framebuffer_manager(),
            GetFramebufferInfoForTarget(GL_READ_FRAMEBUFFER_EXT));
      }
      return true;
    case GL_RENDERBUFFER_BINDING:
      *num_written = 1;
      if (params) {
        Renderbuffer* renderbuffer =
            GetRenderbufferInfoForTarget(GL_RENDERBUFFER);
        if (renderbuffer) {
          *params = renderbuffer->client_id();
        } else {
          *params = 0;
        }
      }
      return true;
    case GL_CURRENT_PROGRAM:
      *num_written = 1;
      if (params) {
        *params = GetClientId(program_manager(), state_.current_program.get());
      }
      return true;
    case GL_VERTEX_ARRAY_BINDING_OES:
      *num_written = 1;
      if (params) {
        if (state_.vertex_attrib_manager.get() !=
            state_.default_vertex_attrib_manager.get()) {
          GLuint client_id = 0;
          vertex_array_manager_->GetClientId(
              state_.vertex_attrib_manager->service_id(), &client_id);
          *params = client_id;
        } else {
          *params = 0;
        }
      }
      return true;
    case GL_TEXTURE_BINDING_2D:
      *num_written = 1;
      if (params) {
        TextureUnit& unit = state_.texture_units[state_.active_texture_unit];
        if (unit.bound_texture_2d.get()) {
          *params = unit.bound_texture_2d->client_id();
        } else {
          *params = 0;
        }
      }
      return true;
    case GL_TEXTURE_BINDING_CUBE_MAP:
      *num_written = 1;
      if (params) {
        TextureUnit& unit = state_.texture_units[state_.active_texture_unit];
        if (unit.bound_texture_cube_map.get()) {
          *params = unit.bound_texture_cube_map->client_id();
        } else {
          *params = 0;
        }
      }
      return true;
    case GL_TEXTURE_BINDING_EXTERNAL_OES:
      *num_written = 1;
      if (params) {
        TextureUnit& unit = state_.texture_units[state_.active_texture_unit];
        if (unit.bound_texture_external_oes.get()) {
          *params = unit.bound_texture_external_oes->client_id();
        } else {
          *params = 0;
        }
      }
      return true;
    case GL_TEXTURE_BINDING_RECTANGLE_ARB:
      *num_written = 1;
      if (params) {
        TextureUnit& unit = state_.texture_units[state_.active_texture_unit];
        if (unit.bound_texture_rectangle_arb.get()) {
          *params = unit.bound_texture_rectangle_arb->client_id();
        } else {
          *params = 0;
        }
      }
      return true;
    case GL_BIND_GENERATES_RESOURCE_CHROMIUM:
      *num_written = 1;
      if (params) {
        params[0] = group_->bind_generates_resource() ? 1 : 0;
      }
      return true;
    case GL_MAX_DUAL_SOURCE_DRAW_BUFFERS_EXT:
      *num_written = 1;
      if (params) {
        params[0] = group_->max_dual_source_draw_buffers();
      }
      return true;

    case GL_MAJOR_VERSION:
      *num_written = 1;
      if (params) {
        // TODO(zmo): once we switch to MANGLE, we should query version numbers.
        params[0] = 3;
      }
      return true;
    case GL_MINOR_VERSION:
      // TODO(zmo): once we switch to MANGLE, we should query version numbers.
      *num_written = 1;
      if (params) {
        params[0] = 0;
      }
      return true;

    case GL_NUM_EXTENSIONS:
      // TODO(vmiura): Should the command buffer support this?
      *num_written = 1;
      if (params) {
        params[0] = 0;
      }
      return true;
    case GL_GPU_DISJOINT_EXT:
      // TODO(vmiura): Should the command buffer support this?
      *num_written = 1;
      if (params) {
        params[0] = 0;
      }
      return true;
    case GL_TIMESTAMP_EXT:
      // TODO(vmiura): Should the command buffer support this?
      *num_written = 1;
      if (params) {
        params[0] = 0;
      }
      return true;
    case GL_TEXTURE_BINDING_2D_ARRAY:
      *num_written = 1;
      if (params) {
        TextureUnit& unit = state_.texture_units[state_.active_texture_unit];
        if (unit.bound_texture_2d_array.get()) {
          *params = unit.bound_texture_2d_array->client_id();
        } else {
          *params = 0;
        }
      }
      return true;
    case GL_TEXTURE_BINDING_3D:
      *num_written = 1;
      if (params) {
        TextureUnit& unit = state_.texture_units[state_.active_texture_unit];
        if (unit.bound_texture_3d.get()) {
          *params = unit.bound_texture_3d->client_id();
        } else {
          *params = 0;
        }
      }
      return true;
    case GL_SAMPLER_BINDING:
      *num_written = 1;
      if (params) {
        // TODO(vmiura): Need to implement this for ES3 clients.  WebGL 2 tracks
        // this on the client side.
        *params = 0;
      }
      return true;
    case GL_TRANSFORM_FEEDBACK_BINDING:
      *num_written = 1;
      if (params) {
        // TODO(vmiura): Need to implement this for ES3 clients.  WebGL 2 tracks
        // this on the client side.
        *params = 0;
      }
      return true;
    case GL_NUM_PROGRAM_BINARY_FORMATS:
      *num_written = 1;
      if (params) {
        *params = 0;
      }
      return true;
    case GL_PROGRAM_BINARY_FORMATS:
      *num_written = 0;
      return true;

    default:
      if (pname >= GL_DRAW_BUFFER0_ARB && pname <= GL_DRAW_BUFFER15_ARB) {
        *num_written = 1;
        if (params) {
          if (pname < GL_DRAW_BUFFER0_ARB + group_->max_draw_buffers()) {
            Framebuffer* framebuffer =
                GetFramebufferInfoForTarget(GL_FRAMEBUFFER);
            if (framebuffer) {
              *params = framebuffer->GetDrawBuffer(pname);
            } else {  // backbuffer
              if (pname == GL_DRAW_BUFFER0_ARB)
                *params = back_buffer_draw_buffer_;
              else
                *params = GL_NONE;
            }
          } else {
            *params = GL_NONE;
          }
        }
        return true;
      }

      *num_written = util_.GLGetNumValuesReturned(pname);
      if (*num_written)
        break;

      return false;
  }

  if (params) {
    DCHECK(*num_written);
    pname = AdjustGetPname(pname);
    glGetIntegerv(pname, params);
  }
  return true;
}

bool GLES2DecoderImpl::GetNumValuesReturnedForGLGet(
    GLenum pname, GLsizei* num_values) {
  *num_values = 0;
  if (state_.GetStateAsGLint(pname, NULL, num_values)) {
    return true;
  }
  return GetHelper(pname, NULL, num_values);
}

GLenum GLES2DecoderImpl::AdjustGetPname(GLenum pname) {
  if (GL_MAX_SAMPLES == pname &&
      features().use_img_for_multisampled_render_to_texture) {
    return GL_MAX_SAMPLES_IMG;
  }
  if (GL_ALIASED_POINT_SIZE_RANGE == pname &&
      gl_version_info().is_desktop_core_profile) {
    return GL_POINT_SIZE_RANGE;
  }
  return pname;
}

void GLES2DecoderImpl::DoGetBooleanv(GLenum pname,
                                     GLboolean* params,
                                     GLsizei params_size) {
  DCHECK(params);
  std::unique_ptr<GLint[]> values(new GLint[params_size]);
  DoGetIntegerv(pname, values.get(), params_size);
  for (GLsizei ii = 0; ii < params_size; ++ii) {
    params[ii] = static_cast<GLboolean>(values[ii]);
  }
}

void GLES2DecoderImpl::DoGetFloatv(GLenum pname,
                                   GLfloat* params,
                                   GLsizei params_size) {
  DCHECK(params);
  GLsizei num_written = 0;
  if (state_.GetStateAsGLfloat(pname, params, &num_written)) {
    DCHECK_EQ(num_written, params_size);
    return;
  }

  switch (pname) {
    case GL_ALIASED_POINT_SIZE_RANGE:
    case GL_ALIASED_LINE_WIDTH_RANGE:
    case GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT:
      DCHECK_EQ(params_size, util_.GLGetNumValuesReturned(pname));
      pname = AdjustGetPname(pname);
      glGetFloatv(pname, params);
      return;
  }

  std::unique_ptr<GLint[]> values(new GLint[params_size]);
  DoGetIntegerv(pname, values.get(), params_size);
  for (GLsizei ii = 0; ii < params_size; ++ii) {
    params[ii] = static_cast<GLfloat>(values[ii]);
  }
}

void GLES2DecoderImpl::DoGetInteger64v(GLenum pname,
                                       GLint64* params,
                                       GLsizei params_size) {
  DCHECK(params);
  if (feature_info_->IsWebGL2OrES3Context()) {
    switch (pname) {
      case GL_MAX_ELEMENT_INDEX: {
        DCHECK_EQ(params_size, 1);
        if (gl_version_info().IsAtLeastGLES(3, 0) ||
            gl_version_info().IsAtLeastGL(4, 3)) {
          glGetInteger64v(GL_MAX_ELEMENT_INDEX, params);
        } else {
          // Assume that desktop GL implementations can generally support
          // 32-bit indices.
          if (params) {
            *params = std::numeric_limits<unsigned int>::max();
          }
        }
        return;
      }
    }
  }

  std::unique_ptr<GLint[]> values(new GLint[params_size]);
  DoGetIntegerv(pname, values.get(), params_size);
  for (GLsizei ii = 0; ii < params_size; ++ii) {
    params[ii] = static_cast<GLint64>(values[ii]);
  }
}

void GLES2DecoderImpl::DoGetIntegerv(GLenum pname,
                                     GLint* params,
                                     GLsizei params_size) {
  DCHECK(params);
  GLsizei num_written = 0;
  if (state_.GetStateAsGLint(pname, params, &num_written) ||
      GetHelper(pname, params, &num_written)) {
    DCHECK_EQ(num_written, params_size);
    return;
  }
  NOTREACHED() << "Unhandled enum " << pname;
}

template <typename TYPE>
void GLES2DecoderImpl::GetIndexedIntegerImpl(
    const char* function_name, GLenum target, GLuint index, TYPE* data) {
  DCHECK(data);
  scoped_refptr<IndexedBufferBindingHost> bindings;
  switch (target) {
    case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
    case GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
    case GL_TRANSFORM_FEEDBACK_BUFFER_START:
      if (index >= group_->max_transform_feedback_separate_attribs()) {
        LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "invalid index");
        return;
      }
      bindings = state_.bound_transform_feedback.get();
      break;
    case GL_UNIFORM_BUFFER_BINDING:
    case GL_UNIFORM_BUFFER_SIZE:
    case GL_UNIFORM_BUFFER_START:
      if (index >= group_->max_uniform_buffer_bindings()) {
        LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "invalid index");
        return;
      }
      bindings = state_.indexed_uniform_buffer_bindings.get();
      break;
    default:
      NOTREACHED();
      break;
  }
  DCHECK(bindings);
  switch (target) {
    case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
    case GL_UNIFORM_BUFFER_BINDING:
      {
        Buffer* buffer = bindings->GetBufferBinding(index);
        *data = static_cast<TYPE>(buffer ? buffer->service_id() : 0);
      }
      break;
    case GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
    case GL_UNIFORM_BUFFER_SIZE:
      *data = static_cast<TYPE>(bindings->GetBufferSize(index));
      break;
    case GL_TRANSFORM_FEEDBACK_BUFFER_START:
    case GL_UNIFORM_BUFFER_START:
      *data = static_cast<TYPE>(bindings->GetBufferStart(index));
      break;
    default:
      NOTREACHED();
      break;
  }
}

void GLES2DecoderImpl::DoGetIntegeri_v(GLenum target,
                                       GLuint index,
                                       GLint* params,
                                       GLsizei params_size) {
  GetIndexedIntegerImpl<GLint>("glGetIntegeri_v", target, index, params);
}

void GLES2DecoderImpl::DoGetInteger64i_v(GLenum target,
                                         GLuint index,
                                         GLint64* params,
                                         GLsizei params_size) {
  GetIndexedIntegerImpl<GLint64>("glGetInteger64i_v", target, index, params);
}

void GLES2DecoderImpl::DoGetProgramiv(GLuint program_id,
                                      GLenum pname,
                                      GLint* params,
                                      GLsizei params_size) {
  Program* program = GetProgramInfoNotShader(program_id, "glGetProgramiv");
  if (!program) {
    return;
  }
  program->GetProgramiv(pname, params);
}

void GLES2DecoderImpl::DoGetBufferParameteri64v(GLenum target,
                                                GLenum pname,
                                                GLint64* params,
                                                GLsizei params_size) {
  // Just delegate it. Some validation is actually done before this.
  buffer_manager()->ValidateAndDoGetBufferParameteri64v(
      &state_, target, pname, params);
}

void GLES2DecoderImpl::DoGetBufferParameteriv(GLenum target,
                                              GLenum pname,
                                              GLint* params,
                                              GLsizei params_size) {
  // Just delegate it. Some validation is actually done before this.
  buffer_manager()->ValidateAndDoGetBufferParameteriv(
      &state_, target, pname, params);
}

void GLES2DecoderImpl::DoBindAttribLocation(GLuint program_id,
                                            GLuint index,
                                            const std::string& name) {
  if (!StringIsValidForGLES(name)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glBindAttribLocation", "Invalid character");
    return;
  }
  if (ProgramManager::HasBuiltInPrefix(name)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, "glBindAttribLocation", "reserved prefix");
    return;
  }
  if (index >= group_->max_vertex_attribs()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glBindAttribLocation", "index out of range");
    return;
  }
  Program* program = GetProgramInfoNotShader(
      program_id, "glBindAttribLocation");
  if (!program) {
    return;
  }
  // At this point, the program's shaders may not be translated yet,
  // therefore, we may not find the hashed attribute name.
  // glBindAttribLocation call with original name is useless.
  // So instead, we should simply cache the binding, and then call
  // Program::ExecuteBindAttribLocationCalls() right before link.
  program->SetAttribLocationBinding(name, static_cast<GLint>(index));
  // TODO(zmo): Get rid of the following glBindAttribLocation call.
  glBindAttribLocation(program->service_id(), index, name.c_str());
}

error::Error GLES2DecoderImpl::HandleBindAttribLocationBucket(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BindAttribLocationBucket& c =
      *static_cast<const volatile gles2::cmds::BindAttribLocationBucket*>(
          cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLuint index = static_cast<GLuint>(c.index);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  DoBindAttribLocation(program, index, name_str);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::DoBindFragDataLocation(GLuint program_id,
                                                      GLuint colorName,
                                                      const std::string& name) {
  const char kFunctionName[] = "glBindFragDataLocationEXT";
  if (!StringIsValidForGLES(name)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, kFunctionName, "invalid character");
    return error::kNoError;
  }
  if (ProgramManager::HasBuiltInPrefix(name)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, kFunctionName, "reserved prefix");
    return error::kNoError;
  }
  if (colorName >= group_->max_draw_buffers()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, kFunctionName,
                       "colorName out of range");
    return error::kNoError;
  }
  Program* program = GetProgramInfoNotShader(program_id, kFunctionName);
  if (!program) {
    return error::kNoError;
  }
  program->SetProgramOutputLocationBinding(name, colorName);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBindFragDataLocationEXTBucket(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().ext_blend_func_extended) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::BindFragDataLocationEXTBucket& c =
      *static_cast<const volatile gles2::cmds::BindFragDataLocationEXTBucket*>(
          cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLuint colorNumber = static_cast<GLuint>(c.colorNumber);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  return DoBindFragDataLocation(program, colorNumber, name_str);
}

error::Error GLES2DecoderImpl::DoBindFragDataLocationIndexed(
    GLuint program_id,
    GLuint colorName,
    GLuint index,
    const std::string& name) {
  const char kFunctionName[] = "glBindFragDataLocationIndexEXT";
  if (!StringIsValidForGLES(name)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, kFunctionName, "invalid character");
    return error::kNoError;
  }
  if (ProgramManager::HasBuiltInPrefix(name)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, kFunctionName, "reserved prefix");
    return error::kNoError;
  }
  if (index != 0 && index != 1) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, kFunctionName, "index out of range");
    return error::kNoError;
  }
  if ((index == 0 && colorName >= group_->max_draw_buffers()) ||
      (index == 1 && colorName >= group_->max_dual_source_draw_buffers())) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, kFunctionName,
                       "colorName out of range for the color index");
    return error::kNoError;
  }
  Program* program = GetProgramInfoNotShader(program_id, kFunctionName);
  if (!program) {
    return error::kNoError;
  }
  program->SetProgramOutputLocationIndexedBinding(name, colorName, index);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBindFragDataLocationIndexedEXTBucket(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().ext_blend_func_extended) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::BindFragDataLocationIndexedEXTBucket& c =
      *static_cast<
          const volatile gles2::cmds::BindFragDataLocationIndexedEXTBucket*>(
          cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLuint colorNumber = static_cast<GLuint>(c.colorNumber);
  GLuint index = static_cast<GLuint>(c.index);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  return DoBindFragDataLocationIndexed(program, colorNumber, index, name_str);
}

void GLES2DecoderImpl::DoBindUniformLocationCHROMIUM(GLuint program_id,
                                                     GLint location,
                                                     const std::string& name) {
  if (!StringIsValidForGLES(name)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE,
        "glBindUniformLocationCHROMIUM", "Invalid character");
    return;
  }
  if (ProgramManager::HasBuiltInPrefix(name)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        "glBindUniformLocationCHROMIUM", "reserved prefix");
    return;
  }
  if (location < 0 ||
      static_cast<uint32_t>(location) >=
          (group_->max_fragment_uniform_vectors() +
           group_->max_vertex_uniform_vectors()) *
              4) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE,
        "glBindUniformLocationCHROMIUM", "location out of range");
    return;
  }
  Program* program = GetProgramInfoNotShader(
      program_id, "glBindUniformLocationCHROMIUM");
  if (!program) {
    return;
  }
  if (!program->SetUniformLocationBinding(name, location)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE,
        "glBindUniformLocationCHROMIUM", "location out of range");
  }
}

error::Error GLES2DecoderImpl::HandleBindUniformLocationCHROMIUMBucket(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BindUniformLocationCHROMIUMBucket& c =
      *static_cast<
          const volatile gles2::cmds::BindUniformLocationCHROMIUMBucket*>(
          cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLint location = static_cast<GLint>(c.location);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  DoBindUniformLocationCHROMIUM(program, location, name_str);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDeleteShader(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DeleteShader& c =
      *static_cast<const volatile gles2::cmds::DeleteShader*>(cmd_data);
  GLuint client_id = c.shader;
  if (client_id) {
    Shader* shader = GetShader(client_id);
    if (shader) {
      if (!shader->IsDeleted()) {
        shader_manager()->Delete(shader);
      }
    } else {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glDeleteShader", "unknown shader");
    }
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDeleteProgram(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DeleteProgram& c =
      *static_cast<const volatile gles2::cmds::DeleteProgram*>(cmd_data);
  GLuint client_id = c.program;
  if (client_id) {
    Program* program = GetProgram(client_id);
    if (program) {
      if (!program->IsDeleted()) {
        program_manager()->MarkAsDeleted(shader_manager(), program);
      }
    } else {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_VALUE, "glDeleteProgram", "unknown program");
    }
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::DoClear(GLbitfield mask) {
  const char* func_name = "glClear";
  DCHECK(!ShouldDeferDraws());
  if (CheckBoundDrawFramebufferValid(func_name)) {
    ApplyDirtyState();
    if (workarounds().gl_clear_broken) {
      if (!BoundFramebufferHasDepthAttachment())
        mask &= ~GL_DEPTH_BUFFER_BIT;
      if (!BoundFramebufferHasStencilAttachment())
        mask &= ~GL_STENCIL_BUFFER_BIT;
      ClearFramebufferForWorkaround(mask);
      return error::kNoError;
    }
    if (mask & GL_COLOR_BUFFER_BIT) {
      Framebuffer* framebuffer =
          framebuffer_state_.bound_draw_framebuffer.get();
      if (framebuffer && framebuffer->ContainsActiveIntegerAttachments()) {
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
            "can't be called on integer buffers");
        return error::kNoError;
      }
    }
    glClear(mask);
  }
  return error::kNoError;
}

void GLES2DecoderImpl::DoClearBufferiv(GLenum buffer,
                                       GLint drawbuffer,
                                       const volatile GLint* value) {
  const char* func_name = "glClearBufferiv";
  if (!CheckBoundDrawFramebufferValid(func_name))
    return;
  ApplyDirtyState();

  if (buffer == GL_COLOR) {
    if (drawbuffer < 0 ||
        drawbuffer >= static_cast<GLint>(group_->max_draw_buffers())) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "invalid drawBuffer");
      return;
    }
    GLenum internal_format =
        GetBoundColorDrawBufferInternalFormat(drawbuffer);
    if (!GLES2Util::IsSignedIntegerFormat(internal_format)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
          "can only be called on signed integer buffers");
      return;
    }
  } else {
    DCHECK(buffer == GL_STENCIL);
    if (drawbuffer != 0) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "invalid drawBuffer");
      return;
    }
    if (!BoundFramebufferHasStencilAttachment()) {
      return;
    }
  }
  MarkDrawBufferAsCleared(buffer, drawbuffer);
  glClearBufferiv(buffer, drawbuffer, const_cast<const GLint*>(value));
}

void GLES2DecoderImpl::DoClearBufferuiv(GLenum buffer,
                                        GLint drawbuffer,
                                        const volatile GLuint* value) {
  const char* func_name = "glClearBufferuiv";
  if (!CheckBoundDrawFramebufferValid(func_name))
    return;
  ApplyDirtyState();

  if (drawbuffer < 0 ||
      drawbuffer >= static_cast<GLint>(group_->max_draw_buffers())) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "invalid drawBuffer");
    return;
  }
  GLenum internal_format =
      GetBoundColorDrawBufferInternalFormat(drawbuffer);
  if (!GLES2Util::IsUnsignedIntegerFormat(internal_format)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
        "can only be called on unsigned integer buffers");
    return;
  }
  MarkDrawBufferAsCleared(buffer, drawbuffer);
  glClearBufferuiv(buffer, drawbuffer, const_cast<const GLuint*>(value));
}

void GLES2DecoderImpl::DoClearBufferfv(GLenum buffer,
                                       GLint drawbuffer,
                                       const volatile GLfloat* value) {
  const char* func_name = "glClearBufferfv";
  if (!CheckBoundDrawFramebufferValid(func_name))
    return;
  ApplyDirtyState();

  if (buffer == GL_COLOR) {
    if (drawbuffer < 0 ||
        drawbuffer >= static_cast<GLint>(group_->max_draw_buffers())) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "invalid drawBuffer");
      return;
    }
    GLenum internal_format =
        GetBoundColorDrawBufferInternalFormat(drawbuffer);
    if (GLES2Util::IsIntegerFormat(internal_format)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
          "can only be called on float buffers");
      return;
    }
  } else {
    DCHECK(buffer == GL_DEPTH);
    if (drawbuffer != 0) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "invalid drawBuffer");
      return;
    }
    if (!BoundFramebufferHasDepthAttachment()) {
      return;
    }
  }
  MarkDrawBufferAsCleared(buffer, drawbuffer);
  glClearBufferfv(buffer, drawbuffer, const_cast<const GLfloat*>(value));
}

void GLES2DecoderImpl::DoClearBufferfi(
    GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil) {
  const char* func_name = "glClearBufferfi";
  if (!CheckBoundDrawFramebufferValid(func_name))
    return;
  ApplyDirtyState();

  if (drawbuffer != 0) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, func_name, "invalid drawBuffer");
    return;
  }
  if (!BoundFramebufferHasDepthAttachment() &&
      !BoundFramebufferHasStencilAttachment()) {
    return;
  }
  MarkDrawBufferAsCleared(GL_DEPTH, drawbuffer);
  MarkDrawBufferAsCleared(GL_STENCIL, drawbuffer);
  glClearBufferfi(buffer, drawbuffer, depth, stencil);
}

void GLES2DecoderImpl::DoFramebufferRenderbuffer(
    GLenum target, GLenum attachment, GLenum renderbuffertarget,
    GLuint client_renderbuffer_id) {
  Framebuffer* framebuffer = GetFramebufferInfoForTarget(target);
  if (!framebuffer) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        "glFramebufferRenderbuffer", "no framebuffer bound");
    return;
  }
  GLuint service_id = 0;
  Renderbuffer* renderbuffer = NULL;
  if (client_renderbuffer_id) {
    renderbuffer = GetRenderbuffer(client_renderbuffer_id);
    if (!renderbuffer) {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_OPERATION,
          "glFramebufferRenderbuffer", "unknown renderbuffer");
      return;
    }
    service_id = renderbuffer->service_id();
  }
  std::vector<GLenum> attachments;
  if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
    attachments.push_back(GL_DEPTH_ATTACHMENT);
    attachments.push_back(GL_STENCIL_ATTACHMENT);
  } else {
    attachments.push_back(attachment);
  }
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("glFramebufferRenderbuffer");
  for (GLenum attachment_point : attachments) {
    glFramebufferRenderbufferEXT(
        target, attachment_point, renderbuffertarget, service_id);
    GLenum error = LOCAL_PEEK_GL_ERROR("glFramebufferRenderbuffer");
    if (error == GL_NO_ERROR) {
      framebuffer->AttachRenderbuffer(attachment_point, renderbuffer);
    }
  }
  if (framebuffer == framebuffer_state_.bound_draw_framebuffer.get()) {
    framebuffer_state_.clear_state_dirty = true;
  }
  OnFboChanged();
}

void GLES2DecoderImpl::DoDisable(GLenum cap) {
  if (SetCapabilityState(cap, false)) {
    if (cap == GL_PRIMITIVE_RESTART_FIXED_INDEX &&
        feature_info_->feature_flags().emulate_primitive_restart_fixed_index) {
      // Enable and Disable PRIMITIVE_RESTART only before and after
      // DrawElements* for old desktop GL.
      return;
    }
    glDisable(cap);
  }
}

void GLES2DecoderImpl::DoEnable(GLenum cap) {
  if (SetCapabilityState(cap, true)) {
    if (cap == GL_PRIMITIVE_RESTART_FIXED_INDEX &&
        feature_info_->feature_flags().emulate_primitive_restart_fixed_index) {
      // Enable and Disable PRIMITIVE_RESTART only before and after
      // DrawElements* for old desktop GL.
      return;
    }
    glEnable(cap);
  }
}

void GLES2DecoderImpl::DoDepthRangef(GLclampf znear, GLclampf zfar) {
  state_.z_near = std::min(1.0f, std::max(0.0f, znear));
  state_.z_far = std::min(1.0f, std::max(0.0f, zfar));
  glDepthRange(znear, zfar);
}

void GLES2DecoderImpl::DoSampleCoverage(GLclampf value, GLboolean invert) {
  state_.sample_coverage_value = std::min(1.0f, std::max(0.0f, value));
  state_.sample_coverage_invert = (invert != 0);
  glSampleCoverage(state_.sample_coverage_value, invert);
}

// Assumes framebuffer is complete.
void GLES2DecoderImpl::ClearUnclearedAttachments(
    GLenum target, Framebuffer* framebuffer) {
  // Clear textures that we can't use glClear first. These textures will be
  // marked as cleared after the call and no longer be part of the following
  // code.
  framebuffer->ClearUnclearedIntOr3DTexturesOrPartiallyClearedTextures(
      this, texture_manager());

  bool cleared_int_renderbuffers = false;
  Framebuffer* draw_framebuffer = GetBoundDrawFramebuffer();
  if (framebuffer->HasUnclearedIntRenderbufferAttachments()) {
    if (target == GL_READ_FRAMEBUFFER && draw_framebuffer != framebuffer) {
      // TODO(zmo): There is no guarantee that an FBO that is complete on the
      // READ attachment will be complete as a DRAW attachment.
      glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, framebuffer->service_id());
    }
    state_.SetDeviceColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    state_.SetDeviceCapabilityState(GL_SCISSOR_TEST, false);

    // TODO(zmo): Assume DrawBuffers() does not affect ClearBuffer().
    framebuffer->ClearUnclearedIntRenderbufferAttachments(
        renderbuffer_manager());

    cleared_int_renderbuffers = true;
  }

  GLbitfield clear_bits = 0;
  bool reset_draw_buffers = false;
  if (framebuffer->HasUnclearedColorAttachments()) {
    // We should always use alpha == 0 here, because 1) some draw buffers may
    // have alpha and some may not; 2) we won't have the same situation as the
    // back buffer where alpha channel exists but is not requested.
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    state_.SetDeviceColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    clear_bits |= GL_COLOR_BUFFER_BIT;

    if (SupportsDrawBuffers()) {
      reset_draw_buffers =
          framebuffer->PrepareDrawBuffersForClearingUninitializedAttachments();
    }
  }

  if (framebuffer->HasUnclearedAttachment(GL_STENCIL_ATTACHMENT)) {
    glClearStencil(0);
    state_.SetDeviceStencilMaskSeparate(GL_FRONT, kDefaultStencilMask);
    state_.SetDeviceStencilMaskSeparate(GL_BACK, kDefaultStencilMask);
    clear_bits |= GL_STENCIL_BUFFER_BIT;
  }

  if (framebuffer->HasUnclearedAttachment(GL_DEPTH_ATTACHMENT)) {
    glClearDepth(1.0f);
    state_.SetDeviceDepthMask(GL_TRUE);
    clear_bits |= GL_DEPTH_BUFFER_BIT;
  }

  if (clear_bits) {
    if (!cleared_int_renderbuffers &&
        target == GL_READ_FRAMEBUFFER && draw_framebuffer != framebuffer) {
      // TODO(zmo): There is no guarantee that an FBO that is complete on the
      // READ attachment will be complete as a DRAW attachment.
      glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, framebuffer->service_id());
    }
    state_.SetDeviceCapabilityState(GL_SCISSOR_TEST, false);
    if (workarounds().gl_clear_broken) {
      ClearFramebufferForWorkaround(clear_bits);
    } else {
      glClear(clear_bits);
    }
  }

  if (cleared_int_renderbuffers || clear_bits) {
    if (reset_draw_buffers)
      framebuffer->RestoreDrawBuffers();
    RestoreClearState();
    if (target == GL_READ_FRAMEBUFFER && draw_framebuffer != framebuffer) {
      GLuint service_id = draw_framebuffer ? draw_framebuffer->service_id() :
                                             GetBackbufferServiceId();
      glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, service_id);
    }
  }

  framebuffer_manager()->MarkAttachmentsAsCleared(
      framebuffer, renderbuffer_manager(), texture_manager());
}

void GLES2DecoderImpl::RestoreClearState() {
  framebuffer_state_.clear_state_dirty = true;
  glClearColor(
      state_.color_clear_red, state_.color_clear_green, state_.color_clear_blue,
      state_.color_clear_alpha);
  glClearStencil(state_.stencil_clear);
  glClearDepth(state_.depth_clear);
  state_.SetDeviceCapabilityState(GL_SCISSOR_TEST,
                                  state_.enable_flags.scissor_test);
  glScissor(state_.scissor_x, state_.scissor_y, state_.scissor_width,
            state_.scissor_height);
}

GLenum GLES2DecoderImpl::DoCheckFramebufferStatus(GLenum target) {
  Framebuffer* framebuffer =
      GetFramebufferInfoForTarget(target);
  if (!framebuffer) {
    return GL_FRAMEBUFFER_COMPLETE;
  }
  GLenum completeness = framebuffer->IsPossiblyComplete(feature_info_.get());
  if (completeness != GL_FRAMEBUFFER_COMPLETE) {
    return completeness;
  }
  return framebuffer->GetStatus(texture_manager(), target);
}

void GLES2DecoderImpl::DoFramebufferTexture2D(
    GLenum target, GLenum attachment, GLenum textarget,
    GLuint client_texture_id, GLint level) {
  DoFramebufferTexture2DCommon(
    "glFramebufferTexture2D", target, attachment,
    textarget, client_texture_id, level, 0);
}

void GLES2DecoderImpl::DoFramebufferTexture2DMultisample(
    GLenum target, GLenum attachment, GLenum textarget,
    GLuint client_texture_id, GLint level, GLsizei samples) {
  DoFramebufferTexture2DCommon(
    "glFramebufferTexture2DMultisample", target, attachment,
    textarget, client_texture_id, level, samples);
}

void GLES2DecoderImpl::DoFramebufferTexture2DCommon(
    const char* name, GLenum target, GLenum attachment, GLenum textarget,
    GLuint client_texture_id, GLint level, GLsizei samples) {
  if (samples > renderbuffer_manager()->max_samples()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE,
        "glFramebufferTexture2DMultisample", "samples too large");
    return;
  }
  Framebuffer* framebuffer = GetFramebufferInfoForTarget(target);
  if (!framebuffer) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        name, "no framebuffer bound.");
    return;
  }
  GLuint service_id = 0;
  TextureRef* texture_ref = NULL;
  if (client_texture_id) {
    texture_ref = GetTexture(client_texture_id);
    if (!texture_ref) {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_OPERATION,
          name, "unknown texture_ref");
      return;
    }
    GLenum texture_target = texture_ref->texture()->target();
    if (texture_target != GLES2Util::GLFaceTargetToTextureTarget(textarget)) {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_OPERATION,
          name, "Attachment textarget doesn't match texture target");
      return;
    }
    service_id = texture_ref->service_id();
  }

  if ((level > 0 && !feature_info_->IsWebGL2OrES3Context()) ||
      !texture_manager()->ValidForTarget(textarget, level, 0, 0, 1)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE,
        name, "level out of range");
    return;
  }

  if (texture_ref)
    DoCopyTexImageIfNeeded(texture_ref->texture(), textarget);

  std::vector<GLenum> attachments;
  if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
    attachments.push_back(GL_DEPTH_ATTACHMENT);
    attachments.push_back(GL_STENCIL_ATTACHMENT);
  } else {
    attachments.push_back(attachment);
  }
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER(name);
  for (size_t ii = 0; ii < attachments.size(); ++ii) {
    if (0 == samples) {
      glFramebufferTexture2DEXT(
          target, attachments[ii], textarget, service_id, level);
    } else {
      if (features().use_img_for_multisampled_render_to_texture) {
        glFramebufferTexture2DMultisampleIMG(
            target, attachments[ii], textarget, service_id, level, samples);
      } else {
        glFramebufferTexture2DMultisampleEXT(
            target, attachments[ii], textarget, service_id, level, samples);
      }
    }
    GLenum error = LOCAL_PEEK_GL_ERROR(name);
    if (error == GL_NO_ERROR) {
      framebuffer->AttachTexture(attachments[ii], texture_ref, textarget, level,
                                 samples);
    }
  }
  if (framebuffer == framebuffer_state_.bound_draw_framebuffer.get()) {
    framebuffer_state_.clear_state_dirty = true;
  }

  OnFboChanged();
}

void GLES2DecoderImpl::DoFramebufferTextureLayer(
    GLenum target, GLenum attachment, GLuint client_texture_id,
    GLint level, GLint layer) {
  const char* function_name = "glFramebufferTextureLayer";

  TextureRef* texture_ref = nullptr;
  Framebuffer* framebuffer = GetFramebufferInfoForTarget(target);
  if (!framebuffer) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, function_name, "no framebuffer bound.");
    return;
  }
  GLuint service_id = 0;
  GLenum texture_target = 0;
  if (client_texture_id) {
    texture_ref = GetTexture(client_texture_id);
    if (!texture_ref) {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_VALUE, function_name, "unknown texture");
      return;
    }
    service_id = texture_ref->service_id();

    texture_target = texture_ref->texture()->target();
    switch (texture_target) {
      case GL_TEXTURE_3D:
      case GL_TEXTURE_2D_ARRAY:
        break;
      default:
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
            "texture is neither TEXTURE_3D nor TEXTURE_2D_ARRAY");
        return;
    }
    if (!texture_manager()->ValidForTarget(texture_target, level,
                                           0, 0, layer)) {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_VALUE, function_name, "invalid level or layer");
      return;
    }
  }
  glFramebufferTextureLayer(target, attachment, service_id, level, layer);
  if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
    framebuffer->AttachTextureLayer(
        GL_DEPTH_ATTACHMENT, texture_ref, texture_target, level, layer);
    framebuffer->AttachTextureLayer(
        GL_STENCIL_ATTACHMENT, texture_ref, texture_target, level, layer);
  } else {
    framebuffer->AttachTextureLayer(
        attachment, texture_ref, texture_target, level, layer);
  }
  if (framebuffer == framebuffer_state_.bound_draw_framebuffer.get()) {
    framebuffer_state_.clear_state_dirty = true;
  }
}

void GLES2DecoderImpl::DoGetFramebufferAttachmentParameteriv(
    GLenum target,
    GLenum attachment,
    GLenum pname,
    GLint* params,
    GLsizei params_size) {
  const char kFunctionName[] = "glGetFramebufferAttachmentParameteriv";
  Framebuffer* framebuffer = GetFramebufferInfoForTarget(target);
  if (!framebuffer) {
    if (!feature_info_->IsWebGL2OrES3Context()) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, kFunctionName,
          "no framebuffer bound");
      return;
    }
    if (!validators_->backbuffer_attachment.IsValid(attachment)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, kFunctionName,
          "invalid attachment for backbuffer");
      return;
    }
    switch (pname) {
      case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
        *params = static_cast<GLint>(GL_FRAMEBUFFER_DEFAULT);
        return;
      case GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE:
      case GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE:
      case GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE:
      case GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE:
      case GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE:
      case GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE:
      case GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE:
      case GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING:
        // Delegate to underlying driver.
        break;
      default:
        LOCAL_SET_GL_ERROR(GL_INVALID_ENUM, kFunctionName,
            "invalid pname for backbuffer");
        return;
    }
    if (GetBackbufferServiceId() != 0) {  // Emulated backbuffer.
      switch (attachment) {
        case GL_BACK:
          attachment = GL_COLOR_ATTACHMENT0;
          break;
        case GL_DEPTH:
          attachment = GL_DEPTH_ATTACHMENT;
          break;
        case GL_STENCIL:
          attachment = GL_STENCIL_ATTACHMENT;
          break;
        default:
          NOTREACHED();
          break;
      }
    }
  } else {
    if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
      const Framebuffer::Attachment* depth =
          framebuffer->GetAttachment(GL_DEPTH_ATTACHMENT);
      const Framebuffer::Attachment* stencil =
          framebuffer->GetAttachment(GL_STENCIL_ATTACHMENT);
      if ((!depth && !stencil) ||
          (depth && stencil && depth->IsSameAttachment(stencil))) {
        attachment = GL_DEPTH_ATTACHMENT;
      } else {
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, kFunctionName,
                           "depth and stencil attachment mismatch");
        return;
      }
    }
  }
  if (pname == GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_SAMPLES_EXT &&
      features().use_img_for_multisampled_render_to_texture) {
    pname = GL_TEXTURE_SAMPLES_IMG;
  }
  if (pname == GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME) {
    DCHECK(framebuffer);
    // If we query from the driver, it will be service ID; however, we need to
    // return the client ID here.
    const Framebuffer::Attachment* attachment_object =
        framebuffer->GetAttachment(attachment);
    *params = attachment_object ? attachment_object->object_name() : 0;
    return;
  }

  glGetFramebufferAttachmentParameterivEXT(target, attachment, pname, params);
  // We didn't perform a full error check before gl call.
  LOCAL_PEEK_GL_ERROR(kFunctionName);
}

void GLES2DecoderImpl::DoGetRenderbufferParameteriv(GLenum target,
                                                    GLenum pname,
                                                    GLint* params,
                                                    GLsizei params_size) {
  Renderbuffer* renderbuffer =
      GetRenderbufferInfoForTarget(GL_RENDERBUFFER);
  if (!renderbuffer) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        "glGetRenderbufferParameteriv", "no renderbuffer bound");
    return;
  }

  EnsureRenderbufferBound();
  switch (pname) {
    case GL_RENDERBUFFER_INTERNAL_FORMAT:
      *params = renderbuffer->internal_format();
      break;
    case GL_RENDERBUFFER_WIDTH:
      *params = renderbuffer->width();
      break;
    case GL_RENDERBUFFER_HEIGHT:
      *params = renderbuffer->height();
      break;
    case GL_RENDERBUFFER_SAMPLES_EXT:
      if (features().use_img_for_multisampled_render_to_texture) {
        glGetRenderbufferParameterivEXT(target, GL_RENDERBUFFER_SAMPLES_IMG,
            params);
      } else {
        glGetRenderbufferParameterivEXT(target, GL_RENDERBUFFER_SAMPLES_EXT,
            params);
      }
      break;
    default:
      glGetRenderbufferParameterivEXT(target, pname, params);
      break;
  }
}

void GLES2DecoderImpl::DoBlitFramebufferCHROMIUM(
    GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
    GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
    GLbitfield mask, GLenum filter) {
  const char* func_name = "glBlitFramebufferCHROMIUM";
  DCHECK(!ShouldDeferReads() && !ShouldDeferDraws());

  if (!CheckBoundFramebufferValid(func_name)) {
    return;
  }

  if (GetBoundFramebufferSamples(GL_DRAW_FRAMEBUFFER) > 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
                       "destination framebuffer is multisampled");
    return;
  }

  GLsizei read_buffer_samples = GetBoundFramebufferSamples(GL_READ_FRAMEBUFFER);
  if (read_buffer_samples > 0 &&
      (srcX0 != dstX0 || srcY0 != dstY0 || srcX1 != dstX1 || srcY1 != dstY1)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
        "src framebuffer is multisampled, but src/dst regions are different");
    return;
  }

  // Check whether read framebuffer and draw framebuffer have identical image
  // TODO(yunchao): consider doing something like CheckFramebufferStatus().
  // We cache the validation results, and if read_framebuffer doesn't change,
  // draw_framebuffer doesn't change, then use the cached status.
  enum FeedbackLoopState {
    FeedbackLoopTrue,
    FeedbackLoopFalse,
    FeedbackLoopUnknown
  };

  FeedbackLoopState is_feedback_loop = FeedbackLoopUnknown;
  Framebuffer* read_framebuffer =
      framebuffer_state_.bound_read_framebuffer.get();
  Framebuffer* draw_framebuffer =
      framebuffer_state_.bound_draw_framebuffer.get();
  // If both read framebuffer and draw framebuffer are default framebuffer,
  // They always have identical image. Otherwise, if one of read framebuffer
  // and draw framebuffe is default framebuffer, but the other is fbo, they
  // always have no identical image.
  if (!read_framebuffer && !draw_framebuffer) {
    is_feedback_loop = FeedbackLoopTrue;
  } else if (!read_framebuffer || !draw_framebuffer) {
    is_feedback_loop = FeedbackLoopFalse;
  } else {
    DCHECK(read_framebuffer && draw_framebuffer);
    if ((mask & GL_DEPTH_BUFFER_BIT) != 0) {
      const Framebuffer::Attachment* depth_buffer_read =
          read_framebuffer->GetAttachment(GL_DEPTH_ATTACHMENT);
      const Framebuffer::Attachment* depth_buffer_draw =
          draw_framebuffer->GetAttachment(GL_DEPTH_ATTACHMENT);
      if (!depth_buffer_draw || !depth_buffer_read) {
        mask &= ~GL_DEPTH_BUFFER_BIT;
      } else if (depth_buffer_draw->IsSameAttachment(depth_buffer_read)) {
        is_feedback_loop = FeedbackLoopTrue;
      }
    }
    if ((mask & GL_STENCIL_BUFFER_BIT) != 0) {
      const Framebuffer::Attachment* stencil_buffer_read =
          read_framebuffer->GetAttachment(GL_STENCIL_ATTACHMENT);
      const Framebuffer::Attachment* stencil_buffer_draw =
          draw_framebuffer->GetAttachment(GL_STENCIL_ATTACHMENT);
      if (!stencil_buffer_draw || !stencil_buffer_read) {
        mask &= ~GL_STENCIL_BUFFER_BIT;
      } else if (stencil_buffer_draw->IsSameAttachment(stencil_buffer_read)) {
        is_feedback_loop = FeedbackLoopTrue;
      }
    }
    if (!mask)
      return;
  }

  GLenum src_internal_format = GetBoundReadFramebufferInternalFormat();
  GLenum src_type = GetBoundReadFramebufferTextureType();

  bool read_buffer_has_srgb =
      GetColorEncodingFromInternalFormat(src_internal_format) == GL_SRGB;
  bool draw_buffers_has_srgb = false;
  if ((mask & GL_COLOR_BUFFER_BIT) != 0) {
    bool is_src_signed_int =
        GLES2Util::IsSignedIntegerFormat(src_internal_format);
    bool is_src_unsigned_int =
        GLES2Util::IsUnsignedIntegerFormat(src_internal_format);
    DCHECK(!is_src_signed_int || !is_src_unsigned_int);

    if ((is_src_signed_int || is_src_unsigned_int) && filter == GL_LINEAR) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
                         "invalid filter for integer format");
      return;
    }

    GLenum src_sized_format =
        GLES2Util::ConvertToSizedFormat(src_internal_format, src_type);
    DCHECK(read_framebuffer || (is_feedback_loop != FeedbackLoopUnknown));
    const Framebuffer::Attachment* read_buffer =
        is_feedback_loop == FeedbackLoopUnknown ?
        read_framebuffer->GetReadBufferAttachment() : nullptr;
    for (uint32_t ii = 0; ii < group_->max_draw_buffers(); ++ii) {
      GLenum dst_format = GetBoundColorDrawBufferInternalFormat(
          static_cast<GLint>(ii));
      GLenum dst_type = GetBoundColorDrawBufferType(static_cast<GLint>(ii));
      if (dst_format == 0)
        continue;
      if (GetColorEncodingFromInternalFormat(dst_format) == GL_SRGB)
        draw_buffers_has_srgb = true;
      if (read_buffer_samples > 0 &&
          (src_sized_format !=
           GLES2Util::ConvertToSizedFormat(dst_format, dst_type))) {
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
                           "src and dst formats differ for color");
        return;
      }
      bool is_dst_signed_int = GLES2Util::IsSignedIntegerFormat(dst_format);
      bool is_dst_unsigned_int = GLES2Util::IsUnsignedIntegerFormat(dst_format);
      DCHECK(!is_dst_signed_int || !is_dst_unsigned_int);
      if (is_src_signed_int != is_dst_signed_int ||
          is_src_unsigned_int != is_dst_unsigned_int) {
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
                           "incompatible src/dst color formats");
        return;
      }
      // Check whether draw buffers have identical color image with read buffer
      if (is_feedback_loop == FeedbackLoopUnknown) {
        GLenum attachment = static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + ii);
        DCHECK(draw_framebuffer);
        const Framebuffer::Attachment* draw_buffer =
            draw_framebuffer->GetAttachment(attachment);
        if (!draw_buffer || !read_buffer) {
          continue;
        }
        if (draw_buffer->IsSameAttachment(read_buffer)) {
          is_feedback_loop = FeedbackLoopTrue;
          break;
        }
      }
    }
  }
  if (is_feedback_loop == FeedbackLoopTrue) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
        "source buffer and destination buffers are identical");
    return;
  }

  if ((mask & (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) != 0) {
    if (filter != GL_NEAREST) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
                         "invalid filter for depth/stencil");
      return;
    }

    if ((GetBoundFramebufferDepthFormat(GL_READ_FRAMEBUFFER) !=
         GetBoundFramebufferDepthFormat(GL_DRAW_FRAMEBUFFER)) ||
        (GetBoundFramebufferStencilFormat(GL_READ_FRAMEBUFFER) !=
         GetBoundFramebufferStencilFormat(GL_DRAW_FRAMEBUFFER))) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
                         "src and dst formats differ for depth/stencil");
      return;
    }
  }

  if (workarounds().adjust_src_dst_region_for_blitframebuffer) {
    gfx::Size read_size = GetBoundReadFramebufferSize();
    gfx::Rect src_bounds(0, 0, read_size.width(), read_size.height());
    GLint src_x = srcX1 > srcX0 ? srcX0 : srcX1;
    GLint src_y = srcY1 > srcY0 ? srcY0 : srcY1;
    base::CheckedNumeric<GLint> src_width_temp = srcX1;
    src_width_temp -= srcX0;
    base::CheckedNumeric<GLint> src_height_temp = srcY1;
    src_height_temp -= srcY0;
    GLuint src_width = 0, src_height = 0;
    if (!src_width_temp.IsValid() || !src_height_temp.IsValid()) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
                         "the width or height of src region overflow");
      return;
    }
    src_width = std::abs(src_width_temp.ValueOrDefault(0));
    src_height = std::abs(src_height_temp.ValueOrDefault(0));

    gfx::Rect src_region(src_x, src_y, src_width, src_height);
    if (!src_bounds.Contains(src_region) &&
        (src_width != 0) && (src_height != 0)) {
      // If pixels lying outside the read framebuffer, adjust src region
      // and dst region to appropriate in-bounds regions respectively.
      src_bounds.Intersect(src_region);
      GLuint src_real_width = src_bounds.width();
      GLuint src_real_height = src_bounds.height();
      GLuint xoffset = src_bounds.x() - src_x;
      GLuint yoffset = src_bounds.y() - src_y;
      // if X/Y is reversed, use the top/right out-of-bounds region for mapping
      // to dst region, instead of left/bottom out-of-bounds region for mapping.
      if (((srcX1 > srcX0) && (dstX1 < dstX0)) ||
          ((srcX1 < srcX0) && (dstX1 > dstX0))) {
        xoffset = src_x + src_width - src_bounds.x() - src_bounds.width();
      }
      if (((srcY1 > srcY0) && (dstY1 < dstY0)) ||
          ((srcY1 < srcY0) && (dstY1 > dstY0))) {
        yoffset = src_y + src_height - src_bounds.y() - src_bounds.height();
      }

      GLint dst_x = dstX1 > dstX0 ? dstX0 : dstX1;
      GLint dst_y = dstY1 > dstY0 ? dstY0 : dstY1;
      base::CheckedNumeric<GLint> dst_width_temp = dstX1;
      dst_width_temp -= dstX0;
      base::CheckedNumeric<GLint> dst_height_temp = dstY1;
      dst_height_temp -= dstY0;
      GLuint dst_width = 0, dst_height = 0;
      if (!dst_width_temp.IsValid() || !dst_height_temp.IsValid()) {
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
                           "the width or height of dst region overflow");
        return;
      }
      dst_width = std::abs(dst_width_temp.ValueOrDefault(0));
      dst_height = std::abs(dst_height_temp.ValueOrDefault(0));

      GLfloat dst_mapping_width =
          static_cast<GLfloat>(src_real_width) * dst_width / src_width;
      GLfloat dst_mapping_height =
          static_cast<GLfloat>(src_real_height) * dst_height / src_height;
      GLfloat dst_mapping_xoffset =
          static_cast<GLfloat>(xoffset) * dst_width / src_width;
      GLfloat dst_mapping_yoffset =
          static_cast<GLfloat>(yoffset) * dst_height / src_height;

      GLuint dst_mapping_x0 =
          std::round(dst_x + dst_mapping_xoffset);
      GLuint dst_mapping_y0 =
          std::round(dst_y + dst_mapping_yoffset);

      GLuint dst_mapping_x1 =
          std::round(dst_x + dst_mapping_xoffset + dst_mapping_width);
      GLuint dst_mapping_y1 =
          std::round(dst_y + dst_mapping_yoffset + dst_mapping_height);

      // adjust the src region and dst region to fit the read framebuffer
      srcX0 = srcX0 < srcX1 ?
          src_bounds.x() : src_bounds.x() + src_bounds.width();
      srcY0 = srcY0 < srcY1 ?
          src_bounds.y() : src_bounds.y() + src_bounds.height();
      srcX1 = srcX0 < srcX1 ?
          src_bounds.x() + src_bounds.width() : src_bounds.x();
      srcY1 = srcY0 < srcY1 ?
          src_bounds.y() + src_bounds.height() : src_bounds.y();

      dstX0 = dstX0 < dstX1 ? dst_mapping_x0 : dst_mapping_x1;
      dstY0 = dstY0 < dstY1 ? dst_mapping_y0 : dst_mapping_y1;
      dstX1 = dstX0 < dstX1 ? dst_mapping_x1 : dst_mapping_x0;
      dstY1 = dstY0 < dstY1 ? dst_mapping_y1 : dst_mapping_y0;
    }
  }

  bool enable_srgb =
      (read_buffer_has_srgb || draw_buffers_has_srgb) &&
      ((mask & GL_COLOR_BUFFER_BIT) != 0);
  bool encode_srgb_only =
      (draw_buffers_has_srgb && !read_buffer_has_srgb) &&
      ((mask & GL_COLOR_BUFFER_BIT) != 0);
  if (!enable_srgb ||
      read_buffer_samples > 0 ||
      !feature_info_->feature_flags().desktop_srgb_support ||
      gl_version_info().IsAtLeastGL(4, 4) ||
      (gl_version_info().IsAtLeastGL(4, 2) && encode_srgb_only)) {
    if (enable_srgb && gl_version_info().IsAtLeastGL(4, 2)) {
      state_.EnableDisableFramebufferSRGB(enable_srgb);
    }

    BlitFramebufferHelper(
        srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    return;
  }

  // emulate srgb for desktop core profile when GL version < 4.4
  // TODO(yunchao): Need to handle this situation:
  // There are multiple draw buffers. Some of them are srgb images.
  // The others are not.
  state_.EnableDisableFramebufferSRGB(true);
  if (!InitializeSRGBConverter(func_name)) {
    return;
  }
  GLenum src_format =
      TextureManager::ExtractFormatFromStorageFormat(src_internal_format);
  srgb_converter_->Blit(this, srcX0, srcY0, srcX1, srcY1,
                        dstX0, dstY0, dstX1, dstY1,
                        mask, filter,
                        GetBoundReadFramebufferSize(),
                        GetBoundReadFramebufferServiceId(),
                        src_internal_format, src_format, src_type,
                        GetBoundDrawFramebufferServiceId(),
                        read_buffer_has_srgb, draw_buffers_has_srgb,
                        state_.enable_flags.scissor_test);
}

bool GLES2DecoderImpl::InitializeSRGBConverter(
    const char* function_name) {
  if (!srgb_converter_.get()) {
    LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER(function_name);
    srgb_converter_.reset(
        new SRGBConverter(feature_info_.get()));
    srgb_converter_->InitializeSRGBConverter(this);
    if (LOCAL_PEEK_GL_ERROR(function_name) != GL_NO_ERROR) {
      return false;
    }
  }
  return true;
}

void GLES2DecoderImpl::EnsureRenderbufferBound() {
  if (!state_.bound_renderbuffer_valid) {
    state_.bound_renderbuffer_valid = true;
    glBindRenderbufferEXT(GL_RENDERBUFFER,
                          state_.bound_renderbuffer.get()
                              ? state_.bound_renderbuffer->service_id()
                              : 0);
  }
}

void GLES2DecoderImpl::RenderbufferStorageMultisampleHelper(
    const FeatureInfo* feature_info,
    GLenum target,
    GLsizei samples,
    GLenum internal_format,
    GLsizei width,
    GLsizei height) {
  // TODO(sievers): This could be resolved at the GL binding level, but the
  // binding process is currently a bit too 'brute force'.
  if (feature_info->feature_flags().use_core_framebuffer_multisample) {
    glRenderbufferStorageMultisample(
        target, samples, internal_format, width, height);
  } else if (feature_info->gl_version_info().is_angle) {
    // This is ES2 only.
    glRenderbufferStorageMultisampleANGLE(
        target, samples, internal_format, width, height);
  } else {
    glRenderbufferStorageMultisampleEXT(
        target, samples, internal_format, width, height);
  }
}

void GLES2DecoderImpl::BlitFramebufferHelper(GLint srcX0,
                                             GLint srcY0,
                                             GLint srcX1,
                                             GLint srcY1,
                                             GLint dstX0,
                                             GLint dstY0,
                                             GLint dstX1,
                                             GLint dstY1,
                                             GLbitfield mask,
                                             GLenum filter) {
  // TODO(sievers): This could be resolved at the GL binding level, but the
  // binding process is currently a bit too 'brute force'.
  if (feature_info_->feature_flags().use_core_framebuffer_multisample) {
    glBlitFramebuffer(
        srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
  } else if (gl_version_info().is_angle) {
    // This is ES2 only.
    glBlitFramebufferANGLE(
        srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
  } else {
    glBlitFramebufferEXT(
        srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
  }
}

bool GLES2DecoderImpl::ValidateRenderbufferStorageMultisample(
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height) {
  if (samples > renderbuffer_manager()->max_samples()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE,
        "glRenderbufferStorageMultisample", "samples too large");
    return false;
  }

  if (width > renderbuffer_manager()->max_renderbuffer_size() ||
      height > renderbuffer_manager()->max_renderbuffer_size()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE,
        "glRenderbufferStorageMultisample", "dimensions too large");
    return false;
  }

  uint32_t estimated_size = 0;
  if (!renderbuffer_manager()->ComputeEstimatedRenderbufferSize(
           width, height, samples, internalformat, &estimated_size)) {
    LOCAL_SET_GL_ERROR(
        GL_OUT_OF_MEMORY,
        "glRenderbufferStorageMultisample", "dimensions too large");
    return false;
  }

  if (!EnsureGPUMemoryAvailable(estimated_size)) {
    LOCAL_SET_GL_ERROR(
        GL_OUT_OF_MEMORY,
        "glRenderbufferStorageMultisample", "out of memory");
    return false;
  }

  return true;
}

void GLES2DecoderImpl::DoRenderbufferStorageMultisampleCHROMIUM(
    GLenum target, GLsizei samples, GLenum internalformat,
    GLsizei width, GLsizei height) {
  Renderbuffer* renderbuffer = GetRenderbufferInfoForTarget(GL_RENDERBUFFER);
  if (!renderbuffer) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                       "glRenderbufferStorageMultisampleCHROMIUM",
                       "no renderbuffer bound");
    return;
  }

  if (!ValidateRenderbufferStorageMultisample(
      samples, internalformat, width, height)) {
    return;
  }

  EnsureRenderbufferBound();
  GLenum impl_format =
      renderbuffer_manager()->InternalRenderbufferFormatToImplFormat(
          internalformat);
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER(
      "glRenderbufferStorageMultisampleCHROMIUM");
  RenderbufferStorageMultisampleHelper(
      feature_info_.get(), target, samples, impl_format, width, height);
  GLenum error =
      LOCAL_PEEK_GL_ERROR("glRenderbufferStorageMultisampleCHROMIUM");
  if (error == GL_NO_ERROR) {
    if (workarounds().validate_multisample_buffer_allocation) {
      if (!VerifyMultisampleRenderbufferIntegrity(
          renderbuffer->service_id(), impl_format)) {
        LOCAL_SET_GL_ERROR(
            GL_OUT_OF_MEMORY,
            "glRenderbufferStorageMultisampleCHROMIUM", "out of memory");
        return;
      }
    }

    // TODO(gman): If renderbuffers tracked which framebuffers they were
    // attached to we could just mark those framebuffers as not complete.
    framebuffer_manager()->IncFramebufferStateChangeCount();
    renderbuffer_manager()->SetInfo(
        renderbuffer, samples, internalformat, width, height);
  }
}

// This is the handler for multisampled_render_to_texture extensions.
void GLES2DecoderImpl::DoRenderbufferStorageMultisampleEXT(
    GLenum target, GLsizei samples, GLenum internalformat,
    GLsizei width, GLsizei height) {
  Renderbuffer* renderbuffer = GetRenderbufferInfoForTarget(GL_RENDERBUFFER);
  if (!renderbuffer) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                       "glRenderbufferStorageMultisampleEXT",
                       "no renderbuffer bound");
    return;
  }

  if (!ValidateRenderbufferStorageMultisample(
      samples, internalformat, width, height)) {
    return;
  }

  EnsureRenderbufferBound();
  GLenum impl_format =
      renderbuffer_manager()->InternalRenderbufferFormatToImplFormat(
          internalformat);
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("glRenderbufferStorageMultisampleEXT");
  if (features().use_img_for_multisampled_render_to_texture) {
    glRenderbufferStorageMultisampleIMG(
        target, samples, impl_format, width, height);
  } else {
    glRenderbufferStorageMultisampleEXT(
        target, samples, impl_format, width, height);
  }
  GLenum error = LOCAL_PEEK_GL_ERROR("glRenderbufferStorageMultisampleEXT");
  if (error == GL_NO_ERROR) {
    // TODO(gman): If renderbuffers tracked which framebuffers they were
    // attached to we could just mark those framebuffers as not complete.
    framebuffer_manager()->IncFramebufferStateChangeCount();
    renderbuffer_manager()->SetInfo(
        renderbuffer, samples, internalformat, width, height);
  }
}

// This function validates the allocation of a multisampled renderbuffer
// by clearing it to a key color, blitting the contents to a texture, and
// reading back the color to ensure it matches the key.
bool GLES2DecoderImpl::VerifyMultisampleRenderbufferIntegrity(
    GLuint renderbuffer, GLenum format) {

  // Only validate color buffers.
  // These formats have been selected because they are very common or are known
  // to be used by the WebGL backbuffer. If problems are observed with other
  // color formats they can be added here.
  switch (format) {
    case GL_RGB8:
    case GL_RGBA8:
      break;
    default:
      return true;
  }

  GLint draw_framebuffer, read_framebuffer;

  // Cache framebuffer and texture bindings.
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_framebuffer);
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_framebuffer);

  if (!validation_fbo_) {
    glGenFramebuffersEXT(1, &validation_fbo_multisample_);
    glGenFramebuffersEXT(1, &validation_fbo_);
  }

  GLint bound_texture;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound_texture);
  GLuint validation_texture;
  TextureMap::iterator iter = validation_textures_.find(format);
  if (iter == validation_textures_.end()) {
    // Create additional resources needed for the verification.
    glGenTextures(1, &validation_texture);
    validation_textures_.insert(std::make_pair(format, validation_texture));

    // Texture only needs to be 1x1.
    glBindTexture(GL_TEXTURE_2D, validation_texture);
    glTexStorage2DEXT(GL_TEXTURE_2D, 1, format, 1, 1);
  } else {
    validation_texture = iter->second;
  }
  glBindFramebufferEXT(GL_FRAMEBUFFER, validation_fbo_);
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
      GL_TEXTURE_2D, validation_texture, 0);
  glBindTexture(GL_TEXTURE_2D, bound_texture);

  glBindFramebufferEXT(GL_FRAMEBUFFER, validation_fbo_multisample_);
  glFramebufferRenderbufferEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
      GL_RENDERBUFFER, renderbuffer);

  // Cache current state and reset it to the values we require.
  GLboolean scissor_enabled = false;
  glGetBooleanv(GL_SCISSOR_TEST, &scissor_enabled);
  if (scissor_enabled)
    state_.SetDeviceCapabilityState(GL_SCISSOR_TEST, false);

  GLboolean color_mask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
  glGetBooleanv(GL_COLOR_WRITEMASK, color_mask);
  state_.SetDeviceColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  GLfloat clear_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  glGetFloatv(GL_COLOR_CLEAR_VALUE, clear_color);
  glClearColor(1.0f, 0.0f, 1.0f, 1.0f);

  // Clear the buffer to the desired key color.
  glClear(GL_COLOR_BUFFER_BIT);

  // Blit from the multisample buffer to a standard texture.
  glBindFramebufferEXT(GL_READ_FRAMEBUFFER, validation_fbo_multisample_);
  glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, validation_fbo_);

  BlitFramebufferHelper(
      0, 0, 1, 1, 0, 0, 1, 1, GL_COLOR_BUFFER_BIT, GL_NEAREST);

  // Read a pixel from the buffer.
  glBindFramebufferEXT(GL_FRAMEBUFFER, validation_fbo_);

  unsigned char pixel[3] = {0, 0, 0};
  glReadPixels(0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, &pixel);

  // Detach the renderbuffer.
  glBindFramebufferEXT(GL_FRAMEBUFFER, validation_fbo_multisample_);
  glFramebufferRenderbufferEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
      GL_RENDERBUFFER, 0);

  // Restore cached state.
  if (scissor_enabled)
    state_.SetDeviceCapabilityState(GL_SCISSOR_TEST, true);

  state_.SetDeviceColorMask(
      color_mask[0], color_mask[1], color_mask[2], color_mask[3]);
  glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
  glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, draw_framebuffer);
  glBindFramebufferEXT(GL_READ_FRAMEBUFFER, read_framebuffer);

  // Return true if the pixel matched the desired key color.
  return (pixel[0] == 0xFF &&
      pixel[1] == 0x00 &&
      pixel[2] == 0xFF);
}

void GLES2DecoderImpl::DoRenderbufferStorage(
  GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
  Renderbuffer* renderbuffer =
      GetRenderbufferInfoForTarget(GL_RENDERBUFFER);
  if (!renderbuffer) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        "glRenderbufferStorage", "no renderbuffer bound");
    return;
  }

  if (width > renderbuffer_manager()->max_renderbuffer_size() ||
      height > renderbuffer_manager()->max_renderbuffer_size()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glRenderbufferStorage", "dimensions too large");
    return;
  }

  uint32_t estimated_size = 0;
  if (!renderbuffer_manager()->ComputeEstimatedRenderbufferSize(
           width, height, 1, internalformat, &estimated_size)) {
    LOCAL_SET_GL_ERROR(
        GL_OUT_OF_MEMORY, "glRenderbufferStorage", "dimensions too large");
    return;
  }

  if (!EnsureGPUMemoryAvailable(estimated_size)) {
    LOCAL_SET_GL_ERROR(
        GL_OUT_OF_MEMORY, "glRenderbufferStorage", "out of memory");
    return;
  }

  EnsureRenderbufferBound();
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("glRenderbufferStorage");
  glRenderbufferStorageEXT(
      target,
      renderbuffer_manager()->InternalRenderbufferFormatToImplFormat(
          internalformat),
      width,
      height);
  GLenum error = LOCAL_PEEK_GL_ERROR("glRenderbufferStorage");
  if (error == GL_NO_ERROR) {
    // TODO(gman): If tetxures tracked which framebuffers they were attached to
    // we could just mark those framebuffers as not complete.
    framebuffer_manager()->IncFramebufferStateChangeCount();
    renderbuffer_manager()->SetInfo(
        renderbuffer, 0, internalformat, width, height);
  }
}

void GLES2DecoderImpl::DoLineWidth(GLfloat width) {
  glLineWidth(
      std::min(std::max(width, line_width_range_[0]), line_width_range_[1]));
}

void GLES2DecoderImpl::DoLinkProgram(GLuint program_id) {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::DoLinkProgram");
  SCOPED_UMA_HISTOGRAM_TIMER("GPU.DoLinkProgramTime");
  Program* program = GetProgramInfoNotShader(
      program_id, "glLinkProgram");
  if (!program) {
    return;
  }

  LogClientServiceForInfo(program, program_id, "glLinkProgram");
  if (program->Link(shader_manager(),
                    workarounds().count_all_in_varyings_packing ?
                        Program::kCountAll : Program::kCountOnlyStaticallyUsed,
                    shader_cache_callback_)) {
    if (program == state_.current_program.get()) {
      if (workarounds().clear_uniforms_before_first_program_use)
        program_manager()->ClearUniforms(program);
    }
  }

  // LinkProgram can be very slow.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();
}

void GLES2DecoderImpl::DoReadBuffer(GLenum src) {
  Framebuffer* framebuffer = GetFramebufferInfoForTarget(GL_READ_FRAMEBUFFER);
  if (framebuffer) {
    if (src == GL_BACK) {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_ENUM, "glReadBuffer",
          "invalid src for a named framebuffer");
      return;
    }
    framebuffer->set_read_buffer(src);
  } else {
    if (src != GL_NONE && src != GL_BACK) {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_ENUM, "glReadBuffer",
          "invalid src for the default framebuffer");
      return;
    }
    back_buffer_read_buffer_ = src;
    if (GetBackbufferServiceId() && src == GL_BACK)
      src = GL_COLOR_ATTACHMENT0;
  }
  glReadBuffer(src);
}

void GLES2DecoderImpl::DoSamplerParameterf(
    GLuint client_id, GLenum pname, GLfloat param) {
  Sampler* sampler = GetSampler(client_id);
  if (!sampler) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glSamplerParameterf", "unknown sampler");
    return;
  }
  sampler_manager()->SetParameterf(
      "glSamplerParameterf", GetErrorState(), sampler, pname, param);
}

void GLES2DecoderImpl::DoSamplerParameteri(
    GLuint client_id, GLenum pname, GLint param) {
  Sampler* sampler = GetSampler(client_id);
  if (!sampler) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glSamplerParameteri", "unknown sampler");
    return;
  }
  sampler_manager()->SetParameteri(
      "glSamplerParameteri", GetErrorState(), sampler, pname, param);
}

void GLES2DecoderImpl::DoSamplerParameterfv(GLuint client_id,
                                            GLenum pname,
                                            const volatile GLfloat* params) {
  DCHECK(params);
  Sampler* sampler = GetSampler(client_id);
  if (!sampler) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glSamplerParameterfv", "unknown sampler");
    return;
  }
  sampler_manager()->SetParameterf(
      "glSamplerParameterfv", GetErrorState(), sampler, pname, params[0]);
}

void GLES2DecoderImpl::DoSamplerParameteriv(GLuint client_id,
                                            GLenum pname,
                                            const volatile GLint* params) {
  DCHECK(params);
  Sampler* sampler = GetSampler(client_id);
  if (!sampler) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glSamplerParameteriv", "unknown sampler");
    return;
  }
  sampler_manager()->SetParameteri(
      "glSamplerParameteriv", GetErrorState(), sampler, pname, params[0]);
}

void GLES2DecoderImpl::DoTexParameterf(
    GLenum target, GLenum pname, GLfloat param) {
  TextureRef* texture = texture_manager()->GetTextureInfoForTarget(
      &state_, target);
  if (!texture) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glTexParameterf", "unknown texture");
    return;
  }

  texture_manager()->SetParameterf(
      "glTexParameterf", GetErrorState(), texture, pname, param);
}

void GLES2DecoderImpl::DoTexParameteri(
    GLenum target, GLenum pname, GLint param) {
  TextureRef* texture = texture_manager()->GetTextureInfoForTarget(
      &state_, target);
  if (!texture) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glTexParameteri", "unknown texture");
    return;
  }

  texture_manager()->SetParameteri(
      "glTexParameteri", GetErrorState(), texture, pname, param);
}

void GLES2DecoderImpl::DoTexParameterfv(GLenum target,
                                        GLenum pname,
                                        const volatile GLfloat* params) {
  TextureRef* texture = texture_manager()->GetTextureInfoForTarget(
      &state_, target);
  if (!texture) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glTexParameterfv", "unknown texture");
    return;
  }

  texture_manager()->SetParameterf(
      "glTexParameterfv", GetErrorState(), texture, pname, *params);
}

void GLES2DecoderImpl::DoTexParameteriv(GLenum target,
                                        GLenum pname,
                                        const volatile GLint* params) {
  TextureRef* texture = texture_manager()->GetTextureInfoForTarget(
      &state_, target);
  if (!texture) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glTexParameteriv", "unknown texture");
    return;
  }

  texture_manager()->SetParameteri(
      "glTexParameteriv", GetErrorState(), texture, pname, *params);
}

bool GLES2DecoderImpl::CheckCurrentProgram(const char* function_name) {
  if (!state_.current_program.get()) {
    // The program does not exist.
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, function_name, "no program in use");
    return false;
  }
  if (!state_.current_program->InUse()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, function_name, "program not linked");
    return false;
  }
  return true;
}

bool GLES2DecoderImpl::CheckCurrentProgramForUniform(
    GLint location, const char* function_name) {
  if (!CheckCurrentProgram(function_name)) {
    return false;
  }
  return !state_.current_program->IsInactiveUniformLocationByFakeLocation(
      location);
}

bool GLES2DecoderImpl::CheckDrawingFeedbackLoops() {
  Framebuffer* framebuffer = GetFramebufferInfoForTarget(GL_FRAMEBUFFER);
  if (!framebuffer)
    return false;
  const Framebuffer::Attachment* attachment =
      framebuffer->GetAttachment(GL_COLOR_ATTACHMENT0);
  if (!attachment)
    return false;

  DCHECK(state_.current_program.get());
  const Program::SamplerIndices& sampler_indices =
      state_.current_program->sampler_indices();
  for (size_t ii = 0; ii < sampler_indices.size(); ++ii) {
    const Program::UniformInfo* uniform_info =
        state_.current_program->GetUniformInfo(sampler_indices[ii]);
    DCHECK(uniform_info);
    for (size_t jj = 0; jj < uniform_info->texture_units.size(); ++jj) {
      GLuint texture_unit_index = uniform_info->texture_units[jj];
      if (texture_unit_index >= state_.texture_units.size())
        continue;
      TextureUnit& texture_unit = state_.texture_units[texture_unit_index];
      TextureRef* texture_ref =
          texture_unit.GetInfoForSamplerType(uniform_info->type).get();
      if (attachment->IsTexture(texture_ref))
        return true;
    }
  }
  return false;
}

bool GLES2DecoderImpl::SupportsDrawBuffers() const {
  return feature_info_->IsWebGL1OrES2Context() ?
      feature_info_->feature_flags().ext_draw_buffers : true;
}

bool GLES2DecoderImpl::ValidateAndAdjustDrawBuffers(const char* func_name) {
  if (!SupportsDrawBuffers()) {
    return true;
  }
  Framebuffer* framebuffer = framebuffer_state_.bound_draw_framebuffer.get();
  if (!state_.current_program.get() || !framebuffer) {
    return true;
  }
  uint32_t fragment_output_type_mask =
      state_.current_program->fragment_output_type_mask();
  uint32_t fragment_output_written_mask =
      state_.current_program->fragment_output_written_mask();
  if (!framebuffer->ValidateAndAdjustDrawBuffers(
      fragment_output_type_mask, fragment_output_written_mask)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
          "buffer format and fragment output variable type incompatible");
      return false;
  }
  return true;
}

bool GLES2DecoderImpl::ValidateUniformBlockBackings(const char* func_name) {
  DCHECK(feature_info_->IsWebGL2OrES3Context());
  if (!state_.current_program.get())
    return true;
  int32_t max_index = -1;
  for (auto info : state_.current_program->uniform_block_size_info()) {
    int32_t index = static_cast<int32_t>(info.binding);
    if (index > max_index)
      max_index = index;
  }
  if (max_index < 0)
    return true;
  std::vector<GLsizeiptr> uniform_block_sizes(max_index + 1);
  for (int32_t ii = 0; ii <= max_index; ++ii)
    uniform_block_sizes[ii] = 0;
  for (auto info : state_.current_program->uniform_block_size_info()) {
    uint32_t index = info.binding;
    uniform_block_sizes[index] = static_cast<GLsizeiptr>(info.data_size);
  }
  return buffer_manager()->RequestBuffersAccess(
      state_.GetErrorState(), state_.indexed_uniform_buffer_bindings.get(),
      uniform_block_sizes, 1, func_name, "uniform buffers");
}

bool GLES2DecoderImpl::CheckUniformForApiType(
    const Program::UniformInfo* info,
    const char* function_name,
    Program::UniformApiType api_type) {
  DCHECK(info);
  if ((api_type & info->accepts_api_type) == 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                       "wrong uniform function for type");
    return false;
  }
  return true;
}

bool GLES2DecoderImpl::PrepForSetUniformByLocation(
    GLint fake_location,
    const char* function_name,
    Program::UniformApiType api_type,
    GLint* real_location,
    GLenum* type,
    GLsizei* count) {
  DCHECK(type);
  DCHECK(count);
  DCHECK(real_location);

  if (!CheckCurrentProgramForUniform(fake_location, function_name)) {
    return false;
  }
  GLint array_index = -1;
  const Program::UniformInfo* info =
      state_.current_program->GetUniformInfoByFakeLocation(
          fake_location, real_location, &array_index);
  if (!info) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, function_name, "unknown location");
    return false;
  }
  if (!CheckUniformForApiType(info, function_name, api_type)) {
    return false;
  }
  if (*count > 1 && !info->is_array) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, function_name, "count > 1 for non-array");
    return false;
  }
  *count = std::min(info->size - array_index, *count);
  if (*count <= 0) {
    return false;
  }
  *type = info->type;
  return true;
}

void GLES2DecoderImpl::DoUniform1i(GLint fake_location, GLint v0) {
  GLenum type = 0;
  GLsizei count = 1;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location,
                                   "glUniform1i",
                                   Program::kUniform1i,
                                   &real_location,
                                   &type,
                                   &count)) {
    return;
  }
  if (!state_.current_program->SetSamplers(
      state_.texture_units.size(), fake_location, 1, &v0)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glUniform1i", "texture unit out of range");
    return;
  }
  glUniform1i(real_location, v0);
}

void GLES2DecoderImpl::DoUniform1iv(GLint fake_location,
                                    GLsizei count,
                                    const volatile GLint* values) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location,
                                   "glUniform1iv",
                                   Program::kUniform1i,
                                   &real_location,
                                   &type,
                                   &count)) {
    return;
  }
  auto values_copy = base::MakeUnique<GLint[]>(count);
  GLint* safe_values = values_copy.get();
  std::copy(values, values + count, safe_values);
  if (type == GL_SAMPLER_2D || type == GL_SAMPLER_2D_RECT_ARB ||
      type == GL_SAMPLER_CUBE || type == GL_SAMPLER_EXTERNAL_OES) {
    if (!state_.current_program->SetSamplers(
            state_.texture_units.size(), fake_location, count, safe_values)) {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_VALUE, "glUniform1iv", "texture unit out of range");
      return;
    }
  }
  glUniform1iv(real_location, count, safe_values);
}

void GLES2DecoderImpl::DoUniform1uiv(GLint fake_location,
                                     GLsizei count,
                                     const volatile GLuint* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location,
                                   "glUniform1uiv",
                                   Program::kUniform1ui,
                                   &real_location,
                                   &type,
                                   &count)) {
    return;
  }
  glUniform1uiv(real_location, count, const_cast<const GLuint*>(value));
}

void GLES2DecoderImpl::DoUniform1fv(GLint fake_location,
                                    GLsizei count,
                                    const volatile GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location,
                                   "glUniform1fv",
                                   Program::kUniform1f,
                                   &real_location,
                                   &type,
                                   &count)) {
    return;
  }
  if (type == GL_BOOL) {
    std::unique_ptr<GLint[]> temp(new GLint[count]);
    for (GLsizei ii = 0; ii < count; ++ii) {
      temp[ii] = static_cast<GLint>(value[ii] != 0.0f);
    }
    glUniform1iv(real_location, count, temp.get());
  } else {
    glUniform1fv(real_location, count, const_cast<const GLfloat*>(value));
  }
}

void GLES2DecoderImpl::DoUniform2fv(GLint fake_location,
                                    GLsizei count,
                                    const volatile GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location,
                                   "glUniform2fv",
                                   Program::kUniform2f,
                                   &real_location,
                                   &type,
                                   &count)) {
    return;
  }
  if (type == GL_BOOL_VEC2) {
    GLsizei num_values = count * 2;
    std::unique_ptr<GLint[]> temp(new GLint[num_values]);
    for (GLsizei ii = 0; ii < num_values; ++ii) {
      temp[ii] = static_cast<GLint>(value[ii] != 0.0f);
    }
    glUniform2iv(real_location, count, temp.get());
  } else {
    glUniform2fv(real_location, count, const_cast<const GLfloat*>(value));
  }
}

void GLES2DecoderImpl::DoUniform3fv(GLint fake_location,
                                    GLsizei count,
                                    const volatile GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location,
                                   "glUniform3fv",
                                   Program::kUniform3f,
                                   &real_location,
                                   &type,
                                   &count)) {
    return;
  }
  if (type == GL_BOOL_VEC3) {
    GLsizei num_values = count * 3;
    std::unique_ptr<GLint[]> temp(new GLint[num_values]);
    for (GLsizei ii = 0; ii < num_values; ++ii) {
      temp[ii] = static_cast<GLint>(value[ii] != 0.0f);
    }
    glUniform3iv(real_location, count, temp.get());
  } else {
    glUniform3fv(real_location, count, const_cast<const GLfloat*>(value));
  }
}

void GLES2DecoderImpl::DoUniform4fv(GLint fake_location,
                                    GLsizei count,
                                    const volatile GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location,
                                   "glUniform4fv",
                                   Program::kUniform4f,
                                   &real_location,
                                   &type,
                                   &count)) {
    return;
  }
  if (type == GL_BOOL_VEC4) {
    GLsizei num_values = count * 4;
    std::unique_ptr<GLint[]> temp(new GLint[num_values]);
    for (GLsizei ii = 0; ii < num_values; ++ii) {
      temp[ii] = static_cast<GLint>(value[ii] != 0.0f);
    }
    glUniform4iv(real_location, count, temp.get());
  } else {
    glUniform4fv(real_location, count, const_cast<const GLfloat*>(value));
  }
}

void GLES2DecoderImpl::DoUniform2iv(GLint fake_location,
                                    GLsizei count,
                                    const volatile GLint* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location,
                                   "glUniform2iv",
                                   Program::kUniform2i,
                                   &real_location,
                                   &type,
                                   &count)) {
    return;
  }
  glUniform2iv(real_location, count, const_cast<const GLint*>(value));
}

void GLES2DecoderImpl::DoUniform2uiv(GLint fake_location,
                                     GLsizei count,
                                     const volatile GLuint* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location,
                                   "glUniform2uiv",
                                   Program::kUniform2ui,
                                   &real_location,
                                   &type,
                                   &count)) {
    return;
  }
  glUniform2uiv(real_location, count, const_cast<const GLuint*>(value));
}

void GLES2DecoderImpl::DoUniform3iv(GLint fake_location,
                                    GLsizei count,
                                    const volatile GLint* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location,
                                   "glUniform3iv",
                                   Program::kUniform3i,
                                   &real_location,
                                   &type,
                                   &count)) {
    return;
  }
  glUniform3iv(real_location, count, const_cast<const GLint*>(value));
}

void GLES2DecoderImpl::DoUniform3uiv(GLint fake_location,
                                     GLsizei count,
                                     const volatile GLuint* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location,
                                   "glUniform3uiv",
                                   Program::kUniform3ui,
                                   &real_location,
                                   &type,
                                   &count)) {
    return;
  }
  glUniform3uiv(real_location, count, const_cast<const GLuint*>(value));
}

void GLES2DecoderImpl::DoUniform4iv(GLint fake_location,
                                    GLsizei count,
                                    const volatile GLint* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location,
                                   "glUniform4iv",
                                   Program::kUniform4i,
                                   &real_location,
                                   &type,
                                   &count)) {
    return;
  }
  glUniform4iv(real_location, count, const_cast<const GLint*>(value));
}

void GLES2DecoderImpl::DoUniform4uiv(GLint fake_location,
                                     GLsizei count,
                                     const volatile GLuint* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location,
                                   "glUniform4uiv",
                                   Program::kUniform4ui,
                                   &real_location,
                                   &type,
                                   &count)) {
    return;
  }
  glUniform4uiv(real_location, count, const_cast<const GLuint*>(value));
}

void GLES2DecoderImpl::DoUniformMatrix2fv(GLint fake_location,
                                          GLsizei count,
                                          GLboolean transpose,
                                          const volatile GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (transpose && !feature_info_->IsWebGL2OrES3Context()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glUniformMatrix2fv", "transpose not FALSE");
    return;
  }
  if (!PrepForSetUniformByLocation(fake_location,
                                   "glUniformMatrix2fv",
                                   Program::kUniformMatrix2f,
                                   &real_location,
                                   &type,
                                   &count)) {
    return;
  }
  glUniformMatrix2fv(real_location, count, transpose,
                     const_cast<const GLfloat*>(value));
}

void GLES2DecoderImpl::DoUniformMatrix3fv(GLint fake_location,
                                          GLsizei count,
                                          GLboolean transpose,
                                          const volatile GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (transpose && !feature_info_->IsWebGL2OrES3Context()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glUniformMatrix3fv", "transpose not FALSE");
    return;
  }
  if (!PrepForSetUniformByLocation(fake_location,
                                   "glUniformMatrix3fv",
                                   Program::kUniformMatrix3f,
                                   &real_location,
                                   &type,
                                   &count)) {
    return;
  }
  glUniformMatrix3fv(real_location, count, transpose,
                     const_cast<const GLfloat*>(value));
}

void GLES2DecoderImpl::DoUniformMatrix4fv(GLint fake_location,
                                          GLsizei count,
                                          GLboolean transpose,
                                          const volatile GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (transpose && !feature_info_->IsWebGL2OrES3Context()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glUniformMatrix4fv", "transpose not FALSE");
    return;
  }
  if (!PrepForSetUniformByLocation(fake_location,
                                   "glUniformMatrix4fv",
                                   Program::kUniformMatrix4f,
                                   &real_location,
                                   &type,
                                   &count)) {
    return;
  }
  glUniformMatrix4fv(real_location, count, transpose,
                     const_cast<const GLfloat*>(value));
}

void GLES2DecoderImpl::DoUniformMatrix4fvStreamTextureMatrixCHROMIUM(
    GLint fake_location,
    GLboolean transpose,
    const volatile GLfloat* transform) {
  float gl_matrix[16];

  // This refers to the bound external texture on the active unit.
  TextureUnit& unit = state_.texture_units[state_.active_texture_unit];
  if (TextureRef* texture_ref = unit.bound_texture_external_oes.get()) {
    if (GLStreamTextureImage* image =
            texture_ref->texture()->GetLevelStreamTextureImage(
                GL_TEXTURE_EXTERNAL_OES, 0)) {
      gfx::Transform st_transform(gfx::Transform::kSkipInitialization);
      gfx::Transform pre_transform(gfx::Transform::kSkipInitialization);
      image->GetTextureMatrix(gl_matrix);
      st_transform.matrix().setColMajorf(gl_matrix);
      // const_cast is safe, because setColMajorf only does a memcpy.
      // TODO(piman): can we remove this assumption without having to introduce
      // an extra copy?
      pre_transform.matrix().setColMajorf(
          const_cast<const GLfloat*>(transform));
      gfx::Transform(pre_transform, st_transform)
          .matrix()
          .asColMajorf(gl_matrix);
    } else {
      // Missing stream texture. Treat matrix as identity.
      memcpy(gl_matrix, const_cast<const GLfloat*>(transform),
             sizeof(gl_matrix));
    }
  } else {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                       "DoUniformMatrix4vStreamTextureMatrix",
                       "no texture bound");
    return;
  }

  GLenum type = 0;
  GLint real_location = -1;
  GLsizei count = 1;
  if (!PrepForSetUniformByLocation(fake_location, "glUniformMatrix4fv",
                                   Program::kUniformMatrix4f, &real_location,
                                   &type, &count)) {
    return;
  }

  glUniformMatrix4fv(real_location, count, transpose, gl_matrix);
}

void GLES2DecoderImpl::DoUniformMatrix2x3fv(GLint fake_location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const volatile GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location,
                                   "glUniformMatrix2x3fv",
                                   Program::kUniformMatrix2x3f,
                                   &real_location,
                                   &type,
                                   &count)) {
    return;
  }
  glUniformMatrix2x3fv(real_location, count, transpose,
                       const_cast<const GLfloat*>(value));
}

void GLES2DecoderImpl::DoUniformMatrix2x4fv(GLint fake_location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const volatile GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location,
                                   "glUniformMatrix2x4fv",
                                   Program::kUniformMatrix2x4f,
                                   &real_location,
                                   &type,
                                   &count)) {
    return;
  }
  glUniformMatrix2x4fv(real_location, count, transpose,
                       const_cast<const GLfloat*>(value));
}

void GLES2DecoderImpl::DoUniformMatrix3x2fv(GLint fake_location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const volatile GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location,
                                   "glUniformMatrix3x2fv",
                                   Program::kUniformMatrix3x2f,
                                   &real_location,
                                   &type,
                                   &count)) {
    return;
  }
  glUniformMatrix3x2fv(real_location, count, transpose,
                       const_cast<const GLfloat*>(value));
}

void GLES2DecoderImpl::DoUniformMatrix3x4fv(GLint fake_location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const volatile GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location,
                                   "glUniformMatrix3x4fv",
                                   Program::kUniformMatrix3x4f,
                                   &real_location,
                                   &type,
                                   &count)) {
    return;
  }
  glUniformMatrix3x4fv(real_location, count, transpose,
                       const_cast<const GLfloat*>(value));
}

void GLES2DecoderImpl::DoUniformMatrix4x2fv(GLint fake_location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const volatile GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location,
                                   "glUniformMatrix4x2fv",
                                   Program::kUniformMatrix4x2f,
                                   &real_location,
                                   &type,
                                   &count)) {
    return;
  }
  glUniformMatrix4x2fv(real_location, count, transpose,
                       const_cast<const GLfloat*>(value));
}

void GLES2DecoderImpl::DoUniformMatrix4x3fv(GLint fake_location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const volatile GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(fake_location,
                                   "glUniformMatrix4x3fv",
                                   Program::kUniformMatrix4x3f,
                                   &real_location,
                                   &type,
                                   &count)) {
    return;
  }
  glUniformMatrix4x3fv(real_location, count, transpose,
                       const_cast<const GLfloat*>(value));
}

void GLES2DecoderImpl::DoUseProgram(GLuint program_id) {
  const char* function_name = "glUseProgram";
  GLuint service_id = 0;
  Program* program = nullptr;
  if (program_id) {
    program = GetProgramInfoNotShader(program_id, function_name);
    if (!program) {
      return;
    }
    if (!program->IsValid()) {
      // Program was not linked successfully. (ie, glLinkProgram)
      LOCAL_SET_GL_ERROR(
          GL_INVALID_OPERATION, function_name, "program not linked");
      return;
    }
    service_id = program->service_id();
  }
  if (state_.bound_transform_feedback.get() &&
      state_.bound_transform_feedback->active() &&
      !state_.bound_transform_feedback->paused()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
        "transformfeedback is active and not paused");
    return;
  }
  if (program == state_.current_program.get())
    return;
  if (state_.current_program.get()) {
    program_manager()->UnuseProgram(shader_manager(),
                                    state_.current_program.get());
  }
  state_.current_program = program;
  LogClientServiceMapping(function_name, program_id, service_id);
  glUseProgram(service_id);
  if (state_.current_program.get()) {
    program_manager()->UseProgram(state_.current_program.get());
    if (workarounds().clear_uniforms_before_first_program_use)
      program_manager()->ClearUniforms(program);
  }
}

void GLES2DecoderImpl::RenderWarning(
    const char* filename, int line, const std::string& msg) {
  logger_.LogMessage(filename, line, std::string("RENDER WARNING: ") + msg);
}

void GLES2DecoderImpl::PerformanceWarning(
    const char* filename, int line, const std::string& msg) {
  logger_.LogMessage(filename, line,
                     std::string("PERFORMANCE WARNING: ") + msg);
}

void GLES2DecoderImpl::DoCopyTexImage(Texture* texture,
                                      GLenum textarget,
                                      gl::GLImage* image) {
  // Note: We update the state to COPIED prior to calling CopyTexImage()
  // as that allows the GLImage implemenatation to set it back to UNBOUND
  // and ensure that CopyTexImage() is called each time the texture is
  // used.
  texture->SetLevelImageState(textarget, 0, Texture::COPIED);
  bool rv = image->CopyTexImage(textarget);
  DCHECK(rv) << "CopyTexImage() failed";
}

void GLES2DecoderImpl::DoCopyTexImageIfNeeded(Texture* texture,
                                              GLenum textarget) {
  // Image is already in use if texture is attached to a framebuffer.
  if (texture && !texture->IsAttachedToFramebuffer()) {
    Texture::ImageState image_state;
    gl::GLImage* image = texture->GetLevelImage(textarget, 0, &image_state);
    if (image && image_state == Texture::UNBOUND) {
      ScopedGLErrorSuppressor suppressor(
          "GLES2DecoderImpl::DoCopyTexImageIfNeeded", GetErrorState());
      glBindTexture(textarget, texture->service_id());
      DoCopyTexImage(texture, textarget, image);
      RestoreCurrentTextureBindings(&state_, textarget);
    }
  }
}

void GLES2DecoderImpl::DoCopyBufferSubData(GLenum readtarget,
                                           GLenum writetarget,
                                           GLintptr readoffset,
                                           GLintptr writeoffset,
                                           GLsizeiptr size) {
  // Just delegate it. Some validation is actually done before this.
  buffer_manager()->ValidateAndDoCopyBufferSubData(
      &state_, readtarget, writetarget, readoffset, writeoffset, size);
}

bool GLES2DecoderImpl::PrepareTexturesForRender() {
  DCHECK(state_.current_program.get());
  bool textures_set = false;
  const Program::SamplerIndices& sampler_indices =
     state_.current_program->sampler_indices();
  for (size_t ii = 0; ii < sampler_indices.size(); ++ii) {
    const Program::UniformInfo* uniform_info =
        state_.current_program->GetUniformInfo(sampler_indices[ii]);
    DCHECK(uniform_info);
    for (size_t jj = 0; jj < uniform_info->texture_units.size(); ++jj) {
      GLuint texture_unit_index = uniform_info->texture_units[jj];
      if (texture_unit_index < state_.texture_units.size()) {
        TextureUnit& texture_unit = state_.texture_units[texture_unit_index];
        TextureRef* texture_ref =
            texture_unit.GetInfoForSamplerType(uniform_info->type).get();
        GLenum textarget = GetBindTargetForSamplerType(uniform_info->type);
        const SamplerState& sampler_state = GetSamplerStateForTextureUnit(
            uniform_info->type, texture_unit_index);
        if (!texture_ref ||
            !texture_manager()->CanRenderWithSampler(
                texture_ref, sampler_state)) {
          textures_set = true;
          glActiveTexture(GL_TEXTURE0 + texture_unit_index);
          glBindTexture(
              textarget,
              texture_manager()->black_texture_id(uniform_info->type));
          if (!texture_ref) {
            LOCAL_RENDER_WARNING(
                std::string("there is no texture bound to the unit ") +
                base::UintToString(texture_unit_index));
          } else {
            LOCAL_RENDER_WARNING(
                std::string("texture bound to texture unit ") +
                base::UintToString(texture_unit_index) +
                " is not renderable. It maybe non-power-of-2 and have"
                " incompatible texture filtering.");
          }
          continue;
        }

        if (textarget != GL_TEXTURE_CUBE_MAP) {
          Texture* texture = texture_ref->texture();
          Texture::ImageState image_state;
          gl::GLImage* image =
              texture->GetLevelImage(textarget, 0, &image_state);
          if (image && image_state == Texture::UNBOUND &&
              !texture->IsAttachedToFramebuffer()) {
            ScopedGLErrorSuppressor suppressor(
                "GLES2DecoderImpl::PrepareTexturesForRender", GetErrorState());
            textures_set = true;
            glActiveTexture(GL_TEXTURE0 + texture_unit_index);
            DoCopyTexImage(texture, textarget, image);
            continue;
          }
        }
      }
      // else: should this be an error?
    }
  }
  return !textures_set;
}

void GLES2DecoderImpl::RestoreStateForTextures() {
  DCHECK(state_.current_program.get());
  const Program::SamplerIndices& sampler_indices =
      state_.current_program->sampler_indices();
  for (size_t ii = 0; ii < sampler_indices.size(); ++ii) {
    const Program::UniformInfo* uniform_info =
        state_.current_program->GetUniformInfo(sampler_indices[ii]);
    DCHECK(uniform_info);
    for (size_t jj = 0; jj < uniform_info->texture_units.size(); ++jj) {
      GLuint texture_unit_index = uniform_info->texture_units[jj];
      if (texture_unit_index < state_.texture_units.size()) {
        TextureUnit& texture_unit = state_.texture_units[texture_unit_index];
        TextureRef* texture_ref =
            texture_unit.GetInfoForSamplerType(uniform_info->type).get();
        const SamplerState& sampler_state = GetSamplerStateForTextureUnit(
            uniform_info->type, texture_unit_index);
        if (!texture_ref ||
            !texture_manager()->CanRenderWithSampler(
                texture_ref, sampler_state)) {
          glActiveTexture(GL_TEXTURE0 + texture_unit_index);
          // Get the texture_ref info that was previously bound here.
          texture_ref =
              texture_unit.GetInfoForTarget(texture_unit.bind_target).get();
          glBindTexture(texture_unit.bind_target,
                        texture_ref ? texture_ref->service_id() : 0);
          continue;
        }
      }
    }
  }
  // Set the active texture back to whatever the user had it as.
  glActiveTexture(GL_TEXTURE0 + state_.active_texture_unit);
}

bool GLES2DecoderImpl::ClearUnclearedTextures() {
  // Only check if there are some uncleared textures.
  if (!texture_manager()->HaveUnsafeTextures()) {
    return true;
  }

  // 1: Check all textures we are about to render with.
  if (state_.current_program.get()) {
    const Program::SamplerIndices& sampler_indices =
        state_.current_program->sampler_indices();
    for (size_t ii = 0; ii < sampler_indices.size(); ++ii) {
      const Program::UniformInfo* uniform_info =
          state_.current_program->GetUniformInfo(sampler_indices[ii]);
      DCHECK(uniform_info);
      for (size_t jj = 0; jj < uniform_info->texture_units.size(); ++jj) {
        GLuint texture_unit_index = uniform_info->texture_units[jj];
        if (texture_unit_index < state_.texture_units.size()) {
          TextureUnit& texture_unit = state_.texture_units[texture_unit_index];
          TextureRef* texture_ref =
              texture_unit.GetInfoForSamplerType(uniform_info->type).get();
          if (texture_ref && !texture_ref->texture()->SafeToRenderFrom()) {
            if (!texture_manager()->ClearRenderableLevels(this, texture_ref)) {
              return false;
            }
          }
        }
      }
    }
  }
  return true;
}

bool GLES2DecoderImpl::IsDrawValid(
    const char* function_name, GLuint max_vertex_accessed, bool instanced,
    GLsizei primcount) {
  DCHECK(instanced || primcount == 1);

  // NOTE: We specifically do not check current_program->IsValid() because
  // it could never be invalid since glUseProgram would have failed. While
  // glLinkProgram could later mark the program as invalid the previous
  // valid program will still function if it is still the current program.
  if (!state_.current_program.get()) {
    // The program does not exist.
    // But GL says no ERROR.
    LOCAL_RENDER_WARNING("Drawing with no current shader program.");
    return false;
  }

  if (CheckDrawingFeedbackLoops()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, function_name,
        "Source and destination textures of the draw are the same.");
    return false;
  }

  return state_.vertex_attrib_manager
      ->ValidateBindings(function_name,
                         this,
                         feature_info_.get(),
                         buffer_manager(),
                         state_.current_program.get(),
                         max_vertex_accessed,
                         instanced,
                         primcount);
}

bool GLES2DecoderImpl::SimulateAttrib0(
    const char* function_name, GLuint max_vertex_accessed, bool* simulated) {
  DCHECK(simulated);
  *simulated = false;

  if (gl_version_info().BehavesLikeGLES())
    return true;

  const VertexAttrib* attrib =
      state_.vertex_attrib_manager->GetVertexAttrib(0);
  // If it's enabled or it's not used then we don't need to do anything.
  bool attrib_0_used =
      state_.current_program->GetAttribInfoByLocation(0) != NULL;
  if (attrib->enabled() && attrib_0_used) {
    return true;
  }

  // Make a buffer with a single repeated vec4 value enough to
  // simulate the constant value that is supposed to be here.
  // This is required to emulate GLES2 on GL.
  GLuint num_vertices = max_vertex_accessed + 1;
  uint32_t size_needed = 0;

  if (num_vertices == 0 ||
      !SafeMultiplyUint32(num_vertices, sizeof(Vec4f), &size_needed) ||
      size_needed > 0x7FFFFFFFU) {
    LOCAL_SET_GL_ERROR(GL_OUT_OF_MEMORY, function_name, "Simulating attrib 0");
    return false;
  }

  LOCAL_PERFORMANCE_WARNING(
      "Attribute 0 is disabled. This has significant performance penalty");

  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER(function_name);
  glBindBuffer(GL_ARRAY_BUFFER, attrib_0_buffer_id_);

  bool new_buffer = static_cast<GLsizei>(size_needed) > attrib_0_size_;
  if (new_buffer) {
    glBufferData(GL_ARRAY_BUFFER, size_needed, NULL, GL_DYNAMIC_DRAW);
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
      LOCAL_SET_GL_ERROR(
          GL_OUT_OF_MEMORY, function_name, "Simulating attrib 0");
      return false;
    }
  }

  const Vec4& value = state_.attrib_values[0];
  if (new_buffer ||
      (attrib_0_used &&
       (!attrib_0_buffer_matches_value_ || !value.Equal(attrib_0_value_)))){
    // TODO(zmo): This is not 100% correct because we might lose data when
    // casting to float type, but it is a corner case and once we migrate to
    // core profiles on desktop GL, it is no longer relevant.
    Vec4f fvalue(value);
    std::vector<Vec4f> temp(num_vertices, fvalue);
    glBufferSubData(GL_ARRAY_BUFFER, 0, size_needed, &temp[0].v[0]);
    attrib_0_buffer_matches_value_ = true;
    attrib_0_value_ = value;
    attrib_0_size_ = size_needed;
  }

  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);

  if (feature_info_->feature_flags().angle_instanced_arrays)
    glVertexAttribDivisorANGLE(0, 0);

  *simulated = true;
  return true;
}

void GLES2DecoderImpl::RestoreStateForAttrib(
    GLuint attrib_index, bool restore_array_binding) {
  const VertexAttrib* attrib =
      state_.vertex_attrib_manager->GetVertexAttrib(attrib_index);
  if (restore_array_binding) {
    const void* ptr = reinterpret_cast<const void*>(attrib->offset());
    Buffer* buffer = attrib->buffer();
    glBindBuffer(GL_ARRAY_BUFFER, buffer ? buffer->service_id() : 0);
    glVertexAttribPointer(
        attrib_index, attrib->size(), attrib->type(), attrib->normalized(),
        attrib->gl_stride(), ptr);
  }

  // Attrib divisors should only be non-zero when the ANGLE_instanced_arrays
  // extension is available
  DCHECK(attrib->divisor() == 0 ||
      feature_info_->feature_flags().angle_instanced_arrays);

  if (feature_info_->feature_flags().angle_instanced_arrays)
    glVertexAttribDivisorANGLE(attrib_index, attrib->divisor());
  glBindBuffer(
      GL_ARRAY_BUFFER, state_.bound_array_buffer.get() ?
          state_.bound_array_buffer->service_id() : 0);

  // Never touch vertex attribute 0's state (in particular, never disable it)
  // when running on desktop GL with compatibility profile because it will
  // never be re-enabled.
  if (attrib_index != 0 || gl_version_info().BehavesLikeGLES()) {
    if (attrib->enabled()) {
      glEnableVertexAttribArray(attrib_index);
    } else {
      glDisableVertexAttribArray(attrib_index);
    }
  }
}

bool GLES2DecoderImpl::SimulateFixedAttribs(
    const char* function_name,
    GLuint max_vertex_accessed, bool* simulated, GLsizei primcount) {
  DCHECK(simulated);
  *simulated = false;
  if (gl_version_info().SupportsFixedType())
    return true;

  if (!state_.vertex_attrib_manager->HaveFixedAttribs()) {
    return true;
  }

  LOCAL_PERFORMANCE_WARNING(
      "GL_FIXED attributes have a significant performance penalty");

  // NOTE: we could be smart and try to check if a buffer is used
  // twice in 2 different attribs, find the overlapping parts and therefore
  // duplicate the minimum amount of data but this whole code path is not meant
  // to be used normally. It's just here to pass that OpenGL ES 2.0 conformance
  // tests so we just add to the buffer attrib used.

  GLuint elements_needed = 0;
  const VertexAttribManager::VertexAttribList& enabled_attribs =
      state_.vertex_attrib_manager->GetEnabledVertexAttribs();
  for (VertexAttribManager::VertexAttribList::const_iterator it =
       enabled_attribs.begin(); it != enabled_attribs.end(); ++it) {
    const VertexAttrib* attrib = *it;
    const Program::VertexAttrib* attrib_info =
        state_.current_program->GetAttribInfoByLocation(attrib->index());
    GLuint max_accessed = attrib->MaxVertexAccessed(primcount,
                                                    max_vertex_accessed);
    GLuint num_vertices = max_accessed + 1;
    if (num_vertices == 0) {
      LOCAL_SET_GL_ERROR(
          GL_OUT_OF_MEMORY, function_name, "Simulating attrib 0");
      return false;
    }
    if (attrib_info &&
        attrib->CanAccess(max_accessed) &&
        attrib->type() == GL_FIXED) {
      uint32_t elements_used = 0;
      if (!SafeMultiplyUint32(num_vertices, attrib->size(), &elements_used) ||
          !SafeAddUint32(elements_needed, elements_used, &elements_needed)) {
        LOCAL_SET_GL_ERROR(
            GL_OUT_OF_MEMORY, function_name, "simulating GL_FIXED attribs");
        return false;
      }
    }
  }

  const uint32_t kSizeOfFloat = sizeof(float);  // NOLINT
  uint32_t size_needed = 0;
  if (!SafeMultiplyUint32(elements_needed, kSizeOfFloat, &size_needed) ||
      size_needed > 0x7FFFFFFFU) {
    LOCAL_SET_GL_ERROR(
        GL_OUT_OF_MEMORY, function_name, "simulating GL_FIXED attribs");
    return false;
  }

  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER(function_name);

  glBindBuffer(GL_ARRAY_BUFFER, fixed_attrib_buffer_id_);
  if (static_cast<GLsizei>(size_needed) > fixed_attrib_buffer_size_) {
    glBufferData(GL_ARRAY_BUFFER, size_needed, NULL, GL_DYNAMIC_DRAW);
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
      LOCAL_SET_GL_ERROR(
          GL_OUT_OF_MEMORY, function_name, "simulating GL_FIXED attribs");
      return false;
    }
  }

  // Copy the elements and convert to float
  GLintptr offset = 0;
  for (VertexAttribManager::VertexAttribList::const_iterator it =
       enabled_attribs.begin(); it != enabled_attribs.end(); ++it) {
    const VertexAttrib* attrib = *it;
    const Program::VertexAttrib* attrib_info =
        state_.current_program->GetAttribInfoByLocation(attrib->index());
    GLuint max_accessed = attrib->MaxVertexAccessed(primcount,
                                                  max_vertex_accessed);
    GLuint num_vertices = max_accessed + 1;
    if (num_vertices == 0) {
      LOCAL_SET_GL_ERROR(
          GL_OUT_OF_MEMORY, function_name, "Simulating attrib 0");
      return false;
    }
    if (attrib_info &&
        attrib->CanAccess(max_accessed) &&
        attrib->type() == GL_FIXED) {
      int num_elements = attrib->size() * num_vertices;
      const int src_size = num_elements * sizeof(int32_t);
      const int dst_size = num_elements * sizeof(float);
      std::unique_ptr<float[]> data(new float[num_elements]);
      const int32_t* src = reinterpret_cast<const int32_t*>(
          attrib->buffer()->GetRange(attrib->offset(), src_size));
      const int32_t* end = src + num_elements;
      float* dst = data.get();
      while (src != end) {
        *dst++ = static_cast<float>(*src++) / 65536.0f;
      }
      glBufferSubData(GL_ARRAY_BUFFER, offset, dst_size, data.get());
      glVertexAttribPointer(
          attrib->index(), attrib->size(), GL_FLOAT, false, 0,
          reinterpret_cast<GLvoid*>(offset));
      offset += dst_size;
    }
  }
  *simulated = true;
  return true;
}

void GLES2DecoderImpl::RestoreStateForSimulatedFixedAttribs() {
  // There's no need to call glVertexAttribPointer because we shadow all the
  // settings and passing GL_FIXED to it will not work.
  glBindBuffer(
      GL_ARRAY_BUFFER,
      state_.bound_array_buffer.get() ? state_.bound_array_buffer->service_id()
                                      : 0);
}

bool GLES2DecoderImpl::AttribsTypeMatch() {
  if (!state_.current_program.get())
    return true;
  const std::vector<uint32_t>& shader_attrib_active_mask =
      state_.current_program->vertex_input_active_mask();
  const std::vector<uint32_t>& shader_attrib_type_mask =
      state_.current_program->vertex_input_base_type_mask();
  const std::vector<uint32_t>& generic_vertex_attrib_type_mask =
      state_.generic_attrib_base_type_mask();
  const std::vector<uint32_t>& vertex_attrib_array_enabled_mask =
      state_.vertex_attrib_manager->attrib_enabled_mask();
  const std::vector<uint32_t>& vertex_attrib_array_type_mask =
      state_.vertex_attrib_manager->attrib_base_type_mask();
  DCHECK_EQ(shader_attrib_active_mask.size(),
            shader_attrib_type_mask.size());
  DCHECK_EQ(shader_attrib_active_mask.size(),
            generic_vertex_attrib_type_mask.size());
  DCHECK_EQ(shader_attrib_active_mask.size(),
            vertex_attrib_array_enabled_mask.size());
  DCHECK_EQ(shader_attrib_active_mask.size(),
            vertex_attrib_array_type_mask.size());
  for (size_t ii = 0; ii < shader_attrib_active_mask.size(); ++ii) {
    uint32_t vertex_attrib_source_type_mask =
        (~vertex_attrib_array_enabled_mask[ii] &
         generic_vertex_attrib_type_mask[ii]) |
        (vertex_attrib_array_enabled_mask[ii] &
         vertex_attrib_array_type_mask[ii]);
    if ((shader_attrib_type_mask[ii] & shader_attrib_active_mask[ii]) !=
        (vertex_attrib_source_type_mask & shader_attrib_active_mask[ii])) {
      return false;
    }
  }
  return true;
}

error::Error GLES2DecoderImpl::DoDrawArrays(
    const char* function_name,
    bool instanced,
    GLenum mode,
    GLint first,
    GLsizei count,
    GLsizei primcount) {
  error::Error error = WillAccessBoundFramebufferForDraw();
  if (error != error::kNoError)
    return error;
  if (!validators_->draw_mode.IsValid(mode)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(function_name, mode, "mode");
    return error::kNoError;
  }
  if (count < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "count < 0");
    return error::kNoError;
  }
  if (primcount < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "primcount < 0");
    return error::kNoError;
  }
  if (!CheckBoundDrawFramebufferValid(function_name)) {
    return error::kNoError;
  }
  // We have to check this here because the prototype for glDrawArrays
  // is GLint not GLsizei.
  if (first < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "first < 0");
    return error::kNoError;
  }

  if (feature_info_->IsWebGL2OrES3Context()) {
    if (!AttribsTypeMatch()) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                         "vertexAttrib function must match shader attrib type");
      return error::kNoError;
    }

    DCHECK(state_.bound_transform_feedback.get());
    if (state_.bound_transform_feedback->active() &&
        !state_.bound_transform_feedback->paused()) {
      if (mode != state_.bound_transform_feedback->primitive_mode()) {
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
            "mode differs from active transformfeedback's primitiveMode");
        return error::kNoError;
      }
      if (!buffer_manager()->RequestBuffersAccess(
              state_.GetErrorState(),
              state_.bound_transform_feedback.get(),
              state_.current_program->GetTransformFeedbackVaryingSizes(),
              count,
              function_name,
              "transformfeedback buffers")) {
        return error::kNoError;
      }
    }

    if (!ValidateUniformBlockBackings(function_name)) {
      return error::kNoError;
    }
  }

  if (count == 0 || primcount == 0) {
    LOCAL_RENDER_WARNING("Render count or primcount is 0.");
    return error::kNoError;
  }

  base::CheckedNumeric<GLuint> checked_max_vertex = first;
  checked_max_vertex += count - 1;
  // first and count-1 are both a non-negative int, so their sum fits an
  // unsigned int.
  if (!checked_max_vertex.IsValid()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name,
                       "first + count overflow");
    return error::kNoError;
  }
  GLuint max_vertex_accessed = checked_max_vertex.ValueOrDefault(0);
  if (IsDrawValid(function_name, max_vertex_accessed, instanced, primcount)) {
    if (!ClearUnclearedTextures()) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "out of memory");
      return error::kNoError;
    }
    bool simulated_attrib_0 = false;
    if (!SimulateAttrib0(
        function_name, max_vertex_accessed, &simulated_attrib_0)) {
      return error::kNoError;
    }
    bool simulated_fixed_attribs = false;
    if (SimulateFixedAttribs(
        function_name, max_vertex_accessed, &simulated_fixed_attribs,
        primcount)) {
      bool textures_set = !PrepareTexturesForRender();
      ApplyDirtyState();
      if (!ValidateAndAdjustDrawBuffers(function_name)) {
        return error::kNoError;
      }
      if (!instanced) {
        glDrawArrays(mode, first, count);
      } else {
        glDrawArraysInstancedANGLE(mode, first, count, primcount);
      }
      if (textures_set) {
        RestoreStateForTextures();
      }
      if (simulated_fixed_attribs) {
        RestoreStateForSimulatedFixedAttribs();
      }
    }
    if (simulated_attrib_0) {
      // We don't have to restore attrib 0 generic data at the end of this
      // function even if it is simulated. This is because we will simulate
      // it in each draw call, and attrib 0 generic data queries use cached
      // values instead of passing down to the underlying driver.
      RestoreStateForAttrib(0, false);
    }
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDrawArrays(uint32_t immediate_data_size,
                                                const volatile void* cmd_data) {
  const volatile cmds::DrawArrays& c =
      *static_cast<const volatile cmds::DrawArrays*>(cmd_data);
  return DoDrawArrays("glDrawArrays",
                      false,
                      static_cast<GLenum>(c.mode),
                      static_cast<GLint>(c.first),
                      static_cast<GLsizei>(c.count),
                      1);
}

error::Error GLES2DecoderImpl::HandleDrawArraysInstancedANGLE(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DrawArraysInstancedANGLE& c =
      *static_cast<const volatile gles2::cmds::DrawArraysInstancedANGLE*>(
          cmd_data);
  if (!features().angle_instanced_arrays)
    return error::kUnknownCommand;

  return DoDrawArrays("glDrawArraysIntancedANGLE",
                      true,
                      static_cast<GLenum>(c.mode),
                      static_cast<GLint>(c.first),
                      static_cast<GLsizei>(c.count),
                      static_cast<GLsizei>(c.primcount));
}

error::Error GLES2DecoderImpl::DoDrawElements(const char* function_name,
                                              bool instanced,
                                              GLenum mode,
                                              GLsizei count,
                                              GLenum type,
                                              int32_t offset,
                                              GLsizei primcount) {
  error::Error error = WillAccessBoundFramebufferForDraw();
  if (error != error::kNoError)
    return error;

  if (count < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "count < 0");
    return error::kNoError;
  }
  if (offset < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "offset < 0");
    return error::kNoError;
  }
  if (!validators_->draw_mode.IsValid(mode)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(function_name, mode, "mode");
    return error::kNoError;
  }
  if (!validators_->index_type.IsValid(type)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(function_name, type, "type");
    return error::kNoError;
  }
  if (primcount < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "primcount < 0");
    return error::kNoError;
  }
  Buffer* element_array_buffer = buffer_manager()->RequestBufferAccess(
      &state_, GL_ELEMENT_ARRAY_BUFFER, function_name);
  if (!element_array_buffer) {
    return error::kNoError;
  }

  if (!CheckBoundDrawFramebufferValid(function_name)) {
    return error::kNoError;
  }

  if (state_.bound_transform_feedback.get() &&
      state_.bound_transform_feedback->active() &&
      !state_.bound_transform_feedback->paused()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
        "transformfeedback is active and not paused");
    return error::kNoError;
  }

  if (feature_info_->IsWebGL2OrES3Context()) {
    if (!AttribsTypeMatch()) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                         "vertexAttrib function must match shader attrib type");
      return error::kNoError;
    }
    if (!ValidateUniformBlockBackings(function_name)) {
      return error::kNoError;
    }
  }

  if (count == 0 || primcount == 0) {
    return error::kNoError;
  }

  GLuint max_vertex_accessed;
  if (!element_array_buffer->GetMaxValueForRange(
          offset, count, type,
          state_.enable_flags.primitive_restart_fixed_index,
          &max_vertex_accessed)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, function_name, "range out of bounds for buffer");
    return error::kNoError;
  }

  if (IsDrawValid(function_name, max_vertex_accessed, instanced, primcount)) {
    if (!ClearUnclearedTextures()) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "out of memory");
      return error::kNoError;
    }
    bool simulated_attrib_0 = false;
    if (!SimulateAttrib0(
        function_name, max_vertex_accessed, &simulated_attrib_0)) {
      return error::kNoError;
    }
    bool simulated_fixed_attribs = false;
    if (SimulateFixedAttribs(
        function_name, max_vertex_accessed, &simulated_fixed_attribs,
        primcount)) {
      bool textures_set = !PrepareTexturesForRender();
      ApplyDirtyState();
      // TODO(gman): Refactor to hide these details in BufferManager or
      // VertexAttribManager.
      const GLvoid* indices = reinterpret_cast<const GLvoid*>(offset);
      bool used_client_side_array = false;
      if (element_array_buffer->IsClientSideArray()) {
        used_client_side_array = true;
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        indices = element_array_buffer->GetRange(offset, 0);
      }
      if (!ValidateAndAdjustDrawBuffers(function_name)) {
        return error::kNoError;
      }
      if (state_.enable_flags.primitive_restart_fixed_index &&
          feature_info_->feature_flags().
              emulate_primitive_restart_fixed_index) {
        glEnable(GL_PRIMITIVE_RESTART);
        buffer_manager()->SetPrimitiveRestartFixedIndexIfNecessary(type);
      }
      if (!instanced) {
        glDrawElements(mode, count, type, indices);
      } else {
        glDrawElementsInstancedANGLE(mode, count, type, indices, primcount);
      }
      if (state_.enable_flags.primitive_restart_fixed_index &&
          feature_info_->feature_flags().
              emulate_primitive_restart_fixed_index) {
        glDisable(GL_PRIMITIVE_RESTART);
      }
      if (used_client_side_array) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
                     element_array_buffer->service_id());
      }
      if (textures_set) {
        RestoreStateForTextures();
      }
      if (simulated_fixed_attribs) {
        RestoreStateForSimulatedFixedAttribs();
      }
    }
    if (simulated_attrib_0) {
      // We don't have to restore attrib 0 generic data at the end of this
      // function even if it is simulated. This is because we will simulate
      // it in each draw call, and attrib 0 generic data queries use cached
      // values instead of passing down to the underlying driver.
      RestoreStateForAttrib(0, false);
    }
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDrawElements(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DrawElements& c =
      *static_cast<const volatile gles2::cmds::DrawElements*>(cmd_data);
  return DoDrawElements("glDrawElements", false, static_cast<GLenum>(c.mode),
                        static_cast<GLsizei>(c.count),
                        static_cast<GLenum>(c.type),
                        static_cast<int32_t>(c.index_offset), 1);
}

error::Error GLES2DecoderImpl::HandleDrawElementsInstancedANGLE(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DrawElementsInstancedANGLE& c =
      *static_cast<const volatile gles2::cmds::DrawElementsInstancedANGLE*>(
          cmd_data);
  if (!features().angle_instanced_arrays)
    return error::kUnknownCommand;

  return DoDrawElements(
      "glDrawElementsInstancedANGLE", true, static_cast<GLenum>(c.mode),
      static_cast<GLsizei>(c.count), static_cast<GLenum>(c.type),
      static_cast<int32_t>(c.index_offset), static_cast<GLsizei>(c.primcount));
}

GLuint GLES2DecoderImpl::DoGetMaxValueInBufferCHROMIUM(
    GLuint buffer_id, GLsizei count, GLenum type, GLuint offset) {
  GLuint max_vertex_accessed = 0;
  Buffer* buffer = GetBuffer(buffer_id);
  if (!buffer) {
    // TODO(gman): Should this be a GL error or a command buffer error?
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "GetMaxValueInBufferCHROMIUM", "unknown buffer");
  } else {
    // The max value is used here to emulate client-side vertex
    // arrays, by uploading enough vertices into buffer objects to
    // cover the DrawElements call. Baking the primitive restart bit
    // into this result isn't strictly correct in all cases; the
    // client side code should pass down the bit and decide how to use
    // the result. However, the only caller makes the draw call
    // immediately afterward, so the state won't change between this
    // query and the draw call.
    if (!buffer->GetMaxValueForRange(
            offset, count, type,
            state_.enable_flags.primitive_restart_fixed_index,
            &max_vertex_accessed)) {
      // TODO(gman): Should this be a GL error or a command buffer error?
      LOCAL_SET_GL_ERROR(
          GL_INVALID_OPERATION,
          "GetMaxValueInBufferCHROMIUM", "range out of bounds for buffer");
    }
  }
  return max_vertex_accessed;
}

void GLES2DecoderImpl::DoShaderSource(
    GLuint client_id, GLsizei count, const char** data, const GLint* length) {
  std::string str;
  for (GLsizei ii = 0; ii < count; ++ii) {
    if (length && length[ii] > 0)
      str.append(data[ii], length[ii]);
    else
      str.append(data[ii]);
  }
  Shader* shader = GetShaderInfoNotProgram(client_id, "glShaderSource");
  if (!shader) {
    return;
  }
  // Note: We don't actually call glShaderSource here. We wait until
  // we actually compile the shader.
  shader->set_source(str);
}

void GLES2DecoderImpl::DoTransformFeedbackVaryings(
    GLuint client_program_id, GLsizei count, const char* const* varyings,
    GLenum buffer_mode) {
  Program* program = GetProgramInfoNotShader(
      client_program_id, "glTransformFeedbackVaryings");
  if (!program) {
    return;
  }
  program->TransformFeedbackVaryings(count, varyings, buffer_mode);
}

scoped_refptr<ShaderTranslatorInterface> GLES2DecoderImpl::GetTranslator(
    GLenum type) {
  return type == GL_VERTEX_SHADER ? vertex_translator_ : fragment_translator_;
}

void GLES2DecoderImpl::DoCompileShader(GLuint client_id) {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::DoCompileShader");
  Shader* shader = GetShaderInfoNotProgram(client_id, "glCompileShader");
  if (!shader) {
    return;
  }

  scoped_refptr<ShaderTranslatorInterface> translator;
  if (!feature_info_->disable_shader_translator())
      translator = GetTranslator(shader->shader_type());

  const Shader::TranslatedShaderSourceType source_type =
      feature_info_->feature_flags().angle_translated_shader_source ?
      Shader::kANGLE : Shader::kGL;
  shader->RequestCompile(translator, source_type);
}

void GLES2DecoderImpl::DoGetShaderiv(GLuint shader_id,
                                     GLenum pname,
                                     GLint* params,
                                     GLsizei params_size) {
  Shader* shader = GetShaderInfoNotProgram(shader_id, "glGetShaderiv");
  if (!shader) {
    return;
  }

  // Compile now for statuses that require it.
  switch (pname) {
    case GL_COMPILE_STATUS:
    case GL_INFO_LOG_LENGTH:
    case GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE:
      shader->DoCompile();
      break;

    default:
    break;
  }

  switch (pname) {
    case GL_SHADER_SOURCE_LENGTH:
      *params = shader->source().size();
      if (*params)
        ++(*params);
      return;
    case GL_COMPILE_STATUS:
      *params = compile_shader_always_succeeds_ ? true : shader->valid();
      return;
    case GL_INFO_LOG_LENGTH:
      *params = shader->log_info().size();
      if (*params)
        ++(*params);
      return;
    case GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE:
      *params = shader->translated_source().size();
      if (*params)
        ++(*params);
      return;
    default:
      break;
  }
  glGetShaderiv(shader->service_id(), pname, params);
}

error::Error GLES2DecoderImpl::HandleGetShaderSource(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetShaderSource& c =
      *static_cast<const volatile gles2::cmds::GetShaderSource*>(cmd_data);
  GLuint shader_id = c.shader;
  uint32_t bucket_id = static_cast<uint32_t>(c.bucket_id);
  Bucket* bucket = CreateBucket(bucket_id);
  Shader* shader = GetShaderInfoNotProgram(shader_id, "glGetShaderSource");
  if (!shader || shader->source().empty()) {
    bucket->SetSize(0);
    return error::kNoError;
  }
  bucket->SetFromString(shader->source().c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetTranslatedShaderSourceANGLE(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetTranslatedShaderSourceANGLE& c =
      *static_cast<const volatile gles2::cmds::GetTranslatedShaderSourceANGLE*>(
          cmd_data);
  GLuint shader_id = c.shader;
  uint32_t bucket_id = static_cast<uint32_t>(c.bucket_id);
  Bucket* bucket = CreateBucket(bucket_id);
  Shader* shader = GetShaderInfoNotProgram(
      shader_id, "glGetTranslatedShaderSourceANGLE");
  if (!shader) {
    bucket->SetSize(0);
    return error::kNoError;
  }

  // Make sure translator has been utilized in compile.
  shader->DoCompile();

  bucket->SetFromString(shader->translated_source().c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetProgramInfoLog(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetProgramInfoLog& c =
      *static_cast<const volatile gles2::cmds::GetProgramInfoLog*>(cmd_data);
  GLuint program_id = c.program;
  uint32_t bucket_id = static_cast<uint32_t>(c.bucket_id);
  Bucket* bucket = CreateBucket(bucket_id);
  Program* program = GetProgramInfoNotShader(
      program_id, "glGetProgramInfoLog");
  if (!program || !program->log_info()) {
    bucket->SetFromString("");
    return error::kNoError;
  }
  bucket->SetFromString(program->log_info()->c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetShaderInfoLog(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetShaderInfoLog& c =
      *static_cast<const volatile gles2::cmds::GetShaderInfoLog*>(cmd_data);
  GLuint shader_id = c.shader;
  uint32_t bucket_id = static_cast<uint32_t>(c.bucket_id);
  Bucket* bucket = CreateBucket(bucket_id);
  Shader* shader = GetShaderInfoNotProgram(shader_id, "glGetShaderInfoLog");
  if (!shader) {
    bucket->SetFromString("");
    return error::kNoError;
  }

  // Shader must be compiled in order to get the info log.
  shader->DoCompile();

  bucket->SetFromString(shader->log_info().c_str());
  return error::kNoError;
}

bool GLES2DecoderImpl::DoIsEnabled(GLenum cap) {
  return state_.GetEnabled(cap);
}

bool GLES2DecoderImpl::DoIsBuffer(GLuint client_id) {
  const Buffer* buffer = GetBuffer(client_id);
  return buffer && buffer->IsValid() && !buffer->IsDeleted();
}

bool GLES2DecoderImpl::DoIsFramebuffer(GLuint client_id) {
  const Framebuffer* framebuffer =
      GetFramebuffer(client_id);
  return framebuffer && framebuffer->IsValid() && !framebuffer->IsDeleted();
}

bool GLES2DecoderImpl::DoIsProgram(GLuint client_id) {
  // IsProgram is true for programs as soon as they are created, until they are
  // deleted and no longer in use.
  const Program* program = GetProgram(client_id);
  return program != NULL && !program->IsDeleted();
}

bool GLES2DecoderImpl::DoIsRenderbuffer(GLuint client_id) {
  const Renderbuffer* renderbuffer =
      GetRenderbuffer(client_id);
  return renderbuffer && renderbuffer->IsValid() && !renderbuffer->IsDeleted();
}

bool GLES2DecoderImpl::DoIsShader(GLuint client_id) {
  // IsShader is true for shaders as soon as they are created, until they
  // are deleted and not attached to any programs.
  const Shader* shader = GetShader(client_id);
  return shader != NULL && !shader->IsDeleted();
}

bool GLES2DecoderImpl::DoIsTexture(GLuint client_id) {
  const TextureRef* texture_ref = GetTexture(client_id);
  return texture_ref && texture_ref->texture()->IsValid();
}

bool GLES2DecoderImpl::DoIsSampler(GLuint client_id) {
  const Sampler* sampler = GetSampler(client_id);
  return sampler && !sampler->IsDeleted();
}

bool GLES2DecoderImpl::DoIsTransformFeedback(GLuint client_id) {
  const TransformFeedback* transform_feedback =
      GetTransformFeedback(client_id);
  return transform_feedback && transform_feedback->has_been_bound();
}

void GLES2DecoderImpl::DoAttachShader(
    GLuint program_client_id, GLint shader_client_id) {
  Program* program = GetProgramInfoNotShader(
      program_client_id, "glAttachShader");
  if (!program) {
    return;
  }
  Shader* shader = GetShaderInfoNotProgram(shader_client_id, "glAttachShader");
  if (!shader) {
    return;
  }
  if (!program->AttachShader(shader_manager(), shader)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        "glAttachShader",
        "can not attach more than one shader of the same type.");
    return;
  }
  glAttachShader(program->service_id(), shader->service_id());
}

void GLES2DecoderImpl::DoDetachShader(
    GLuint program_client_id, GLint shader_client_id) {
  Program* program = GetProgramInfoNotShader(
      program_client_id, "glDetachShader");
  if (!program) {
    return;
  }
  Shader* shader = GetShaderInfoNotProgram(shader_client_id, "glDetachShader");
  if (!shader) {
    return;
  }
  if (!program->IsShaderAttached(shader)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        "glDetachShader", "shader not attached to program");
    return;
  }
  glDetachShader(program->service_id(), shader->service_id());
  program->DetachShader(shader_manager(), shader);
}

void GLES2DecoderImpl::DoValidateProgram(GLuint program_client_id) {
  Program* program = GetProgramInfoNotShader(
      program_client_id, "glValidateProgram");
  if (!program) {
    return;
  }
  program->Validate();
}

void GLES2DecoderImpl::GetVertexAttribHelper(
    const VertexAttrib* attrib, GLenum pname, GLint* params) {
  switch (pname) {
    case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
      {
        Buffer* buffer = attrib->buffer();
        if (buffer && !buffer->IsDeleted()) {
          GLuint client_id;
          buffer_manager()->GetClientId(buffer->service_id(), &client_id);
          *params = client_id;
        }
        break;
      }
    case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
      *params = attrib->enabled();
      break;
    case GL_VERTEX_ATTRIB_ARRAY_SIZE:
      *params = attrib->size();
      break;
    case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
      *params = attrib->gl_stride();
      break;
    case GL_VERTEX_ATTRIB_ARRAY_TYPE:
      *params = attrib->type();
      break;
    case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
      *params = attrib->normalized();
      break;
    case GL_VERTEX_ATTRIB_ARRAY_DIVISOR:
      *params = attrib->divisor();
      break;
    case GL_VERTEX_ATTRIB_ARRAY_INTEGER:
      *params = attrib->integer();
      break;
    default:
      NOTREACHED();
      break;
  }
}

void GLES2DecoderImpl::DoGetSamplerParameterfv(GLuint client_id,
                                               GLenum pname,
                                               GLfloat* params,
                                               GLsizei params_size) {
  Sampler* sampler = GetSampler(client_id);
  if (!sampler) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, "glGetSamplerParamterfv", "unknown sampler");
    return;
  }
  glGetSamplerParameterfv(sampler->service_id(), pname, params);
}

void GLES2DecoderImpl::DoGetSamplerParameteriv(GLuint client_id,
                                               GLenum pname,
                                               GLint* params,
                                               GLsizei params_size) {
  Sampler* sampler = GetSampler(client_id);
  if (!sampler) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, "glGetSamplerParamteriv", "unknown sampler");
    return;
  }
  glGetSamplerParameteriv(sampler->service_id(), pname, params);
}

void GLES2DecoderImpl::GetTexParameterImpl(
    GLenum target, GLenum pname, GLfloat* fparams, GLint* iparams,
    const char* function_name) {
  TextureRef* texture_ref = texture_manager()->GetTextureInfoForTarget(
      &state_, target);
  if (!texture_ref) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, function_name, "unknown texture for target");
    return;
  }
  Texture* texture = texture_ref->texture();
  switch (pname) {
    case GL_TEXTURE_MAX_ANISOTROPY_EXT:
      if (workarounds().init_texture_max_anisotropy) {
        texture->InitTextureMaxAnisotropyIfNeeded(target);
      }
      break;
    case GL_TEXTURE_IMMUTABLE_LEVELS:
      if (gl_version_info().IsLowerThanGL(4, 2)) {
        GLint levels = texture->GetImmutableLevels();
        if (fparams) {
          fparams[0] = static_cast<GLfloat>(levels);
        } else {
          iparams[0] = levels;
        }
        return;
      }
      break;
    // Get the level information from the texture to avoid a Mac driver
    // bug where they store the levels in int16_t, making values bigger
    // than 2^15-1 overflow in the negative range.
    case GL_TEXTURE_BASE_LEVEL:
      if (workarounds().use_shadowed_tex_level_params) {
        if (fparams) {
          fparams[0] = static_cast<GLfloat>(texture->base_level());
        } else {
          iparams[0] = texture->base_level();
        }
        return;
      }
    case GL_TEXTURE_MAX_LEVEL:
      if (workarounds().use_shadowed_tex_level_params) {
        if (fparams) {
          fparams[0] = static_cast<GLfloat>(texture->max_level());
        } else {
          iparams[0] = texture->max_level();
        }
        return;
      }
    default:
      break;
  }
  if (fparams) {
    glGetTexParameterfv(target, pname, fparams);
  } else {
    glGetTexParameteriv(target, pname, iparams);
  }
}

void GLES2DecoderImpl::DoGetTexParameterfv(GLenum target,
                                           GLenum pname,
                                           GLfloat* params,
                                           GLsizei params_size) {
  GetTexParameterImpl(target, pname, params, nullptr, "glGetTexParameterfv");
}

void GLES2DecoderImpl::DoGetTexParameteriv(GLenum target,
                                           GLenum pname,
                                           GLint* params,
                                           GLsizei params_size) {
  GetTexParameterImpl(target, pname, nullptr, params, "glGetTexParameteriv");
}

template <typename T>
void GLES2DecoderImpl::DoGetVertexAttribImpl(
    GLuint index, GLenum pname, T* params) {
  VertexAttrib* attrib = state_.vertex_attrib_manager->GetVertexAttrib(index);
  if (!attrib) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glGetVertexAttrib", "index out of range");
    return;
  }
  switch (pname) {
    case GL_CURRENT_VERTEX_ATTRIB:
      state_.attrib_values[index].GetValues(params);
      break;
    default: {
      GLint value = 0;
      GetVertexAttribHelper(attrib, pname, &value);
      *params = static_cast<T>(value);
      break;
    }
  }
}

void GLES2DecoderImpl::DoGetVertexAttribfv(GLuint index,
                                           GLenum pname,
                                           GLfloat* params,
                                           GLsizei params_size) {
  DoGetVertexAttribImpl<GLfloat>(index, pname, params);
}

void GLES2DecoderImpl::DoGetVertexAttribiv(GLuint index,
                                           GLenum pname,
                                           GLint* params,
                                           GLsizei params_size) {
  DoGetVertexAttribImpl<GLint>(index, pname, params);
}

void GLES2DecoderImpl::DoGetVertexAttribIiv(GLuint index,
                                            GLenum pname,
                                            GLint* params,
                                            GLsizei params_size) {
  DoGetVertexAttribImpl<GLint>(index, pname, params);
}

void GLES2DecoderImpl::DoGetVertexAttribIuiv(GLuint index,
                                             GLenum pname,
                                             GLuint* params,
                                             GLsizei params_size) {
  DoGetVertexAttribImpl<GLuint>(index, pname, params);
}

template <typename T>
bool GLES2DecoderImpl::SetVertexAttribValue(
    const char* function_name, GLuint index, const T* value) {
  if (index >= state_.attrib_values.size()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "index out of range");
    return false;
  }
  state_.attrib_values[index].SetValues(value);
  return true;
}

void GLES2DecoderImpl::DoVertexAttrib1f(GLuint index, GLfloat v0) {
  GLfloat v[4] = { v0, 0.0f, 0.0f, 1.0f, };
  if (SetVertexAttribValue("glVertexAttrib1f", index, v)) {
    state_.SetGenericVertexAttribBaseType(
        index, SHADER_VARIABLE_FLOAT);
    glVertexAttrib1f(index, v0);
  }
}

void GLES2DecoderImpl::DoVertexAttrib2f(GLuint index, GLfloat v0, GLfloat v1) {
  GLfloat v[4] = { v0, v1, 0.0f, 1.0f, };
  if (SetVertexAttribValue("glVertexAttrib2f", index, v)) {
    state_.SetGenericVertexAttribBaseType(
        index, SHADER_VARIABLE_FLOAT);
    glVertexAttrib2f(index, v0, v1);
  }
}

void GLES2DecoderImpl::DoVertexAttrib3f(
    GLuint index, GLfloat v0, GLfloat v1, GLfloat v2) {
  GLfloat v[4] = { v0, v1, v2, 1.0f, };
  if (SetVertexAttribValue("glVertexAttrib3f", index, v)) {
    state_.SetGenericVertexAttribBaseType(
        index, SHADER_VARIABLE_FLOAT);
    glVertexAttrib3f(index, v0, v1, v2);
  }
}

void GLES2DecoderImpl::DoVertexAttrib4f(
    GLuint index, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
  GLfloat v[4] = { v0, v1, v2, v3, };
  if (SetVertexAttribValue("glVertexAttrib4f", index, v)) {
    state_.SetGenericVertexAttribBaseType(
        index, SHADER_VARIABLE_FLOAT);
    glVertexAttrib4f(index, v0, v1, v2, v3);
  }
}

void GLES2DecoderImpl::DoVertexAttrib1fv(GLuint index,
                                         const volatile GLfloat* v) {
  GLfloat t[4] = { v[0], 0.0f, 0.0f, 1.0f, };
  if (SetVertexAttribValue("glVertexAttrib1fv", index, t)) {
    state_.SetGenericVertexAttribBaseType(
        index, SHADER_VARIABLE_FLOAT);
    glVertexAttrib1fv(index, t);
  }
}

void GLES2DecoderImpl::DoVertexAttrib2fv(GLuint index,
                                         const volatile GLfloat* v) {
  GLfloat t[4] = { v[0], v[1], 0.0f, 1.0f, };
  if (SetVertexAttribValue("glVertexAttrib2fv", index, t)) {
    state_.SetGenericVertexAttribBaseType(
        index, SHADER_VARIABLE_FLOAT);
    glVertexAttrib2fv(index, t);
  }
}

void GLES2DecoderImpl::DoVertexAttrib3fv(GLuint index,
                                         const volatile GLfloat* v) {
  GLfloat t[4] = { v[0], v[1], v[2], 1.0f, };
  if (SetVertexAttribValue("glVertexAttrib3fv", index, t)) {
    state_.SetGenericVertexAttribBaseType(
        index, SHADER_VARIABLE_FLOAT);
    glVertexAttrib3fv(index, t);
  }
}

void GLES2DecoderImpl::DoVertexAttrib4fv(GLuint index,
                                         const volatile GLfloat* v) {
  GLfloat t[4] = {v[0], v[1], v[2], v[3]};
  if (SetVertexAttribValue("glVertexAttrib4fv", index, t)) {
    state_.SetGenericVertexAttribBaseType(
        index, SHADER_VARIABLE_FLOAT);
    glVertexAttrib4fv(index, t);
  }
}

void GLES2DecoderImpl::DoVertexAttribI4i(
    GLuint index, GLint v0, GLint v1, GLint v2, GLint v3) {
  GLint v[4] = { v0, v1, v2, v3 };
  if (SetVertexAttribValue("glVertexAttribI4i", index, v)) {
    state_.SetGenericVertexAttribBaseType(
        index, SHADER_VARIABLE_INT);
    glVertexAttribI4i(index, v0, v1, v2, v3);
  }
}

void GLES2DecoderImpl::DoVertexAttribI4iv(GLuint index,
                                          const volatile GLint* v) {
  GLint t[4] = {v[0], v[1], v[2], v[3]};
  if (SetVertexAttribValue("glVertexAttribI4iv", index, t)) {
    state_.SetGenericVertexAttribBaseType(
        index, SHADER_VARIABLE_INT);
    glVertexAttribI4iv(index, t);
  }
}

void GLES2DecoderImpl::DoVertexAttribI4ui(
    GLuint index, GLuint v0, GLuint v1, GLuint v2, GLuint v3) {
  GLuint v[4] = { v0, v1, v2, v3 };
  if (SetVertexAttribValue("glVertexAttribI4ui", index, v)) {
    state_.SetGenericVertexAttribBaseType(
        index, SHADER_VARIABLE_UINT);
    glVertexAttribI4ui(index, v0, v1, v2, v3);
  }
}

void GLES2DecoderImpl::DoVertexAttribI4uiv(GLuint index,
                                           const volatile GLuint* v) {
  GLuint t[4] = {v[0], v[1], v[2], v[3]};
  if (SetVertexAttribValue("glVertexAttribI4uiv", index, t)) {
    state_.SetGenericVertexAttribBaseType(
        index, SHADER_VARIABLE_UINT);
    glVertexAttribI4uiv(index, t);
  }
}

error::Error GLES2DecoderImpl::HandleVertexAttribIPointer(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::VertexAttribIPointer& c =
      *static_cast<const volatile gles2::cmds::VertexAttribIPointer*>(cmd_data);
  GLuint indx = c.indx;
  GLint size = c.size;
  GLenum type = c.type;
  GLsizei stride = c.stride;
  GLsizei offset = c.offset;

  if (!state_.bound_array_buffer.get() ||
      state_.bound_array_buffer->IsDeleted()) {
    if (state_.vertex_attrib_manager.get() ==
        state_.default_vertex_attrib_manager.get()) {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_OPERATION,
          "glVertexAttribIPointer", "no array buffer bound");
      return error::kNoError;
    } else if (offset != 0) {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_OPERATION,
          "glVertexAttribIPointer", "client side arrays are not allowed");
      return error::kNoError;
    }
  }

  const void* ptr = reinterpret_cast<const void*>(offset);
  if (!validators_->vertex_attrib_i_type.IsValid(type)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glVertexAttribIPointer", type, "type");
    return error::kNoError;
  }
  if (size < 1 || size > 4) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glVertexAttribIPointer", "size GL_INVALID_VALUE");
    return error::kNoError;
  }
  if (indx >= group_->max_vertex_attribs()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glVertexAttribIPointer", "index out of range");
    return error::kNoError;
  }
  if (stride < 0) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glVertexAttribIPointer", "stride < 0");
    return error::kNoError;
  }
  if (stride > 255) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glVertexAttribIPointer", "stride > 255");
    return error::kNoError;
  }
  if (offset < 0) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glVertexAttribIPointer", "offset < 0");
    return error::kNoError;
  }
  GLsizei type_size = GLES2Util::GetGLTypeSizeForBuffers(type);
  // type_size must be a power of two to use & as optimized modulo.
  DCHECK(GLES2Util::IsPOT(type_size));
  if (offset & (type_size - 1)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        "glVertexAttribIPointer", "offset not valid for type");
    return error::kNoError;
  }
  if (stride & (type_size - 1)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        "glVertexAttribIPointer", "stride not valid for type");
    return error::kNoError;
  }

  GLenum base_type = (type == GL_BYTE || type == GL_SHORT || type == GL_INT) ?
                      SHADER_VARIABLE_INT : SHADER_VARIABLE_UINT;
  state_.vertex_attrib_manager->UpdateAttribBaseTypeAndMask(indx, base_type);

  GLsizei group_size = GLES2Util::GetGroupSizeForBufferType(size, type);
  state_.vertex_attrib_manager
      ->SetAttribInfo(indx,
                      state_.bound_array_buffer.get(),
                      size,
                      type,
                      GL_FALSE,
                      stride,
                      stride != 0 ? stride : group_size,
                      offset,
                      GL_TRUE);
  glVertexAttribIPointer(indx, size, type, stride, ptr);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleVertexAttribPointer(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::VertexAttribPointer& c =
      *static_cast<const volatile gles2::cmds::VertexAttribPointer*>(cmd_data);
  GLuint indx = c.indx;
  GLint size = c.size;
  GLenum type = c.type;
  GLboolean normalized = static_cast<GLboolean>(c.normalized);
  GLsizei stride = c.stride;
  GLsizei offset = c.offset;

  if (!state_.bound_array_buffer.get() ||
      state_.bound_array_buffer->IsDeleted()) {
    if (state_.vertex_attrib_manager.get() ==
        state_.default_vertex_attrib_manager.get()) {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_OPERATION,
          "glVertexAttribPointer", "no array buffer bound");
      return error::kNoError;
    } else if (offset != 0) {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_OPERATION,
          "glVertexAttribPointer", "client side arrays are not allowed");
      return error::kNoError;
    }
  }
  const void* ptr = reinterpret_cast<const void*>(offset);
  if (!validators_->vertex_attrib_type.IsValid(type)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glVertexAttribPointer", type, "type");
    return error::kNoError;
  }
  if (size < 1 || size > 4) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glVertexAttribPointer", "size GL_INVALID_VALUE");
    return error::kNoError;
  }
  if ((type == GL_INT_2_10_10_10_REV || type == GL_UNSIGNED_INT_2_10_10_10_REV)
      && size != 4) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, "glVertexAttribPointer", "size != 4");
    return error::kNoError;
  }
  if (indx >= group_->max_vertex_attribs()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glVertexAttribPointer", "index out of range");
    return error::kNoError;
  }
  if (stride < 0) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glVertexAttribPointer", "stride < 0");
    return error::kNoError;
  }
  if (stride > 255) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glVertexAttribPointer", "stride > 255");
    return error::kNoError;
  }
  if (offset < 0) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glVertexAttribPointer", "offset < 0");
    return error::kNoError;
  }
  GLsizei type_size = GLES2Util::GetGLTypeSizeForBuffers(type);
  // type_size must be a power of two to use & as optimized modulo.
  DCHECK(GLES2Util::IsPOT(type_size));
  if (offset & (type_size - 1)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        "glVertexAttribPointer", "offset not valid for type");
    return error::kNoError;
  }
  if (stride & (type_size - 1)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        "glVertexAttribPointer", "stride not valid for type");
    return error::kNoError;
  }

  state_.vertex_attrib_manager->UpdateAttribBaseTypeAndMask(
      indx, SHADER_VARIABLE_FLOAT);

  GLsizei group_size = GLES2Util::GetGroupSizeForBufferType(size, type);
  state_.vertex_attrib_manager
      ->SetAttribInfo(indx,
                      state_.bound_array_buffer.get(),
                      size,
                      type,
                      normalized,
                      stride,
                      stride != 0 ? stride : group_size,
                      offset,
                      GL_FALSE);
  if (type != GL_FIXED || gl_version_info().SupportsFixedType()) {
    glVertexAttribPointer(indx, size, type, normalized, stride, ptr);
  }
  return error::kNoError;
}

void GLES2DecoderImpl::DoViewport(GLint x, GLint y, GLsizei width,
                                  GLsizei height) {
  state_.viewport_x = x;
  state_.viewport_y = y;
  state_.viewport_width = std::min(width, viewport_max_width_);
  state_.viewport_height = std::min(height, viewport_max_height_);
  glViewport(x, y, width, height);
}

error::Error GLES2DecoderImpl::HandleVertexAttribDivisorANGLE(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::VertexAttribDivisorANGLE& c =
      *static_cast<const volatile gles2::cmds::VertexAttribDivisorANGLE*>(
          cmd_data);
  if (!features().angle_instanced_arrays)
    return error::kUnknownCommand;

  GLuint index = c.index;
  GLuint divisor = c.divisor;
  if (index >= group_->max_vertex_attribs()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE,
        "glVertexAttribDivisorANGLE", "index out of range");
    return error::kNoError;
  }

  state_.vertex_attrib_manager->SetDivisor(
      index,
      divisor);
  glVertexAttribDivisorANGLE(index, divisor);
  return error::kNoError;
}

template <typename pixel_data_type>
static void WriteAlphaData(void* pixels,
                           uint32_t row_count,
                           uint32_t channel_count,
                           uint32_t alpha_channel_index,
                           uint32_t unpadded_row_size,
                           uint32_t padded_row_size,
                           pixel_data_type alpha_value) {
  DCHECK_GT(channel_count, 0U);
  DCHECK_EQ(unpadded_row_size % sizeof(pixel_data_type), 0U);
  uint32_t unpadded_row_size_in_elements =
      unpadded_row_size / sizeof(pixel_data_type);
  DCHECK_EQ(padded_row_size % sizeof(pixel_data_type), 0U);
  uint32_t padded_row_size_in_elements =
      padded_row_size / sizeof(pixel_data_type);
  pixel_data_type* dst =
      static_cast<pixel_data_type*>(pixels) + alpha_channel_index;
  for (uint32_t yy = 0; yy < row_count; ++yy) {
    pixel_data_type* end = dst + unpadded_row_size_in_elements;
    for (pixel_data_type* d = dst; d < end; d += channel_count) {
      *d = alpha_value;
    }
    dst += padded_row_size_in_elements;
  }
}

void GLES2DecoderImpl::FinishReadPixels(GLsizei width,
                                        GLsizei height,
                                        GLsizei format,
                                        GLsizei type,
                                        uint32_t pixels_shm_id,
                                        uint32_t pixels_shm_offset,
                                        uint32_t result_shm_id,
                                        uint32_t result_shm_offset,
                                        GLint pack_alignment,
                                        GLenum read_format,
                                        GLuint buffer) {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::FinishReadPixels");
  typedef cmds::ReadPixels::Result Result;
  uint32_t pixels_size;
  Result* result = NULL;
  if (result_shm_id != 0) {
    result = GetSharedMemoryAs<Result*>(result_shm_id, result_shm_offset,
                                        sizeof(*result));
    if (!result) {
      if (buffer != 0) {
        glDeleteBuffersARB(1, &buffer);
      }
      return;
    }
  }
  GLES2Util::ComputeImageDataSizes(width, height, 1, format, type,
                                   pack_alignment, &pixels_size, NULL, NULL);
  void* pixels =
      GetSharedMemoryAs<void*>(pixels_shm_id, pixels_shm_offset, pixels_size);
  if (!pixels) {
    if (buffer != 0) {
      glDeleteBuffersARB(1, &buffer);
    }
    return;
  }

  if (buffer != 0) {
    glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, buffer);
    void* data;
    if (features().map_buffer_range) {
      data = glMapBufferRange(
          GL_PIXEL_PACK_BUFFER_ARB, 0, pixels_size, GL_MAP_READ_BIT);
    } else {
      data = glMapBuffer(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY);
    }
    if (!data) {
      LOCAL_SET_GL_ERROR(GL_OUT_OF_MEMORY, "glMapBuffer",
                         "Unable to map memory for readback.");
      return;
    }
    memcpy(pixels, data, pixels_size);
    glUnmapBuffer(GL_PIXEL_PACK_BUFFER_ARB);
    glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB,
                 GetServiceId(state_.bound_pixel_pack_buffer.get()));
    glDeleteBuffersARB(1, &buffer);
  }

  if (result != NULL) {
    result->success = 1;
  }

  uint32_t channels_exist = GLES2Util::GetChannelsForFormat(read_format);
  if ((channels_exist & 0x0008) == 0 &&
      workarounds().clear_alpha_in_readpixels) {
    // Set the alpha to 255 because some drivers are buggy in this regard.
    uint32_t temp_size;

    uint32_t unpadded_row_size;
    uint32_t padded_row_size;
    if (!GLES2Util::ComputeImageDataSizes(
            width, 2, 1, format, type, pack_alignment, &temp_size,
            &unpadded_row_size, &padded_row_size)) {
      return;
    }

    uint32_t channel_count = 0;
    uint32_t alpha_channel = 0;
    switch (format) {
      case GL_RGBA:
      case GL_BGRA_EXT:
        channel_count = 4;
        alpha_channel = 3;
        break;
      case GL_ALPHA:
        channel_count = 1;
        alpha_channel = 0;
        break;
    }

    if (channel_count > 0) {
      switch (type) {
        case GL_UNSIGNED_BYTE:
          WriteAlphaData<uint8_t>(pixels, height, channel_count, alpha_channel,
                                  unpadded_row_size, padded_row_size, 0xFF);
          break;
        case GL_FLOAT:
          WriteAlphaData<float>(
              pixels, height, channel_count, alpha_channel, unpadded_row_size,
              padded_row_size, 1.0f);
          break;
        case GL_HALF_FLOAT:
          WriteAlphaData<uint16_t>(pixels, height, channel_count, alpha_channel,
                                   unpadded_row_size, padded_row_size, 0x3C00);
          break;
      }
    }
  }
}

error::Error GLES2DecoderImpl::HandleReadPixels(uint32_t immediate_data_size,
                                                const volatile void* cmd_data) {
  const char* func_name = "glReadPixels";
  const volatile gles2::cmds::ReadPixels& c =
      *static_cast<const volatile gles2::cmds::ReadPixels*>(cmd_data);
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::HandleReadPixels");
  error::Error fbo_error = WillAccessBoundFramebufferForRead();
  if (fbo_error != error::kNoError)
    return fbo_error;
  GLint x = c.x;
  GLint y = c.y;
  GLsizei width = c.width;
  GLsizei height = c.height;
  GLenum format = c.format;
  GLenum type = c.type;
  uint32_t pixels_shm_id = c.pixels_shm_id;
  uint32_t pixels_shm_offset = c.pixels_shm_offset;
  uint32_t result_shm_id = c.result_shm_id;
  uint32_t result_shm_offset = c.result_shm_offset;
  GLboolean async = static_cast<GLboolean>(c.async);
  if (width < 0 || height < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "dimensions < 0");
    return error::kNoError;
  }
  typedef cmds::ReadPixels::Result Result;

  PixelStoreParams params;
  if (pixels_shm_id == 0) {
    params = state_.GetPackParams();
  } else {
    // When reading into client buffer, we actually set pack parameters to 0
    // (except for alignment) before calling glReadPixels. This makes sure we
    // only send back meaningful pixel data to the command buffer client side,
    // and the client side will take the responsibility to take the pixels and
    // write to the client buffer according to the full ES3 pack parameters.
    params.alignment = state_.pack_alignment;
  }
  uint32_t pixels_size = 0;
  uint32_t unpadded_row_size = 0;
  uint32_t padded_row_size = 0;
  uint32_t skip_size = 0;
  uint32_t padding = 0;
  if (!GLES2Util::ComputeImageDataSizesES3(width, height, 1,
                                           format, type,
                                           params,
                                           &pixels_size,
                                           &unpadded_row_size,
                                           &padded_row_size,
                                           &skip_size,
                                           &padding)) {
    return error::kOutOfBounds;
  }

  uint8_t* pixels = nullptr;
  Buffer* buffer = state_.bound_pixel_pack_buffer.get();
  if (pixels_shm_id == 0) {
    if (!buffer) {
      return error::kInvalidArguments;
    }
    if (!buffer_manager()->RequestBufferAccess(
            state_.GetErrorState(), buffer, func_name, "pixel pack buffer")) {
      return error::kNoError;
    }
    uint32_t size = 0;
    if (!SafeAddUint32(pixels_size + skip_size, pixels_shm_offset, &size)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "size + offset overflow");
      return error::kNoError;
    }
    if (static_cast<uint32_t>(buffer->size()) < size) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glReadPixels",
                         "pixel pack buffer is not large enough");
      return error::kNoError;
    }
    pixels = reinterpret_cast<uint8_t *>(pixels_shm_offset);
    pixels += skip_size;
  } else {
    if (buffer) {
      return error::kInvalidArguments;
    }
    DCHECK_EQ(0u, skip_size);
    pixels = GetSharedMemoryAs<uint8_t*>(
        pixels_shm_id, pixels_shm_offset, pixels_size);
    if (!pixels) {
      return error::kOutOfBounds;
    }
  }

  Result* result = nullptr;
  if (result_shm_id != 0) {
    result = GetSharedMemoryAs<Result*>(
        result_shm_id, result_shm_offset, sizeof(*result));
    if (!result) {
      return error::kOutOfBounds;
    }
    if (result->success != 0) {
      return error::kInvalidArguments;
    }
  }

  if (!validators_->read_pixel_format.IsValid(format)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(func_name, format, "format");
    return error::kNoError;
  }
  if (!validators_->read_pixel_type.IsValid(type)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(func_name, type, "type");
    return error::kNoError;
  }

  if (!CheckBoundReadFramebufferValid(
          func_name, GL_INVALID_FRAMEBUFFER_OPERATION)) {
    return error::kNoError;
  }
  GLenum src_internal_format = GetBoundReadFramebufferInternalFormat();
  if (src_internal_format == 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name, "no valid color image");
    return error::kNoError;
  }
  std::vector<GLenum> accepted_formats;
  std::vector<GLenum> accepted_types;
  switch (src_internal_format) {
    case GL_R8UI:
    case GL_R16UI:
    case GL_R32UI:
    case GL_RG8UI:
    case GL_RG16UI:
    case GL_RG32UI:
    // All the RGB_INTEGER formats are not renderable.
    case GL_RGBA8UI:
    case GL_RGB10_A2UI:
    case GL_RGBA16UI:
    case GL_RGBA32UI:
      accepted_formats.push_back(GL_RGBA_INTEGER);
      accepted_types.push_back(GL_UNSIGNED_INT);
      break;
    case GL_R8I:
    case GL_R16I:
    case GL_R32I:
    case GL_RG8I:
    case GL_RG16I:
    case GL_RG32I:
    case GL_RGBA8I:
    case GL_RGBA16I:
    case GL_RGBA32I:
      accepted_formats.push_back(GL_RGBA_INTEGER);
      accepted_types.push_back(GL_INT);
      break;
    case GL_RGB10_A2:
      accepted_formats.push_back(GL_RGBA);
      accepted_types.push_back(GL_UNSIGNED_BYTE);
      // Special case with an extra supported format/type.
      accepted_formats.push_back(GL_RGBA);
      accepted_types.push_back(GL_UNSIGNED_INT_2_10_10_10_REV);
      break;
    default:
      accepted_formats.push_back(GL_RGBA);
      {
        GLenum src_type = GetBoundReadFramebufferTextureType();
        switch (src_type) {
          case GL_HALF_FLOAT:
          case GL_HALF_FLOAT_OES:
          case GL_FLOAT:
          case GL_UNSIGNED_INT_10F_11F_11F_REV:
            accepted_types.push_back(GL_FLOAT);
            break;
          default:
            accepted_types.push_back(GL_UNSIGNED_BYTE);
            break;
        }
      }
      break;
  }
  if (!feature_info_->IsWebGLContext()) {
    accepted_formats.push_back(GL_BGRA_EXT);
    accepted_types.push_back(GL_UNSIGNED_BYTE);
  }
  DCHECK_EQ(accepted_formats.size(), accepted_types.size());
  bool format_type_acceptable = false;
  for (size_t ii = 0; ii < accepted_formats.size(); ++ii) {
    if (format == accepted_formats[ii] && type == accepted_types[ii]) {
      format_type_acceptable = true;
      break;
    }
  }
  if (!format_type_acceptable) {
    // format and type are acceptable enums but not guaranteed to be supported
    // for this framebuffer.  Have to ask gl if they are valid.
    GLint preferred_format = 0;
    DoGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &preferred_format, 1);
    GLint preferred_type = 0;
    DoGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &preferred_type, 1);
    if (format == static_cast<GLenum>(preferred_format) &&
        type == static_cast<GLenum>(preferred_type)) {
      format_type_acceptable = true;
    }
  }
  if (!format_type_acceptable) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
        "format and type incompatible with the current read framebuffer");
    return error::kNoError;
  }
  if (type == GL_HALF_FLOAT_OES && !gl_version_info().is_es) {
    type = GL_HALF_FLOAT;
  }
  if (width == 0 || height == 0) {
    return error::kNoError;
  }

  // Get the size of the current fbo or backbuffer.
  gfx::Size max_size = GetBoundReadFramebufferSize();

  int32_t max_x;
  int32_t max_y;
  if (!SafeAddInt32(x, width, &max_x) || !SafeAddInt32(y, height, &max_y)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "dimensions out of range");
    return error::kNoError;
  }

  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER(func_name);

  ScopedResolvedFramebufferBinder binder(this, false, true);
  GLenum read_format = GetBoundReadFramebufferInternalFormat();

  gfx::Rect rect(x, y, width, height);  // Safe before we checked above.
  gfx::Rect max_rect(max_size);
  if (!max_rect.Contains(rect)) {
    rect.Intersect(max_rect);
    if (!rect.IsEmpty()) {
      if (y < 0) {
        pixels += static_cast<uint32_t>(-y) * padded_row_size;;
      }
      if (x < 0) {
        uint32_t group_size = GLES2Util::ComputeImageGroupSize(format, type);
        uint32_t leading_bytes = static_cast<uint32_t>(-x) * group_size;
        pixels += leading_bytes;
      }
      for (GLint iy = rect.y(); iy < rect.bottom(); ++iy) {
        bool reset_row_length = false;
        if (iy + 1 == max_y && pixels_shm_id == 0 &&
            workarounds().pack_parameters_workaround_with_pack_buffer &&
            state_.pack_row_length > 0 && state_.pack_row_length < width) {
          // Some drivers (for example, Mac AMD) incorrecly limit the last
          // row to ROW_LENGTH in this case.
          glPixelStorei(GL_PACK_ROW_LENGTH, width);
          reset_row_length = true;
        }
        glReadPixels(rect.x(), iy, rect.width(), 1, format, type, pixels);
        if (reset_row_length) {
          glPixelStorei(GL_PACK_ROW_LENGTH, state_.pack_row_length);
        }
        pixels += padded_row_size;
      }
    }
  } else {
    if (async && features().use_async_readpixels &&
        !state_.bound_pixel_pack_buffer.get()) {
      // To simply the state tracking, we don't go down the async path if
      // a PIXEL_PACK_BUFFER is bound (in which case the client can
      // implement something similar on their own - all necessary functions
      // should be exposed).
      GLuint buffer = 0;
      glGenBuffersARB(1, &buffer);
      glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, buffer);
      // For ANGLE client version 2, GL_STREAM_READ is not available.
      const GLenum usage_hint =
          gl_version_info().is_angle ? GL_STATIC_DRAW : GL_STREAM_READ;
      glBufferData(GL_PIXEL_PACK_BUFFER_ARB, pixels_size, NULL, usage_hint);
      GLenum error = glGetError();
      if (error == GL_NO_ERROR) {
        // No need to worry about ES3 pixel pack parameters, because no
        // PIXEL_PACK_BUFFER is bound, and all these settings haven't been
        // sent to GL.
        glReadPixels(x, y, width, height, format, type, 0);
        pending_readpixel_fences_.push(FenceCallback());
        WaitForReadPixels(base::Bind(
            &GLES2DecoderImpl::FinishReadPixels, base::AsWeakPtr(this), width,
            height, format, type, pixels_shm_id, pixels_shm_offset,
            result_shm_id, result_shm_offset, state_.pack_alignment,
            read_format, buffer));
        glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 0);
        return error::kNoError;
      } else {
        // On error, unbind pack buffer and fall through to sync readpixels
        glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 0);
        glDeleteBuffersARB(1, &buffer);
      }
    }
    if (pixels_shm_id == 0 &&
        workarounds().pack_parameters_workaround_with_pack_buffer) {
      if (state_.pack_row_length > 0 && state_.pack_row_length < width) {
        // Some drivers (for example, NVidia Linux) reset in this case.
        // Some drivers (for example, Mac AMD) incorrecly limit the last
        // row to ROW_LENGTH in this case.
        glPixelStorei(GL_PACK_ROW_LENGTH, width);
        for (GLint iy = y; iy < max_y; ++iy) {
          // Need to set PACK_ALIGNMENT for last row. See comment below.
          if (iy + 1 == max_y && padding > 0)
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
          glReadPixels(x, iy, width, 1, format, type, pixels);
          if (iy + 1 == max_y && padding > 0)
            glPixelStorei(GL_PACK_ALIGNMENT, state_.pack_alignment);
          pixels += padded_row_size;
        }
        glPixelStorei(GL_PACK_ROW_LENGTH, state_.pack_row_length);
      } else if (padding > 0) {
        // Some drivers (for example, NVidia Linux) incorrectly require the
        // pack buffer to have padding for the last row.
        if (height > 1)
          glReadPixels(x, y, width, height - 1, format, type, pixels);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        pixels += padded_row_size * (height - 1);
        glReadPixels(x, max_y - 1, width, 1, format, type, pixels);
        glPixelStorei(GL_PACK_ALIGNMENT, state_.pack_alignment);
      } else {
        glReadPixels(x, y, width, height, format, type, pixels);
      }
    } else {
      glReadPixels(x, y, width, height, format, type, pixels);
    }
  }
  if (pixels_shm_id != 0) {
    GLenum error = LOCAL_PEEK_GL_ERROR(func_name);
    if (error == GL_NO_ERROR) {
      if (result) {
        result->success = 1;
        result->row_length = static_cast<uint32_t>(rect.width());
        result->num_rows = static_cast<uint32_t>(rect.height());
      }
      FinishReadPixels(width, height, format, type, pixels_shm_id,
                       pixels_shm_offset, result_shm_id, result_shm_offset,
                       state_.pack_alignment, read_format, 0);
    }
  }

  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandlePixelStorei(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::PixelStorei& c =
      *static_cast<const volatile gles2::cmds::PixelStorei*>(cmd_data);
  GLenum pname = c.pname;
  GLint param = c.param;
  if (!validators_->pixel_store.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glPixelStorei", pname, "pname");
    return error::kNoError;
  }
  switch (pname) {
    case GL_PACK_ALIGNMENT:
    case GL_UNPACK_ALIGNMENT:
      if (!validators_->pixel_store_alignment.IsValid(param)) {
        LOCAL_SET_GL_ERROR(
            GL_INVALID_VALUE, "glPixelStorei", "invalid param");
        return error::kNoError;
      }
      break;
    case GL_PACK_ROW_LENGTH:
    case GL_UNPACK_ROW_LENGTH:
    case GL_UNPACK_IMAGE_HEIGHT:
      if (param < 0) {
        LOCAL_SET_GL_ERROR(
            GL_INVALID_VALUE, "glPixelStorei", "invalid param");
        return error::kNoError;
      }
      break;
    case GL_PACK_SKIP_PIXELS:
    case GL_PACK_SKIP_ROWS:
    case GL_UNPACK_SKIP_PIXELS:
    case GL_UNPACK_SKIP_ROWS:
    case GL_UNPACK_SKIP_IMAGES:
      // All SKIP parameters are handled on the client side and should never
      // be passed to the service side.
      return error::kInvalidArguments;
    default:
      break;
  }
  // For alignment parameters, we always apply them.
  // For other parameters, we don't apply them if no buffer is bound at
  // PIXEL_PACK or PIXEL_UNPACK. We will handle pack and unpack according to
  // the user specified parameters on the client side.
  switch (pname) {
    case GL_PACK_ROW_LENGTH:
      if (state_.bound_pixel_pack_buffer.get())
        glPixelStorei(pname, param);
      break;
    case GL_UNPACK_ROW_LENGTH:
    case GL_UNPACK_IMAGE_HEIGHT:
      if (state_.bound_pixel_unpack_buffer.get())
        glPixelStorei(pname, param);
      break;
    default:
      glPixelStorei(pname, param);
      break;
  }
  switch (pname) {
    case GL_PACK_ALIGNMENT:
      state_.pack_alignment = param;
      break;
    case GL_PACK_ROW_LENGTH:
      state_.pack_row_length = param;
      break;
    case GL_UNPACK_ALIGNMENT:
      state_.unpack_alignment = param;
      break;
    case GL_UNPACK_ROW_LENGTH:
      state_.unpack_row_length = param;
      break;
    case GL_UNPACK_IMAGE_HEIGHT:
      state_.unpack_image_height = param;
      break;
    default:
      // Validation should have prevented us from getting here.
      NOTREACHED();
      break;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleSwapBuffersWithDamageCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::SwapBuffersWithDamageCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::SwapBuffersWithDamageCHROMIUM*>(
          cmd_data);
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::SwapBuffersWithDamageCHROMIUM");
  { TRACE_EVENT_SYNTHETIC_DELAY("gpu.PresentingFrame"); }
  if (!supports_swap_buffers_with_damage_) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glSwapBuffersWithDamageCHROMIUM",
                       "command not supported by surface");
    return error::kNoError;
  }
  bool is_tracing;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("gpu.debug"),
                                     &is_tracing);
  if (is_tracing) {
    bool is_offscreen = !!offscreen_target_frame_buffer_.get();
    ScopedFramebufferBinder binder(this, GetBoundDrawFramebufferServiceId());
    gpu_state_tracer_->TakeSnapshotWithCurrentFramebuffer(
        is_offscreen ? offscreen_size_ : surface_->GetSize());
  }

  ClearScheduleCALayerState();

  FinishSwapBuffers(
      surface_->SwapBuffersWithDamage(c.x, c.y, c.width, c.height));

  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandlePostSubBufferCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::PostSubBufferCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::PostSubBufferCHROMIUM*>(
          cmd_data);
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::HandlePostSubBufferCHROMIUM");
  {
    TRACE_EVENT_SYNTHETIC_DELAY("gpu.PresentingFrame");
  }
  if (!supports_post_sub_buffer_) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        "glPostSubBufferCHROMIUM", "command not supported by surface");
    return error::kNoError;
  }
  bool is_tracing;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("gpu.debug"),
                                     &is_tracing);
  if (is_tracing) {
    bool is_offscreen = !!offscreen_target_frame_buffer_.get();
    ScopedFramebufferBinder binder(this, GetBoundDrawFramebufferServiceId());
    gpu_state_tracer_->TakeSnapshotWithCurrentFramebuffer(
        is_offscreen ? offscreen_size_ : surface_->GetSize());
  }

  ClearScheduleCALayerState();

  if (supports_async_swap_) {
    TRACE_EVENT_ASYNC_BEGIN0("cc", "GLES2DecoderImpl::AsyncSwapBuffers", this);
    surface_->PostSubBufferAsync(
        c.x, c.y, c.width, c.height,
        base::Bind(&GLES2DecoderImpl::FinishSwapBuffers,
                   base::AsWeakPtr(this)));
  } else {
    FinishSwapBuffers(surface_->PostSubBuffer(c.x, c.y, c.width, c.height));
  }

  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleScheduleOverlayPlaneCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ScheduleOverlayPlaneCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::ScheduleOverlayPlaneCHROMIUM*>(
          cmd_data);
  TextureRef* ref = texture_manager()->GetTexture(c.overlay_texture_id);
  if (!ref) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE,
                       "glScheduleOverlayPlaneCHROMIUM",
                       "unknown texture");
    return error::kNoError;
  }
  Texture::ImageState image_state;
  gl::GLImage* image =
      ref->texture()->GetLevelImage(ref->texture()->target(), 0, &image_state);
  if (!image) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE,
                       "glScheduleOverlayPlaneCHROMIUM",
                       "unsupported texture format");
    return error::kNoError;
  }
  gfx::OverlayTransform transform = GetGFXOverlayTransform(c.plane_transform);
  if (transform == gfx::OVERLAY_TRANSFORM_INVALID) {
    LOCAL_SET_GL_ERROR(GL_INVALID_ENUM,
                       "glScheduleOverlayPlaneCHROMIUM",
                       "invalid transform enum");
    return error::kNoError;
  }
  if (!surface_->ScheduleOverlayPlane(
          c.plane_z_order,
          transform,
          image,
          gfx::Rect(c.bounds_x, c.bounds_y, c.bounds_width, c.bounds_height),
          gfx::RectF(c.uv_x, c.uv_y, c.uv_width, c.uv_height))) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                       "glScheduleOverlayPlaneCHROMIUM",
                       "failed to schedule overlay");
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleScheduleCALayerSharedStateCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ScheduleCALayerSharedStateCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::ScheduleCALayerSharedStateCHROMIUM*>(
          cmd_data);

  const GLfloat* mem = GetSharedMemoryAs<const GLfloat*>(c.shm_id, c.shm_offset,
                                                         20 * sizeof(GLfloat));
  if (!mem) {
    return error::kOutOfBounds;
  }
  gfx::RectF clip_rect(mem[0], mem[1], mem[2], mem[3]);
  gfx::Transform transform(mem[4], mem[8], mem[12], mem[16],
                           mem[5], mem[9], mem[13], mem[17],
                           mem[6], mem[10], mem[14], mem[18],
                           mem[7], mem[11], mem[15], mem[19]);
  ca_layer_shared_state_.reset(new CALayerSharedState);
  ca_layer_shared_state_->opacity = c.opacity;
  ca_layer_shared_state_->is_clipped = c.is_clipped ? true : false;
  ca_layer_shared_state_->clip_rect = gfx::ToEnclosingRect(clip_rect);
  ca_layer_shared_state_->sorting_context_id = c.sorting_context_id;
  ca_layer_shared_state_->transform = transform;
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleScheduleCALayerCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ScheduleCALayerCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::ScheduleCALayerCHROMIUM*>(
          cmd_data);
  GLuint filter = c.filter;
  if (filter != GL_NEAREST && filter != GL_LINEAR) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glScheduleCALayerCHROMIUM",
                       "invalid filter");
    return error::kNoError;
  }

  if (!ca_layer_shared_state_) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, "glScheduleCALayerCHROMIUM",
        "glScheduleCALayerSharedStateCHROMIUM has not been called");
    return error::kNoError;
  }

  gl::GLImage* image = nullptr;
  GLuint contents_texture_id = c.contents_texture_id;
  if (contents_texture_id) {
    TextureRef* ref = texture_manager()->GetTexture(contents_texture_id);
    if (!ref) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glScheduleCALayerCHROMIUM",
                         "unknown texture");
      return error::kNoError;
    }
    Texture::ImageState image_state;
    image = ref->texture()->GetLevelImage(ref->texture()->target(), 0,
                                          &image_state);
    if (!image) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glScheduleCALayerCHROMIUM",
                         "unsupported texture format");
      return error::kNoError;
    }
  }

  const GLfloat* mem = GetSharedMemoryAs<const GLfloat*>(c.shm_id, c.shm_offset,
                                                         8 * sizeof(GLfloat));
  if (!mem) {
    return error::kOutOfBounds;
  }
  gfx::RectF contents_rect(mem[0], mem[1], mem[2], mem[3]);
  gfx::RectF bounds_rect(mem[4], mem[5], mem[6], mem[7]);

  ui::CARendererLayerParams params = ui::CARendererLayerParams(
      ca_layer_shared_state_->is_clipped, ca_layer_shared_state_->clip_rect,
      ca_layer_shared_state_->sorting_context_id,
      ca_layer_shared_state_->transform, image, contents_rect,
      gfx::ToEnclosingRect(bounds_rect), c.background_color, c.edge_aa_mask,
      ca_layer_shared_state_->opacity, filter);
  if (!surface_->ScheduleCALayer(params)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glScheduleCALayerCHROMIUM",
                       "failed to schedule CALayer");
  }
  return error::kNoError;
}

void GLES2DecoderImpl::DoScheduleCALayerInUseQueryCHROMIUM(
    GLsizei count,
    const volatile GLuint* textures) {
  std::vector<gl::GLSurface::CALayerInUseQuery> queries;
  queries.reserve(count);
  for (GLsizei i = 0; i < count; ++i) {
    gl::GLImage* image = nullptr;
    GLuint texture_id = textures[i];
    if (texture_id) {
      TextureRef* ref = texture_manager()->GetTexture(texture_id);
      if (!ref) {
        LOCAL_SET_GL_ERROR(GL_INVALID_VALUE,
                           "glScheduleCALayerInUseQueryCHROMIUM",
                           "unknown texture");
        return;
      }
      Texture::ImageState image_state;
      image = ref->texture()->GetLevelImage(ref->texture()->target(), 0,
                                            &image_state);
    }
    gl::GLSurface::CALayerInUseQuery query;
    query.image = image;
    query.texture = texture_id;
    queries.push_back(query);
  }

  surface_->ScheduleCALayerInUseQuery(std::move(queries));
}

error::Error GLES2DecoderImpl::GetAttribLocationHelper(
    GLuint client_id,
    uint32_t location_shm_id,
    uint32_t location_shm_offset,
    const std::string& name_str) {
  if (!StringIsValidForGLES(name_str)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glGetAttribLocation", "Invalid character");
    return error::kNoError;
  }
  Program* program = GetProgramInfoNotShader(
      client_id, "glGetAttribLocation");
  if (!program) {
    return error::kNoError;
  }
  if (!program->IsValid()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, "glGetAttribLocation", "program not linked");
    return error::kNoError;
  }
  GLint* location = GetSharedMemoryAs<GLint*>(
      location_shm_id, location_shm_offset, sizeof(GLint));
  if (!location) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (*location != -1) {
    return error::kInvalidArguments;
  }
  *location = program->GetAttribLocation(name_str);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetAttribLocation(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetAttribLocation& c =
      *static_cast<const volatile gles2::cmds::GetAttribLocation*>(cmd_data);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  return GetAttribLocationHelper(
    c.program, c.location_shm_id, c.location_shm_offset, name_str);
}

error::Error GLES2DecoderImpl::HandleGetBufferSubDataAsyncCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context()) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::GetBufferSubDataAsyncCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::GetBufferSubDataAsyncCHROMIUM*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLintptr offset = static_cast<GLintptr>(c.offset);
  GLsizeiptr size = static_cast<GLsizeiptr>(c.size);
  uint32_t data_shm_id = static_cast<uint32_t>(c.data_shm_id);

  int8_t* mem =
      GetSharedMemoryAs<int8_t*>(data_shm_id, c.data_shm_offset, size);
  if (!mem) {
    return error::kOutOfBounds;
  }

  if (!validators_->buffer_target.IsValid(target)) {
    return error::kInvalidArguments;
  }

  Buffer* buffer = buffer_manager()->GetBufferInfoForTarget(&state_, target);
  if (!buffer) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glGetBufferSubDataAsyncCHROMIUM",
                       "no buffer bound to target");
    return error::kNoError;
  }
  if (!buffer->CheckRange(offset, size)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glGetBufferSubDataAsyncCHROMIUM",
                       "invalid range");
    return error::kNoError;
  }

  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("glGetBufferSubDataAsyncCHROMIUM");

  void* ptr = glMapBufferRange(target, offset, size, GL_MAP_READ_BIT);
  if (ptr == nullptr) {
    return error::kInvalidArguments;
  }
  memcpy(mem, ptr, size);
  glUnmapBuffer(target);

  GLenum error = LOCAL_PEEK_GL_ERROR("glGetBufferSubDataAsyncCHROMIUM");
  if (error != GL_NO_ERROR) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::GetUniformLocationHelper(
    GLuint client_id,
    uint32_t location_shm_id,
    uint32_t location_shm_offset,
    const std::string& name_str) {
  if (!StringIsValidForGLES(name_str)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glGetUniformLocation", "Invalid character");
    return error::kNoError;
  }
  Program* program = GetProgramInfoNotShader(
      client_id, "glGetUniformLocation");
  if (!program) {
    return error::kNoError;
  }
  if (!program->IsValid()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, "glGetUniformLocation", "program not linked");
    return error::kNoError;
  }
  GLint* location = GetSharedMemoryAs<GLint*>(
      location_shm_id, location_shm_offset, sizeof(GLint));
  if (!location) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (*location != -1) {
    return error::kInvalidArguments;
  }
  *location = program->GetUniformFakeLocation(name_str);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetUniformLocation(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetUniformLocation& c =
      *static_cast<const volatile gles2::cmds::GetUniformLocation*>(cmd_data);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  return GetUniformLocationHelper(
    c.program, c.location_shm_id, c.location_shm_offset, name_str);
}

error::Error GLES2DecoderImpl::HandleGetUniformIndices(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GetUniformIndices& c =
      *static_cast<const volatile gles2::cmds::GetUniformIndices*>(cmd_data);
  Bucket* bucket = GetBucket(c.names_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  GLsizei count = 0;
  std::vector<char*> names;
  std::vector<GLint> len;
  if (!bucket->GetAsStrings(&count, &names, &len) || count <= 0) {
    return error::kInvalidArguments;
  }
  typedef cmds::GetUniformIndices::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.indices_shm_id, c.indices_shm_offset,
      Result::ComputeSize(static_cast<size_t>(count)));
  GLuint* indices = result ? result->GetData() : NULL;
  if (indices == NULL) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  Program* program = GetProgramInfoNotShader(c.program, "glGetUniformIndices");
  if (!program) {
    return error::kNoError;
  }
  GLuint service_id = program->service_id();
  GLint link_status = GL_FALSE;
  glGetProgramiv(service_id, GL_LINK_STATUS, &link_status);
  if (link_status != GL_TRUE) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
        "glGetUniformIndices", "program not linked");
    return error::kNoError;
  }
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("GetUniformIndices");
  glGetUniformIndices(service_id, count, &names[0], indices);
  GLenum error = glGetError();
  if (error == GL_NO_ERROR) {
    result->SetNumResults(count);
  } else {
    LOCAL_SET_GL_ERROR(error, "GetUniformIndices", "");
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::GetFragDataLocationHelper(
    GLuint client_id,
    uint32_t location_shm_id,
    uint32_t location_shm_offset,
    const std::string& name_str) {
  const char kFunctionName[] = "glGetFragDataLocation";
  GLint* location = GetSharedMemoryAs<GLint*>(
      location_shm_id, location_shm_offset, sizeof(GLint));
  if (!location) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (*location != -1) {
    return error::kInvalidArguments;
  }
  Program* program = GetProgramInfoNotShader(client_id, kFunctionName);
  if (!program) {
    return error::kNoError;
  }
  if (!program->IsValid()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, kFunctionName,
                       "program not linked");
    return error::kNoError;
  }

  *location = program->GetFragDataLocation(name_str);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetFragDataLocation(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GetFragDataLocation& c =
      *static_cast<const volatile gles2::cmds::GetFragDataLocation*>(cmd_data);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  return GetFragDataLocationHelper(
      c.program, c.location_shm_id, c.location_shm_offset, name_str);
}

error::Error GLES2DecoderImpl::GetFragDataIndexHelper(
    GLuint program_id,
    uint32_t index_shm_id,
    uint32_t index_shm_offset,
    const std::string& name_str) {
  const char kFunctionName[] = "glGetFragDataIndexEXT";
  GLint* index =
      GetSharedMemoryAs<GLint*>(index_shm_id, index_shm_offset, sizeof(GLint));
  if (!index) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (*index != -1) {
    return error::kInvalidArguments;
  }
  Program* program = GetProgramInfoNotShader(program_id, kFunctionName);
  if (!program) {
    return error::kNoError;
  }
  if (!program->IsValid()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, kFunctionName,
                       "program not linked");
    return error::kNoError;
  }

  *index = program->GetFragDataIndex(name_str);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetFragDataIndexEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().ext_blend_func_extended) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::GetFragDataIndexEXT& c =
      *static_cast<const volatile gles2::cmds::GetFragDataIndexEXT*>(cmd_data);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  return GetFragDataIndexHelper(c.program, c.index_shm_id, c.index_shm_offset,
                                name_str);
}

error::Error GLES2DecoderImpl::HandleGetUniformBlockIndex(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GetUniformBlockIndex& c =
      *static_cast<const volatile gles2::cmds::GetUniformBlockIndex*>(cmd_data);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  GLuint* index = GetSharedMemoryAs<GLuint*>(
      c.index_shm_id, c.index_shm_offset, sizeof(GLuint));
  if (!index) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (*index != GL_INVALID_INDEX) {
    return error::kInvalidArguments;
  }
  Program* program = GetProgramInfoNotShader(
      c.program, "glGetUniformBlockIndex");
  if (!program) {
    return error::kNoError;
  }
  *index = glGetUniformBlockIndex(program->service_id(), name_str.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetString(uint32_t immediate_data_size,
                                               const volatile void* cmd_data) {
  const volatile gles2::cmds::GetString& c =
      *static_cast<const volatile gles2::cmds::GetString*>(cmd_data);
  GLenum name = static_cast<GLenum>(c.name);
  if (!validators_->string_type.IsValid(name)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetString", name, "name");
    return error::kNoError;
  }

  const char* str = nullptr;
  std::string extensions;
  switch (name) {
    case GL_VERSION:
      str = GetServiceVersionString(feature_info_.get());
      break;
    case GL_SHADING_LANGUAGE_VERSION:
      str = GetServiceShadingLanguageVersionString(feature_info_.get());
      break;
    case GL_RENDERER:
      str = GetServiceRendererString(feature_info_.get());
      break;
    case GL_VENDOR:
      str = GetServiceVendorString(feature_info_.get());
      break;
      break;
    case GL_EXTENSIONS:
      {
        // For WebGL contexts, strip out shader extensions if they have not
        // been enabled on WebGL1 or no longer exist (become core) in WebGL2.
        if (feature_info_->IsWebGLContext()) {
          extensions = feature_info_->extensions();
          if (!derivatives_explicitly_enabled_) {
            size_t offset = extensions.find(kOESDerivativeExtension);
            if (std::string::npos != offset) {
              extensions.replace(offset, arraysize(kOESDerivativeExtension),
                                 std::string());
            }
          }
          if (!frag_depth_explicitly_enabled_) {
            size_t offset = extensions.find(kEXTFragDepthExtension);
            if (std::string::npos != offset) {
              extensions.replace(offset, arraysize(kEXTFragDepthExtension),
                                 std::string());
            }
          }
          if (!draw_buffers_explicitly_enabled_) {
            size_t offset = extensions.find(kEXTDrawBuffersExtension);
            if (std::string::npos != offset) {
              extensions.replace(offset, arraysize(kEXTDrawBuffersExtension),
                                 std::string());
            }
          }
          if (!shader_texture_lod_explicitly_enabled_) {
            size_t offset = extensions.find(kEXTShaderTextureLodExtension);
            if (std::string::npos != offset) {
              extensions.replace(offset,
                                 arraysize(kEXTShaderTextureLodExtension),
                                 std::string());
            }
          }
        } else {
          extensions = feature_info_->extensions().c_str();
        }
        if (supports_post_sub_buffer_)
          extensions += " GL_CHROMIUM_post_sub_buffer";
        str = extensions.c_str();
      }
      break;
    default:
      str = reinterpret_cast<const char*>(glGetString(name));
      break;
  }
  Bucket* bucket = CreateBucket(c.bucket_id);
  bucket->SetFromString(str);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBufferData(uint32_t immediate_data_size,
                                                const volatile void* cmd_data) {
  const volatile gles2::cmds::BufferData& c =
      *static_cast<const volatile gles2::cmds::BufferData*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLsizeiptr size = static_cast<GLsizeiptr>(c.size);
  uint32_t data_shm_id = static_cast<uint32_t>(c.data_shm_id);
  uint32_t data_shm_offset = static_cast<uint32_t>(c.data_shm_offset);
  GLenum usage = static_cast<GLenum>(c.usage);
  const void* data = NULL;
  if (data_shm_id != 0 || data_shm_offset != 0) {
    data = GetSharedMemoryAs<const void*>(data_shm_id, data_shm_offset, size);
    if (!data) {
      return error::kOutOfBounds;
    }
  }
  buffer_manager()->ValidateAndDoBufferData(&state_, target, size, data, usage);
  return error::kNoError;
}

void GLES2DecoderImpl::DoBufferSubData(
  GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid * data) {
  // Just delegate it. Some validation is actually done before this.
  buffer_manager()->ValidateAndDoBufferSubData(
      &state_, target, offset, size, data);
}

bool GLES2DecoderImpl::ClearLevel(Texture* texture,
                                  unsigned target,
                                  int level,
                                  unsigned format,
                                  unsigned type,
                                  int xoffset,
                                  int yoffset,
                                  int width,
                                  int height) {
  DCHECK(target != GL_TEXTURE_3D && target != GL_TEXTURE_2D_ARRAY &&
         target != GL_TEXTURE_EXTERNAL_OES);
  uint32_t channels = GLES2Util::GetChannelsForFormat(format);
  if ((channels & GLES2Util::kDepth) != 0 &&
      feature_info_->feature_flags().angle_depth_texture &&
      feature_info_->gl_version_info().is_es2) {
    // It's a depth format and ANGLE doesn't allow texImage2D or texSubImage2D
    // on depth formats in ES2.
    GLuint fb = 0;
    glGenFramebuffersEXT(1, &fb);
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, fb);

    glFramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT,
                              target, texture->service_id(), level);
    bool have_stencil = (channels & GLES2Util::kStencil) != 0;
    if (have_stencil) {
      glFramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT,
                                target, texture->service_id(), level);
    }

    // ANGLE promises a depth only attachment ok.
    if (glCheckFramebufferStatusEXT(GL_DRAW_FRAMEBUFFER_EXT) !=
        GL_FRAMEBUFFER_COMPLETE) {
      return false;
    }
    glClearStencil(0);
    state_.SetDeviceStencilMaskSeparate(GL_FRONT, kDefaultStencilMask);
    state_.SetDeviceStencilMaskSeparate(GL_BACK, kDefaultStencilMask);
    glClearDepth(1.0f);
    state_.SetDeviceDepthMask(GL_TRUE);
    state_.SetDeviceCapabilityState(GL_SCISSOR_TEST, true);
    glScissor(xoffset, yoffset, width, height);
    glClear(GL_DEPTH_BUFFER_BIT | (have_stencil ? GL_STENCIL_BUFFER_BIT : 0));

    RestoreClearState();

    glDeleteFramebuffersEXT(1, &fb);
    Framebuffer* framebuffer =
        GetFramebufferInfoForTarget(GL_DRAW_FRAMEBUFFER_EXT);
    GLuint fb_service_id =
        framebuffer ? framebuffer->service_id() : GetBackbufferServiceId();
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, fb_service_id);
    return true;
  }

  const uint32_t kMaxZeroSize = 1024 * 1024 * 4;

  uint32_t size;
  uint32_t padded_row_size;
  if (!GLES2Util::ComputeImageDataSizes(
          width, height, 1, format, type, state_.unpack_alignment, &size,
          NULL, &padded_row_size)) {
    return false;
  }

  TRACE_EVENT1("gpu", "GLES2DecoderImpl::ClearLevel", "size", size);

  int tile_height;

  if (size > kMaxZeroSize) {
    if (kMaxZeroSize < padded_row_size) {
      // That'd be an awfully large texture.
      return false;
    }
    // We should never have a large total size with a zero row size.
    DCHECK_GT(padded_row_size, 0U);
    tile_height = kMaxZeroSize / padded_row_size;
    if (!GLES2Util::ComputeImageDataSizes(
            width, tile_height, 1, format, type, state_.unpack_alignment, &size,
            NULL, NULL)) {
      return false;
    }
  } else {
    tile_height = height;
  }

  {
    // Add extra scope to destroy zero and the object it owns right
    // after its usage.
    // Assumes the size has already been checked.
    std::unique_ptr<char[]> zero(new char[size]);
    memset(zero.get(), 0, size);
    glBindTexture(texture->target(), texture->service_id());

    GLint y = 0;
    while (y < height) {
      GLint h = y + tile_height > height ? height - y : tile_height;
      glTexSubImage2D(
        target, level, xoffset, yoffset + y, width, h,
        TextureManager::AdjustTexFormat(feature_info_.get(), format),
        type,
        zero.get());
      y += tile_height;
    }
  }
  TextureRef* bound_texture =
      texture_manager()->GetTextureInfoForTarget(&state_, texture->target());
  glBindTexture(texture->target(),
                bound_texture ? bound_texture->service_id() : 0);
  return true;
}

bool GLES2DecoderImpl::ClearCompressedTextureLevel(Texture* texture,
                                                   unsigned target,
                                                   int level,
                                                   unsigned format,
                                                   int width,
                                                   int height) {
  DCHECK(target != GL_TEXTURE_3D && target != GL_TEXTURE_2D_ARRAY);
  // This code path can only be called if the texture was originally
  // allocated via TexStorage2D. Note that TexStorage2D is exposed
  // internally for ES 2.0 contexts, but compressed texture support is
  // not part of that exposure.
  DCHECK(feature_info_->IsWebGL2OrES3Context());

  GLsizei bytes_required = 0;
  if (!GetCompressedTexSizeInBytes(
          "ClearCompressedTextureLevel", width, height, 1, format,
          &bytes_required)) {
    return false;
  }

  TRACE_EVENT1("gpu", "GLES2DecoderImpl::ClearCompressedTextureLevel",
               "bytes_required", bytes_required);

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  {
    // Add extra scope to destroy zero and the object it owns right
    // after its usage.
    std::unique_ptr<char[]> zero(new char[bytes_required]);
    memset(zero.get(), 0, bytes_required);
    glBindTexture(texture->target(), texture->service_id());
    glCompressedTexSubImage2D(
      target, level, 0, 0, width, height, format, bytes_required, zero.get());
  }
  TextureRef* bound_texture =
      texture_manager()->GetTextureInfoForTarget(&state_, texture->target());
  glBindTexture(texture->target(),
                bound_texture ? bound_texture->service_id() : 0);
  Buffer* bound_buffer = buffer_manager()->GetBufferInfoForTarget(
      &state_, GL_PIXEL_UNPACK_BUFFER);
  if (bound_buffer) {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, bound_buffer->service_id());
  }
  return true;
}

bool GLES2DecoderImpl::IsCompressedTextureFormat(unsigned format) {
  return feature_info_->validators()->compressed_texture_format.IsValid(
      format);
}

bool GLES2DecoderImpl::ClearLevel3D(Texture* texture,
                                    unsigned target,
                                    int level,
                                    unsigned format,
                                    unsigned type,
                                    int width,
                                    int height,
                                    int depth) {
  DCHECK(target == GL_TEXTURE_3D || target == GL_TEXTURE_2D_ARRAY);
  DCHECK(feature_info_->IsWebGL2OrES3Context());
  if (width == 0 || height == 0 || depth == 0)
    return true;

  uint32_t size;
  uint32_t padded_row_size;
  uint32_t padding;
  // Here we use unpack buffer to upload zeros into the texture, one layer
  // at a time.
  // We only take into consideration UNPACK_ALIGNMENT, and clear other unpack
  // parameters if necessary before TexSubImage3D calls.
  PixelStoreParams params;
  params.alignment = state_.unpack_alignment;
  if (!GLES2Util::ComputeImageDataSizesES3(width, height, depth,
                                           format, type,
                                           params,
                                           &size,
                                           nullptr,
                                           &padded_row_size,
                                           nullptr,
                                           &padding)) {
    return false;
  }
  const uint32_t kMaxZeroSize = 1024 * 1024 * 2;
  uint32_t buffer_size;
  std::vector<TexSubCoord3D> subs;
  if (size < kMaxZeroSize) {
    // Case 1: one TexSubImage3D call clears the entire 3D texture.
    buffer_size = size;
    subs.push_back(TexSubCoord3D(0, 0, 0, width, height, depth));
  } else {
    uint32_t size_per_layer;
    if (!SafeMultiplyUint32(padded_row_size, height, &size_per_layer)) {
      return false;
    }
    if (size_per_layer < kMaxZeroSize) {
      // Case 2: Each TexSubImage3D call clears 1 or more layers.
      uint32_t depth_step = kMaxZeroSize / size_per_layer;
      uint32_t num_of_slices = depth / depth_step;
      if (num_of_slices * depth_step < static_cast<uint32_t>(depth))
        num_of_slices++;
      DCHECK_LT(0u, num_of_slices);
      buffer_size = size_per_layer * depth_step;
      int depth_of_last_slice = depth - (num_of_slices - 1) * depth_step;
      DCHECK_LT(0, depth_of_last_slice);
      for (uint32_t ii = 0; ii < num_of_slices; ++ii) {
        int depth_ii =
            (ii + 1 == num_of_slices ? depth_of_last_slice : depth_step);
        subs.push_back(
            TexSubCoord3D(0, 0, depth_step * ii, width, height, depth_ii));
      }
    } else {
      // Case 3: Multiple TexSubImage3D calls clear 1 layer.
      if (kMaxZeroSize < padded_row_size) {
        // That'd be an awfully large texture.
        return false;
      }
      uint32_t height_step = kMaxZeroSize / padded_row_size;
      uint32_t num_of_slices = height / height_step;
      if (num_of_slices * height_step < static_cast<uint32_t>(height))
        num_of_slices++;
      DCHECK_LT(0u, num_of_slices);
      buffer_size = padded_row_size * height_step;
      int height_of_last_slice = height - (num_of_slices - 1) * height_step;
      DCHECK_LT(0, height_of_last_slice);
      for (int zz = 0; zz < depth; ++zz) {
        for (uint32_t ii = 0; ii < num_of_slices; ++ii) {
          int height_ii =
              (ii + 1 == num_of_slices ? height_of_last_slice : height_step);
          subs.push_back(
              TexSubCoord3D(0, height_step * ii, zz, width, height_ii, 1));
        }
      }
    }
  }

  TRACE_EVENT1("gpu", "GLES2DecoderImpl::ClearLevel3D", "size", size);

  GLuint buffer_id = 0;
  glGenBuffersARB(1, &buffer_id);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer_id);
  {
    // Include padding as some drivers incorrectly requires padding for the
    // last row.
    buffer_size += padding;
    std::unique_ptr<char[]> zero(new char[buffer_size]);
    memset(zero.get(), 0, buffer_size);
    // TODO(zmo): Consider glMapBufferRange instead.
    glBufferData(
        GL_PIXEL_UNPACK_BUFFER, buffer_size, zero.get(), GL_STATIC_DRAW);
  }

  Buffer* bound_buffer = buffer_manager()->GetBufferInfoForTarget(
      &state_, GL_PIXEL_UNPACK_BUFFER);
  if (bound_buffer) {
    // If an unpack buffer is bound, we need to clear unpack parameters
    // because they have been applied to the driver.
    if (state_.unpack_row_length > 0)
      glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    if (state_.unpack_image_height > 0)
      glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
    DCHECK_EQ(0, state_.unpack_skip_pixels);
    DCHECK_EQ(0, state_.unpack_skip_rows);
    DCHECK_EQ(0, state_.unpack_skip_images);
  }

  glBindTexture(texture->target(), texture->service_id());

  for (size_t ii = 0; ii < subs.size(); ++ii) {
    glTexSubImage3D(target, level, subs[ii].xoffset, subs[ii].yoffset,
                    subs[ii].zoffset, subs[ii].width, subs[ii].height,
                    subs[ii].depth, format, type, nullptr);
  }

  if (bound_buffer) {
    if (state_.unpack_row_length > 0)
      glPixelStorei(GL_UNPACK_ROW_LENGTH, state_.unpack_row_length);
    if (state_.unpack_image_height > 0)
      glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, state_.unpack_image_height);
  }

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER,
               bound_buffer ? bound_buffer->service_id() : 0);
  glDeleteBuffersARB(1, &buffer_id);

  TextureRef* bound_texture =
      texture_manager()->GetTextureInfoForTarget(&state_, texture->target());
  glBindTexture(texture->target(),
                bound_texture ? bound_texture->service_id() : 0);
  return true;
}

namespace {

const int kASTCBlockSize = 16;
const int kS3TCBlockWidth = 4;
const int kS3TCBlockHeight = 4;
const int kS3TCDXT1BlockSize = 8;
const int kS3TCDXT3AndDXT5BlockSize = 16;
const int kEACAndETC2BlockSize = 4;

typedef struct {
  int blockWidth;
  int blockHeight;
} ASTCBlockArray;

const ASTCBlockArray kASTCBlockArray[] = {
    {4, 4},   /* GL_COMPRESSED_RGBA_ASTC_4x4_KHR */
    {5, 4},   /* and GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR */
    {5, 5},
    {6, 5},
    {6, 6},
    {8, 5},
    {8, 6},
    {8, 8},
    {10, 5},
    {10, 6},
    {10, 8},
    {10, 10},
    {12, 10},
    {12, 12}};

bool CheckETCFormatSupport(const FeatureInfo& feature_info) {
  const gl::GLVersionInfo& version_info = feature_info.gl_version_info();
  return version_info.IsAtLeastGL(4, 3) || version_info.IsAtLeastGLES(3, 0) ||
         feature_info.feature_flags().arb_es3_compatibility;
}

using CompressedFormatSupportCheck = bool (*)(const FeatureInfo&);
using CompressedFormatDecompressionFunction = void (*)(size_t width,
                                                       size_t height,
                                                       size_t depth,
                                                       const uint8_t* input,
                                                       size_t inputRowPitch,
                                                       size_t inputDepthPitch,
                                                       uint8_t* output,
                                                       size_t outputRowPitch,
                                                       size_t outputDepthPitch);

struct CompressedFormatInfo {
  GLenum format;
  uint32_t block_size;
  uint32_t bytes_per_block;
  CompressedFormatSupportCheck support_check;
  CompressedFormatDecompressionFunction decompression_function;
  GLenum decompressed_internal_format;
  GLenum decompressed_format;
  GLenum decompressed_type;
};

const CompressedFormatInfo kCompressedFormatInfoArray[] = {
    {
        GL_COMPRESSED_R11_EAC, 4, 8, CheckETCFormatSupport,
        angle::LoadEACR11ToR8, GL_R8, GL_RED, GL_UNSIGNED_BYTE,
    },
    {
        GL_COMPRESSED_SIGNED_R11_EAC, 4, 8, CheckETCFormatSupport,
        angle::LoadEACR11SToR8, GL_R8_SNORM, GL_RED, GL_BYTE,
    },
    {
        GL_COMPRESSED_RG11_EAC, 4, 16, CheckETCFormatSupport,
        angle::LoadEACRG11ToRG8, GL_RG8, GL_RG, GL_UNSIGNED_BYTE,
    },
    {
        GL_COMPRESSED_SIGNED_RG11_EAC, 4, 16, CheckETCFormatSupport,
        angle::LoadEACRG11SToRG8, GL_RG8_SNORM, GL_RG, GL_BYTE,
    },
    {
        GL_COMPRESSED_RGB8_ETC2, 4, 8, CheckETCFormatSupport,
        angle::LoadETC2RGB8ToRGBA8, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE,
    },
    {
        GL_COMPRESSED_SRGB8_ETC2, 4, 8, CheckETCFormatSupport,
        angle::LoadETC2SRGB8ToRGBA8, GL_SRGB8_ALPHA8, GL_SRGB_ALPHA,
        GL_UNSIGNED_BYTE,
    },
    {
        GL_COMPRESSED_RGBA8_ETC2_EAC, 4, 16, CheckETCFormatSupport,
        angle::LoadETC2RGBA8ToRGBA8, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE,
    },
    {
        GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2, 4, 8,
        CheckETCFormatSupport, angle::LoadETC2RGB8A1ToRGBA8, GL_RGBA8, GL_RGBA,
        GL_UNSIGNED_BYTE,
    },
    {
        GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC, 4, 16, CheckETCFormatSupport,
        angle::LoadETC2SRGBA8ToSRGBA8, GL_SRGB8_ALPHA8, GL_SRGB_ALPHA,
        GL_UNSIGNED_BYTE,
    },
    {
        GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2, 4, 8,
        CheckETCFormatSupport, angle::LoadETC2SRGB8A1ToRGBA8, GL_SRGB8_ALPHA8,
        GL_SRGB_ALPHA, GL_UNSIGNED_BYTE,
    },
};

const CompressedFormatInfo* GetCompressedFormatInfo(GLenum format) {
  for (size_t i = 0; i < arraysize(kCompressedFormatInfoArray); i++) {
    if (kCompressedFormatInfoArray[i].format == format) {
      return &kCompressedFormatInfoArray[i];
    }
  }
  return nullptr;
}

uint32_t GetCompressedFormatRowPitch(const CompressedFormatInfo& info,
                                     uint32_t width) {
  uint32_t num_blocks_wide = (width + info.block_size - 1) / info.block_size;
  return num_blocks_wide * info.bytes_per_block;
}

uint32_t GetCompressedFormatDepthPitch(const CompressedFormatInfo& info,
                                       uint32_t width,
                                       uint32_t height) {
  uint32_t num_blocks_high = (height + info.block_size - 1) / info.block_size;
  return num_blocks_high * GetCompressedFormatRowPitch(info, width);
}

std::unique_ptr<uint8_t[]> DecompressTextureData(
    const ContextState& state,
    const CompressedFormatInfo& info,
    uint32_t width,
    uint32_t height,
    uint32_t depth,
    GLsizei image_size,
    const void* data) {
  uint32_t output_pixel_size = GLES2Util::ComputeImageGroupSize(
      info.decompressed_format, info.decompressed_type);
  std::unique_ptr<uint8_t[]> decompressed_data(
      new uint8_t[output_pixel_size * width * height]);

  // If a PBO is bound, map it to decompress the data.
  const void* input_data = data;
  if (state.bound_pixel_unpack_buffer) {
    input_data = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER,
                                  reinterpret_cast<GLintptr>(data), image_size,
                                  GL_MAP_READ_BIT);
    if (input_data == nullptr) {
      LOG(ERROR) << "Failed to map pixel unpack buffer.";
      return nullptr;
    }
  }

  DCHECK_NE(input_data, nullptr);
  info.decompression_function(
      width, height, depth, static_cast<const uint8_t*>(input_data),
      GetCompressedFormatRowPitch(info, width),
      GetCompressedFormatDepthPitch(info, width, height),
      decompressed_data.get(), output_pixel_size * width,
      output_pixel_size * width * height);

  if (state.bound_pixel_unpack_buffer) {
    if (glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER) != GL_TRUE) {
      LOG(ERROR) << "glUnmapBuffer unexpectedly returned GL_FALSE";
      return nullptr;
    }
  }

  return decompressed_data;
}

bool IsValidS3TCSizeForWebGL(GLint level, GLsizei size) {
  // WebGL only allows multiple-of-4 sizes, except for levels > 0 where it also
  // allows 1 or 2. See WEBGL_compressed_texture_s3tc.
  return (level && size == 1) ||
         (level && size == 2) ||
         !(size % kS3TCBlockWidth);
}

bool IsValidPVRTCSize(GLint level, GLsizei size) {
  return GLES2Util::IsPOT(size);
}

}  // anonymous namespace.

bool GLES2DecoderImpl::GetCompressedTexSizeInBytes(
    const char* function_name, GLsizei width, GLsizei height, GLsizei depth,
    GLenum format, GLsizei* size_in_bytes) {
  base::CheckedNumeric<GLsizei> bytes_required(0);

  switch (format) {
    case GL_ATC_RGB_AMD:
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
    case GL_ETC1_RGB8_OES:
      bytes_required =
          (width + kS3TCBlockWidth - 1) / kS3TCBlockWidth;
      bytes_required *=
          (height + kS3TCBlockHeight - 1) / kS3TCBlockHeight;
      bytes_required *= kS3TCDXT1BlockSize;
      break;
    case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
    case GL_COMPRESSED_RGBA_ASTC_5x4_KHR:
    case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_6x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:
    case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_8x6_KHR:
    case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x6_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x8_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:
    case GL_COMPRESSED_RGBA_ASTC_12x10_KHR:
    case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR: {
      const int index = (format < GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR) ?
          static_cast<int>(format - GL_COMPRESSED_RGBA_ASTC_4x4_KHR) :
          static_cast<int>(format - GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR);

      const int kBlockWidth = kASTCBlockArray[index].blockWidth;
      const int kBlockHeight = kASTCBlockArray[index].blockHeight;

      bytes_required =
          (width + kBlockWidth - 1) / kBlockWidth;
      bytes_required *=
          (height + kBlockHeight - 1) / kBlockHeight;

      bytes_required *= kASTCBlockSize;
      break;
    }
    case GL_ATC_RGBA_EXPLICIT_ALPHA_AMD:
    case GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD:
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
      bytes_required =
          (width + kS3TCBlockWidth - 1) / kS3TCBlockWidth;
      bytes_required *=
          (height + kS3TCBlockHeight - 1) / kS3TCBlockHeight;
      bytes_required *= kS3TCDXT3AndDXT5BlockSize;
      break;
    case GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG:
    case GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:
      bytes_required = std::max(width, 8);
      bytes_required *= std::max(height, 8);
      bytes_required *= 4;
      bytes_required += 7;
      bytes_required /= 8;
      break;
    case GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG:
    case GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG:
      bytes_required = std::max(width, 16);
      bytes_required *= std::max(height, 8);
      bytes_required *= 2;
      bytes_required += 7;
      bytes_required /= 8;
      break;

    // ES3 formats.
    case GL_COMPRESSED_R11_EAC:
    case GL_COMPRESSED_SIGNED_R11_EAC:
    case GL_COMPRESSED_RGB8_ETC2:
    case GL_COMPRESSED_SRGB8_ETC2:
    case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
      bytes_required =
          (width + kEACAndETC2BlockSize - 1) / kEACAndETC2BlockSize;
      bytes_required *=
          (height + kEACAndETC2BlockSize - 1) / kEACAndETC2BlockSize;
      bytes_required *= 8;
      bytes_required *= depth;
      break;
    case GL_COMPRESSED_RG11_EAC:
    case GL_COMPRESSED_SIGNED_RG11_EAC:
    case GL_COMPRESSED_RGBA8_ETC2_EAC:
    case GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
      bytes_required =
          (width + kEACAndETC2BlockSize - 1) / kEACAndETC2BlockSize;
      bytes_required *=
          (height + kEACAndETC2BlockSize - 1) / kEACAndETC2BlockSize;
      bytes_required *= 16;
      bytes_required *= depth;
      break;
    default:
      LOCAL_SET_GL_ERROR_INVALID_ENUM(function_name, format, "format");
      return false;
  }

  if (!bytes_required.IsValid()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "invalid size");
    return false;
  }

  *size_in_bytes = bytes_required.ValueOrDefault(0);
  return true;
}

bool GLES2DecoderImpl::ValidateCompressedTexFuncData(const char* function_name,
                                                     GLsizei width,
                                                     GLsizei height,
                                                     GLsizei depth,
                                                     GLenum format,
                                                     GLsizei size,
                                                     const GLvoid* data) {
  GLsizei bytes_required = 0;
  if (!GetCompressedTexSizeInBytes(
          function_name, width, height, depth, format, &bytes_required)) {
    return false;
  }

  if (size != bytes_required) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, function_name, "size is not correct for dimensions");
    return false;
  }

  Buffer* buffer = state_.bound_pixel_unpack_buffer.get();
  if (buffer &&
      !buffer_manager()->RequestBufferAccess(
          state_.GetErrorState(), buffer, reinterpret_cast<GLintptr>(data),
          static_cast<GLsizeiptr>(bytes_required), function_name,
          "pixel unpack buffer")) {
    return false;
  }

  return true;
}

bool GLES2DecoderImpl::ValidateCompressedTexDimensions(
    const char* function_name, GLenum target, GLint level,
    GLsizei width, GLsizei height, GLsizei depth, GLenum format) {
  switch (format) {
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
    case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
      DCHECK_EQ(1, depth);  // 2D formats.
      if (feature_info_->IsWebGLContext() &&
          (!IsValidS3TCSizeForWebGL(level, width) ||
           !IsValidS3TCSizeForWebGL(level, height))) {
        LOCAL_SET_GL_ERROR(
            GL_INVALID_OPERATION, function_name,
            "width or height invalid for level");
        return false;
      }
      return true;
    case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
    case GL_COMPRESSED_RGBA_ASTC_5x4_KHR:
    case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_6x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:
    case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_8x6_KHR:
    case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x6_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x8_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:
    case GL_COMPRESSED_RGBA_ASTC_12x10_KHR:
    case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR:
    case GL_ATC_RGB_AMD:
    case GL_ATC_RGBA_EXPLICIT_ALPHA_AMD:
    case GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD:
    case GL_ETC1_RGB8_OES:
      DCHECK_EQ(1, depth);  // 2D formats.
      if (width <= 0 || height <= 0) {
        LOCAL_SET_GL_ERROR(
            GL_INVALID_OPERATION, function_name,
            "width or height invalid for level");
        return false;
      }
      return true;
    case GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG:
    case GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG:
    case GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:
    case GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG:
      DCHECK_EQ(1, depth);  // 2D formats.
      if (!IsValidPVRTCSize(level, width) ||
          !IsValidPVRTCSize(level, height)) {
        LOCAL_SET_GL_ERROR(
            GL_INVALID_OPERATION, function_name,
            "width or height invalid for level");
        return false;
      }
      return true;

    // ES3 formats.
    case GL_COMPRESSED_R11_EAC:
    case GL_COMPRESSED_SIGNED_R11_EAC:
    case GL_COMPRESSED_RG11_EAC:
    case GL_COMPRESSED_SIGNED_RG11_EAC:
    case GL_COMPRESSED_RGB8_ETC2:
    case GL_COMPRESSED_SRGB8_ETC2:
    case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case GL_COMPRESSED_RGBA8_ETC2_EAC:
    case GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
      if (width < 0 || height < 0 || depth < 0) {
        LOCAL_SET_GL_ERROR(
            GL_INVALID_OPERATION, function_name,
            "width, height, or depth invalid");
        return false;
      }
      if (target == GL_TEXTURE_3D) {
        LOCAL_SET_GL_ERROR(
            GL_INVALID_OPERATION, function_name,
            "target invalid for format");
        return false;
      }
      return true;
    default:
      return false;
  }
}

bool GLES2DecoderImpl::ValidateCompressedTexSubDimensions(
    const char* function_name,
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
    GLsizei width, GLsizei height, GLsizei depth, GLenum format,
    Texture* texture) {
  if (xoffset < 0 || yoffset < 0 || zoffset < 0) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, function_name, "x/y/z offset < 0");
    return false;
  }

  switch (format) {
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
    case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT: {
      const int kBlockWidth = 4;
      const int kBlockHeight = 4;
      if ((xoffset % kBlockWidth) || (yoffset % kBlockHeight)) {
        LOCAL_SET_GL_ERROR(
            GL_INVALID_OPERATION, function_name,
            "xoffset or yoffset not multiple of 4");
        return false;
      }
      GLsizei tex_width = 0;
      GLsizei tex_height = 0;
      if (!texture->GetLevelSize(target, level,
                                 &tex_width, &tex_height, nullptr) ||
          width - xoffset > tex_width ||
          height - yoffset > tex_height) {
        LOCAL_SET_GL_ERROR(
            GL_INVALID_OPERATION, function_name, "dimensions out of range");
        return false;
      }
      if ((((width % kBlockWidth) != 0) && (width + xoffset != tex_width)) ||
          (((height % kBlockHeight) != 0) &&
           (height + yoffset != tex_height))) {
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                           "dimensions do not align to a block boundary");
        return false;
      }
      return true;
    }
    case GL_ATC_RGB_AMD:
    case GL_ATC_RGBA_EXPLICIT_ALPHA_AMD:
    case GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD: {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_OPERATION, function_name,
          "not supported for ATC textures");
      return false;
    }
    case GL_ETC1_RGB8_OES: {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_OPERATION, function_name,
          "not supported for ECT1_RGB8_OES textures");
      return false;
    }
    case GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG:
    case GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG:
    case GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:
    case GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG: {
      if ((xoffset != 0) || (yoffset != 0)) {
        LOCAL_SET_GL_ERROR(
            GL_INVALID_OPERATION, function_name,
            "xoffset and yoffset must be zero");
        return false;
      }
      GLsizei tex_width = 0;
      GLsizei tex_height = 0;
      if (!texture->GetLevelSize(target, level,
                                 &tex_width, &tex_height, nullptr) ||
          width != tex_width ||
          height != tex_height) {
        LOCAL_SET_GL_ERROR(
            GL_INVALID_OPERATION, function_name,
            "dimensions must match existing texture level dimensions");
        return false;
      }
      return ValidateCompressedTexDimensions(
          function_name, target, level, width, height, 1, format);
    }

    // ES3 formats
    case GL_COMPRESSED_R11_EAC:
    case GL_COMPRESSED_SIGNED_R11_EAC:
    case GL_COMPRESSED_RG11_EAC:
    case GL_COMPRESSED_SIGNED_RG11_EAC:
    case GL_COMPRESSED_RGB8_ETC2:
    case GL_COMPRESSED_SRGB8_ETC2:
    case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case GL_COMPRESSED_RGBA8_ETC2_EAC:
    case GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
      {
        const int kBlockSize = 4;
        GLsizei tex_width, tex_height;
        if (target == GL_TEXTURE_3D ||
            !texture->GetLevelSize(target, level,
                                   &tex_width, &tex_height, nullptr) ||
            (xoffset % kBlockSize) || (yoffset % kBlockSize) ||
            ((width % kBlockSize) && xoffset + width != tex_width) ||
            ((height % kBlockSize) && yoffset + height != tex_height)) {
          LOCAL_SET_GL_ERROR(
              GL_INVALID_OPERATION, function_name,
              "dimensions must match existing texture level dimensions");
          return false;
        }
        return true;
      }
    default:
      return false;
  }
}

error::Error GLES2DecoderImpl::HandleCompressedTexImage2DBucket(
    uint32_t immediate_data_size, const volatile void* cmd_data) {
  const volatile gles2::cmds::CompressedTexImage2DBucket& c =
      *static_cast<const volatile gles2::cmds::CompressedTexImage2DBucket*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLenum internal_format = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLuint bucket_id = static_cast<GLuint>(c.bucket_id);
  GLint border = static_cast<GLint>(c.border);

  if (state_.bound_pixel_unpack_buffer.get()) {
    return error::kInvalidArguments;
  }
  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket)
    return error::kInvalidArguments;
  uint32_t image_size = bucket->size();
  const void* data = bucket->GetData(0, image_size);
  DCHECK(data || !image_size);
  return DoCompressedTexImage(target, level, internal_format, width, height, 1,
                              border, image_size, data, ContextState::k2D);
}

error::Error GLES2DecoderImpl::HandleCompressedTexImage2D(
    uint32_t immediate_data_size, const volatile void* cmd_data) {
  const volatile gles2::cmds::CompressedTexImage2D& c =
      *static_cast<const volatile gles2::cmds::CompressedTexImage2D*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLenum internal_format = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLint border = static_cast<GLint>(c.border);
  GLsizei image_size = static_cast<GLsizei>(c.imageSize);
  uint32_t data_shm_id = c.data_shm_id;
  uint32_t data_shm_offset = c.data_shm_offset;

  const void* data;
  if (state_.bound_pixel_unpack_buffer.get()) {
    if (data_shm_id) {
      return error::kInvalidArguments;
    }
    data = reinterpret_cast<const void*>(data_shm_offset);
  } else {
    if (!data_shm_id && data_shm_offset) {
      return error::kInvalidArguments;
    }
    data = GetSharedMemoryAs<const void*>(
        data_shm_id, data_shm_offset, image_size);
  }
  return DoCompressedTexImage(target, level, internal_format, width, height, 1,
                              border, image_size, data, ContextState::k2D);
}

error::Error GLES2DecoderImpl::HandleCompressedTexImage3DBucket(
    uint32_t immediate_data_size, const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::CompressedTexImage3DBucket& c =
      *static_cast<const volatile gles2::cmds::CompressedTexImage3DBucket*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLenum internal_format = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLsizei depth = static_cast<GLsizei>(c.depth);
  GLuint bucket_id = static_cast<GLuint>(c.bucket_id);
  GLint border = static_cast<GLint>(c.border);

  if (state_.bound_pixel_unpack_buffer.get()) {
    return error::kInvalidArguments;
  }
  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket)
    return error::kInvalidArguments;
  uint32_t image_size = bucket->size();
  const void* data = bucket->GetData(0, image_size);
  DCHECK(data || !image_size);
  return DoCompressedTexImage(target, level, internal_format, width, height,
                              depth, border, image_size, data,
                              ContextState::k3D);
}

error::Error GLES2DecoderImpl::HandleCompressedTexImage3D(
    uint32_t immediate_data_size, const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::CompressedTexImage3D& c =
      *static_cast<const volatile gles2::cmds::CompressedTexImage3D*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLenum internal_format = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLsizei depth = static_cast<GLsizei>(c.depth);
  GLint border = static_cast<GLint>(c.border);
  GLsizei image_size = static_cast<GLsizei>(c.imageSize);
  uint32_t data_shm_id = c.data_shm_id;
  uint32_t data_shm_offset = c.data_shm_offset;

  const void* data;
  if (state_.bound_pixel_unpack_buffer.get()) {
    if (data_shm_id) {
      return error::kInvalidArguments;
    }
    data = reinterpret_cast<const void*>(data_shm_offset);
  } else {
    if (!data_shm_id && data_shm_offset) {
      return error::kInvalidArguments;
    }
    data = GetSharedMemoryAs<const void*>(
        data_shm_id, data_shm_offset, image_size);
  }
  return DoCompressedTexImage(target, level, internal_format, width, height,
                              depth, border, image_size, data,
                              ContextState::k3D);
}

error::Error GLES2DecoderImpl::HandleCompressedTexSubImage3DBucket(
    uint32_t immediate_data_size, const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::CompressedTexSubImage3DBucket& c =
      *static_cast<const volatile gles2::cmds::CompressedTexSubImage3DBucket*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLint zoffset = static_cast<GLint>(c.zoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLsizei depth = static_cast<GLsizei>(c.depth);
  GLenum format = static_cast<GLenum>(c.format);
  GLuint bucket_id = static_cast<GLuint>(c.bucket_id);

  if (state_.bound_pixel_unpack_buffer.get()) {
    return error::kInvalidArguments;
  }
  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket)
    return error::kInvalidArguments;
  uint32_t image_size = bucket->size();
  const void* data = bucket->GetData(0, image_size);
  DCHECK(data || !image_size);
  return DoCompressedTexSubImage(target, level, xoffset, yoffset, zoffset,
                                 width, height, depth, format, image_size,
                                 data, ContextState::k3D);
}

error::Error GLES2DecoderImpl::HandleCompressedTexSubImage3D(
    uint32_t immediate_data_size, const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::CompressedTexSubImage3D& c =
      *static_cast<const volatile gles2::cmds::CompressedTexSubImage3D*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLint zoffset = static_cast<GLint>(c.zoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLsizei depth = static_cast<GLsizei>(c.depth);
  GLenum format = static_cast<GLenum>(c.format);
  GLsizei image_size = static_cast<GLsizei>(c.imageSize);
  uint32_t data_shm_id = c.data_shm_id;
  uint32_t data_shm_offset = c.data_shm_offset;

  const void* data;
  if (state_.bound_pixel_unpack_buffer.get()) {
    if (data_shm_id) {
      return error::kInvalidArguments;
    }
    data = reinterpret_cast<const void*>(data_shm_offset);
  } else {
    if (!data_shm_id && data_shm_offset) {
      return error::kInvalidArguments;
    }
    data = GetSharedMemoryAs<const void*>(
        data_shm_id, data_shm_offset, image_size);
  }
  return DoCompressedTexSubImage(target, level, xoffset, yoffset, zoffset,
                                 width, height, depth, format, image_size,
                                 data, ContextState::k3D);
}

error::Error GLES2DecoderImpl::DoCompressedTexImage(
    GLenum target, GLint level, GLenum internal_format, GLsizei width,
    GLsizei height, GLsizei depth, GLint border, GLsizei image_size,
    const void* data, ContextState::Dimension dimension) {
  const char* func_name;
  if (dimension == ContextState::k2D) {
    func_name = "glCompressedTexImage2D";
    if (!validators_->texture_target.IsValid(target)) {
      LOCAL_SET_GL_ERROR_INVALID_ENUM(func_name, target, "target");
      return error::kNoError;
    }
    // TODO(ccameron): Add a separate texture from |texture_target| for
    // [Compressed]Tex[Sub]Image2D and related functions.
    // http://crbug.com/536854
    if (target == GL_TEXTURE_RECTANGLE_ARB) {
      LOCAL_SET_GL_ERROR_INVALID_ENUM(func_name, target, "target");
      return error::kNoError;
    }
  } else {
    DCHECK_EQ(ContextState::k3D, dimension);
    func_name = "glCompressedTexImage3D";
    if (!validators_->texture_3_d_target.IsValid(target)) {
      LOCAL_SET_GL_ERROR_INVALID_ENUM(func_name, target, "target");
      return error::kNoError;
    }
  }
  if (!validators_->compressed_texture_format.IsValid(internal_format)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(
        func_name, internal_format, "internalformat");
    return error::kNoError;
  }
  if (image_size < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "imageSize < 0");
    return error::kNoError;
  }
  if (!texture_manager()->ValidForTarget(target, level, width, height, depth) ||
      border != 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "dimensions out of range");
    return error::kNoError;
  }
  TextureRef* texture_ref = texture_manager()->GetTextureInfoForTarget(
      &state_, target);
  if (!texture_ref) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, func_name, "no texture bound at target");
    return error::kNoError;
  }
  Texture* texture = texture_ref->texture();
  if (texture->IsImmutable()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name, "texture is immutable");
    return error::kNoError;
  }

  if (!ValidateCompressedTexDimensions(func_name, target, level,
                                       width, height, depth, internal_format) ||
      !ValidateCompressedTexFuncData(func_name, width, height, depth,
                                     internal_format, image_size, data)) {
    return error::kNoError;
  }

  if (!EnsureGPUMemoryAvailable(image_size)) {
    LOCAL_SET_GL_ERROR(GL_OUT_OF_MEMORY, func_name, "out of memory");
    return error::kNoError;
  }

  if (texture->IsAttachedToFramebuffer()) {
    framebuffer_state_.clear_state_dirty = true;
  }

  std::unique_ptr<int8_t[]> zero;
  if (!state_.bound_pixel_unpack_buffer && !data) {
    zero.reset(new int8_t[image_size]);
    memset(zero.get(), 0, image_size);
    data = zero.get();
  }
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER(func_name);
  const CompressedFormatInfo* format_info =
      GetCompressedFormatInfo(internal_format);
  if (format_info != nullptr && !format_info->support_check(*feature_info_)) {
    std::unique_ptr<uint8_t[]> decompressed_data = DecompressTextureData(
        state_, *format_info, width, height, depth, image_size, data);
    if (!decompressed_data) {
      MarkContextLost(error::kGuilty);
      group_->LoseContexts(error::kInnocent);
      return error::kLostContext;
    }
    state_.PushTextureDecompressionUnpackState();
    if (dimension == ContextState::k2D) {
      glTexImage2D(target, level, format_info->decompressed_internal_format,
                   width, height, border, format_info->decompressed_format,
                   format_info->decompressed_type, decompressed_data.get());
    } else {
      glTexImage3D(
          target, level, format_info->decompressed_internal_format,
          width, height, depth, border, format_info->decompressed_format,
          format_info->decompressed_type, decompressed_data.get());
    }
    state_.RestoreUnpackState();
  } else {
    if (dimension == ContextState::k2D) {
      glCompressedTexImage2D(target, level, internal_format, width, height,
                             border, image_size, data);
    } else {
      glCompressedTexImage3D(target, level, internal_format, width, height,
                             depth, border, image_size, data);
    }
  }
  GLenum error = LOCAL_PEEK_GL_ERROR(func_name);
  if (error == GL_NO_ERROR) {
    texture_manager()->SetLevelInfo(texture_ref, target, level, internal_format,
                                    width, height, depth, border, 0, 0,
                                    gfx::Rect(width, height));
  }

  // This may be a slow command.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTexImage2D(uint32_t immediate_data_size,
                                                const volatile void* cmd_data) {
  const char* func_name = "glTexImage2D";
  const volatile gles2::cmds::TexImage2D& c =
      *static_cast<const volatile gles2::cmds::TexImage2D*>(cmd_data);
  TRACE_EVENT2("gpu", "GLES2DecoderImpl::HandleTexImage2D",
      "width", c.width, "height", c.height);
  // Set as failed for now, but if it successed, this will be set to not failed.
  texture_state_.tex_image_failed = true;
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint internal_format = static_cast<GLint>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLint border = static_cast<GLint>(c.border);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum type = static_cast<GLenum>(c.type);
  uint32_t pixels_shm_id = static_cast<uint32_t>(c.pixels_shm_id);
  uint32_t pixels_shm_offset = static_cast<uint32_t>(c.pixels_shm_offset);

  if (width < 0 || height < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "dimensions < 0");
    return error::kNoError;
  }

  PixelStoreParams params;
  Buffer* buffer = state_.bound_pixel_unpack_buffer.get();
  if (buffer) {
    if (pixels_shm_id)
      return error::kInvalidArguments;
    if (buffer->GetMappedRange()) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
          "pixel unpack buffer should not be mapped to client memory");
      return error::kNoError;
    }
    params = state_.GetUnpackParams(ContextState::k2D);
  } else {
    if (!pixels_shm_id && pixels_shm_offset)
      return error::kInvalidArguments;
    // When reading from client buffer, the command buffer client side took
    // the responsibility to take the pixels from the client buffer and
    // unpack them according to the full ES3 pack parameters as source, all
    // parameters for 0 (except for alignment) as destination mem for the
    // service side.
    params.alignment = state_.unpack_alignment;
  }
  uint32_t pixels_size;
  uint32_t skip_size;
  uint32_t padding;
  if (!GLES2Util::ComputeImageDataSizesES3(width, height, 1,
                                           format, type,
                                           params,
                                           &pixels_size,
                                           nullptr,
                                           nullptr,
                                           &skip_size,
                                           &padding)) {
    return error::kOutOfBounds;
  }
  DCHECK_EQ(0u, skip_size);

  const void* pixels;
  if (pixels_shm_id) {
    pixels = GetSharedMemoryAs<const void*>(
        pixels_shm_id, pixels_shm_offset, pixels_size);
    if (!pixels)
      return error::kOutOfBounds;
  } else {
    pixels = reinterpret_cast<const void*>(pixels_shm_offset);
  }

  // For testing only. Allows us to stress the ability to respond to OOM errors.
  if (workarounds().simulate_out_of_memory_on_large_textures &&
      (width * height >= 4096 * 4096)) {
    LOCAL_SET_GL_ERROR(GL_OUT_OF_MEMORY, func_name, "synthetic out of memory");
    return error::kNoError;
  }

  TextureManager::DoTexImageArguments args = {
    target, level, internal_format, width, height, 1, border, format, type,
    pixels, pixels_size, padding,
    TextureManager::DoTexImageArguments::kTexImage2D };
  texture_manager()->ValidateAndDoTexImage(
      &texture_state_, &state_, &framebuffer_state_, func_name, args);

  // This may be a slow command.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTexImage3D(uint32_t immediate_data_size,
                                                const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;

  const char* func_name = "glTexImage3D";
  const volatile gles2::cmds::TexImage3D& c =
      *static_cast<const volatile gles2::cmds::TexImage3D*>(cmd_data);
  TRACE_EVENT2("gpu", "GLES2DecoderImpl::HandleTexImage3D",
      "widthXheight", c.width * c.height, "depth", c.depth);
  // Set as failed for now, but if it successed, this will be set to not failed.
  texture_state_.tex_image_failed = true;
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint internal_format = static_cast<GLint>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLsizei depth = static_cast<GLsizei>(c.depth);
  GLint border = static_cast<GLint>(c.border);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum type = static_cast<GLenum>(c.type);
  uint32_t pixels_shm_id = static_cast<uint32_t>(c.pixels_shm_id);
  uint32_t pixels_shm_offset = static_cast<uint32_t>(c.pixels_shm_offset);

  if (width < 0 || height < 0 || depth < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "dimensions < 0");
    return error::kNoError;
  }

  PixelStoreParams params;
  Buffer* buffer = state_.bound_pixel_unpack_buffer.get();
  if (buffer) {
    if (pixels_shm_id)
      return error::kInvalidArguments;
    if (buffer->GetMappedRange()) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
          "pixel unpack buffer should not be mapped to client memory");
      return error::kNoError;
    }
    params = state_.GetUnpackParams(ContextState::k3D);
  } else {
    if (!pixels_shm_id && pixels_shm_offset)
      return error::kInvalidArguments;
    // When reading from client buffer, the command buffer client side took
    // the responsibility to take the pixels from the client buffer and
    // unpack them according to the full ES3 pack parameters as source, all
    // parameters for 0 (except for alignment) as destination mem for the
    // service side.
    params.alignment = state_.unpack_alignment;
  }
  uint32_t pixels_size;
  uint32_t skip_size;
  uint32_t padding;
  if (!GLES2Util::ComputeImageDataSizesES3(width, height, depth,
                                           format, type,
                                           params,
                                           &pixels_size,
                                           nullptr,
                                           nullptr,
                                           &skip_size,
                                           &padding)) {
    return error::kOutOfBounds;
  }
  DCHECK_EQ(0u, skip_size);

  const void* pixels;
  if (pixels_shm_id) {
    pixels = GetSharedMemoryAs<const void*>(
        pixels_shm_id, pixels_shm_offset, pixels_size);
    if (!pixels)
      return error::kOutOfBounds;
  } else {
    pixels = reinterpret_cast<const void*>(pixels_shm_offset);
  }

  // For testing only. Allows us to stress the ability to respond to OOM errors.
  if (workarounds().simulate_out_of_memory_on_large_textures &&
      (width * height * depth >= 4096 * 4096)) {
    LOCAL_SET_GL_ERROR(GL_OUT_OF_MEMORY, func_name, "synthetic out of memory");
    return error::kNoError;
  }

  TextureManager::DoTexImageArguments args = {
    target, level, internal_format, width, height, depth, border, format, type,
    pixels, pixels_size, padding,
    TextureManager::DoTexImageArguments::kTexImage3D };
  texture_manager()->ValidateAndDoTexImage(
      &texture_state_, &state_, &framebuffer_state_, func_name, args);

  // This may be a slow command.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCompressedTexSubImage2DBucket(
    uint32_t immediate_data_size, const volatile void* cmd_data) {
  const volatile gles2::cmds::CompressedTexSubImage2DBucket& c =
      *static_cast<const volatile gles2::cmds::CompressedTexSubImage2DBucket*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLenum format = static_cast<GLenum>(c.format);
  GLuint bucket_id = static_cast<GLuint>(c.bucket_id);

  if (state_.bound_pixel_unpack_buffer.get()) {
    return error::kInvalidArguments;
  }
  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket)
    return error::kInvalidArguments;
  uint32_t image_size = bucket->size();
  const void* data = bucket->GetData(0, image_size);
  DCHECK(data || !image_size);
  return DoCompressedTexSubImage(target, level, xoffset, yoffset, 0,
                                 width, height, 1, format, image_size, data,
                                 ContextState::k2D);
}

error::Error GLES2DecoderImpl::HandleCompressedTexSubImage2D(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::CompressedTexSubImage2D& c =
      *static_cast<const volatile gles2::cmds::CompressedTexSubImage2D*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLenum format = static_cast<GLenum>(c.format);
  GLsizei image_size = static_cast<GLsizei>(c.imageSize);
  uint32_t data_shm_id = c.data_shm_id;
  uint32_t data_shm_offset = c.data_shm_offset;

  const void* data;
  if (state_.bound_pixel_unpack_buffer.get()) {
    if (data_shm_id) {
      return error::kInvalidArguments;
    }
    data = reinterpret_cast<const void*>(data_shm_offset);
  } else {
    if (!data_shm_id && data_shm_offset) {
      return error::kInvalidArguments;
    }
    data = GetSharedMemoryAs<const void*>(
        data_shm_id, data_shm_offset, image_size);
  }
  return DoCompressedTexSubImage(target, level, xoffset, yoffset, 0,
                                 width, height, 1, format, image_size, data,
                                 ContextState::k2D);
}

error::Error GLES2DecoderImpl::DoCompressedTexSubImage(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
    GLsizei width, GLsizei height, GLsizei depth, GLenum format,
    GLsizei image_size, const void * data, ContextState::Dimension dimension) {
  const char* func_name;
  if (dimension == ContextState::k2D) {
    func_name = "glCompressedTexSubImage2D";
    if (!validators_->texture_target.IsValid(target)) {
      LOCAL_SET_GL_ERROR_INVALID_ENUM(func_name, target, "target");
      return error::kNoError;
    }
  } else {
    DCHECK_EQ(ContextState::k3D, dimension);
    func_name = "glCompressedTexSubImage3D";
    if (!validators_->texture_3_d_target.IsValid(target)) {
      LOCAL_SET_GL_ERROR_INVALID_ENUM(func_name, target, "target");
      return error::kNoError;
    }
  }
  if (!validators_->compressed_texture_format.IsValid(format)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(func_name, format, "format");
    return error::kNoError;
  }
  if (image_size < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "imageSize < 0");
    return error::kNoError;
  }
  if (!texture_manager()->ValidForTarget(target, level, width, height, depth)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "dimensions out of range");
    return error::kNoError;
  }
  TextureRef* texture_ref = texture_manager()->GetTextureInfoForTarget(
      &state_, target);
  if (!texture_ref) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, func_name, "no texture bound at target");
    return error::kNoError;
  }
  Texture* texture = texture_ref->texture();
  GLenum type = 0;
  GLenum internal_format = 0;
  if (!texture->GetLevelType(target, level, &type, &internal_format)) {
    std::string msg = base::StringPrintf("level %d does not exist", level);
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name, msg.c_str());
    return error::kNoError;
  }
  if (internal_format != format) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
                       "format does not match internalformat.");
    return error::kNoError;
  }
  if (!texture->ValidForTexture(target, level, xoffset, yoffset, zoffset,
                                width, height, depth)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "bad dimensions.");
    return error::kNoError;
  }

  if (!ValidateCompressedTexFuncData(
          func_name, width, height, depth, format, image_size, data) ||
      !ValidateCompressedTexSubDimensions(
          func_name, target, level, xoffset, yoffset, zoffset,
          width, height, depth, format, texture)) {
    return error::kNoError;
  }

  if (!texture->IsLevelCleared(target, level)) {
    // This can only happen if the compressed texture was allocated
    // using TexStorage{2|3}D.
    DCHECK(texture->IsImmutable());
    GLsizei level_width = 0, level_height = 0, level_depth = 0;
    bool success = texture->GetLevelSize(
        target, level, &level_width, &level_height, &level_depth);
    DCHECK(success);
    if (xoffset == 0 && width == level_width &&
        yoffset == 0 && height == level_height &&
        zoffset == 0 && depth == level_depth) {
      // We can skip the clear if we're uploading the entire level.
      texture_manager()->SetLevelCleared(texture_ref, target, level, true);
    } else {
      texture_manager()->ClearTextureLevel(this, texture_ref, target, level);
    }
    DCHECK(texture->IsLevelCleared(target, level));
  }

  const CompressedFormatInfo* format_info =
      GetCompressedFormatInfo(internal_format);
  if (format_info != nullptr && !format_info->support_check(*feature_info_)) {
    std::unique_ptr<uint8_t[]> decompressed_data = DecompressTextureData(
        state_, *format_info, width, height, depth, image_size, data);
    if (!decompressed_data) {
      MarkContextLost(error::kGuilty);
      group_->LoseContexts(error::kInnocent);
      return error::kLostContext;
    }
    state_.PushTextureDecompressionUnpackState();
    if (dimension == ContextState::k2D) {
      glTexSubImage2D(target, level, xoffset, yoffset, width, height,
                      format_info->decompressed_format,
                      format_info->decompressed_type, decompressed_data.get());
    } else {
      glTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height,
                      depth, format_info->decompressed_format,
                      format_info->decompressed_type, decompressed_data.get());
    }
    state_.RestoreUnpackState();
  } else {
    if (dimension == ContextState::k2D) {
      glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height,
                                format, image_size, data);
    } else {
      glCompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width,
                                height, depth, format, image_size, data);
    }
  }

  // This may be a slow command.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();
  return error::kNoError;
}

bool GLES2DecoderImpl::ValidateCopyTexFormat(
    const char* func_name, GLenum internal_format,
    GLenum read_format, GLenum read_type) {
  if (read_format == 0) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, func_name, "no valid color image");
    return false;
  }
  // Check we have compatible formats.
  uint32_t channels_exist = GLES2Util::GetChannelsForFormat(read_format);
  uint32_t channels_needed = GLES2Util::GetChannelsForFormat(internal_format);
  if (!channels_needed ||
      (channels_needed & channels_exist) != channels_needed) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, func_name, "incompatible format");
    return false;
  }
  if (feature_info_->IsWebGL2OrES3Context()) {
    GLint color_encoding = GetColorEncodingFromInternalFormat(read_format);
    bool float_mismatch= feature_info_->ext_color_buffer_float_available() ?
        (GLES2Util::IsIntegerFormat(internal_format) !=
         GLES2Util::IsIntegerFormat(read_format)) :
        GLES2Util::IsFloatFormat(internal_format);
    if (color_encoding != GetColorEncodingFromInternalFormat(internal_format) ||
        float_mismatch ||
        (GLES2Util::IsSignedIntegerFormat(internal_format) !=
         GLES2Util::IsSignedIntegerFormat(read_format)) ||
        (GLES2Util::IsUnsignedIntegerFormat(internal_format) !=
         GLES2Util::IsUnsignedIntegerFormat(read_format))) {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_OPERATION, func_name, "incompatible format");
      return false;
    }
  }
  if ((channels_needed & (GLES2Util::kDepth | GLES2Util::kStencil)) != 0) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        func_name, "can not be used with depth or stencil textures");
    return false;
  }
  if (feature_info_->IsWebGL2OrES3Context()) {
    if (GLES2Util::IsSizedColorFormat(internal_format)) {
      int sr, sg, sb, sa;
      GLES2Util::GetColorFormatComponentSizes(
          read_format, read_type, &sr, &sg, &sb, &sa);
      DCHECK(sr > 0 || sg > 0 || sb > 0 || sa > 0);
      int dr, dg, db, da;
      GLES2Util::GetColorFormatComponentSizes(
          internal_format, 0, &dr, &dg, &db, &da);
      DCHECK(dr > 0 || dg > 0 || db > 0 || da > 0);
      if ((dr > 0 && sr != dr) ||
          (dg > 0 && sg != dg) ||
          (db > 0 && sb != db) ||
          (da > 0 && sa != da)) {
        LOCAL_SET_GL_ERROR(
            GL_INVALID_OPERATION,
            func_name, "incompatible color component sizes");
        return false;
      }
    }
  }
  return true;
}

void GLES2DecoderImpl::DoCopyTexImage2D(
    GLenum target,
    GLint level,
    GLenum internal_format,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLint border) {
  const char* func_name = "glCopyTexImage2D";
  DCHECK(!ShouldDeferReads());
  TextureRef* texture_ref = texture_manager()->GetTextureInfoForTarget(
      &state_, target);
  if (!texture_ref) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, func_name, "unknown texture for target");
    return;
  }
  Texture* texture = texture_ref->texture();
  if (texture->IsImmutable()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, func_name, "texture is immutable");
    return;
  }
  if (!texture_manager()->ValidForTarget(target, level, width, height, 1) ||
      border != 0) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, func_name, "dimensions out of range");
    return;
  }

  if (!CheckBoundReadFramebufferValid(func_name,
                                      GL_INVALID_FRAMEBUFFER_OPERATION)) {
    return;
  }

  GLenum read_format = GetBoundReadFramebufferInternalFormat();
  GLenum read_type = GetBoundReadFramebufferTextureType();
  if (!ValidateCopyTexFormat(func_name, internal_format,
                             read_format, read_type)) {
    return;
  }

  uint32_t pixels_size = 0;
  // TODO(piman): OpenGL ES 3.0.4 Section 3.8.5 specifies how to pick an
  // effective internal format if internal_format is unsized, which is a fairly
  // involved logic. For now, just make sure we pick something valid.
  GLenum format =
      TextureManager::ExtractFormatFromStorageFormat(internal_format);
  GLenum type = TextureManager::ExtractTypeFromStorageFormat(internal_format);
  bool internal_format_unsized = internal_format == format;
  // The picks made by the temporary logic above may not be valid on ES3.
  // This if-block checks the temporary logic and should be removed with it.
  if (internal_format_unsized && feature_info_->IsWebGL2OrES3Context()) {
    // While there are other possible types for unsized formats (cf. OpenGL ES
    // 3.0.5, section 3.7, table 3.3, page 113), they won't appear here.
    // ExtractTypeFromStorageFormat will always return UNSIGNED_BYTE for
    // unsized formats.
    DCHECK(type == GL_UNSIGNED_BYTE);
    switch (internal_format) {
      case GL_RGB:
      case GL_RGBA:
      case GL_LUMINANCE_ALPHA:
      case GL_LUMINANCE:
      case GL_ALPHA:
      case GL_BGRA_EXT:
        break;
      default:
        // Other unsized internal_formats are invalid in ES3.
        format = GL_NONE;
        break;
    }
  }
  if (!format || !type) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        func_name, "Invalid unsized internal format.");
    return;
  }

  DCHECK(texture_manager()->ValidateTextureParameters(
      GetErrorState(), func_name, true, format, type, internal_format, level));

  // Only target image size is validated here.
  if (!GLES2Util::ComputeImageDataSizes(
      width, height, 1, format, type,
      state_.unpack_alignment, &pixels_size, NULL, NULL)) {
    LOCAL_SET_GL_ERROR(GL_OUT_OF_MEMORY, func_name, "dimensions too large");
    return;
  }

  if (!EnsureGPUMemoryAvailable(pixels_size)) {
    LOCAL_SET_GL_ERROR(GL_OUT_OF_MEMORY, func_name, "out of memory");
    return;
  }

  if (FormsTextureCopyingFeedbackLoop(texture_ref, level, 0)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        func_name, "source and destination textures are the same");
    return;
  }

  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER(func_name);
  ScopedResolvedFramebufferBinder binder(this, false, true);
  gfx::Size size = GetBoundReadFramebufferSize();

  if (texture->IsAttachedToFramebuffer()) {
    framebuffer_state_.clear_state_dirty = true;
  }

  bool requires_luma_blit =
      CopyTexImageResourceManager::CopyTexImageRequiresBlit(feature_info_.get(),
                                                            format);
  if (requires_luma_blit &&
      !InitializeCopyTexImageBlitter(func_name)) {
    return;
  }

  // Clip to size to source dimensions
  gfx::Rect src(x, y, width, height);
  const gfx::Rect dst(0, 0, size.width(), size.height());
  src.Intersect(dst);

  if (src.x() != x || src.y() != y ||
      src.width() != width || src.height() != height) {
    {
      // Add extra scope to destroy zero and the object it owns right
      // after its usage.
      // some part was clipped so clear the rect.

      std::unique_ptr<char[]> zero(new char[pixels_size]);
      memset(zero.get(), 0, pixels_size);
      glTexImage2D(target, level, TextureManager::AdjustTexInternalFormat(
        feature_info_.get(), internal_format),
        width, height, border, format, type, zero.get());
    }

    if (!src.IsEmpty()) {
      GLint destX = src.x() - x;
      GLint destY = src.y() - y;
      if (requires_luma_blit) {
        copy_tex_image_blit_->DoCopyTexSubImageToLUMACompatibilityTexture(
            this, texture->service_id(), texture->target(), target, format,
            type, level, destX, destY, 0,
            src.x(), src.y(), src.width(), src.height(),
            GetBoundReadFramebufferServiceId(),
            GetBoundReadFramebufferInternalFormat());
      } else {
        glCopyTexSubImage2D(target, level, destX, destY,
                            src.x(), src.y(), src.width(), src.height());
      }
    }
  } else {
    GLenum final_internal_format = TextureManager::AdjustTexInternalFormat(
        feature_info_.get(), internal_format);
    if (workarounds().init_two_cube_map_levels_before_copyteximage &&
        texture->target() == GL_TEXTURE_CUBE_MAP &&
        target != GL_TEXTURE_CUBE_MAP_POSITIVE_X) {
      for (int i = 0; i < 2; ++i) {
        TextureManager::DoTexImageArguments args = {
          target, i, final_internal_format, 1, 1, 1, border,
          format, type, nullptr, 1, 0,
          TextureManager::DoTexImageArguments::kTexImage2D };
        texture_manager()->WorkaroundCopyTexImageCubeMap(&texture_state_,
            &state_, &framebuffer_state_, texture_ref, func_name, args);
      }
    }

    // The service id and target of the texture attached to READ_FRAMEBUFFER.
    GLuint source_texture_service_id = 0;
    GLenum source_texture_target = 0;
    uint32_t channels_exist = GLES2Util::GetChannelsForFormat(read_format);
    bool use_workaround = NeedsCopyTextureImageWorkaround(
        final_internal_format, channels_exist, &source_texture_service_id,
        &source_texture_target);
    if (requires_luma_blit) {
      copy_tex_image_blit_->DoCopyTexImage2DToLUMACompatibilityTexture(
          this, texture->service_id(), texture->target(), target, format,
          type, level, internal_format, x, y, width, height,
          GetBoundReadFramebufferServiceId(),
          GetBoundReadFramebufferInternalFormat());
    } else if (use_workaround) {
      GLenum dest_texture_target = target;
      GLenum framebuffer_target = GetReadFramebufferTarget();

      GLenum temp_internal_format = 0;
      if (channels_exist == GLES2Util::kRGBA) {
        temp_internal_format = GL_RGBA;
      } else if (channels_exist == GLES2Util::kRGB) {
        temp_internal_format = GL_RGB;
      } else {
        NOTREACHED();
      }

      GLuint temp_texture;
      {
        // Copy from the read framebuffer into |temp_texture|.
        glGenTextures(1, &temp_texture);
        ScopedTextureBinder binder(&state_, temp_texture,
                                   source_texture_target);
        glCopyTexImage2D(source_texture_target, 0, temp_internal_format,
                         x, y, width, height, border);

        // Attach the temp texture to the read framebuffer.
        glFramebufferTexture2DEXT(framebuffer_target, GL_COLOR_ATTACHMENT0,
                                  source_texture_target, temp_texture, 0);
      }

      // Copy to the final texture.
      DCHECK_EQ(static_cast<GLuint>(GL_TEXTURE_2D), dest_texture_target);
      glCopyTexImage2D(dest_texture_target, level, final_internal_format, 0, 0,
                       width, height, 0);

      // Rebind source texture.
      glFramebufferTexture2DEXT(framebuffer_target, GL_COLOR_ATTACHMENT0,
                                source_texture_target,
                                source_texture_service_id, 0);

      glDeleteTextures(1, &temp_texture);
    } else {
      if (workarounds().init_one_cube_map_level_before_copyteximage &&
          texture->target() == GL_TEXTURE_CUBE_MAP &&
          target != GL_TEXTURE_CUBE_MAP_POSITIVE_X) {
        TextureManager::DoTexImageArguments args = {
          target, level, final_internal_format, width, height, 1, border,
          format, type, nullptr, pixels_size, 0,
          TextureManager::DoTexImageArguments::kTexImage2D };
        texture_manager()->WorkaroundCopyTexImageCubeMap(&texture_state_,
            &state_, &framebuffer_state_, texture_ref, func_name, args);
      }
      glCopyTexImage2D(target, level, final_internal_format,
                       x, y, width, height, border);
    }
  }
  GLenum error = LOCAL_PEEK_GL_ERROR(func_name);
  if (error == GL_NO_ERROR) {
    texture_manager()->SetLevelInfo(texture_ref, target, level, internal_format,
                                    width, height, 1, border, format,
                                    type, gfx::Rect(width, height));
    texture->ApplyFormatWorkarounds(feature_info_.get());
  }

  // This may be a slow command.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();
}

void GLES2DecoderImpl::DoCopyTexSubImage2D(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height) {
  const char* func_name = "glCopyTexSubImage2D";
  DCHECK(!ShouldDeferReads());
  TextureRef* texture_ref = texture_manager()->GetTextureInfoForTarget(
      &state_, target);
  if (!texture_ref) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, func_name, "unknown texture for target");
    return;
  }
  Texture* texture = texture_ref->texture();
  GLenum type = 0;
  GLenum internal_format = 0;
  if (!texture->GetLevelType(target, level, &type, &internal_format) ||
      !texture->ValidForTexture(
          target, level, xoffset, yoffset, 0, width, height, 1)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "bad dimensions.");
    return;
  }

  if (!CheckBoundReadFramebufferValid(func_name,
                                      GL_INVALID_FRAMEBUFFER_OPERATION)) {
    return;
  }

  GLenum read_format = GetBoundReadFramebufferInternalFormat();
  GLenum read_type = GetBoundReadFramebufferTextureType();
  if (!ValidateCopyTexFormat(func_name, internal_format,
                             read_format, read_type)) {
    return;
  }

  if (FormsTextureCopyingFeedbackLoop(texture_ref, level, 0)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        func_name, "source and destination textures are the same");
    return;
  }

  ScopedResolvedFramebufferBinder binder(this, false, true);
  gfx::Size size = GetBoundReadFramebufferSize();

  gfx::Rect src(x, y, width, height);
  const gfx::Rect dst(0, 0, size.width(), size.height());
  src.Intersect(dst);

  if (src.IsEmpty())
    return;

  GLint dx = src.x() - x;
  GLint dy = src.y() - y;
  GLint destX = xoffset + dx;
  GLint destY = yoffset + dy;
  // It's only legal to skip clearing the level of the target texture
  // if the entire level is being redefined.
  GLsizei level_width = 0;
  GLsizei level_height = 0;
  GLsizei level_depth = 0;
  bool have_level = texture->GetLevelSize(
      target, level, &level_width, &level_height, &level_depth);
  // Validated above.
  DCHECK(have_level);
  if (destX == 0 && destY == 0 &&
      src.width() == level_width && src.height() == level_height) {
    // Write all pixels in below.
    texture_manager()->SetLevelCleared(texture_ref, target, level, true);
  } else {
    gfx::Rect cleared_rect;
    if (TextureManager::CombineAdjacentRects(
            texture->GetLevelClearedRect(target, level),
            gfx::Rect(destX, destY, src.width(), src.height()),
            &cleared_rect)) {
      DCHECK_GE(cleared_rect.size().GetArea(),
                texture->GetLevelClearedRect(target, level).size().GetArea());
      texture_manager()->SetLevelClearedRect(texture_ref, target, level,
                                             cleared_rect);
    } else {
      // Otherwise clear part of texture level that is not already cleared.
      if (!texture_manager()->ClearTextureLevel(this, texture_ref, target,
                                                level)) {
        LOCAL_SET_GL_ERROR(GL_OUT_OF_MEMORY, func_name, "dimensions too big");
        return;
      }
    }
  }

  if (CopyTexImageResourceManager::CopyTexImageRequiresBlit(
          feature_info_.get(), internal_format)) {
    if (!InitializeCopyTexImageBlitter("glCopyTexSubImage2D")) {
      return;
    }
    copy_tex_image_blit_->DoCopyTexSubImageToLUMACompatibilityTexture(
        this, texture->service_id(), texture->target(), target,
        internal_format, type, level, destX, destY, 0,
        src.x(), src.y(), src.width(), src.height(),
        GetBoundReadFramebufferServiceId(),
        GetBoundReadFramebufferInternalFormat());
  } else {
    glCopyTexSubImage2D(target, level, destX, destY,
                        src.x(), src.y(), src.width(), src.height());
  }

  // This may be a slow command.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();
}

void GLES2DecoderImpl::DoCopyTexSubImage3D(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height) {
  const char* func_name = "glCopyTexSubImage3D";
  DCHECK(!ShouldDeferReads());
  TextureRef* texture_ref = texture_manager()->GetTextureInfoForTarget(
      &state_, target);
  if (!texture_ref) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, func_name, "unknown texture for target");
    return;
  }
  Texture* texture = texture_ref->texture();
  GLenum type = 0;
  GLenum internal_format = 0;
  if (!texture->GetLevelType(target, level, &type, &internal_format) ||
      !texture->ValidForTexture(
          target, level, xoffset, yoffset, zoffset, width, height, 1)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "bad dimensions.");
    return;
  }

  if (!CheckBoundReadFramebufferValid(func_name,
                                      GL_INVALID_FRAMEBUFFER_OPERATION)) {
    return;
  }

  GLenum read_format = GetBoundReadFramebufferInternalFormat();
  GLenum read_type = GetBoundReadFramebufferTextureType();
  if (!ValidateCopyTexFormat(func_name, internal_format,
                             read_format, read_type)) {
    return;
  }

  if (FormsTextureCopyingFeedbackLoop(texture_ref, level, zoffset)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        func_name, "source and destination textures are the same");
    return;
  }

  ScopedResolvedFramebufferBinder binder(this, false, true);
  gfx::Size size = GetBoundReadFramebufferSize();

  gfx::Rect src(x, y, width, height);
  const gfx::Rect dst(0, 0, size.width(), size.height());
  src.Intersect(dst);
  if (src.IsEmpty())
    return;

  GLint dx = src.x() - x;
  GLint dy = src.y() - y;
  GLint destX = xoffset + dx;
  GLint destY = yoffset + dy;
  // For 3D textures, we always clear the entire texture to 0 if it is not
  // cleared. See the code in TextureManager::ValidateAndDoTexSubImage
  // for TexSubImage3D.
  if (!texture->IsLevelCleared(target, level)) {
    if (!texture_manager()->ClearTextureLevel(this, texture_ref, target,
                                              level)) {
      LOCAL_SET_GL_ERROR(GL_OUT_OF_MEMORY, func_name, "dimensions too big");
      return;
    }
    DCHECK(texture->IsLevelCleared(target, level));
  }

  if (CopyTexImageResourceManager::CopyTexImageRequiresBlit(
          feature_info_.get(), internal_format)) {
    if (!InitializeCopyTexImageBlitter(func_name)) {
      return;
    }
    copy_tex_image_blit_->DoCopyTexSubImageToLUMACompatibilityTexture(
        this, texture->service_id(), texture->target(), target,
        internal_format, type, level, destX, destY, zoffset,
        src.x(), src.y(), src.width(), src.height(),
        GetBoundReadFramebufferServiceId(),
        GetBoundReadFramebufferInternalFormat());
  } else {
    glCopyTexSubImage3D(target, level, destX, destY, zoffset,
                        src.x(), src.y(), src.width(), src.height());
  }

  // This may be a slow command.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();
}

error::Error GLES2DecoderImpl::HandleTexSubImage2D(
    uint32_t immediate_data_size, const volatile void* cmd_data) {
  const char* func_name = "glTexSubImage2D";
  const volatile gles2::cmds::TexSubImage2D& c =
      *static_cast<const volatile gles2::cmds::TexSubImage2D*>(cmd_data);
  TRACE_EVENT2("gpu", "GLES2DecoderImpl::HandleTexSubImage2D",
      "width", c.width, "height", c.height);
  GLboolean internal = static_cast<GLboolean>(c.internal);
  if (internal == GL_TRUE && texture_state_.tex_image_failed)
    return error::kNoError;

  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum type = static_cast<GLenum>(c.type);
  uint32_t pixels_shm_id = static_cast<uint32_t>(c.pixels_shm_id);
  uint32_t pixels_shm_offset = static_cast<uint32_t>(c.pixels_shm_offset);

  if (width < 0 || height < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "dimensions < 0");
    return error::kNoError;
  }

  PixelStoreParams params;
  Buffer* buffer = state_.bound_pixel_unpack_buffer.get();
  if (buffer) {
    if (pixels_shm_id)
      return error::kInvalidArguments;
    if (buffer->GetMappedRange()) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
          "pixel unpack buffer should not be mapped to client memory");
      return error::kNoError;
    }
    params = state_.GetUnpackParams(ContextState::k2D);
  } else {
    // When reading from client buffer, the command buffer client side took
    // the responsibility to take the pixels from the client buffer and
    // unpack them according to the full ES3 pack parameters as source, all
    // parameters for 0 (except for alignment) as destination mem for the
    // service side.
    params.alignment = state_.unpack_alignment;
  }
  uint32_t pixels_size;
  uint32_t skip_size;
  uint32_t padding;
  if (!GLES2Util::ComputeImageDataSizesES3(width, height, 1,
                                           format, type,
                                           params,
                                           &pixels_size,
                                           nullptr,
                                           nullptr,
                                           &skip_size,
                                           &padding)) {
    return error::kOutOfBounds;
  }
  DCHECK_EQ(0u, skip_size);

  const void* pixels;
  if (pixels_shm_id) {
    pixels = GetSharedMemoryAs<const void*>(
        pixels_shm_id, pixels_shm_offset, pixels_size);
    if (!pixels)
      return error::kOutOfBounds;
  } else {
    pixels = reinterpret_cast<const void*>(pixels_shm_offset);
  }

  TextureManager::DoTexSubImageArguments args = {
      target, level, xoffset, yoffset, 0, width, height, 1,
      format, type, pixels, pixels_size, padding,
      TextureManager::DoTexSubImageArguments::kTexSubImage2D};
  texture_manager()->ValidateAndDoTexSubImage(this, &texture_state_, &state_,
                                              &framebuffer_state_,
                                              func_name, args);

  // This may be a slow command.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTexSubImage3D(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;

  const char* func_name = "glTexSubImage3D";
  const volatile gles2::cmds::TexSubImage3D& c =
      *static_cast<const volatile gles2::cmds::TexSubImage3D*>(cmd_data);
  TRACE_EVENT2("gpu", "GLES2DecoderImpl::HandleTexSubImage3D",
      "widthXheight", c.width * c.height, "depth", c.depth);
  GLboolean internal = static_cast<GLboolean>(c.internal);
  if (internal == GL_TRUE && texture_state_.tex_image_failed)
    return error::kNoError;

  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLint zoffset = static_cast<GLint>(c.zoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLsizei depth = static_cast<GLsizei>(c.depth);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum type = static_cast<GLenum>(c.type);
  uint32_t pixels_shm_id = static_cast<uint32_t>(c.pixels_shm_id);
  uint32_t pixels_shm_offset = static_cast<uint32_t>(c.pixels_shm_offset);

  if (width < 0 || height < 0 || depth < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "dimensions < 0");
    return error::kNoError;
  }

  PixelStoreParams params;
  Buffer* buffer = state_.bound_pixel_unpack_buffer.get();
  if (buffer) {
    if (pixels_shm_id)
      return error::kInvalidArguments;
    if (buffer->GetMappedRange()) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
          "pixel unpack buffer should not be mapped to client memory");
      return error::kNoError;
    }
    params = state_.GetUnpackParams(ContextState::k3D);
  } else {
    // When reading from client buffer, the command buffer client side took
    // the responsibility to take the pixels from the client buffer and
    // unpack them according to the full ES3 pack parameters as source, all
    // parameters for 0 (except for alignment) as destination mem for the
    // service side.
    params.alignment = state_.unpack_alignment;
  }
  uint32_t pixels_size;
  uint32_t skip_size;
  uint32_t padding;
  if (!GLES2Util::ComputeImageDataSizesES3(width, height, depth,
                                           format, type,
                                           params,
                                           &pixels_size,
                                           nullptr,
                                           nullptr,
                                           &skip_size,
                                           &padding)) {
    return error::kOutOfBounds;
  }
  DCHECK_EQ(0u, skip_size);

  const void* pixels;
  if (pixels_shm_id) {
    pixels = GetSharedMemoryAs<const void*>(
        pixels_shm_id, pixels_shm_offset, pixels_size);
    if (!pixels)
      return error::kOutOfBounds;
  } else {
    pixels = reinterpret_cast<const void*>(pixels_shm_offset);
  }

  TextureManager::DoTexSubImageArguments args = {
      target, level, xoffset, yoffset, zoffset, width, height, depth,
      format, type, pixels, pixels_size, padding,
      TextureManager::DoTexSubImageArguments::kTexSubImage3D};
  texture_manager()->ValidateAndDoTexSubImage(this, &texture_state_, &state_,
                                              &framebuffer_state_,
                                              func_name, args);

  // This may be a slow command.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetVertexAttribPointerv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetVertexAttribPointerv& c =
      *static_cast<const volatile gles2::cmds::GetVertexAttribPointerv*>(
          cmd_data);
  GLuint index = static_cast<GLuint>(c.index);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetVertexAttribPointerv::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
        c.pointer_shm_id, c.pointer_shm_offset, Result::ComputeSize(1));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  if (!validators_->vertex_pointer.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(
        "glGetVertexAttribPointerv", pname, "pname");
    return error::kNoError;
  }
  if (index >= group_->max_vertex_attribs()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glGetVertexAttribPointerv", "index out of range.");
    return error::kNoError;
  }
  result->SetNumResults(1);
  *result->GetData() =
      state_.vertex_attrib_manager->GetVertexAttrib(index)->offset();
  return error::kNoError;
}

template <class T>
bool GLES2DecoderImpl::GetUniformSetup(GLuint program_id,
                                       GLint fake_location,
                                       uint32_t shm_id,
                                       uint32_t shm_offset,
                                       error::Error* error,
                                       GLint* real_location,
                                       GLuint* service_id,
                                       SizedResult<T>** result_pointer,
                                       GLenum* result_type,
                                       GLsizei* result_size) {
  DCHECK(error);
  DCHECK(service_id);
  DCHECK(result_pointer);
  DCHECK(result_type);
  DCHECK(result_size);
  DCHECK(real_location);
  *error = error::kNoError;
  // Make sure we have enough room for the result on failure.
  SizedResult<T>* result;
  result = GetSharedMemoryAs<SizedResult<T>*>(
      shm_id, shm_offset, SizedResult<T>::ComputeSize(0));
  if (!result) {
    *error = error::kOutOfBounds;
    return false;
  }
  *result_pointer = result;
  // Set the result size to 0 so the client does not have to check for success.
  result->SetNumResults(0);
  Program* program = GetProgramInfoNotShader(program_id, "glGetUniform");
  if (!program) {
    return false;
  }
  if (!program->IsValid()) {
    // Program was not linked successfully. (ie, glLinkProgram)
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, "glGetUniform", "program not linked");
    return false;
  }
  *service_id = program->service_id();
  GLint array_index = -1;
  const Program::UniformInfo* uniform_info =
      program->GetUniformInfoByFakeLocation(
          fake_location, real_location, &array_index);
  if (!uniform_info) {
    // No such location.
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, "glGetUniform", "unknown location");
    return false;
  }
  GLenum type = uniform_info->type;
  uint32_t num_elements = GLES2Util::GetElementCountForUniformType(type);
  if (num_elements == 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glGetUniform", "unknown type");
    return false;
  }
  result = GetSharedMemoryAs<SizedResult<T>*>(
      shm_id, shm_offset, SizedResult<T>::ComputeSize(num_elements));
  if (!result) {
    *error = error::kOutOfBounds;
    return false;
  }
  result->SetNumResults(num_elements);
  *result_size = num_elements * sizeof(T);
  *result_type = type;
  return true;
}

error::Error GLES2DecoderImpl::HandleGetUniformiv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetUniformiv& c =
      *static_cast<const volatile gles2::cmds::GetUniformiv*>(cmd_data);
  GLuint program = c.program;
  GLint fake_location = c.location;
  GLuint service_id;
  GLenum result_type;
  GLsizei result_size;
  GLint real_location = -1;
  Error error;
  cmds::GetUniformiv::Result* result;
  if (GetUniformSetup<GLint>(program, fake_location, c.params_shm_id,
                             c.params_shm_offset, &error, &real_location,
                             &service_id, &result, &result_type,
                             &result_size)) {
    glGetUniformiv(
        service_id, real_location, result->GetData());
  }
  return error;
}

error::Error GLES2DecoderImpl::HandleGetUniformuiv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;

  const volatile gles2::cmds::GetUniformuiv& c =
      *static_cast<const volatile gles2::cmds::GetUniformuiv*>(cmd_data);
  GLuint program = c.program;
  GLint fake_location = c.location;
  GLuint service_id;
  GLenum result_type;
  GLsizei result_size;
  GLint real_location = -1;
  Error error;
  cmds::GetUniformuiv::Result* result;
  if (GetUniformSetup<GLuint>(program, fake_location, c.params_shm_id,
                              c.params_shm_offset, &error, &real_location,
                              &service_id, &result, &result_type,
                              &result_size)) {
    glGetUniformuiv(
        service_id, real_location, result->GetData());
  }
  return error;
}

error::Error GLES2DecoderImpl::HandleGetUniformfv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetUniformfv& c =
      *static_cast<const volatile gles2::cmds::GetUniformfv*>(cmd_data);
  GLuint program = c.program;
  GLint fake_location = c.location;
  GLuint service_id;
  GLint real_location = -1;
  Error error;
  cmds::GetUniformfv::Result* result;
  GLenum result_type;
  GLsizei result_size;
  if (GetUniformSetup<GLfloat>(program, fake_location, c.params_shm_id,
                               c.params_shm_offset, &error, &real_location,
                               &service_id, &result, &result_type,
                               &result_size)) {
    if (result_type == GL_BOOL || result_type == GL_BOOL_VEC2 ||
        result_type == GL_BOOL_VEC3 || result_type == GL_BOOL_VEC4) {
      GLsizei num_values = result_size / sizeof(GLfloat);
      std::unique_ptr<GLint[]> temp(new GLint[num_values]);
      glGetUniformiv(service_id, real_location, temp.get());
      GLfloat* dst = result->GetData();
      for (GLsizei ii = 0; ii < num_values; ++ii) {
        dst[ii] = (temp[ii] != 0);
      }
    } else {
      glGetUniformfv(service_id, real_location, result->GetData());
    }
  }
  return error;
}

error::Error GLES2DecoderImpl::HandleGetShaderPrecisionFormat(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetShaderPrecisionFormat& c =
      *static_cast<const volatile gles2::cmds::GetShaderPrecisionFormat*>(
          cmd_data);
  GLenum shader_type = static_cast<GLenum>(c.shadertype);
  GLenum precision_type = static_cast<GLenum>(c.precisiontype);
  typedef cmds::GetShaderPrecisionFormat::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->success != 0) {
    return error::kInvalidArguments;
  }
  if (!validators_->shader_type.IsValid(shader_type)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(
        "glGetShaderPrecisionFormat", shader_type, "shader_type");
    return error::kNoError;
  }
  if (!validators_->shader_precision.IsValid(precision_type)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(
        "glGetShaderPrecisionFormat", precision_type, "precision_type");
    return error::kNoError;
  }

  result->success = 1;  // true

  GLint range[2] = { 0, 0 };
  GLint precision = 0;
  QueryShaderPrecisionFormat(gl_version_info(), shader_type, precision_type,
                             range, &precision);

  result->min_range = range[0];
  result->max_range = range[1];
  result->precision = precision;

  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetAttachedShaders(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetAttachedShaders& c =
      *static_cast<const volatile gles2::cmds::GetAttachedShaders*>(cmd_data);
  uint32_t result_size = c.result_size;
  GLuint program_id = static_cast<GLuint>(c.program);
  Program* program = GetProgramInfoNotShader(
      program_id, "glGetAttachedShaders");
  if (!program) {
    return error::kNoError;
  }
  typedef cmds::GetAttachedShaders::Result Result;
  uint32_t max_count = Result::ComputeMaxResults(result_size);
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, Result::ComputeSize(max_count));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  GLsizei count = 0;
  glGetAttachedShaders(
      program->service_id(), max_count, &count, result->GetData());
  for (GLsizei ii = 0; ii < count; ++ii) {
    if (!shader_manager()->GetClientId(result->GetData()[ii],
                                       &result->GetData()[ii])) {
      NOTREACHED();
      return error::kGenericError;
    }
  }
  result->SetNumResults(count);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetActiveUniform(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetActiveUniform& c =
      *static_cast<const volatile gles2::cmds::GetActiveUniform*>(cmd_data);
  GLuint program_id = c.program;
  GLuint index = c.index;
  uint32_t name_bucket_id = c.name_bucket_id;
  typedef cmds::GetActiveUniform::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->success != 0) {
    return error::kInvalidArguments;
  }
  Program* program = GetProgramInfoNotShader(
      program_id, "glGetActiveUniform");
  if (!program) {
    return error::kNoError;
  }
  const Program::UniformInfo* uniform_info =
      program->GetUniformInfo(index);
  if (!uniform_info) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glGetActiveUniform", "index out of range");
    return error::kNoError;
  }
  result->success = 1;  // true.
  result->size = uniform_info->size;
  result->type = uniform_info->type;
  Bucket* bucket = CreateBucket(name_bucket_id);
  bucket->SetFromString(uniform_info->name.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetActiveUniformBlockiv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GetActiveUniformBlockiv& c =
      *static_cast<const volatile gles2::cmds::GetActiveUniformBlockiv*>(
          cmd_data);
  GLuint program_id = c.program;
  GLuint index = static_cast<GLuint>(c.index);
  GLenum pname = static_cast<GLenum>(c.pname);
  Program* program = GetProgramInfoNotShader(
      program_id, "glGetActiveUniformBlockiv");
  if (!program) {
    return error::kNoError;
  }
  GLuint service_id = program->service_id();
  GLint link_status = GL_FALSE;
  glGetProgramiv(service_id, GL_LINK_STATUS, &link_status);
  if (link_status != GL_TRUE) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
        "glGetActiveActiveUniformBlockiv", "program not linked");
    return error::kNoError;
  }
  if (index >= program->uniform_block_size_info().size()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glGetActiveUniformBlockiv",
                       "uniformBlockIndex >= active uniform blocks");
    return error::kNoError;
  }
  GLsizei num_values = 1;
  if (pname == GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES) {
    GLint num = 0;
    glGetActiveUniformBlockiv(
        service_id, index, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &num);
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
      // Assume this will the same error if calling with pname.
      LOCAL_SET_GL_ERROR(error, "GetActiveUniformBlockiv", "");
      return error::kNoError;
    }
    num_values = static_cast<GLsizei>(num);
  }
  typedef cmds::GetActiveUniformBlockiv::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, Result::ComputeSize(num_values));
  GLint* params = result ? result->GetData() : NULL;
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  glGetActiveUniformBlockiv(service_id, index, pname, params);
  result->SetNumResults(num_values);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetActiveUniformBlockName(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GetActiveUniformBlockName& c =
      *static_cast<const volatile gles2::cmds::GetActiveUniformBlockName*>(
          cmd_data);
  GLuint program_id = c.program;
  GLuint index = c.index;
  uint32_t name_bucket_id = c.name_bucket_id;
  typedef cmds::GetActiveUniformBlockName::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (*result != 0) {
    return error::kInvalidArguments;
  }
  Program* program = GetProgramInfoNotShader(
      program_id, "glGetActiveUniformBlockName");
  if (!program) {
    return error::kNoError;
  }
  GLuint service_id = program->service_id();
  GLint link_status = GL_FALSE;
  glGetProgramiv(service_id, GL_LINK_STATUS, &link_status);
  if (link_status != GL_TRUE) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
        "glGetActiveActiveUniformBlockName", "program not linked");
    return error::kNoError;
  }
  if (index >= program->uniform_block_size_info().size()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glGetActiveUniformBlockName",
                       "uniformBlockIndex >= active uniform blocks");
    return error::kNoError;
  }
  GLint max_length = 0;
  glGetProgramiv(
      service_id, GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH, &max_length);
  // Increase one so &buffer[0] is always valid.
  GLsizei buf_size = static_cast<GLsizei>(max_length) + 1;
  std::vector<char> buffer(buf_size);
  GLsizei length = 0;
  glGetActiveUniformBlockName(
      service_id, index, buf_size, &length, &buffer[0]);
  if (length == 0) {
    *result = 0;
    return error::kNoError;
  }
  *result = 1;
  Bucket* bucket = CreateBucket(name_bucket_id);
  DCHECK_GT(buf_size, length);
  DCHECK_EQ(0, buffer[length]);
  bucket->SetFromString(&buffer[0]);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetActiveUniformsiv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GetActiveUniformsiv& c =
      *static_cast<const volatile gles2::cmds::GetActiveUniformsiv*>(cmd_data);
  GLuint program_id = c.program;
  GLenum pname = static_cast<GLenum>(c.pname);
  Bucket* bucket = GetBucket(c.indices_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  if (!validators_->uniform_parameter.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetActiveUniformsiv", pname, "pname");
    return error::kNoError;
  }
  GLsizei count = static_cast<GLsizei>(bucket->size() / sizeof(GLuint));
  const GLuint* indices = bucket->GetDataAs<const GLuint*>(0, bucket->size());
  typedef cmds::GetActiveUniformsiv::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, Result::ComputeSize(count));
  GLint* params = result ? result->GetData() : NULL;
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  Program* program = GetProgramInfoNotShader(
      program_id, "glGetActiveUniformsiv");
  if (!program) {
    return error::kNoError;
  }
  GLint activeUniforms = 0;
  program->GetProgramiv(GL_ACTIVE_UNIFORMS, &activeUniforms);
  for (int i = 0; i < count; i++) {
    if (indices[i] >= static_cast<GLuint>(activeUniforms)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE,
          "glGetActiveUniformsiv", "index >= active uniforms");
      return error::kNoError;
    }
  }
  GLuint service_id = program->service_id();
  GLint link_status = GL_FALSE;
  glGetProgramiv(service_id, GL_LINK_STATUS, &link_status);
  if (link_status != GL_TRUE) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
        "glGetActiveUniformsiv", "program not linked");
    return error::kNoError;
  }
  glGetActiveUniformsiv(service_id, count, indices, pname, params);
  result->SetNumResults(count);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetActiveAttrib(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetActiveAttrib& c =
      *static_cast<const volatile gles2::cmds::GetActiveAttrib*>(cmd_data);
  GLuint program_id = c.program;
  GLuint index = c.index;
  uint32_t name_bucket_id = c.name_bucket_id;
  typedef cmds::GetActiveAttrib::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->success != 0) {
    return error::kInvalidArguments;
  }
  Program* program = GetProgramInfoNotShader(
      program_id, "glGetActiveAttrib");
  if (!program) {
    return error::kNoError;
  }
  const Program::VertexAttrib* attrib_info =
      program->GetAttribInfo(index);
  if (!attrib_info) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, "glGetActiveAttrib", "index out of range");
    return error::kNoError;
  }
  result->success = 1;  // true.
  result->size = attrib_info->size;
  result->type = attrib_info->type;
  Bucket* bucket = CreateBucket(name_bucket_id);
  bucket->SetFromString(attrib_info->name.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleShaderBinary(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
#if 1  // No binary shader support.
  LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glShaderBinary", "not supported");
  return error::kNoError;
#else
  GLsizei n = static_cast<GLsizei>(c.n);
  if (n < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glShaderBinary", "n < 0");
    return error::kNoError;
  }
  GLsizei length = static_cast<GLsizei>(c.length);
  if (length < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glShaderBinary", "length < 0");
    return error::kNoError;
  }
  uint32_t data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  const GLuint* shaders = GetSharedMemoryAs<const GLuint*>(
      c.shaders_shm_id, c.shaders_shm_offset, data_size);
  GLenum binaryformat = static_cast<GLenum>(c.binaryformat);
  const void* binary = GetSharedMemoryAs<const void*>(
      c.binary_shm_id, c.binary_shm_offset, length);
  if (shaders == NULL || binary == NULL) {
    return error::kOutOfBounds;
  }
  std::unique_ptr<GLuint[]> service_ids(new GLuint[n]);
  for (GLsizei ii = 0; ii < n; ++ii) {
    Shader* shader = GetShader(shaders[ii]);
    if (!shader) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glShaderBinary", "unknown shader");
      return error::kNoError;
    }
    service_ids[ii] = shader->service_id();
  }
  // TODO(gman): call glShaderBinary
  return error::kNoError;
#endif
}

void GLES2DecoderImpl::DoSwapBuffers() {
  bool is_offscreen = !!offscreen_target_frame_buffer_.get();

  int this_frame_number = frame_number_++;
  // TRACE_EVENT for gpu tests:
  TRACE_EVENT_INSTANT2(
      "test_gpu", "SwapBuffersLatency", TRACE_EVENT_SCOPE_THREAD, "GLImpl",
      static_cast<int>(gl::GetGLImplementation()), "width",
      (is_offscreen ? offscreen_size_.width() : surface_->GetSize().width()));
  TRACE_EVENT2("gpu", "GLES2DecoderImpl::DoSwapBuffers",
               "offscreen", is_offscreen,
               "frame", this_frame_number);
  {
    TRACE_EVENT_SYNTHETIC_DELAY("gpu.PresentingFrame");
  }

  ScopedGPUTrace scoped_gpu_trace(gpu_tracer_.get(), kTraceDecoder,
                                  "GLES2Decoder", "SwapBuffer");

  bool is_tracing;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("gpu.debug"),
                                     &is_tracing);
  if (is_tracing) {
    ScopedFramebufferBinder binder(this, GetBoundDrawFramebufferServiceId());
    gpu_state_tracer_->TakeSnapshotWithCurrentFramebuffer(
        is_offscreen ? offscreen_size_ : surface_->GetSize());
  }

  ClearScheduleCALayerState();

  // If offscreen then don't actually SwapBuffers to the display. Just copy
  // the rendered frame to another frame buffer.
  if (is_offscreen) {
    TRACE_EVENT2("gpu", "Offscreen",
        "width", offscreen_size_.width(), "height", offscreen_size_.height());
    if (offscreen_size_ != offscreen_saved_color_texture_->size()) {
      // Workaround for NVIDIA driver bug on OS X; crbug.com/89557,
      // crbug.com/94163. TODO(kbr): figure out reproduction so Apple will
      // fix this.
      if (workarounds().needs_offscreen_buffer_workaround) {
        offscreen_saved_frame_buffer_->Create();
        glFinish();
      }

      // The size has changed, so none of the cached BackTextures are useful
      // anymore.
      ReleaseNotInUseBackTextures();

      // Allocate the offscreen saved color texture.
      DCHECK(offscreen_saved_color_format_);
      offscreen_saved_color_texture_->AllocateStorage(
          offscreen_size_, offscreen_saved_color_format_, false);

      offscreen_saved_frame_buffer_->AttachRenderTexture(
          offscreen_saved_color_texture_.get());
      if (offscreen_size_.width() != 0 && offscreen_size_.height() != 0) {
        if (offscreen_saved_frame_buffer_->CheckStatus() !=
            GL_FRAMEBUFFER_COMPLETE) {
          LOG(ERROR) << "GLES2DecoderImpl::ResizeOffscreenFramebuffer failed "
                     << "because offscreen saved FBO was incomplete.";
          MarkContextLost(error::kUnknown);
          group_->LoseContexts(error::kUnknown);
          return;
        }

        // Clear the offscreen color texture.
        // TODO(piman): Is this still necessary?
        {
          ScopedFramebufferBinder binder(this,
                                         offscreen_saved_frame_buffer_->id());
          glClearColor(0, 0, 0, BackBufferAlphaClearColor());
          state_.SetDeviceColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
          state_.SetDeviceCapabilityState(GL_SCISSOR_TEST, false);
          glClear(GL_COLOR_BUFFER_BIT);
          RestoreClearState();
        }
      }
    }

    if (offscreen_size_.width() == 0 || offscreen_size_.height() == 0)
      return;
    ScopedGLErrorSuppressor suppressor(
        "GLES2DecoderImpl::DoSwapBuffers", GetErrorState());

    if (IsOffscreenBufferMultisampled()) {
      // For multisampled buffers, resolve the frame buffer.
      ScopedResolvedFramebufferBinder binder(this, true, false);
    } else {
      ScopedFramebufferBinder binder(this,
                                     offscreen_target_frame_buffer_->id());

      if (offscreen_target_buffer_preserved_) {
        // Copy the target frame buffer to the saved offscreen texture.
        offscreen_saved_color_texture_->Copy();
      } else {
        offscreen_saved_color_texture_.swap(offscreen_target_color_texture_);
        offscreen_target_frame_buffer_->AttachRenderTexture(
            offscreen_target_color_texture_.get());
        offscreen_saved_frame_buffer_->AttachRenderTexture(
            offscreen_saved_color_texture_.get());
      }

      // Ensure the side effects of the copy are visible to the parent
      // context. There is no need to do this for ANGLE because it uses a
      // single D3D device for all contexts.
      if (!gl_version_info().is_angle)
        glFlush();
    }
  } else if (supports_async_swap_) {
    TRACE_EVENT_ASYNC_BEGIN0("cc", "GLES2DecoderImpl::AsyncSwapBuffers", this);
    surface_->SwapBuffersAsync(base::Bind(&GLES2DecoderImpl::FinishSwapBuffers,
                                          base::AsWeakPtr(this)));
  } else {
    FinishSwapBuffers(surface_->SwapBuffers());
  }

  // This may be a slow command.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();
}

void GLES2DecoderImpl::FinishSwapBuffers(gfx::SwapResult result) {
  if (result == gfx::SwapResult::SWAP_FAILED) {
    LOG(ERROR) << "Context lost because SwapBuffers failed.";
    if (!CheckResetStatus()) {
      MarkContextLost(error::kUnknown);
      group_->LoseContexts(error::kUnknown);
    }
  }
  ++swaps_since_resize_;
  if (swaps_since_resize_ == 1 && surface_->BuffersFlipped()) {
    // The second buffer after a resize is new and needs to be cleared to
    // known values.
    backbuffer_needs_clear_bits_ |= GL_COLOR_BUFFER_BIT;
  }

  if (supports_async_swap_) {
    TRACE_EVENT_ASYNC_END0("cc", "GLES2DecoderImpl::AsyncSwapBuffers", this);
  }
}

void GLES2DecoderImpl::DoCommitOverlayPlanes() {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::DoCommitOverlayPlanes");
  if (!supports_commit_overlay_planes_) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glCommitOverlayPlanes",
                       "command not supported by surface");
    return;
  }
  ClearScheduleCALayerState();
  if (supports_async_swap_) {
    surface_->CommitOverlayPlanesAsync(base::Bind(
        &GLES2DecoderImpl::FinishSwapBuffers, base::AsWeakPtr(this)));
  } else {
    FinishSwapBuffers(surface_->CommitOverlayPlanes());
  }
}

void GLES2DecoderImpl::DoSwapInterval(int interval) {
  context_->SetSwapInterval(interval);
}

error::Error GLES2DecoderImpl::HandleEnableFeatureCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::EnableFeatureCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::EnableFeatureCHROMIUM*>(
          cmd_data);
  Bucket* bucket = GetBucket(c.bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  typedef cmds::EnableFeatureCHROMIUM::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (*result != 0) {
    return error::kInvalidArguments;
  }
  std::string feature_str;
  if (!bucket->GetAsString(&feature_str)) {
    return error::kInvalidArguments;
  }

  // TODO(gman): make this some kind of table to function pointer thingy.
  if (feature_str.compare("pepper3d_allow_buffers_on_multiple_targets") == 0) {
    buffer_manager()->set_allow_buffers_on_multiple_targets(true);
  } else if (feature_str.compare("pepper3d_support_fixed_attribs") == 0) {
    buffer_manager()->set_allow_fixed_attribs(true);
    // TODO(gman): decide how to remove the need for this const_cast.
    // I could make validators_ non const but that seems bad as this is the only
    // place it is needed. I could make some special friend class of validators
    // just to allow this to set them. That seems silly. I could refactor this
    // code to use the extension mechanism or the initialization attributes to
    // turn this feature on. Given that the only real point of this is to make
    // the conformance tests pass and given that there is lots of real work that
    // needs to be done it seems like refactoring for one to one of those
    // methods is a very low priority.
    const_cast<Validators*>(validators_)->vertex_attrib_type.AddValue(GL_FIXED);
  } else {
    return error::kNoError;
  }

  *result = 1;  // true.
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetRequestableExtensionsCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetRequestableExtensionsCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::GetRequestableExtensionsCHROMIUM*>(
          cmd_data);
  Bucket* bucket = CreateBucket(c.bucket_id);
  scoped_refptr<FeatureInfo> info(new FeatureInfo(workarounds()));
  DisallowedFeatures disallowed_features = feature_info_->disallowed_features();
  disallowed_features.AllowExtensions();
  info->Initialize(feature_info_->context_type(), disallowed_features);
  bucket->SetFromString(info->extensions().c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleRequestExtensionCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::RequestExtensionCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::RequestExtensionCHROMIUM*>(
          cmd_data);
  Bucket* bucket = GetBucket(c.bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string feature_str;
  if (!bucket->GetAsString(&feature_str)) {
    return error::kInvalidArguments;
  }
  feature_str = feature_str + " ";

  bool desire_standard_derivatives = false;
  bool desire_frag_depth = false;
  bool desire_draw_buffers = false;
  bool desire_shader_texture_lod = false;
  if (feature_info_->context_type() == CONTEXT_TYPE_WEBGL1) {
    desire_standard_derivatives =
        feature_str.find("GL_OES_standard_derivatives ") != std::string::npos;
    desire_frag_depth =
        feature_str.find("GL_EXT_frag_depth ") != std::string::npos;
    desire_draw_buffers =
        feature_str.find("GL_EXT_draw_buffers ") != std::string::npos;
    desire_shader_texture_lod =
        feature_str.find("GL_EXT_shader_texture_lod ") != std::string::npos;
  }
  if (desire_standard_derivatives != derivatives_explicitly_enabled_ ||
      desire_frag_depth != frag_depth_explicitly_enabled_ ||
      desire_draw_buffers != draw_buffers_explicitly_enabled_ ||
      desire_shader_texture_lod != shader_texture_lod_explicitly_enabled_) {
    derivatives_explicitly_enabled_ |= desire_standard_derivatives;
    frag_depth_explicitly_enabled_ |= desire_frag_depth;
    draw_buffers_explicitly_enabled_ |= desire_draw_buffers;
    shader_texture_lod_explicitly_enabled_ |= desire_shader_texture_lod;
    InitializeShaderTranslator();
  }

  if (feature_str.find("GL_CHROMIUM_color_buffer_float_rgba ") !=
      std::string::npos) {
    feature_info_->EnableCHROMIUMColorBufferFloatRGBA();
  }
  if (feature_str.find("GL_CHROMIUM_color_buffer_float_rgb ") !=
      std::string::npos) {
    feature_info_->EnableCHROMIUMColorBufferFloatRGB();
  }
  if (feature_str.find("GL_EXT_color_buffer_float ") != std::string::npos) {
    feature_info_->EnableEXTColorBufferFloat();
  }
  if (feature_str.find("GL_OES_texture_float_linear ") != std::string::npos) {
    feature_info_->EnableOESTextureFloatLinear();
  }
  if (feature_str.find("GL_OES_texture_half_float_linear ") !=
      std::string::npos) {
    feature_info_->EnableOESTextureHalfFloatLinear();
  }

  UpdateCapabilities();

  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetProgramInfoCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetProgramInfoCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::GetProgramInfoCHROMIUM*>(
          cmd_data);
  GLuint program_id = static_cast<GLuint>(c.program);
  uint32_t bucket_id = c.bucket_id;
  Bucket* bucket = CreateBucket(bucket_id);
  bucket->SetSize(sizeof(ProgramInfoHeader));  // in case we fail.
  Program* program = NULL;
  program = GetProgram(program_id);
  if (!program || !program->IsValid()) {
    return error::kNoError;
  }
  program->GetProgramInfo(program_manager(), bucket);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetUniformBlocksCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GetUniformBlocksCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::GetUniformBlocksCHROMIUM*>(
          cmd_data);
  GLuint program_id = static_cast<GLuint>(c.program);
  uint32_t bucket_id = c.bucket_id;
  Bucket* bucket = CreateBucket(bucket_id);
  bucket->SetSize(sizeof(UniformBlocksHeader));  // in case we fail.
  Program* program = NULL;
  program = GetProgram(program_id);
  if (!program || !program->IsValid()) {
    return error::kNoError;
  }
  program->GetUniformBlocks(bucket);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetUniformsES3CHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GetUniformsES3CHROMIUM& c =
      *static_cast<const volatile gles2::cmds::GetUniformsES3CHROMIUM*>(
          cmd_data);
  GLuint program_id = static_cast<GLuint>(c.program);
  uint32_t bucket_id = c.bucket_id;
  Bucket* bucket = CreateBucket(bucket_id);
  bucket->SetSize(sizeof(UniformsES3Header));  // in case we fail.
  Program* program = NULL;
  program = GetProgram(program_id);
  if (!program || !program->IsValid()) {
    return error::kNoError;
  }
  program->GetUniformsES3(bucket);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetTransformFeedbackVarying(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GetTransformFeedbackVarying& c =
      *static_cast<const volatile gles2::cmds::GetTransformFeedbackVarying*>(
          cmd_data);
  GLuint program_id = c.program;
  GLuint index = c.index;
  uint32_t name_bucket_id = c.name_bucket_id;
  typedef cmds::GetTransformFeedbackVarying::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->success != 0) {
    return error::kInvalidArguments;
  }
  Program* program = GetProgramInfoNotShader(
      program_id, "glGetTransformFeedbackVarying");
  if (!program) {
    // An error is already set.
    return error::kNoError;
  }
  GLuint service_id = program->service_id();
  GLint link_status = GL_FALSE;
  glGetProgramiv(service_id, GL_LINK_STATUS, &link_status);
  if (link_status != GL_TRUE) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
        "glGetTransformFeedbackVarying", "program not linked");
    return error::kNoError;
  }
  GLint num_varyings = 0;
  glGetProgramiv(service_id, GL_TRANSFORM_FEEDBACK_VARYINGS, &num_varyings);
  if (index >= static_cast<GLuint>(num_varyings)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE,
        "glGetTransformFeedbackVarying", "index out of bounds");
    return error::kNoError;
  }
  GLint max_length = 0;
  glGetProgramiv(
      service_id, GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH, &max_length);
  max_length = std::max(1, max_length);
  std::vector<char> buffer(max_length);
  GLsizei length = 0;
  GLsizei size = 0;
  GLenum type = 0;
  glGetTransformFeedbackVarying(
      service_id, index, max_length, &length, &size, &type, &buffer[0]);
  result->success = 1;  // true.
  result->size = static_cast<int32_t>(size);
  result->type = static_cast<uint32_t>(type);
  Bucket* bucket = CreateBucket(name_bucket_id);
  DCHECK(length >= 0 && length < max_length);
  buffer[length] = '\0';  // Just to be safe.
  bucket->SetFromString(&buffer[0]);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetTransformFeedbackVaryingsCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GetTransformFeedbackVaryingsCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::GetTransformFeedbackVaryingsCHROMIUM*>(
          cmd_data);
  GLuint program_id = static_cast<GLuint>(c.program);
  uint32_t bucket_id = c.bucket_id;
  Bucket* bucket = CreateBucket(bucket_id);
  bucket->SetSize(sizeof(TransformFeedbackVaryingsHeader));  // in case we fail.
  Program* program = NULL;
  program = GetProgram(program_id);
  if (!program || !program->IsValid()) {
    return error::kNoError;
  }
  program->GetTransformFeedbackVaryings(bucket);
  return error::kNoError;
}

error::ContextLostReason GLES2DecoderImpl::GetContextLostReason() {
  return context_lost_reason_;
}

error::ContextLostReason GLES2DecoderImpl::GetContextLostReasonFromResetStatus(
    GLenum reset_status) const {
  switch (reset_status) {
    case GL_NO_ERROR:
      // TODO(kbr): improve the precision of the error code in this case.
      // Consider delegating to context for error code if MakeCurrent fails.
      return error::kUnknown;
    case GL_GUILTY_CONTEXT_RESET_ARB:
      return error::kGuilty;
    case GL_INNOCENT_CONTEXT_RESET_ARB:
      return error::kInnocent;
    case GL_UNKNOWN_CONTEXT_RESET_ARB:
      return error::kUnknown;
  }

  NOTREACHED();
  return error::kUnknown;
}

bool GLES2DecoderImpl::WasContextLost() const {
  return context_was_lost_;
}

bool GLES2DecoderImpl::WasContextLostByRobustnessExtension() const {
  return WasContextLost() && reset_by_robustness_extension_;
}

void GLES2DecoderImpl::MarkContextLost(error::ContextLostReason reason) {
  // Only lose the context once.
  if (WasContextLost())
    return;

  // Don't make GL calls in here, the context might not be current.
  context_lost_reason_ = reason;
  current_decoder_error_ = error::kLostContext;
  context_was_lost_ = true;

  if (transform_feedback_manager_.get()) {
    transform_feedback_manager_->MarkContextLost();
  }
}

bool GLES2DecoderImpl::CheckResetStatus() {
  DCHECK(!WasContextLost());
  DCHECK(context_->IsCurrent(NULL));

  if (IsRobustnessSupported()) {
    // If the reason for the call was a GL error, we can try to determine the
    // reset status more accurately.
    GLenum driver_status = glGetGraphicsResetStatusARB();
    if (driver_status == GL_NO_ERROR)
      return false;

    LOG(ERROR) << (surface_->IsOffscreen() ? "Offscreen" : "Onscreen")
               << " context lost via ARB/EXT_robustness. Reset status = "
               << GLES2Util::GetStringEnum(driver_status);

    // Don't pretend we know which client was responsible.
    if (workarounds().use_virtualized_gl_contexts)
      driver_status = GL_UNKNOWN_CONTEXT_RESET_ARB;

    switch (driver_status) {
      case GL_GUILTY_CONTEXT_RESET_ARB:
        MarkContextLost(error::kGuilty);
        break;
      case GL_INNOCENT_CONTEXT_RESET_ARB:
        MarkContextLost(error::kInnocent);
        break;
      case GL_UNKNOWN_CONTEXT_RESET_ARB:
        MarkContextLost(error::kUnknown);
        break;
      default:
        NOTREACHED();
        return false;
    }
    reset_by_robustness_extension_ = true;
    return true;
  }
  return false;
}

error::Error GLES2DecoderImpl::HandleDescheduleUntilFinishedCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (deschedule_until_finished_callback_.is_null() ||
      reschedule_after_finished_callback_.is_null()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                       "glDescheduleUntilFinishedCHROMIUM",
                       "Not fully implemented.");
    return error::kNoError;
  }

  std::unique_ptr<gl::GLFence> fence(gl::GLFence::Create());
  deschedule_until_finished_fences_.push_back(std::move(fence));

  if (deschedule_until_finished_fences_.size() == 1)
    return error::kNoError;

  DCHECK_EQ(2u, deschedule_until_finished_fences_.size());
  if (deschedule_until_finished_fences_[0]->HasCompleted()) {
    deschedule_until_finished_fences_.erase(
        deschedule_until_finished_fences_.begin());
    return error::kNoError;
  }

  TRACE_EVENT_ASYNC_BEGIN0("cc", "GLES2DecoderImpl::DescheduleUntilFinished",
                           this);
  deschedule_until_finished_callback_.Run();
  return error::kDeferLaterCommands;
}

error::Error GLES2DecoderImpl::HandleInsertFenceSyncCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::InsertFenceSyncCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::InsertFenceSyncCHROMIUM*>(
          cmd_data);

  const uint64_t release_count = c.release_count();
  if (!fence_sync_release_callback_.is_null())
    fence_sync_release_callback_.Run(release_count);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleWaitSyncTokenCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::WaitSyncTokenCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::WaitSyncTokenCHROMIUM*>(
          cmd_data);

  const gpu::CommandBufferNamespace kMinNamespaceId =
      gpu::CommandBufferNamespace::INVALID;
  const gpu::CommandBufferNamespace kMaxNamespaceId =
      gpu::CommandBufferNamespace::NUM_COMMAND_BUFFER_NAMESPACES;

  gpu::CommandBufferNamespace namespace_id =
      static_cast<gpu::CommandBufferNamespace>(c.namespace_id);
  if ((namespace_id < static_cast<int32_t>(kMinNamespaceId)) ||
       (namespace_id >= static_cast<int32_t>(kMaxNamespaceId))) {
    namespace_id = gpu::CommandBufferNamespace::INVALID;
  }
  const CommandBufferId command_buffer_id =
      CommandBufferId::FromUnsafeValue(c.command_buffer_id());
  const uint64_t release = c.release_count();
  if (wait_fence_sync_callback_.is_null())
    return error::kNoError;

  return wait_fence_sync_callback_.Run(namespace_id, command_buffer_id, release)
             ? error::kNoError
             : error::kDeferCommandUntilLater;
}

error::Error GLES2DecoderImpl::HandleDiscardBackbufferCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (surface_->DeferDraws())
    return error::kDeferCommandUntilLater;
  if (!surface_->SetBackbufferAllocation(false))
    return error::kLostContext;
  backbuffer_needs_clear_bits_ |= GL_COLOR_BUFFER_BIT;
  backbuffer_needs_clear_bits_ |= GL_DEPTH_BUFFER_BIT;
  backbuffer_needs_clear_bits_ |= GL_STENCIL_BUFFER_BIT;
  return error::kNoError;
}

bool GLES2DecoderImpl::GenQueriesEXTHelper(
    GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (query_manager_->IsValidQuery(client_ids[ii])) {
      return false;
    }
  }
  query_manager_->GenQueries(n, client_ids);
  return true;
}

void GLES2DecoderImpl::DeleteQueriesEXTHelper(
    GLsizei n,
    const volatile GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    GLuint client_id = client_ids[ii];
    query_manager_->RemoveQuery(client_id);
  }
}

bool GLES2DecoderImpl::HasPendingQueries() const {
  return query_manager_.get() && query_manager_->HavePendingQueries();
}

void GLES2DecoderImpl::ProcessPendingQueries(bool did_finish) {
  if (!query_manager_.get())
    return;
  if (!query_manager_->ProcessPendingQueries(did_finish))
    current_decoder_error_ = error::kOutOfBounds;
}

// Note that if there are no pending readpixels right now,
// this function will call the callback immediately.
void GLES2DecoderImpl::WaitForReadPixels(base::Closure callback) {
  if (features().use_async_readpixels && !pending_readpixel_fences_.empty()) {
    pending_readpixel_fences_.back().callbacks.push_back(callback);
  } else {
    callback.Run();
  }
}

void GLES2DecoderImpl::ProcessPendingReadPixels(bool did_finish) {
  // Note: |did_finish| guarantees that the GPU has passed the fence but
  // we cannot assume that GLFence::HasCompleted() will return true yet as
  // that's not guaranteed by all GLFence implementations.
  while (!pending_readpixel_fences_.empty() &&
         (did_finish ||
          pending_readpixel_fences_.front().fence->HasCompleted())) {
    std::vector<base::Closure> callbacks =
        std::move(pending_readpixel_fences_.front().callbacks);
    pending_readpixel_fences_.pop();
    for (size_t i = 0; i < callbacks.size(); i++) {
      callbacks[i].Run();
    }
  }
}

void GLES2DecoderImpl::ProcessDescheduleUntilFinished() {
  if (deschedule_until_finished_fences_.size() < 2)
    return;
  DCHECK_EQ(2u, deschedule_until_finished_fences_.size());

  if (!deschedule_until_finished_fences_[0]->HasCompleted())
    return;

  TRACE_EVENT_ASYNC_END0("cc", "GLES2DecoderImpl::DescheduleUntilFinished",
                         this);
  deschedule_until_finished_fences_.erase(
      deschedule_until_finished_fences_.begin());
  reschedule_after_finished_callback_.Run();
}

bool GLES2DecoderImpl::HasMoreIdleWork() const {
  return !pending_readpixel_fences_.empty() ||
         gpu_tracer_->HasTracesToProcess();
}

void GLES2DecoderImpl::PerformIdleWork() {
  gpu_tracer_->ProcessTraces();
  ProcessPendingReadPixels(false);
}

bool GLES2DecoderImpl::HasPollingWork() const {
  return deschedule_until_finished_fences_.size() >= 2;
}

void GLES2DecoderImpl::PerformPollingWork() {
  ProcessDescheduleUntilFinished();
}

error::Error GLES2DecoderImpl::HandleBeginQueryEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BeginQueryEXT& c =
      *static_cast<const volatile gles2::cmds::BeginQueryEXT*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLuint client_id = static_cast<GLuint>(c.id);
  int32_t sync_shm_id = static_cast<int32_t>(c.sync_data_shm_id);
  uint32_t sync_shm_offset = static_cast<uint32_t>(c.sync_data_shm_offset);

  switch (target) {
    case GL_COMMANDS_ISSUED_CHROMIUM:
    case GL_LATENCY_QUERY_CHROMIUM:
    case GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM:
    case GL_GET_ERROR_QUERY_CHROMIUM:
      break;
    case GL_COMMANDS_COMPLETED_CHROMIUM:
      if (!features().chromium_sync_query) {
        LOCAL_SET_GL_ERROR(
            GL_INVALID_OPERATION, "glBeginQueryEXT",
            "not enabled for commands completed queries");
        return error::kNoError;
      }
      break;
    case GL_SAMPLES_PASSED:
    case GL_ANY_SAMPLES_PASSED:
    case GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
      if (!features().occlusion_query_boolean) {
        LOCAL_SET_GL_ERROR(
            GL_INVALID_OPERATION, "glBeginQueryEXT",
            "not enabled for occlusion queries");
        return error::kNoError;
      }
      break;
    case GL_TIME_ELAPSED:
      if (!query_manager_->GPUTimingAvailable()) {
        LOCAL_SET_GL_ERROR(
            GL_INVALID_OPERATION, "glBeginQueryEXT",
            "not enabled for timing queries");
        return error::kNoError;
      }
      break;
    case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
      if (feature_info_->IsWebGL2OrES3Context()) {
        break;
      }
      // Fall through.
    default:
      LOCAL_SET_GL_ERROR(
          GL_INVALID_ENUM, "glBeginQueryEXT",
          "unknown query target");
      return error::kNoError;
  }

  if (query_manager_->GetActiveQuery(target)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, "glBeginQueryEXT", "query already in progress");
    return error::kNoError;
  }

  if (client_id == 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glBeginQueryEXT", "id is 0");
    return error::kNoError;
  }

  QueryManager::Query* query = query_manager_->GetQuery(client_id);
  if (!query) {
    if (!query_manager_->IsValidQuery(client_id)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                         "glBeginQueryEXT",
                         "id not made by glGenQueriesEXT");
      return error::kNoError;
    }
    query = query_manager_->CreateQuery(
        target, client_id, sync_shm_id, sync_shm_offset);
  }

  if (query->target() != target) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, "glBeginQueryEXT", "target does not match");
    return error::kNoError;
  } else if (query->shm_id() != sync_shm_id ||
             query->shm_offset() != sync_shm_offset) {
    DLOG(ERROR) << "Shared memory used by query not the same as before";
    return error::kInvalidArguments;
  }

  if (!query_manager_->BeginQuery(query)) {
    return error::kOutOfBounds;
  }

  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleEndQueryEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::EndQueryEXT& c =
      *static_cast<const volatile gles2::cmds::EndQueryEXT*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  uint32_t submit_count = static_cast<GLuint>(c.submit_count);

  QueryManager::Query* query = query_manager_->GetActiveQuery(target);
  if (!query) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, "glEndQueryEXT", "No active query");
    return error::kNoError;
  }

  if (!query_manager_->EndQuery(query, submit_count)) {
    return error::kOutOfBounds;
  }

  query_manager_->ProcessPendingTransferQueries();

  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleQueryCounterEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::QueryCounterEXT& c =
      *static_cast<const volatile gles2::cmds::QueryCounterEXT*>(cmd_data);
  GLuint client_id = static_cast<GLuint>(c.id);
  GLenum target = static_cast<GLenum>(c.target);
  int32_t sync_shm_id = static_cast<int32_t>(c.sync_data_shm_id);
  uint32_t sync_shm_offset = static_cast<uint32_t>(c.sync_data_shm_offset);
  uint32_t submit_count = static_cast<GLuint>(c.submit_count);

  switch (target) {
    case GL_TIMESTAMP:
      if (!query_manager_->GPUTimingAvailable()) {
        LOCAL_SET_GL_ERROR(
            GL_INVALID_OPERATION, "glQueryCounterEXT",
            "not enabled for timing queries");
        return error::kNoError;
      }
      break;
    default:
      LOCAL_SET_GL_ERROR(
          GL_INVALID_ENUM, "glQueryCounterEXT",
          "unknown query target");
      return error::kNoError;
  }

  QueryManager::Query* query = query_manager_->GetQuery(client_id);
  if (!query) {
    if (!query_manager_->IsValidQuery(client_id)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                         "glQueryCounterEXT",
                         "id not made by glGenQueriesEXT");
      return error::kNoError;
    }
    query = query_manager_->CreateQuery(
        target, client_id, sync_shm_id, sync_shm_offset);
  }
  if (!query_manager_->QueryCounter(query, submit_count)) {
    return error::kOutOfBounds;
  }

  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleSetDisjointValueSyncCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::SetDisjointValueSyncCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::SetDisjointValueSyncCHROMIUM*>(
          cmd_data);
  int32_t sync_shm_id = static_cast<int32_t>(c.sync_data_shm_id);
  uint32_t sync_shm_offset = static_cast<uint32_t>(c.sync_data_shm_offset);

  return query_manager_->SetDisjointSync(sync_shm_id, sync_shm_offset);
}

bool GLES2DecoderImpl::GenVertexArraysOESHelper(
    GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (GetVertexAttribManager(client_ids[ii])) {
      return false;
    }
  }

  if (!features().native_vertex_array_object) {
    // Emulated VAO
    for (GLsizei ii = 0; ii < n; ++ii) {
      CreateVertexAttribManager(client_ids[ii], 0, true);
    }
  } else {
    std::unique_ptr<GLuint[]> service_ids(new GLuint[n]);

    glGenVertexArraysOES(n, service_ids.get());
    for (GLsizei ii = 0; ii < n; ++ii) {
      CreateVertexAttribManager(client_ids[ii], service_ids[ii], true);
    }
  }

  return true;
}

void GLES2DecoderImpl::DeleteVertexArraysOESHelper(
    GLsizei n,
    const volatile GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    GLuint client_id = client_ids[ii];
    VertexAttribManager* vao = GetVertexAttribManager(client_id);
    if (vao && !vao->IsDeleted()) {
      if (state_.vertex_attrib_manager.get() == vao) {
        DoBindVertexArrayOES(0);
      }
      RemoveVertexAttribManager(client_id);
    }
  }
}

void GLES2DecoderImpl::DoBindVertexArrayOES(GLuint client_id) {
  VertexAttribManager* vao = NULL;
  if (client_id != 0) {
    vao = GetVertexAttribManager(client_id);
    if (!vao) {
      // Unlike most Bind* methods, the spec explicitly states that VertexArray
      // only allows names that have been previously generated. As such, we do
      // not generate new names here.
      LOCAL_SET_GL_ERROR(
          GL_INVALID_OPERATION,
          "glBindVertexArrayOES", "bad vertex array id.");
      current_decoder_error_ = error::kNoError;
      return;
    }
  } else {
    vao = state_.default_vertex_attrib_manager.get();
  }

  // Only set the VAO state if it's changed
  if (state_.vertex_attrib_manager.get() != vao) {
    state_.vertex_attrib_manager = vao;
    if (!features().native_vertex_array_object) {
      EmulateVertexArrayState();
    } else {
      GLuint service_id = vao->service_id();
      glBindVertexArrayOES(service_id);
    }
  }
}

// Used when OES_vertex_array_object isn't natively supported
void GLES2DecoderImpl::EmulateVertexArrayState() {
  // Setup the Vertex attribute state
  for (uint32_t vv = 0; vv < group_->max_vertex_attribs(); ++vv) {
    RestoreStateForAttrib(vv, true);
  }

  // Setup the element buffer
  Buffer* element_array_buffer =
      state_.vertex_attrib_manager->element_array_buffer();
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
      element_array_buffer ? element_array_buffer->service_id() : 0);
}

bool GLES2DecoderImpl::DoIsVertexArrayOES(GLuint client_id) {
  const VertexAttribManager* vao =
      GetVertexAttribManager(client_id);
  return vao && vao->IsValid() && !vao->IsDeleted();
}

bool GLES2DecoderImpl::DoIsPathCHROMIUM(GLuint client_id) {
  GLuint service_id = 0;
  return path_manager()->GetPath(client_id, &service_id) &&
         glIsPathNV(service_id) == GL_TRUE;
}

bool GLES2DecoderImpl::DoIsSync(GLuint client_id) {
  GLsync service_sync = 0;
  return group_->GetSyncServiceId(client_id, &service_sync);
}

bool GLES2DecoderImpl::ValidateCopyTextureCHROMIUMTextures(
    const char* function_name,
    TextureRef* source_texture_ref,
    TextureRef* dest_texture_ref) {
  if (!source_texture_ref || !dest_texture_ref) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "unknown texture id");
    return false;
  }

  Texture* source_texture = source_texture_ref->texture();
  Texture* dest_texture = dest_texture_ref->texture();
  if (source_texture == dest_texture) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                       "source and destination textures are the same");
    return false;
  }

  switch (dest_texture->target()) {
    case GL_TEXTURE_2D:
    case GL_TEXTURE_RECTANGLE_ARB:
      break;
    default:
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name,
                       "invalid dest texture target binding");
    return false;
  }

  switch (source_texture->target()) {
    case GL_TEXTURE_2D:
    case GL_TEXTURE_RECTANGLE_ARB:
    case GL_TEXTURE_EXTERNAL_OES:
      break;
    default:
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name,
                       "invalid source texture target binding");
    return false;
  }
  return true;
}

bool GLES2DecoderImpl::ValidateCopyTextureCHROMIUMInternalFormats(
    const char* function_name,
    TextureRef* source_texture_ref,
    GLenum dest_internal_format) {
  GLenum source_type = 0;
  GLenum source_internal_format = 0;
  Texture* source_texture = source_texture_ref->texture();
  source_texture->GetLevelType(source_texture->target(), 0, &source_type,
                               &source_internal_format);

  // The destination format should be GL_RGB, or GL_RGBA. GL_ALPHA,
  // GL_LUMINANCE, and GL_LUMINANCE_ALPHA are not supported because they are not
  // renderable on some platforms.
  bool valid_dest_format =
      dest_internal_format == GL_RGB || dest_internal_format == GL_RGBA ||
      dest_internal_format == GL_RGB8 || dest_internal_format == GL_RGBA8 ||
      dest_internal_format == GL_BGRA_EXT ||
      dest_internal_format == GL_BGRA8_EXT;
  bool valid_source_format =
      source_internal_format == GL_RED || source_internal_format == GL_ALPHA ||
      source_internal_format == GL_RGB || source_internal_format == GL_RGBA ||
      source_internal_format == GL_RGB8 || source_internal_format == GL_RGBA8 ||
      source_internal_format == GL_LUMINANCE ||
      source_internal_format == GL_LUMINANCE_ALPHA ||
      source_internal_format == GL_BGRA_EXT ||
      source_internal_format == GL_BGRA8_EXT ||
      source_internal_format == GL_RGB_YCBCR_420V_CHROMIUM ||
      source_internal_format == GL_RGB_YCBCR_422_CHROMIUM;
  if (!valid_source_format) {
    std::string msg = "invalid source internal format " +
        GLES2Util::GetStringEnum(source_internal_format);
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                       msg.c_str());
    return false;
  }
  if (!valid_dest_format) {
    std::string msg = "invalid dest internal format " +
        GLES2Util::GetStringEnum(dest_internal_format);
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                       msg.c_str());
    return false;
  }
  return true;
}

bool GLES2DecoderImpl::ValidateCompressedCopyTextureCHROMIUM(
    const char* function_name,
    TextureRef* source_texture_ref,
    TextureRef* dest_texture_ref) {
  if (!source_texture_ref || !dest_texture_ref) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "unknown texture id");
    return false;
  }

  Texture* source_texture = source_texture_ref->texture();
  Texture* dest_texture = dest_texture_ref->texture();
  if (source_texture == dest_texture) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                       "source and destination textures are the same");
    return false;
  }

  if (dest_texture->target() != GL_TEXTURE_2D ||
      (source_texture->target() != GL_TEXTURE_2D &&
       source_texture->target() != GL_TEXTURE_RECTANGLE_ARB &&
       source_texture->target() != GL_TEXTURE_EXTERNAL_OES)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name,
                       "invalid texture target binding");
    return false;
  }

  GLenum source_type = 0;
  GLenum source_internal_format = 0;
  source_texture->GetLevelType(source_texture->target(), 0, &source_type,
                               &source_internal_format);

  bool valid_format =
      source_internal_format == GL_ATC_RGB_AMD ||
      source_internal_format == GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD ||
      source_internal_format == GL_COMPRESSED_RGB_S3TC_DXT1_EXT ||
      source_internal_format == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT ||
      source_internal_format == GL_ETC1_RGB8_OES;

  if (!valid_format) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, function_name,
                       "invalid internal format");
    return false;
  }

  return true;
}

void GLES2DecoderImpl::DoCopyTextureCHROMIUM(
    GLuint source_id,
    GLuint dest_id,
    GLenum internal_format,
    GLenum dest_type,
    GLboolean unpack_flip_y,
    GLboolean unpack_premultiply_alpha,
    GLboolean unpack_unmultiply_alpha) {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::DoCopyTextureCHROMIUM");
  static const char kFunctionName[] = "glCopyTextureCHROMIUM";

  TextureRef* source_texture_ref = GetTexture(source_id);
  TextureRef* dest_texture_ref = GetTexture(dest_id);

  if (!texture_manager()->ValidateTextureParameters(
          GetErrorState(), kFunctionName, true, internal_format, dest_type,
          internal_format, 0))
    return;

  if (!ValidateCopyTextureCHROMIUMTextures(kFunctionName, source_texture_ref,
                                           dest_texture_ref)) {
    return;
  }

  if (!ValidateCopyTextureCHROMIUMInternalFormats(
          kFunctionName, source_texture_ref, internal_format)) {
    return;
  }

  Texture* source_texture = source_texture_ref->texture();
  Texture* dest_texture = dest_texture_ref->texture();
  GLenum source_target = source_texture->target();
  GLenum dest_target = dest_texture->target();
  int source_width = 0;
  int source_height = 0;
  gl::GLImage* image =
      source_texture->GetLevelImage(source_target, 0);
  if (image) {
    gfx::Size size = image->GetSize();
    source_width = size.width();
    source_height = size.height();
    if (source_width <= 0 || source_height <= 0) {
      LOCAL_SET_GL_ERROR(
          GL_INVALID_VALUE,
          "glCopyTextureChromium", "invalid image size");
      return;
    }
  } else {
    if (!source_texture->GetLevelSize(source_target, 0,
                                      &source_width, &source_height, nullptr)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE,
                         "glCopyTextureChromium",
                         "source texture has no level 0");
      return;
    }

    // Check that this type of texture is allowed.
    if (!texture_manager()->ValidForTarget(source_target, 0,
                                           source_width, source_height, 1)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, kFunctionName, "Bad dimensions");
      return;
    }
  }

  GLenum source_type = 0;
  GLenum source_internal_format = 0;
  source_texture->GetLevelType(source_target, 0, &source_type,
                               &source_internal_format);

  if (dest_texture->IsImmutable()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, kFunctionName,
                       "texture is immutable");
    return;
  }

  // Clear the source texture if necessary.
  if (!texture_manager()->ClearTextureLevel(this, source_texture_ref,
                                            source_target, 0)) {
    LOCAL_SET_GL_ERROR(GL_OUT_OF_MEMORY, kFunctionName, "dimensions too big");
    return;
  }

  if (!InitializeCopyTextureCHROMIUM(kFunctionName))
    return;

  GLenum dest_type_previous = dest_type;
  GLenum dest_internal_format = internal_format;
  int dest_width = 0;
  int dest_height = 0;
  bool dest_level_defined = dest_texture->GetLevelSize(
      dest_target, 0, &dest_width, &dest_height, nullptr);

  if (dest_level_defined) {
    dest_texture->GetLevelType(dest_target, 0, &dest_type_previous,
                               &dest_internal_format);
  }

  // Resize the destination texture to the dimensions of the source texture.
  if (!dest_level_defined || dest_width != source_width ||
      dest_height != source_height ||
      dest_internal_format != internal_format ||
      dest_type_previous != dest_type) {
    // Ensure that the glTexImage2D succeeds.
    LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER(kFunctionName);
    glBindTexture(dest_target, dest_texture->service_id());
    glTexImage2D(
        dest_target, 0, TextureManager::AdjustTexInternalFormat(
                            feature_info_.get(), internal_format),
        source_width, source_height, 0,
        TextureManager::AdjustTexFormat(feature_info_.get(), internal_format),
        dest_type, NULL);
    GLenum error = LOCAL_PEEK_GL_ERROR(kFunctionName);
    if (error != GL_NO_ERROR) {
      RestoreCurrentTextureBindings(&state_, dest_target);
      return;
    }

    texture_manager()->SetLevelInfo(
        dest_texture_ref, dest_target, 0, internal_format, source_width,
        source_height, 1, 0, internal_format, dest_type,
        gfx::Rect(source_width, source_height));
    dest_texture->ApplyFormatWorkarounds(feature_info_.get());
  } else {
    texture_manager()->SetLevelCleared(dest_texture_ref, dest_target, 0,
                                       true);
  }

  // Try using GLImage::CopyTexImage when possible.
  bool unpack_premultiply_alpha_change =
      (unpack_premultiply_alpha ^ unpack_unmultiply_alpha) != 0;
  if (image && !unpack_flip_y && !unpack_premultiply_alpha_change) {
    glBindTexture(dest_target, dest_texture->service_id());
    if (image->CopyTexImage(dest_target))
      return;
  }

  DoCopyTexImageIfNeeded(source_texture, source_target);

  // GL_TEXTURE_EXTERNAL_OES texture requires that we apply a transform matrix
  // before presenting.
  if (source_target == GL_TEXTURE_EXTERNAL_OES) {
    if (GLStreamTextureImage* image =
            source_texture->GetLevelStreamTextureImage(GL_TEXTURE_EXTERNAL_OES,
                                                       0)) {
      GLfloat transform_matrix[16];
      image->GetTextureMatrix(transform_matrix);
      copy_texture_CHROMIUM_->DoCopyTextureWithTransform(
          this, source_target, source_texture->service_id(), dest_target,
          dest_texture->service_id(), source_width, source_height,
          unpack_flip_y == GL_TRUE, unpack_premultiply_alpha == GL_TRUE,
          unpack_unmultiply_alpha == GL_TRUE, transform_matrix);
      return;
    }
  }
  copy_texture_CHROMIUM_->DoCopyTexture(
      this, source_target, source_texture->service_id(), source_internal_format,
      dest_target, dest_texture->service_id(), internal_format, source_width,
      source_height, unpack_flip_y == GL_TRUE,
      unpack_premultiply_alpha == GL_TRUE, unpack_unmultiply_alpha == GL_TRUE);
}

void GLES2DecoderImpl::DoCopySubTextureCHROMIUM(
    GLuint source_id,
    GLuint dest_id,
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLboolean unpack_flip_y,
    GLboolean unpack_premultiply_alpha,
    GLboolean unpack_unmultiply_alpha) {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::DoCopySubTextureCHROMIUM");

  static const char kFunctionName[] = "glCopySubTextureCHROMIUM";
  TextureRef* source_texture_ref = GetTexture(source_id);
  TextureRef* dest_texture_ref = GetTexture(dest_id);

  if (!ValidateCopyTextureCHROMIUMTextures(kFunctionName, source_texture_ref,
                                           dest_texture_ref)) {
    return;
  }

  Texture* source_texture = source_texture_ref->texture();
  Texture* dest_texture = dest_texture_ref->texture();
  GLenum source_target = source_texture->target();
  GLenum dest_target = dest_texture->target();
  int source_width = 0;
  int source_height = 0;
  gl::GLImage* image =
      source_texture->GetLevelImage(source_target, 0);
  if (image) {
    gfx::Size size = image->GetSize();
    source_width = size.width();
    source_height = size.height();
    if (source_width <= 0 || source_height <= 0) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, kFunctionName, "invalid image size");
      return;
    }

    // Ideally we should not need to check that the sub-texture copy rectangle
    // is valid in two different ways, here and below. However currently there
    // is no guarantee that a texture backed by a GLImage will have sensible
    // level info. If this synchronization were to be enforced then this and
    // other functions in this file could be cleaned up.
    // See: https://crbug.com/586476
    int32_t max_x;
    int32_t max_y;
    if (!SafeAddInt32(x, width, &max_x) || !SafeAddInt32(y, height, &max_y) ||
        x < 0 || y < 0 || max_x > source_width || max_y > source_height) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, kFunctionName,
                         "source texture bad dimensions");
      return;
    }
  } else {
    if (!source_texture->GetLevelSize(source_target, 0,
                                      &source_width, &source_height, nullptr)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, kFunctionName,
                         "source texture has no level 0");
      return;
    }

    // Check that this type of texture is allowed.
    if (!texture_manager()->ValidForTarget(source_target, 0,
                                           source_width, source_height, 1)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, kFunctionName,
                         "source texture bad dimensions");
      return;
    }

    if (!source_texture->ValidForTexture(source_target, 0, x, y, 0, width,
                                         height, 1)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, kFunctionName,
                         "source texture bad dimensions.");
      return;
    }
  }

  GLenum source_type = 0;
  GLenum source_internal_format = 0;
  source_texture->GetLevelType(source_target, 0, &source_type,
                               &source_internal_format);

  GLenum dest_type = 0;
  GLenum dest_internal_format = 0;
  bool dest_level_defined = dest_texture->GetLevelType(
      dest_target, 0, &dest_type, &dest_internal_format);
  if (!dest_level_defined) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, kFunctionName,
                       "destination texture is not defined");
    return;
  }
  if (!dest_texture->ValidForTexture(dest_target, 0, xoffset,
                                     yoffset, 0, width, height, 1)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, kFunctionName,
                       "destination texture bad dimensions.");
    return;
  }

  if (!ValidateCopyTextureCHROMIUMInternalFormats(
          kFunctionName, source_texture_ref, dest_internal_format)) {
    return;
  }

  // Clear the source texture if necessary.
  if (!texture_manager()->ClearTextureLevel(this, source_texture_ref,
                                            source_target, 0)) {
    LOCAL_SET_GL_ERROR(GL_OUT_OF_MEMORY, kFunctionName,
                       "source texture dimensions too big");
    return;
  }

  if (!InitializeCopyTextureCHROMIUM(kFunctionName))
    return;

  int dest_width = 0;
  int dest_height = 0;
  bool ok = dest_texture->GetLevelSize(
      dest_target, 0, &dest_width, &dest_height, nullptr);
  DCHECK(ok);
  if (xoffset != 0 || yoffset != 0 || width != dest_width ||
      height != dest_height) {
    gfx::Rect cleared_rect;
    if (TextureManager::CombineAdjacentRects(
            dest_texture->GetLevelClearedRect(dest_target, 0),
            gfx::Rect(xoffset, yoffset, width, height), &cleared_rect)) {
      DCHECK_GE(
          cleared_rect.size().GetArea(),
          dest_texture->GetLevelClearedRect(dest_target, 0).size().GetArea());
      texture_manager()->SetLevelClearedRect(dest_texture_ref, dest_target, 0,
                                             cleared_rect);
    } else {
      // Otherwise clear part of texture level that is not already cleared.
      if (!texture_manager()->ClearTextureLevel(this, dest_texture_ref,
                                                dest_target, 0)) {
        LOCAL_SET_GL_ERROR(GL_OUT_OF_MEMORY, kFunctionName,
                           "destination texture dimensions too big");
        return;
      }
    }
  } else {
    texture_manager()->SetLevelCleared(dest_texture_ref, dest_target, 0,
                                       true);
  }

  // Try using GLImage::CopyTexSubImage when possible.
  bool unpack_premultiply_alpha_change =
      (unpack_premultiply_alpha ^ unpack_unmultiply_alpha) != 0;
  if (image && !unpack_flip_y && !unpack_premultiply_alpha_change) {
    ScopedTextureBinder binder(
        &state_, dest_texture->service_id(), dest_target);
    if (image->CopyTexSubImage(dest_target, gfx::Point(xoffset, yoffset),
                               gfx::Rect(x, y, width, height))) {
      return;
    }
  }

  DoCopyTexImageIfNeeded(source_texture, source_target);

  // GL_TEXTURE_EXTERNAL_OES texture requires apply a transform matrix
  // before presenting.
  if (source_target == GL_TEXTURE_EXTERNAL_OES) {
    if (GLStreamTextureImage* image =
            source_texture->GetLevelStreamTextureImage(GL_TEXTURE_EXTERNAL_OES,
                                                       0)) {
      GLfloat transform_matrix[16];
      image->GetTextureMatrix(transform_matrix);
      copy_texture_CHROMIUM_->DoCopySubTextureWithTransform(
          this, source_target, source_texture->service_id(),
          source_internal_format, dest_target, dest_texture->service_id(),
          dest_internal_format, xoffset, yoffset, x, y, width, height,
          dest_width, dest_height, source_width, source_height,
          unpack_flip_y == GL_TRUE, unpack_premultiply_alpha == GL_TRUE,
          unpack_unmultiply_alpha == GL_TRUE, transform_matrix);
      return;
    }
  }
  copy_texture_CHROMIUM_->DoCopySubTexture(
      this, source_target, source_texture->service_id(), source_internal_format,
      dest_target, dest_texture->service_id(), dest_internal_format, xoffset,
      yoffset, x, y, width, height, dest_width, dest_height, source_width,
      source_height, unpack_flip_y == GL_TRUE,
      unpack_premultiply_alpha == GL_TRUE, unpack_unmultiply_alpha == GL_TRUE);
}

bool GLES2DecoderImpl::InitializeCopyTexImageBlitter(
    const char* function_name) {
  if (!copy_tex_image_blit_.get()) {
    LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER(function_name);
    copy_tex_image_blit_.reset(
        new CopyTexImageResourceManager(feature_info_.get()));
    copy_tex_image_blit_->Initialize(this);
    if (LOCAL_PEEK_GL_ERROR(function_name) != GL_NO_ERROR)
      return false;
  }
  return true;
}

bool GLES2DecoderImpl::InitializeCopyTextureCHROMIUM(
    const char* function_name) {
  // Defer initializing the CopyTextureCHROMIUMResourceManager until it is
  // needed because it takes 10s of milliseconds to initialize.
  if (!copy_texture_CHROMIUM_.get()) {
    LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER(function_name);
    copy_texture_CHROMIUM_.reset(new CopyTextureCHROMIUMResourceManager());
    copy_texture_CHROMIUM_->Initialize(this, features());
    if (LOCAL_PEEK_GL_ERROR(function_name) != GL_NO_ERROR)
      return false;
  }
  return true;
}

void GLES2DecoderImpl::DoCompressedCopyTextureCHROMIUM(GLuint source_id,
                                                       GLuint dest_id) {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::DoCompressedCopyTextureCHROMIUM");
  static const char kFunctionName[] = "glCompressedCopyTextureCHROMIUM";
  TextureRef* source_texture_ref = GetTexture(source_id);
  TextureRef* dest_texture_ref = GetTexture(dest_id);

  if (!source_texture_ref || !dest_texture_ref) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, kFunctionName, "unknown texture ids");
    return;
  }

  if (!ValidateCompressedCopyTextureCHROMIUM(kFunctionName, source_texture_ref,
                                             dest_texture_ref)) {
    return;
  }

  Texture* source_texture = source_texture_ref->texture();
  Texture* dest_texture = dest_texture_ref->texture();
  int source_width = 0;
  int source_height = 0;
  gl::GLImage* image =
      source_texture->GetLevelImage(source_texture->target(), 0);
  if (image) {
    gfx::Size size = image->GetSize();
    source_width = size.width();
    source_height = size.height();
    if (source_width <= 0 || source_height <= 0) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, kFunctionName, "invalid image size");
      return;
    }
  } else {
    if (!source_texture->GetLevelSize(source_texture->target(), 0,
                                      &source_width, &source_height, nullptr)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, kFunctionName,
                         "source texture has no level 0");
      return;
    }

    // Check that this type of texture is allowed.
    if (!texture_manager()->ValidForTarget(source_texture->target(), 0,
                                           source_width, source_height, 1)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, kFunctionName, "Bad dimensions");
      return;
    }
  }

  GLenum source_type = 0;
  GLenum source_internal_format = 0;
  source_texture->GetLevelType(
      source_texture->target(), 0, &source_type, &source_internal_format);

  if (dest_texture->IsImmutable()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, kFunctionName,
                       "texture is immutable");
    return;
  }

  if (!InitializeCopyTextureCHROMIUM(kFunctionName))
    return;

  // Clear the source texture if necessary.
  if (!texture_manager()->ClearTextureLevel(this, source_texture_ref,
                                            source_texture->target(), 0)) {
    LOCAL_SET_GL_ERROR(GL_OUT_OF_MEMORY, kFunctionName, "dimensions too big");
    return;
  }

  ScopedTextureBinder binder(
      &state_, dest_texture->service_id(), GL_TEXTURE_2D);

  // Try using GLImage::CopyTexImage when possible.
  if (image) {
    GLenum dest_type = 0;
    GLenum dest_internal_format = 0;
    int dest_width = 0;
    int dest_height = 0;
    bool dest_level_defined = dest_texture->GetLevelSize(
        dest_texture->target(), 0, &dest_width, &dest_height, nullptr);

    if (dest_level_defined) {
      dest_texture->GetLevelType(dest_texture->target(), 0, &dest_type,
                                 &dest_internal_format);
    }

    // Resize the destination texture to the dimensions of the source texture.
    if (!dest_level_defined || dest_width != source_width ||
        dest_height != source_height ||
        dest_internal_format != source_internal_format) {
      GLsizei source_size = 0;

      bool did_get_size = GetCompressedTexSizeInBytes(
          kFunctionName, source_width, source_height, 1, source_internal_format,
          &source_size);
      DCHECK(did_get_size);

      // Ensure that the glCompressedTexImage2D succeeds.
      LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER(kFunctionName);
      glCompressedTexImage2D(GL_TEXTURE_2D, 0, source_internal_format,
                             source_width, source_height, 0, source_size,
                             NULL);
      GLenum error = LOCAL_PEEK_GL_ERROR(kFunctionName);
      if (error != GL_NO_ERROR) {
        RestoreCurrentTextureBindings(&state_, dest_texture->target());
        return;
      }

      texture_manager()->SetLevelInfo(
          dest_texture_ref, dest_texture->target(), 0, source_internal_format,
          source_width, source_height, 1, 0, source_internal_format,
          source_type, gfx::Rect(source_width, source_height));
    } else {
      texture_manager()->SetLevelCleared(
          dest_texture_ref, dest_texture->target(), 0, true);
    }

    if (image->CopyTexImage(dest_texture->target()))
      return;
  }

  TRACE_EVENT0(
      "gpu",
      "GLES2DecoderImpl::DoCompressedCopyTextureCHROMIUM, fallback");

  DoCopyTexImageIfNeeded(source_texture, source_texture->target());

  // As a fallback, copy into a non-compressed GL_RGBA texture.
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER(kFunctionName);
  glTexImage2D(dest_texture->target(), 0, GL_RGBA, source_width, source_height,
               0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  GLenum error = LOCAL_PEEK_GL_ERROR(kFunctionName);
  if (error != GL_NO_ERROR) {
    RestoreCurrentTextureBindings(&state_, dest_texture->target());
    return;
  }

  texture_manager()->SetLevelInfo(
      dest_texture_ref, dest_texture->target(), 0, GL_RGBA, source_width,
      source_height, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
      gfx::Rect(source_width, source_height));

  copy_texture_CHROMIUM_->DoCopyTexture(
      this, source_texture->target(), source_texture->service_id(),
      source_internal_format, dest_texture->target(),
      dest_texture->service_id(), GL_RGBA, source_width, source_height, false,
      false, false);
}

void GLES2DecoderImpl::TexStorageImpl(GLenum target,
                                      GLsizei levels,
                                      GLenum internal_format,
                                      GLsizei width,
                                      GLsizei height,
                                      GLsizei depth,
                                      ContextState::Dimension dimension,
                                      const char* function_name) {
  if (levels == 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "levels == 0");
    return;
  }
  bool is_compressed_format = IsCompressedTextureFormat(internal_format);
  if (is_compressed_format && target == GL_TEXTURE_3D) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, function_name, "target invalid for format");
    return;
  }
  if (!texture_manager()->ValidForTarget(target, 0, width, height, depth) ||
      TextureManager::ComputeMipMapCount(
          target, width, height, depth) < levels) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE, function_name, "dimensions out of range");
    return;
  }
  TextureRef* texture_ref = texture_manager()->GetTextureInfoForTarget(
      &state_, target);
  if (!texture_ref) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, function_name, "unknown texture for target");
    return;
  }
  Texture* texture = texture_ref->texture();
  if (texture->IsAttachedToFramebuffer()) {
    framebuffer_state_.clear_state_dirty = true;
  }
  if (texture->IsImmutable()) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, function_name, "texture is immutable");
    return;
  }

  GLenum format = TextureManager::ExtractFormatFromStorageFormat(
      internal_format);
  GLenum type = TextureManager::ExtractTypeFromStorageFormat(internal_format);

  std::vector<int32_t> level_size(levels);
  {
    GLsizei level_width = width;
    GLsizei level_height = height;
    GLsizei level_depth = depth;
    base::CheckedNumeric<uint32_t> estimated_size(0);
    PixelStoreParams params;
    params.alignment = 1;
    for (int ii = 0; ii < levels; ++ii) {
      uint32_t size;
      if (is_compressed_format) {
        if (!GetCompressedTexSizeInBytes(function_name,
                                         level_width, level_height, level_depth,
                                         internal_format, &level_size[ii])) {
          // GetCompressedTexSizeInBytes() already generates a GL error.
          return;
        }
        size = static_cast<uint32_t>(level_size[ii]);
      } else {
        if (!GLES2Util::ComputeImageDataSizesES3(level_width,
                                                 level_height,
                                                 level_depth,
                                                 format, type,
                                                 params,
                                                 &size,
                                                 nullptr, nullptr,
                                                 nullptr, nullptr)) {
          LOCAL_SET_GL_ERROR(
              GL_OUT_OF_MEMORY, function_name, "dimensions too large");
          return;
        }
      }
      estimated_size += size;
      level_width = std::max(1, level_width >> 1);
      level_height = std::max(1, level_height >> 1);
      if (target == GL_TEXTURE_3D)
        level_depth = std::max(1, level_depth >> 1);
    }
    if (!estimated_size.IsValid() ||
        !EnsureGPUMemoryAvailable(estimated_size.ValueOrDefault(0))) {
      LOCAL_SET_GL_ERROR(GL_OUT_OF_MEMORY, function_name, "out of memory");
      return;
    }
  }

  GLenum compatibility_internal_format = internal_format;
  const CompressedFormatInfo* format_info =
      GetCompressedFormatInfo(internal_format);
  if (format_info != nullptr && !format_info->support_check(*feature_info_)) {
    compatibility_internal_format = format_info->decompressed_internal_format;
  }

  if (workarounds().reset_base_mipmap_level_before_texstorage &&
      texture->base_level() > 0)
    glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);

  // TODO(zmo): We might need to emulate TexStorage using TexImage or
  // CompressedTexImage on Mac OSX where we expose ES3 APIs when the underlying
  // driver is lower than 4.2 and ARB_texture_storage extension doesn't exist.
  if (dimension == ContextState::k2D) {
    glTexStorage2DEXT(target, levels, compatibility_internal_format, width,
                      height);
  } else {
    glTexStorage3D(target, levels, compatibility_internal_format, width, height,
                   depth);
  }
  if (workarounds().reset_base_mipmap_level_before_texstorage &&
      texture->base_level() > 0)
    glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, texture->base_level());

  {
    GLsizei level_width = width;
    GLsizei level_height = height;
    GLsizei level_depth = depth;

    GLenum adjusted_internal_format =
        feature_info_->context_type() == CONTEXT_TYPE_OPENGLES2 ?
        format : internal_format;
    for (int ii = 0; ii < levels; ++ii) {
      if (target == GL_TEXTURE_CUBE_MAP) {
        for (int jj = 0; jj < 6; ++jj) {
          GLenum face = GL_TEXTURE_CUBE_MAP_POSITIVE_X + jj;
          texture_manager()->SetLevelInfo(
              texture_ref, face, ii, adjusted_internal_format, level_width,
              level_height, 1, 0, format, type, gfx::Rect());
        }
      } else {
        texture_manager()->SetLevelInfo(
            texture_ref, target, ii, adjusted_internal_format, level_width,
            level_height, level_depth, 0, format, type, gfx::Rect());
      }
      level_width = std::max(1, level_width >> 1);
      level_height = std::max(1, level_height >> 1);
      if (target == GL_TEXTURE_3D)
        level_depth = std::max(1, level_depth >> 1);
    }
    texture->SetImmutable(true);
  }
}

void GLES2DecoderImpl::DoTexStorage2DEXT(GLenum target,
                                         GLsizei levels,
                                         GLenum internal_format,
                                         GLsizei width,
                                         GLsizei height) {
  TRACE_EVENT2("gpu", "GLES2DecoderImpl::DoTexStorage2D",
      "width", width, "height", height);
  TexStorageImpl(target, levels, internal_format, width, height, 1,
                 ContextState::k2D, "glTexStorage2D");
}

void GLES2DecoderImpl::DoTexStorage3D(GLenum target,
                                      GLsizei levels,
                                      GLenum internal_format,
                                      GLsizei width,
                                      GLsizei height,
                                      GLsizei depth) {
  TRACE_EVENT2("gpu", "GLES2DecoderImpl::DoTexStorage3D",
      "widthXheight", width * height, "depth", depth);
  TexStorageImpl(target, levels, internal_format, width, height, depth,
                 ContextState::k3D, "glTexStorage3D");
}

void GLES2DecoderImpl::DoProduceTextureCHROMIUM(GLenum target,
                                                const volatile GLbyte* data) {
  TRACE_EVENT2("gpu", "GLES2DecoderImpl::DoProduceTextureCHROMIUM",
      "context", logger_.GetLogPrefix(),
      "mailbox[0]", static_cast<unsigned char>(data[0]));

  TextureRef* texture_ref = texture_manager()->GetTextureInfoForTarget(
      &state_, target);
  ProduceTextureRef("glProduceTextureCHROMIUM", false, texture_ref, target,
                    data);
}

void GLES2DecoderImpl::DoProduceTextureDirectCHROMIUM(
    GLuint client_id,
    GLenum target,
    const volatile GLbyte* data) {
  TRACE_EVENT2("gpu", "GLES2DecoderImpl::DoProduceTextureDirectCHROMIUM",
      "context", logger_.GetLogPrefix(),
      "mailbox[0]", static_cast<unsigned char>(data[0]));

  ProduceTextureRef("glProduceTextureDirectCHROMIUM", !client_id,
                    GetTexture(client_id), target, data);
}

void GLES2DecoderImpl::ProduceTextureRef(const char* func_name,
                                         bool clear,
                                         TextureRef* texture_ref,
                                         GLenum target,
                                         const volatile GLbyte* data) {
  Mailbox mailbox =
      Mailbox::FromVolatile(*reinterpret_cast<const volatile Mailbox*>(data));
  DLOG_IF(ERROR, !mailbox.Verify()) << func_name << " was passed a "
                                       "mailbox that was not generated by "
                                       "GenMailboxCHROMIUM.";

  if (clear) {
    DCHECK(!texture_ref);

    group_->mailbox_manager()->ProduceTexture(mailbox, nullptr);
    return;
  }

  if (!texture_ref) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, func_name, "unknown texture for target");
    return;
  }

  Texture* produced = texture_manager()->Produce(texture_ref);
  if (!produced) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, func_name, "invalid texture");
    return;
  }

  if (produced->target() != target) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION, func_name, "invalid target");
    return;
  }

  group_->mailbox_manager()->ProduceTexture(mailbox, produced);
}

void GLES2DecoderImpl::DoConsumeTextureCHROMIUM(GLenum target,
                                                const volatile GLbyte* data) {
  TRACE_EVENT2("gpu", "GLES2DecoderImpl::DoConsumeTextureCHROMIUM",
      "context", logger_.GetLogPrefix(),
      "mailbox[0]", static_cast<unsigned char>(data[0]));
  Mailbox mailbox =
      Mailbox::FromVolatile(*reinterpret_cast<const volatile Mailbox*>(data));
  DLOG_IF(ERROR, !mailbox.Verify()) << "ConsumeTextureCHROMIUM was passed a "
                                       "mailbox that was not generated by "
                                       "GenMailboxCHROMIUM.";

  scoped_refptr<TextureRef> texture_ref =
      texture_manager()->GetTextureInfoForTargetUnlessDefault(&state_, target);
  if (!texture_ref.get()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                       "glConsumeTextureCHROMIUM",
                       "unknown texture for target");
    return;
  }
  GLuint client_id = texture_ref->client_id();
  if (!client_id) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        "glConsumeTextureCHROMIUM", "unknown texture for target");
    return;
  }

  Texture* texture =
      static_cast<Texture*>(group_->mailbox_manager()->ConsumeTexture(mailbox));
  if (!texture) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        "glConsumeTextureCHROMIUM", "invalid mailbox name");
    return;
  }
  if (texture->target() != target) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        "glConsumeTextureCHROMIUM", "invalid target");
    return;
  }

  DeleteTexturesHelper(1, &client_id);
  texture_ref = texture_manager()->Consume(client_id, texture);
  glBindTexture(target, texture_ref->service_id());

  TextureUnit& unit = state_.texture_units[state_.active_texture_unit];
  unit.bind_target = target;
  unit.GetInfoForTarget(target) = texture_ref;
}

void GLES2DecoderImpl::EnsureTextureForClientId(
    GLenum target,
    GLuint client_id) {
  TextureRef* texture_ref = GetTexture(client_id);
  if (!texture_ref) {
    GLuint service_id;
    glGenTextures(1, &service_id);
    DCHECK_NE(0u, service_id);
    texture_ref = CreateTexture(client_id, service_id);
    texture_manager()->SetTarget(texture_ref, target);
    glBindTexture(target, service_id);
    RestoreCurrentTextureBindings(&state_, target);
  }
}

void GLES2DecoderImpl::DoCreateAndConsumeTextureINTERNAL(
    GLenum target,
    GLuint client_id,
    const volatile GLbyte* data) {
  TRACE_EVENT2("gpu", "GLES2DecoderImpl::DoCreateAndConsumeTextureINTERNAL",
      "context", logger_.GetLogPrefix(),
      "mailbox[0]", static_cast<unsigned char>(data[0]));
  Mailbox mailbox =
      Mailbox::FromVolatile(*reinterpret_cast<const volatile Mailbox*>(data));
  DLOG_IF(ERROR, !mailbox.Verify()) << "CreateAndConsumeTextureCHROMIUM was "
                                       "passed a mailbox that was not "
                                       "generated by GenMailboxCHROMIUM.";

  TextureRef* texture_ref = GetTexture(client_id);
  if (texture_ref) {
    // No need to call EnsureTextureForClientId here, the client_id already has
    // an associated texture.
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        "glCreateAndConsumeTextureCHROMIUM", "client id already in use");
    return;
  }
  Texture* texture =
      static_cast<Texture*>(group_->mailbox_manager()->ConsumeTexture(mailbox));
  if (!texture) {
    EnsureTextureForClientId(target, client_id);
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        "glCreateAndConsumeTextureCHROMIUM", "invalid mailbox name");
    return;
  }

  if (texture->target() != target) {
    EnsureTextureForClientId(target, client_id);
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        "glCreateAndConsumeTextureCHROMIUM", "invalid target");
    return;
  }

  texture_ref = texture_manager()->Consume(client_id, texture);
}

void GLES2DecoderImpl::DoApplyScreenSpaceAntialiasingCHROMIUM() {
  Framebuffer* bound_framebuffer =
      GetFramebufferInfoForTarget(GL_DRAW_FRAMEBUFFER);
  // TODO(dshwang): support it even after glBindFrameBuffer(GL_FRAMEBUFFER, 0).
  // skia will need to render to the window. crbug.com/656618
  if (!bound_framebuffer) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                       "glApplyScreenSpaceAntialiasingCHROMIUM",
                       "no bound framebuffer object");
    return;
  }

  // Apply CMAA(Conservative Morphological Anti-Aliasing) algorithm to the
  // color attachments of currently bound draw framebuffer.
  // Reference GL_INTEL_framebuffer_CMAA for details.
  // Use platform version if available.
  if (!feature_info_->feature_flags()
           .use_chromium_screen_space_antialiasing_via_shaders) {
    glApplyFramebufferAttachmentCMAAINTEL();
  } else {
    // Defer initializing the CopyTextureCHROMIUMResourceManager until it is
    // needed because it takes ??s of milliseconds to initialize.
    if (!apply_framebuffer_attachment_cmaa_intel_.get()) {
      LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER(
          "glApplyFramebufferAttachmentCMAAINTEL");
      apply_framebuffer_attachment_cmaa_intel_.reset(
          new ApplyFramebufferAttachmentCMAAINTELResourceManager());
      apply_framebuffer_attachment_cmaa_intel_->Initialize(this);
      RestoreCurrentFramebufferBindings();
      if (LOCAL_PEEK_GL_ERROR("glApplyFramebufferAttachmentCMAAINTEL") !=
          GL_NO_ERROR)
        return;
    }
    static const char kFunctionName[] =
        "glApplyScreenSpaceAntialiasingCHROMIUM";
    if (!InitializeCopyTextureCHROMIUM(kFunctionName))
      return;
    apply_framebuffer_attachment_cmaa_intel_
        ->ApplyFramebufferAttachmentCMAAINTEL(this, bound_framebuffer,
                                              copy_texture_CHROMIUM_.get());
  }
}

void GLES2DecoderImpl::DoInsertEventMarkerEXT(
    GLsizei length, const GLchar* marker) {
  if (!marker) {
    marker = "";
  }
  debug_marker_manager_.SetMarker(
      length ? std::string(marker, length) : std::string(marker));
}

void GLES2DecoderImpl::DoPushGroupMarkerEXT(
    GLsizei /*length*/, const GLchar* /*marker*/) {
}

void GLES2DecoderImpl::DoPopGroupMarkerEXT(void) {
}

void GLES2DecoderImpl::DoBindTexImage2DCHROMIUM(
    GLenum target, GLint image_id) {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::DoBindTexImage2DCHROMIUM");

  if (target == GL_TEXTURE_CUBE_MAP) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_ENUM,
        "glBindTexImage2DCHROMIUM", "invalid target");
    return;
  }

  // Default target might be conceptually valid, but disallow it to avoid
  // accidents.
  TextureRef* texture_ref =
      texture_manager()->GetTextureInfoForTargetUnlessDefault(&state_, target);
  if (!texture_ref) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        "glBindTexImage2DCHROMIUM", "no texture bound");
    return;
  }

  gl::GLImage* image = image_manager()->LookupImage(image_id);
  if (!image) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        "glBindTexImage2DCHROMIUM", "no image found with the given ID");
    return;
  }

  Texture::ImageState image_state = Texture::UNBOUND;

  {
    ScopedGLErrorSuppressor suppressor(
        "GLES2DecoderImpl::DoBindTexImage2DCHROMIUM", GetErrorState());

    // Note: We fallback to using CopyTexImage() before the texture is used
    // when BindTexImage() fails.
    if (image->BindTexImage(target))
      image_state = Texture::BOUND;
  }

  gfx::Size size = image->GetSize();
  GLenum internalformat = image->GetInternalFormat();
  texture_manager()->SetLevelInfo(
      texture_ref, target, 0, internalformat, size.width(), size.height(), 1, 0,
      internalformat, GL_UNSIGNED_BYTE, gfx::Rect(size));
  texture_manager()->SetLevelImage(texture_ref, target, 0, image, image_state);
}

void GLES2DecoderImpl::DoReleaseTexImage2DCHROMIUM(
    GLenum target, GLint image_id) {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::DoReleaseTexImage2DCHROMIUM");

  // Default target might be conceptually valid, but disallow it to avoid
  // accidents.
  TextureRef* texture_ref =
      texture_manager()->GetTextureInfoForTargetUnlessDefault(&state_, target);
  if (!texture_ref) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        "glReleaseTexImage2DCHROMIUM", "no texture bound");
    return;
  }

  gl::GLImage* image = image_manager()->LookupImage(image_id);
  if (!image) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        "glReleaseTexImage2DCHROMIUM", "no image found with the given ID");
    return;
  }

  Texture::ImageState image_state;

  // Do nothing when image is not currently bound.
  if (texture_ref->texture()->GetLevelImage(target, 0, &image_state) != image)
    return;

  if (image_state == Texture::BOUND) {
    ScopedGLErrorSuppressor suppressor(
        "GLES2DecoderImpl::DoReleaseTexImage2DCHROMIUM", GetErrorState());

    image->ReleaseTexImage(target);
    texture_manager()->SetLevelInfo(texture_ref, target, 0, GL_RGBA, 0, 0, 1, 0,
                                    GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect());
  }

  texture_manager()->SetLevelImage(texture_ref, target, 0, nullptr,
                                   Texture::UNBOUND);
}

error::Error GLES2DecoderImpl::HandleTraceBeginCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::TraceBeginCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::TraceBeginCHROMIUM*>(cmd_data);
  Bucket* category_bucket = GetBucket(c.category_bucket_id);
  Bucket* name_bucket = GetBucket(c.name_bucket_id);
  if (!category_bucket || category_bucket->size() == 0 ||
      !name_bucket || name_bucket->size() == 0) {
    return error::kInvalidArguments;
  }

  std::string category_name;
  std::string trace_name;
  if (!category_bucket->GetAsString(&category_name) ||
      !name_bucket->GetAsString(&trace_name)) {
    return error::kInvalidArguments;
  }

  debug_marker_manager_.PushGroup(trace_name);
  if (!gpu_tracer_->Begin(category_name, trace_name, kTraceCHROMIUM)) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_OPERATION,
        "glTraceBeginCHROMIUM", "unable to create begin trace");
    return error::kNoError;
  }
  return error::kNoError;
}

void GLES2DecoderImpl::DoTraceEndCHROMIUM() {
  debug_marker_manager_.PopGroup();
  if (!gpu_tracer_->End(kTraceCHROMIUM)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                       "glTraceEndCHROMIUM", "no trace begin found");
    return;
  }
}

void GLES2DecoderImpl::DoDrawBuffersEXT(GLsizei count,
                                        const volatile GLenum* bufs) {
  DCHECK_LE(group_->max_draw_buffers(), 16u);
  if (count > static_cast<GLsizei>(group_->max_draw_buffers())) {
    LOCAL_SET_GL_ERROR(
        GL_INVALID_VALUE,
        "glDrawBuffersEXT", "greater than GL_MAX_DRAW_BUFFERS_EXT");
    return;
  }

  Framebuffer* framebuffer = GetFramebufferInfoForTarget(GL_FRAMEBUFFER);
  if (framebuffer) {
    GLenum safe_bufs[16];
    for (GLsizei i = 0; i < count; ++i) {
      GLenum buf = bufs[i];
      if (buf != static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i) &&
          buf != GL_NONE) {
        LOCAL_SET_GL_ERROR(
            GL_INVALID_OPERATION,
            "glDrawBuffersEXT",
            "bufs[i] not GL_NONE or GL_COLOR_ATTACHMENTi_EXT");
        return;
      }
      safe_bufs[i] = buf;
    }
    glDrawBuffersARB(count, safe_bufs);
    framebuffer->SetDrawBuffers(count, safe_bufs);
  } else {  // backbuffer
    if (count != 1) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glDrawBuffersEXT",
                         "invalid number of buffers");
      return;
    }
    GLenum buf = bufs[0];
    if (buf != GL_BACK && buf != GL_NONE) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glDrawBuffersEXT",
                         "buffer is not GL_NONE or GL_BACK");
      return;
    }
    back_buffer_draw_buffer_ = buf;
    if (buf == GL_BACK && GetBackbufferServiceId() != 0)  // emulated backbuffer
      buf = GL_COLOR_ATTACHMENT0;
    glDrawBuffersARB(count, &buf);
  }
}

void GLES2DecoderImpl::DoLoseContextCHROMIUM(GLenum current, GLenum other) {
  MarkContextLost(GetContextLostReasonFromResetStatus(current));
  group_->LoseContexts(GetContextLostReasonFromResetStatus(other));
  reset_by_robustness_extension_ = true;
}

void GLES2DecoderImpl::DoFlushDriverCachesCHROMIUM(void) {
  // On Adreno Android devices we need to use a workaround to force caches to
  // clear.
  if (workarounds().unbind_egl_context_to_flush_driver_caches) {
    context_->ReleaseCurrent(nullptr);
    context_->MakeCurrent(surface_.get());
  }
}

void GLES2DecoderImpl::DoMatrixLoadfCHROMIUM(GLenum matrix_mode,
                                             const volatile GLfloat* matrix) {
  DCHECK(matrix_mode == GL_PATH_PROJECTION_CHROMIUM ||
         matrix_mode == GL_PATH_MODELVIEW_CHROMIUM);

  GLfloat* target_matrix = matrix_mode == GL_PATH_PROJECTION_CHROMIUM
                               ? state_.projection_matrix
                               : state_.modelview_matrix;
  memcpy(target_matrix, const_cast<const GLfloat*>(matrix),
         sizeof(GLfloat) * 16);
  // The matrix_mode is either GL_PATH_MODELVIEW_NV or GL_PATH_PROJECTION_NV
  // since the values of the _NV and _CHROMIUM tokens match.
  glMatrixLoadfEXT(matrix_mode, target_matrix);
}

void GLES2DecoderImpl::DoMatrixLoadIdentityCHROMIUM(GLenum matrix_mode) {
  DCHECK(matrix_mode == GL_PATH_PROJECTION_CHROMIUM ||
         matrix_mode == GL_PATH_MODELVIEW_CHROMIUM);

  GLfloat* target_matrix = matrix_mode == GL_PATH_PROJECTION_CHROMIUM
                               ? state_.projection_matrix
                               : state_.modelview_matrix;
  memcpy(target_matrix, kIdentityMatrix, sizeof(kIdentityMatrix));
  // The matrix_mode is either GL_PATH_MODELVIEW_NV or GL_PATH_PROJECTION_NV
  // since the values of the _NV and _CHROMIUM tokens match.
  glMatrixLoadIdentityEXT(matrix_mode);
}

error::Error GLES2DecoderImpl::HandleUniformBlockBinding(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const char* func_name = "glUniformBlockBinding";
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::UniformBlockBinding& c =
      *static_cast<const volatile gles2::cmds::UniformBlockBinding*>(cmd_data);
  GLuint client_id = c.program;
  GLuint index = static_cast<GLuint>(c.index);
  GLuint binding = static_cast<GLuint>(c.binding);
  Program* program = GetProgramInfoNotShader(client_id, func_name);
  if (!program) {
    return error::kNoError;
  }
  if (index >= program->uniform_block_size_info().size()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name,
        "uniformBlockIndex is not an active uniform block index");
    return error::kNoError;
  }
  if (binding >= group_->max_uniform_buffer_bindings()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name,
        "uniformBlockBinding >= MAX_UNIFORM_BUFFER_BINDINGS");
    return error::kNoError;
  }
  GLuint service_id = program->service_id();
  glUniformBlockBinding(service_id, index, binding);
  program->SetUniformBlockBinding(index, binding);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleClientWaitSync(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const char* function_name = "glClientWaitSync";
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::ClientWaitSync& c =
      *static_cast<const volatile gles2::cmds::ClientWaitSync*>(cmd_data);
  const GLuint sync = static_cast<GLuint>(c.sync);
  GLbitfield flags = static_cast<GLbitfield>(c.flags);
  const GLuint64 timeout = c.timeout();
  typedef cmds::ClientWaitSync::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  if (*result_dst != GL_WAIT_FAILED) {
    return error::kInvalidArguments;
  }
  GLsync service_sync = 0;
  if (!group_->GetSyncServiceId(sync, &service_sync)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "invalid sync");
    return error::kNoError;
  }
  if ((flags & ~GL_SYNC_FLUSH_COMMANDS_BIT) != 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "invalid flags");
    return error::kNoError;
  }
  // Force GL_SYNC_FLUSH_COMMANDS_BIT to avoid infinite wait.
  flags |= GL_SYNC_FLUSH_COMMANDS_BIT;

  GLenum status = glClientWaitSync(service_sync, flags, timeout);
  switch (status) {
    case GL_ALREADY_SIGNALED:
    case GL_TIMEOUT_EXPIRED:
    case GL_CONDITION_SATISFIED:
      break;
    case GL_WAIT_FAILED:
      // Avoid leaking GL errors when using virtual contexts.
      LOCAL_PEEK_GL_ERROR(function_name);
      *result_dst = status;
      // If validation is complete, this only happens if the context is lost.
      return error::kLostContext;
    default:
      NOTREACHED();
      break;
  }
  *result_dst = status;
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleWaitSync(uint32_t immediate_data_size,
                                              const volatile void* cmd_data) {
  const char* function_name = "glWaitSync";
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::WaitSync& c =
      *static_cast<const volatile gles2::cmds::WaitSync*>(cmd_data);
  const GLuint sync = static_cast<GLuint>(c.sync);
  const GLbitfield flags = static_cast<GLbitfield>(c.flags);
  const GLuint64 timeout = c.timeout();
  GLsync service_sync = 0;
  if (!group_->GetSyncServiceId(sync, &service_sync)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "invalid sync");
    return error::kNoError;
  }
  if (flags != 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "invalid flags");
    return error::kNoError;
  }
  if (timeout != GL_TIMEOUT_IGNORED) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "invalid timeout");
    return error::kNoError;
  }
  glWaitSync(service_sync, flags, timeout);
  return error::kNoError;
}

GLsync GLES2DecoderImpl::DoFenceSync(GLenum condition, GLbitfield flags) {
  const char* function_name = "glFenceSync";
  if (condition != GL_SYNC_GPU_COMMANDS_COMPLETE) {
    LOCAL_SET_GL_ERROR(GL_INVALID_ENUM, function_name, "invalid condition");
    return 0;
  }
  if (flags != 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, function_name, "invalid flags");
    return 0;
  }
  return glFenceSync(condition, flags);
}

error::Error GLES2DecoderImpl::HandleGetInternalformativ(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GetInternalformativ& c =
      *static_cast<const volatile gles2::cmds::GetInternalformativ*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum pname = static_cast<GLenum>(c.pname);
  if (!validators_->render_buffer_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetInternalformativ", target, "target");
    return error::kNoError;
  }
  if (!validators_->render_buffer_format.IsValid(format)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetInternalformativ", format,
                                    "internalformat");
    return error::kNoError;
  }
  if (!validators_->internal_format_parameter.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetInternalformativ", pname, "pname");
    return error::kNoError;
  }

  typedef cmds::GetInternalformativ::Result Result;
  GLsizei num_values = 0;
  std::vector<GLint> samples;
  if (gl_version_info().IsLowerThanGL(4, 2)) {
    if (!GLES2Util::IsIntegerFormat(format)) {
      // No multisampling for integer formats.
      GLint max_samples = renderbuffer_manager()->max_samples();
      while (max_samples > 0) {
        samples.push_back(max_samples);
        --max_samples;
      }
    }
    switch (pname) {
      case GL_NUM_SAMPLE_COUNTS:
        num_values = 1;
        break;
      case GL_SAMPLES:
        num_values = static_cast<GLsizei>(samples.size());
        break;
      default:
        NOTREACHED();
        break;
    }
  } else {
    switch (pname) {
      case GL_NUM_SAMPLE_COUNTS:
        num_values = 1;
        break;
      case GL_SAMPLES:
        {
          GLint value = 0;
          glGetInternalformativ(
              target, format, GL_NUM_SAMPLE_COUNTS, 1, &value);
          num_values = static_cast<GLsizei>(value);
        }
        break;
      default:
        NOTREACHED();
        break;
    }
  }
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, Result::ComputeSize(num_values));
  GLint* params = result ? result->GetData() : NULL;
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  if (gl_version_info().IsLowerThanGL(4, 2)) {
    switch (pname) {
      case GL_NUM_SAMPLE_COUNTS:
        params[0] = static_cast<GLint>(samples.size());
        break;
      case GL_SAMPLES:
        for (size_t ii = 0; ii < samples.size(); ++ii) {
          params[ii] = samples[ii];
        }
        break;
      default:
        NOTREACHED();
        break;
    }
  } else {
    glGetInternalformativ(target, format, pname, num_values, params);
  }
  result->SetNumResults(num_values);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleMapBufferRange(
    uint32_t immediate_data_size, const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context()) {
    return error::kUnknownCommand;
  }

  const char* func_name = "glMapBufferRange";
  const volatile gles2::cmds::MapBufferRange& c =
      *static_cast<const volatile gles2::cmds::MapBufferRange*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLbitfield access = static_cast<GLbitfield>(c.access);
  GLintptr offset = static_cast<GLintptr>(c.offset);
  GLsizeiptr size = static_cast<GLsizeiptr>(c.size);
  uint32_t data_shm_id = static_cast<uint32_t>(c.data_shm_id);
  uint32_t data_shm_offset = static_cast<uint32_t>(c.data_shm_offset);

  typedef cmds::MapBufferRange::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  if (*result != 0) {
    *result = 0;
    return error::kInvalidArguments;
  }
  if (!validators_->buffer_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(func_name, target, "target");
    return error::kNoError;
  }
  if (size == 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name, "length is zero");
    return error::kNoError;
  }
  Buffer* buffer = buffer_manager()->RequestBufferAccess(
      &state_, target, offset, size, func_name);
  if (!buffer) {
    // An error is already set.
    return error::kNoError;
  }
  if (state_.bound_transform_feedback->active() &&
      !state_.bound_transform_feedback->paused()) {
    size_t used_binding_count =
        state_.current_program->effective_transform_feedback_varyings().size();
    if (state_.bound_transform_feedback->UsesBuffer(
            used_binding_count, buffer)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
                         "active transform feedback is using this buffer");
      return error::kNoError;
    }
  }

  int8_t* mem =
      GetSharedMemoryAs<int8_t*>(data_shm_id, data_shm_offset, size);
  if (!mem) {
    return error::kOutOfBounds;
  }
  if (AnyOtherBitsSet(access, (GL_MAP_READ_BIT |
                               GL_MAP_WRITE_BIT |
                               GL_MAP_INVALIDATE_RANGE_BIT |
                               GL_MAP_INVALIDATE_BUFFER_BIT |
                               GL_MAP_FLUSH_EXPLICIT_BIT |
                               GL_MAP_UNSYNCHRONIZED_BIT))) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name, "invalid access bits");
    return error::kNoError;
  }
  if (!AnyBitsSet(access, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
        "neither MAP_READ_BIT nor MAP_WRITE_BIT is set");
    return error::kNoError;
  }
  if (AllBitsSet(access, GL_MAP_READ_BIT) &&
      AnyBitsSet(access, (GL_MAP_INVALIDATE_RANGE_BIT |
                          GL_MAP_INVALIDATE_BUFFER_BIT |
                          GL_MAP_UNSYNCHRONIZED_BIT))) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
        "incompatible access bits with MAP_READ_BIT");
    return error::kNoError;
  }
  if (AllBitsSet(access, GL_MAP_FLUSH_EXPLICIT_BIT) &&
      !AllBitsSet(access, GL_MAP_WRITE_BIT)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
        "MAP_FLUSH_EXPLICIT_BIT set without MAP_WRITE_BIT");
    return error::kNoError;
  }
  GLbitfield filtered_access = access;
  if (AllBitsSet(filtered_access, GL_MAP_INVALIDATE_BUFFER_BIT)) {
    // To be on the safe side, always map GL_MAP_INVALIDATE_BUFFER_BIT to
    // GL_MAP_INVALIDATE_RANGE_BIT.
    filtered_access = (filtered_access & ~GL_MAP_INVALIDATE_BUFFER_BIT);
    filtered_access = (filtered_access | GL_MAP_INVALIDATE_RANGE_BIT);
  }
  // Always filter out GL_MAP_UNSYNCHRONIZED_BIT to get rid of undefined
  // behaviors.
  filtered_access = (filtered_access & ~GL_MAP_UNSYNCHRONIZED_BIT);
  if (AllBitsSet(filtered_access, GL_MAP_WRITE_BIT) &&
      !AllBitsSet(filtered_access, GL_MAP_INVALIDATE_RANGE_BIT)) {
    filtered_access = (filtered_access | GL_MAP_READ_BIT);
  }
  void* ptr = glMapBufferRange(target, offset, size, filtered_access);
  if (ptr == nullptr) {
    // This should mean GL_OUT_OF_MEMORY (or context loss).
    LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER(func_name);
    return error::kNoError;
  }
  // The un-filtered bits in |access| are deliberately used here.
  buffer->SetMappedRange(offset, size, access, ptr,
                         GetSharedMemoryBuffer(data_shm_id),
                         static_cast<unsigned int>(data_shm_offset));
  if ((filtered_access & GL_MAP_INVALIDATE_RANGE_BIT) == 0) {
    memcpy(mem, ptr, size);
  }
  *result = 1;
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUnmapBuffer(
    uint32_t immediate_data_size, const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3Context()) {
    return error::kUnknownCommand;
  }
  const char* func_name = "glUnmapBuffer";

  const volatile gles2::cmds::UnmapBuffer& c =
      *static_cast<const volatile gles2::cmds::UnmapBuffer*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);

  if (!validators_->buffer_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(func_name, target, "target");
    return error::kNoError;
  }

  Buffer* buffer = buffer_manager()->GetBufferInfoForTarget(&state_, target);
  if (!buffer) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name, "no buffer bound");
    return error::kNoError;
  }
  const Buffer::MappedRange* mapped_range = buffer->GetMappedRange();
  if (!mapped_range) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name, "buffer is unmapped");
    return error::kNoError;
  }
  if (!AllBitsSet(mapped_range->access, GL_MAP_WRITE_BIT) ||
      AllBitsSet(mapped_range->access, GL_MAP_FLUSH_EXPLICIT_BIT)) {
    // If we don't need to write back, or explict flush is required, no copying
    // back is needed.
  } else {
    void* mem = mapped_range->GetShmPointer();
    DCHECK(mem);
    DCHECK(mapped_range->pointer);
    memcpy(mapped_range->pointer, mem, mapped_range->size);
    if (buffer->shadowed()) {
      buffer->SetRange(mapped_range->offset, mapped_range->size, mem);
    }
  }
  buffer->RemoveMappedRange();
  GLboolean rt = glUnmapBuffer(target);
  if (rt == GL_FALSE) {
    // At this point, we have already done the necessary validation, so
    // GL_FALSE indicates data corruption.
    // TODO(zmo): We could redo the map / copy data / unmap to recover, but
    // the second unmap could still return GL_FALSE. For now, we simply lose
    // the contexts in the share group.
    LOG(ERROR) << func_name << " unexpectedly returned GL_FALSE";
    // Need to lose current context before broadcasting!
    MarkContextLost(error::kGuilty);
    group_->LoseContexts(error::kInnocent);
    return error::kLostContext;
  }
  return error::kNoError;
}

void GLES2DecoderImpl::DoFlushMappedBufferRange(
    GLenum target, GLintptr offset, GLsizeiptr size) {
  const char* func_name = "glFlushMappedBufferRange";
  // |size| is validated in HandleFlushMappedBufferRange().
  if (offset < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name, "offset < 0");
    return;
  }
  Buffer* buffer = buffer_manager()->GetBufferInfoForTarget(&state_, target);
  if (!buffer) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name, "no buffer bound");
    return;
  }
  const Buffer::MappedRange* mapped_range = buffer->GetMappedRange();
  if (!mapped_range) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name, "buffer is unmapped");
    return;
  }
  if (!AllBitsSet(mapped_range->access, GL_MAP_FLUSH_EXPLICIT_BIT)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, func_name,
        "buffer is mapped without MAP_FLUSH_EXPLICIT_BIT flag");
    return;
  }
  base::CheckedNumeric<int32_t> range_size = size;
  range_size += offset;
  if (!range_size.IsValid() ||
      range_size.ValueOrDefault(0) > mapped_range->size) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, func_name,
        "offset + size out of bounds");
    return;
  }
  char* client_data = reinterpret_cast<char*>(mapped_range->GetShmPointer());
  DCHECK(client_data);
  char* gpu_data = reinterpret_cast<char*>(mapped_range->pointer);
  DCHECK(gpu_data);
  memcpy(gpu_data + offset, client_data + offset, size);
  if (buffer->shadowed()) {
    buffer->SetRange(mapped_range->offset + offset, size, client_data + offset);
  }
  glFlushMappedBufferRange(target, offset, size);
}

// Note that GL_LOST_CONTEXT is specific to GLES.
// For desktop GL we have to query the reset status proactively.
void GLES2DecoderImpl::OnContextLostError() {
  if (!WasContextLost()) {
    // Need to lose current context before broadcasting!
    CheckResetStatus();
    group_->LoseContexts(error::kUnknown);
    reset_by_robustness_extension_ = true;
  }
}

void GLES2DecoderImpl::OnOutOfMemoryError() {
  if (lose_context_when_out_of_memory_ && !WasContextLost()) {
    error::ContextLostReason other = error::kOutOfMemory;
    if (CheckResetStatus()) {
      other = error::kUnknown;
    } else {
      // Need to lose current context before broadcasting!
      MarkContextLost(error::kOutOfMemory);
    }
    group_->LoseContexts(other);
  }
}

// Class to validate path rendering command parameters. Contains validation
// for the common parameters that are used in multiple different commands.
// The individual functions are needed in order to control the order of the
// validation.
// The Get* functions will return false if the function call should be stopped.
// In this case, PathCommandValidatorContext::error() will return the command
// buffer error that should be returned. The decoder error state will be set to
// appropriate GL error if needed.
// The Get* functions will return true if the function call should
// continue as-is.
class PathCommandValidatorContext {
 public:
  PathCommandValidatorContext(GLES2DecoderImpl* decoder,
                              const char* function_name)
      : decoder_(decoder),
        error_state_(decoder->GetErrorState()),
        validators_(decoder->GetContextGroup()->feature_info()->validators()),
        function_name_(function_name),
        error_(error::kNoError) {}

  error::Error error() const { return error_; }

  template <typename Cmd>
  bool GetPathRange(const Cmd& cmd, GLsizei* out_range) {
    GLsizei range = static_cast<GLsizei>(cmd.range);
    if (range < 0) {
      ERRORSTATE_SET_GL_ERROR(error_state_, GL_INVALID_VALUE, function_name_,
                              "range < 0");
      return false;
    }
    *out_range = range;
    return true;
  }
  template <typename Cmd>
  bool GetPathCountAndType(const Cmd& cmd,
                           GLuint* out_num_paths,
                           GLenum* out_path_name_type) {
    int32_t numPaths = cmd.numPaths;
    if (numPaths < 0) {
      ERRORSTATE_SET_GL_ERROR(error_state_, GL_INVALID_VALUE, function_name_,
                              "numPaths < 0");
      return false;
    }
    GLenum path_name_type = static_cast<GLenum>(cmd.pathNameType);
    if (!validators_->path_name_type.IsValid(path_name_type)) {
      ERRORSTATE_SET_GL_ERROR_INVALID_ENUM(error_state_, function_name_,
                                           path_name_type, "pathNameType");
      return false;
    }
    *out_num_paths = static_cast<GLsizei>(numPaths);
    *out_path_name_type = path_name_type;
    return true;
  }
  template <typename Cmd>
  bool GetFillMode(const Cmd& cmd, GLenum* out_fill_mode) {
    GLenum fill_mode = static_cast<GLenum>(cmd.fillMode);
    if (!validators_->path_fill_mode.IsValid(fill_mode)) {
      ERRORSTATE_SET_GL_ERROR_INVALID_ENUM(error_state_, function_name_,
                                           fill_mode, "fillMode");
      return false;
    }
    *out_fill_mode = fill_mode;
    return true;
  }
  template <typename Cmd>
  bool GetFillModeAndMask(const Cmd& cmd,
                          GLenum* out_fill_mode,
                          GLuint* out_mask) {
    GLenum fill_mode;
    if (!GetFillMode(cmd, &fill_mode))
      return false;
    GLuint mask = static_cast<GLuint>(cmd.mask);
    /*  The error INVALID_VALUE is generated if /fillMode/ is COUNT_UP_CHROMIUM
        or COUNT_DOWN_CHROMIUM and the effective /mask/+1 is not an integer
        power of two */
    if ((fill_mode == GL_COUNT_UP_CHROMIUM ||
         fill_mode == GL_COUNT_DOWN_CHROMIUM) &&
        GLES2Util::IsNPOT(mask + 1)) {
      ERRORSTATE_SET_GL_ERROR(error_state_, GL_INVALID_VALUE, function_name_,
                              "mask+1 is not power of two");
      return false;
    }
    *out_fill_mode = fill_mode;
    *out_mask = mask;
    return true;
  }
  template <typename Cmd>
  bool GetTransformType(const Cmd& cmd, GLenum* out_transform_type) {
    GLenum transform_type = static_cast<GLenum>(cmd.transformType);
    if (!validators_->path_transform_type.IsValid(transform_type)) {
      ERRORSTATE_SET_GL_ERROR_INVALID_ENUM(error_state_, function_name_,
                                           transform_type, "transformType");
      return false;
    }
    *out_transform_type = transform_type;
    return true;
  }
  template <typename Cmd>
  bool GetPathNameData(const Cmd& cmd,
                       GLuint num_paths,
                       GLenum path_name_type,
                       std::unique_ptr<GLuint[]>* out_buffer) {
    DCHECK(validators_->path_name_type.IsValid(path_name_type));
    GLuint path_base = static_cast<GLuint>(cmd.pathBase);
    uint32_t shm_id = static_cast<uint32_t>(cmd.paths_shm_id);
    uint32_t shm_offset = static_cast<uint32_t>(cmd.paths_shm_offset);
    if (shm_id == 0 && shm_offset == 0) {
      error_ = error::kOutOfBounds;
      return false;
    }
    switch (path_name_type) {
      case GL_BYTE:
        return GetPathNameDataImpl<GLbyte>(num_paths, path_base, shm_id,
                                           shm_offset, out_buffer);
      case GL_UNSIGNED_BYTE:
        return GetPathNameDataImpl<GLubyte>(num_paths, path_base, shm_id,
                                            shm_offset, out_buffer);
      case GL_SHORT:
        return GetPathNameDataImpl<GLshort>(num_paths, path_base, shm_id,
                                            shm_offset, out_buffer);
      case GL_UNSIGNED_SHORT:
        return GetPathNameDataImpl<GLushort>(num_paths, path_base, shm_id,
                                             shm_offset, out_buffer);
      case GL_INT:
        return GetPathNameDataImpl<GLint>(num_paths, path_base, shm_id,
                                          shm_offset, out_buffer);
      case GL_UNSIGNED_INT:
        return GetPathNameDataImpl<GLuint>(num_paths, path_base, shm_id,
                                           shm_offset, out_buffer);
      default:
        break;
    }
    NOTREACHED();
    error_ = error::kOutOfBounds;
    return false;
  }
  template <typename Cmd>
  bool GetTransforms(const Cmd& cmd,
                     GLuint num_paths,
                     GLenum transform_type,
                     const GLfloat** out_transforms) {
    if (transform_type == GL_NONE) {
      *out_transforms = nullptr;
      return true;
    }
    uint32_t transforms_shm_id =
        static_cast<uint32_t>(cmd.transformValues_shm_id);
    uint32_t transforms_shm_offset =
        static_cast<uint32_t>(cmd.transformValues_shm_offset);
    uint32_t transforms_component_count =
        GLES2Util::GetComponentCountForGLTransformType(transform_type);
    // Below multiplication will not overflow.
    DCHECK(transforms_component_count <= 12);
    uint32_t one_transform_size = sizeof(GLfloat) * transforms_component_count;
    uint32_t transforms_size = 0;
    if (!SafeMultiplyUint32(one_transform_size, num_paths, &transforms_size)) {
      error_ = error::kOutOfBounds;
      return false;
    }
    const GLfloat* transforms = nullptr;
    if (transforms_shm_id != 0 || transforms_shm_offset != 0)
      transforms = decoder_->GetSharedMemoryAs<const GLfloat*>(
          transforms_shm_id, transforms_shm_offset, transforms_size);
    if (!transforms) {
      error_ = error::kOutOfBounds;
      return false;
    }
    *out_transforms = transforms;
    return true;
  }
  template <typename Cmd>
  bool GetCoverMode(const Cmd& cmd, GLenum* out_cover_mode) {
    GLenum cover_mode = static_cast<GLuint>(cmd.coverMode);
    if (!validators_->path_instanced_cover_mode.IsValid(cover_mode)) {
      ERRORSTATE_SET_GL_ERROR_INVALID_ENUM(error_state_, function_name_,
                                           cover_mode, "coverMode");
      return false;
    }
    *out_cover_mode = cover_mode;
    return true;
  }

 private:
  template <typename T>
  bool GetPathNameDataImpl(GLuint num_paths,
                           GLuint path_base,
                           uint32_t shm_id,
                           uint32_t shm_offset,
                           std::unique_ptr<GLuint[]>* out_buffer) {
    uint32_t paths_size = 0;
    if (!SafeMultiplyUint32(num_paths, sizeof(T), &paths_size)) {
      error_ = error::kOutOfBounds;
      return false;
    }
    T* paths = decoder_->GetSharedMemoryAs<T*>(shm_id, shm_offset, paths_size);
    if (!paths) {
      error_ = error::kOutOfBounds;
      return false;
    }
    std::unique_ptr<GLuint[]> result_paths(new GLuint[num_paths]);
    bool has_paths = false;
    for (GLuint i = 0; i < num_paths; ++i) {
      GLuint service_id = 0;
      // The below addition is ok even with over- and underflows.
      // There is no difference if client passes:
      //  * base==4, T=GLbyte, paths[0]==0xfa (-6)
      //  * base==0xffffffff, T=GLuint, paths[0]==0xffffffff
      //  * base==0, T=GLuint, paths[0]==0xfffffffe
      // For the all the cases, the interpretation is that
      // client intends to use the path 0xfffffffe.
      // The client_id verification is only done after the addition.
      uint32_t client_id = path_base + paths[i];
      if (decoder_->path_manager()->GetPath(client_id, &service_id))
        has_paths = true;
      // Will use path 0 if the path is not found. This is in line
      // of the spec: missing paths will produce nothing, let
      // the instanced draw continue.
      result_paths[i] = service_id;
    }
    out_buffer->reset(result_paths.release());

    return has_paths;
  }
  GLES2DecoderImpl* decoder_;
  ErrorState* error_state_;
  const Validators* validators_;
  const char* function_name_;
  error::Error error_;
};

error::Error GLES2DecoderImpl::HandleGenPathsCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GenPathsCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::GenPathsCHROMIUM*>(cmd_data);
  if (!features().chromium_path_rendering)
    return error::kUnknownCommand;

  PathCommandValidatorContext v(this, "glGenPathsCHROMIUM");
  GLsizei range = 0;
  if (!v.GetPathRange(c, &range))
    return v.error();

  GLuint first_client_id = static_cast<GLuint>(c.first_client_id);
  if (first_client_id == 0)
    return error::kInvalidArguments;

  if (range == 0)
    return error::kNoError;

  if (!GenPathsCHROMIUMHelper(first_client_id, range))
    return error::kInvalidArguments;

  return error::kNoError;
}
error::Error GLES2DecoderImpl::HandleDeletePathsCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DeletePathsCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::DeletePathsCHROMIUM*>(cmd_data);
  if (!features().chromium_path_rendering)
    return error::kUnknownCommand;

  PathCommandValidatorContext v(this, "glDeletePathsCHROMIUM");
  GLsizei range = 0;
  if (!v.GetPathRange(c, &range))
    return v.error();

  if (range == 0)
    return error::kNoError;

  GLuint first_client_id = c.first_client_id;
  // first_client_id can be 0, because non-existing path ids are skipped.

  if (!DeletePathsCHROMIUMHelper(first_client_id, range))
    return error::kInvalidArguments;

  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandlePathCommandsCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  static const char kFunctionName[] = "glPathCommandsCHROMIUM";
  const volatile gles2::cmds::PathCommandsCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::PathCommandsCHROMIUM*>(cmd_data);
  if (!features().chromium_path_rendering)
    return error::kUnknownCommand;

  GLuint service_id = 0;
  if (!path_manager()->GetPath(static_cast<GLuint>(c.path), &service_id)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, kFunctionName,
                       "invalid path name");
    return error::kNoError;
  }

  GLsizei num_commands = static_cast<GLsizei>(c.numCommands);
  if (num_commands < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, kFunctionName, "numCommands < 0");
    return error::kNoError;
  }

  GLsizei num_coords = static_cast<uint32_t>(c.numCoords);
  if (num_coords < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, kFunctionName, "numCoords < 0");
    return error::kNoError;
  }

  GLenum coord_type = static_cast<uint32_t>(c.coordType);
  if (!validators_->path_coord_type.IsValid(static_cast<GLint>(coord_type))) {
    LOCAL_SET_GL_ERROR(GL_INVALID_ENUM, kFunctionName, "invalid coordType");
    return error::kNoError;
  }

  std::unique_ptr<GLubyte[]> commands;
  base::CheckedNumeric<GLsizei> num_coords_expected = 0;

  if (num_commands > 0) {
    uint32_t commands_shm_id = static_cast<uint32_t>(c.commands_shm_id);
    uint32_t commands_shm_offset = static_cast<uint32_t>(c.commands_shm_offset);
    if (commands_shm_id != 0 || commands_shm_offset != 0) {
      const GLubyte* shared_commands = GetSharedMemoryAs<const GLubyte*>(
          commands_shm_id, commands_shm_offset, num_commands);
      if (shared_commands) {
        commands.reset(new GLubyte[num_commands]);
        memcpy(commands.get(), shared_commands, num_commands);
      }
    }
    if (!commands)
      return error::kOutOfBounds;

    for (GLsizei i = 0; i < num_commands; ++i) {
      switch (commands[i]) {
        case GL_CLOSE_PATH_CHROMIUM:
          // Close has no coords.
          break;
        case GL_MOVE_TO_CHROMIUM:
        // Fallthrough.
        case GL_LINE_TO_CHROMIUM:
          num_coords_expected += 2;
          break;
        case GL_QUADRATIC_CURVE_TO_CHROMIUM:
          num_coords_expected += 4;
          break;
        case GL_CUBIC_CURVE_TO_CHROMIUM:
          num_coords_expected += 6;
          break;
        case GL_CONIC_CURVE_TO_CHROMIUM:
          num_coords_expected += 5;
          break;
        default:
          LOCAL_SET_GL_ERROR(GL_INVALID_ENUM, kFunctionName, "invalid command");
          return error::kNoError;
      }
    }
  }

  if (!num_coords_expected.IsValid() ||
      num_coords != num_coords_expected.ValueOrDefault(0)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, kFunctionName,
                       "numCoords does not match commands");
    return error::kNoError;
  }

  const void* coords = NULL;

  if (num_coords > 0) {
    uint32_t coords_size = 0;
    uint32_t coord_type_size =
        GLES2Util::GetGLTypeSizeForPathCoordType(coord_type);
    if (!SafeMultiplyUint32(num_coords, coord_type_size, &coords_size))
      return error::kOutOfBounds;

    uint32_t coords_shm_id = static_cast<uint32_t>(c.coords_shm_id);
    uint32_t coords_shm_offset = static_cast<uint32_t>(c.coords_shm_offset);
    if (coords_shm_id != 0 || coords_shm_offset != 0)
      coords = GetSharedMemoryAs<const void*>(coords_shm_id, coords_shm_offset,
                                              coords_size);

    if (!coords)
      return error::kOutOfBounds;
  }

  glPathCommandsNV(service_id, num_commands, commands.get(), num_coords,
                   coord_type, coords);

  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandlePathParameterfCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  static const char kFunctionName[] = "glPathParameterfCHROMIUM";
  const volatile gles2::cmds::PathParameterfCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::PathParameterfCHROMIUM*>(
          cmd_data);
  if (!features().chromium_path_rendering)
    return error::kUnknownCommand;

  GLuint service_id = 0;
  if (!path_manager()->GetPath(static_cast<GLuint>(c.path), &service_id)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, kFunctionName,
                       "invalid path name");
    return error::kNoError;
  }

  GLenum pname = static_cast<GLenum>(c.pname);
  GLfloat value = static_cast<GLfloat>(c.value);
  bool hasValueError = false;

  switch (pname) {
    case GL_PATH_STROKE_WIDTH_CHROMIUM:
    case GL_PATH_MITER_LIMIT_CHROMIUM:
      hasValueError = std::isnan(value) || !std::isfinite(value) || value < 0;
      break;
    case GL_PATH_STROKE_BOUND_CHROMIUM:
      value = std::max(std::min(1.0f, value), 0.0f);
      break;
    case GL_PATH_END_CAPS_CHROMIUM:
      hasValueError = !validators_->path_parameter_cap_values.IsValid(
          static_cast<GLint>(value));
      break;
    case GL_PATH_JOIN_STYLE_CHROMIUM:
      hasValueError = !validators_->path_parameter_join_values.IsValid(
          static_cast<GLint>(value));
      break;
    default:
      DCHECK(!validators_->path_parameter.IsValid(pname));
      LOCAL_SET_GL_ERROR_INVALID_ENUM(kFunctionName, pname, "pname");
      return error::kNoError;
  }
  DCHECK(validators_->path_parameter.IsValid(pname));

  if (hasValueError) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, kFunctionName, "value not correct");
    return error::kNoError;
  }

  glPathParameterfNV(service_id, pname, value);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandlePathParameteriCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  static const char kFunctionName[] = "glPathParameteriCHROMIUM";
  const volatile gles2::cmds::PathParameteriCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::PathParameteriCHROMIUM*>(
          cmd_data);
  if (!features().chromium_path_rendering)
    return error::kUnknownCommand;

  GLuint service_id = 0;
  if (!path_manager()->GetPath(static_cast<GLuint>(c.path), &service_id)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, kFunctionName,
                       "invalid path name");
    return error::kNoError;
  }

  GLenum pname = static_cast<GLenum>(c.pname);
  GLint value = static_cast<GLint>(c.value);
  bool hasValueError = false;

  switch (pname) {
    case GL_PATH_STROKE_WIDTH_CHROMIUM:
    case GL_PATH_MITER_LIMIT_CHROMIUM:
      hasValueError = value < 0;
      break;
    case GL_PATH_STROKE_BOUND_CHROMIUM:
      value = std::max(std::min(1, value), 0);
      break;
    case GL_PATH_END_CAPS_CHROMIUM:
      hasValueError = !validators_->path_parameter_cap_values.IsValid(value);
      break;
    case GL_PATH_JOIN_STYLE_CHROMIUM:
      hasValueError = !validators_->path_parameter_join_values.IsValid(value);
      break;
    default:
      DCHECK(!validators_->path_parameter.IsValid(pname));
      LOCAL_SET_GL_ERROR_INVALID_ENUM(kFunctionName, pname, "pname");
      return error::kNoError;
  }
  DCHECK(validators_->path_parameter.IsValid(pname));

  if (hasValueError) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, kFunctionName, "value not correct");
    return error::kNoError;
  }

  glPathParameteriNV(service_id, pname, value);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleStencilFillPathCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  static const char kFunctionName[] = "glStencilFillPathCHROMIUM";
  const volatile gles2::cmds::StencilFillPathCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::StencilFillPathCHROMIUM*>(
          cmd_data);
  if (!features().chromium_path_rendering)
    return error::kUnknownCommand;
  PathCommandValidatorContext v(this, kFunctionName);
  GLenum fill_mode = GL_COUNT_UP_CHROMIUM;
  GLuint mask = 0;
  if (!v.GetFillModeAndMask(c, &fill_mode, &mask))
    return v.error();
  GLuint service_id = 0;
  if (!path_manager()->GetPath(static_cast<GLuint>(c.path), &service_id)) {
    // "If /path/ does not name an existing path object, the command does
    // nothing (and no error is generated)."
    // This holds for other rendering functions, too.
    return error::kNoError;
  }
  if (!CheckBoundDrawFramebufferValid(kFunctionName))
    return error::kNoError;
  ApplyDirtyState();
  glStencilFillPathNV(service_id, fill_mode, mask);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleStencilStrokePathCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  static const char kFunctionName[] = "glStencilStrokePathCHROMIUM";
  const volatile gles2::cmds::StencilStrokePathCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::StencilStrokePathCHROMIUM*>(
          cmd_data);
  if (!features().chromium_path_rendering)
    return error::kUnknownCommand;

  GLuint service_id = 0;
  if (!path_manager()->GetPath(static_cast<GLuint>(c.path), &service_id)) {
    return error::kNoError;
  }
  GLint reference = static_cast<GLint>(c.reference);
  GLuint mask = static_cast<GLuint>(c.mask);
  if (!CheckBoundDrawFramebufferValid(kFunctionName))
    return error::kNoError;
  ApplyDirtyState();
  glStencilStrokePathNV(service_id, reference, mask);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCoverFillPathCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  static const char kFunctionName[] = "glCoverFillPathCHROMIUM";
  const volatile gles2::cmds::CoverFillPathCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::CoverFillPathCHROMIUM*>(
          cmd_data);
  if (!features().chromium_path_rendering)
    return error::kUnknownCommand;

  PathCommandValidatorContext v(this, kFunctionName);
  GLenum cover_mode = GL_BOUNDING_BOX_CHROMIUM;
  if (!v.GetCoverMode(c, &cover_mode))
    return v.error();

  GLuint service_id = 0;
  if (!path_manager()->GetPath(static_cast<GLuint>(c.path), &service_id))
    return error::kNoError;
  if (!CheckBoundDrawFramebufferValid(kFunctionName))
    return error::kNoError;
  ApplyDirtyState();
  glCoverFillPathNV(service_id, cover_mode);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCoverStrokePathCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  static const char kFunctionName[] = "glCoverStrokePathCHROMIUM";
  const volatile gles2::cmds::CoverStrokePathCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::CoverStrokePathCHROMIUM*>(
          cmd_data);
  if (!features().chromium_path_rendering)
    return error::kUnknownCommand;

  PathCommandValidatorContext v(this, kFunctionName);
  GLenum cover_mode = GL_BOUNDING_BOX_CHROMIUM;
  if (!v.GetCoverMode(c, &cover_mode))
    return v.error();

  GLuint service_id = 0;
  if (!path_manager()->GetPath(static_cast<GLuint>(c.path), &service_id))
    return error::kNoError;

  if (!CheckBoundDrawFramebufferValid(kFunctionName))
    return error::kNoError;
  ApplyDirtyState();
  glCoverStrokePathNV(service_id, cover_mode);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleStencilThenCoverFillPathCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  static const char kFunctionName[] = "glStencilThenCoverFillPathCHROMIUM";
  const volatile gles2::cmds::StencilThenCoverFillPathCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::StencilThenCoverFillPathCHROMIUM*>(
          cmd_data);
  if (!features().chromium_path_rendering)
    return error::kUnknownCommand;

  PathCommandValidatorContext v(this, kFunctionName);
  GLenum fill_mode = GL_COUNT_UP_CHROMIUM;
  GLuint mask = 0;
  GLenum cover_mode = GL_BOUNDING_BOX_CHROMIUM;
  if (!v.GetFillModeAndMask(c, &fill_mode, &mask) ||
      !v.GetCoverMode(c, &cover_mode))
    return v.error();

  GLuint service_id = 0;
  if (!path_manager()->GetPath(static_cast<GLuint>(c.path), &service_id))
    return error::kNoError;

  if (!CheckBoundDrawFramebufferValid(kFunctionName))
    return error::kNoError;
  ApplyDirtyState();
  glStencilThenCoverFillPathNV(service_id, fill_mode, mask, cover_mode);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleStencilThenCoverStrokePathCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  static const char kFunctionName[] = "glStencilThenCoverStrokePathCHROMIUM";
  const volatile gles2::cmds::StencilThenCoverStrokePathCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::StencilThenCoverStrokePathCHROMIUM*>(
          cmd_data);
  if (!features().chromium_path_rendering)
    return error::kUnknownCommand;

  PathCommandValidatorContext v(this, kFunctionName);
  GLenum cover_mode = GL_BOUNDING_BOX_CHROMIUM;
  if (!v.GetCoverMode(c, &cover_mode))
    return v.error();

  GLuint service_id = 0;
  if (!path_manager()->GetPath(static_cast<GLuint>(c.path), &service_id))
    return error::kNoError;

  GLint reference = static_cast<GLint>(c.reference);
  GLuint mask = static_cast<GLuint>(c.mask);

  if (!CheckBoundDrawFramebufferValid(kFunctionName))
    return error::kNoError;
  ApplyDirtyState();
  glStencilThenCoverStrokePathNV(service_id, reference, mask, cover_mode);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleStencilFillPathInstancedCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  static const char kFunctionName[] = "glStencilFillPathInstancedCHROMIUM";
  const volatile gles2::cmds::StencilFillPathInstancedCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::StencilFillPathInstancedCHROMIUM*>(
          cmd_data);
  if (!features().chromium_path_rendering)
    return error::kUnknownCommand;

  PathCommandValidatorContext v(this, kFunctionName);
  GLuint num_paths = 0;
  GLenum path_name_type = GL_NONE;
  GLenum fill_mode = GL_COUNT_UP_CHROMIUM;
  GLuint mask = 0;
  GLenum transform_type = GL_NONE;
  if (!v.GetPathCountAndType(c, &num_paths, &path_name_type) ||
      !v.GetFillModeAndMask(c, &fill_mode, &mask) ||
      !v.GetTransformType(c, &transform_type))
    return v.error();

  if (num_paths == 0)
    return error::kNoError;

  std::unique_ptr<GLuint[]> paths;
  if (!v.GetPathNameData(c, num_paths, path_name_type, &paths))
    return v.error();

  const GLfloat* transforms = nullptr;
  if (!v.GetTransforms(c, num_paths, transform_type, &transforms))
    return v.error();

  if (!CheckBoundDrawFramebufferValid(kFunctionName))
    return error::kNoError;
  ApplyDirtyState();
  glStencilFillPathInstancedNV(num_paths, GL_UNSIGNED_INT, paths.get(), 0,
                               fill_mode, mask, transform_type, transforms);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleStencilStrokePathInstancedCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  static const char kFunctionName[] = "glStencilStrokePathInstancedCHROMIUM";
  const volatile gles2::cmds::StencilStrokePathInstancedCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::StencilStrokePathInstancedCHROMIUM*>(
          cmd_data);
  if (!features().chromium_path_rendering)
    return error::kUnknownCommand;

  PathCommandValidatorContext v(this, kFunctionName);
  GLuint num_paths = 0;
  GLenum path_name_type = GL_NONE;
  GLenum transform_type = GL_NONE;
  if (!v.GetPathCountAndType(c, &num_paths, &path_name_type) ||
      !v.GetTransformType(c, &transform_type))
    return v.error();

  if (num_paths == 0)
    return error::kNoError;

  std::unique_ptr<GLuint[]> paths;
  if (!v.GetPathNameData(c, num_paths, path_name_type, &paths))
    return v.error();

  const GLfloat* transforms = nullptr;
  if (!v.GetTransforms(c, num_paths, transform_type, &transforms))
    return v.error();

  GLint reference = static_cast<GLint>(c.reference);
  GLuint mask = static_cast<GLuint>(c.mask);
  if (!CheckBoundDrawFramebufferValid(kFunctionName))
    return error::kNoError;
  ApplyDirtyState();
  glStencilStrokePathInstancedNV(num_paths, GL_UNSIGNED_INT, paths.get(), 0,
                                 reference, mask, transform_type, transforms);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCoverFillPathInstancedCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  static const char kFunctionName[] = "glCoverFillPathInstancedCHROMIUM";
  const volatile gles2::cmds::CoverFillPathInstancedCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::CoverFillPathInstancedCHROMIUM*>(
          cmd_data);
  if (!features().chromium_path_rendering)
    return error::kUnknownCommand;

  PathCommandValidatorContext v(this, kFunctionName);
  GLuint num_paths = 0;
  GLenum path_name_type = GL_NONE;
  GLenum cover_mode = GL_BOUNDING_BOX_OF_BOUNDING_BOXES_CHROMIUM;
  GLenum transform_type = GL_NONE;
  if (!v.GetPathCountAndType(c, &num_paths, &path_name_type) ||
      !v.GetCoverMode(c, &cover_mode) ||
      !v.GetTransformType(c, &transform_type))
    return v.error();

  if (num_paths == 0)
    return error::kNoError;

  std::unique_ptr<GLuint[]> paths;
  if (!v.GetPathNameData(c, num_paths, path_name_type, &paths))
    return v.error();

  const GLfloat* transforms = nullptr;
  if (!v.GetTransforms(c, num_paths, transform_type, &transforms))
    return v.error();

  if (!CheckBoundDrawFramebufferValid(kFunctionName))
    return error::kNoError;
  ApplyDirtyState();
  glCoverFillPathInstancedNV(num_paths, GL_UNSIGNED_INT, paths.get(), 0,
                             cover_mode, transform_type, transforms);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCoverStrokePathInstancedCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  static const char kFunctionName[] = "glCoverStrokePathInstancedCHROMIUM";
  const volatile gles2::cmds::CoverStrokePathInstancedCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::CoverStrokePathInstancedCHROMIUM*>(
          cmd_data);
  if (!features().chromium_path_rendering)
    return error::kUnknownCommand;

  PathCommandValidatorContext v(this, kFunctionName);
  GLuint num_paths = 0;
  GLenum path_name_type = GL_NONE;
  GLenum cover_mode = GL_BOUNDING_BOX_OF_BOUNDING_BOXES_CHROMIUM;
  GLenum transform_type = GL_NONE;
  if (!v.GetPathCountAndType(c, &num_paths, &path_name_type) ||
      !v.GetCoverMode(c, &cover_mode) ||
      !v.GetTransformType(c, &transform_type))
    return v.error();

  if (num_paths == 0)
    return error::kNoError;

  std::unique_ptr<GLuint[]> paths;
  if (!v.GetPathNameData(c, num_paths, path_name_type, &paths))
    return v.error();

  const GLfloat* transforms = nullptr;
  if (!v.GetTransforms(c, num_paths, transform_type, &transforms))
    return v.error();

  if (!CheckBoundDrawFramebufferValid(kFunctionName))
    return error::kNoError;
  ApplyDirtyState();
  glCoverStrokePathInstancedNV(num_paths, GL_UNSIGNED_INT, paths.get(), 0,
                               cover_mode, transform_type, transforms);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleStencilThenCoverFillPathInstancedCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  static const char kFunctionName[] =
      "glStencilThenCoverFillPathInstancedCHROMIUM";
  const volatile gles2::cmds::StencilThenCoverFillPathInstancedCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::
                       StencilThenCoverFillPathInstancedCHROMIUM*>(cmd_data);
  if (!features().chromium_path_rendering)
    return error::kUnknownCommand;
  PathCommandValidatorContext v(this, kFunctionName);

  GLuint num_paths = 0;
  GLenum path_name_type = GL_NONE;
  GLenum fill_mode = GL_COUNT_UP_CHROMIUM;
  GLuint mask = 0;
  GLenum cover_mode = GL_BOUNDING_BOX_OF_BOUNDING_BOXES_CHROMIUM;
  GLenum transform_type = GL_NONE;
  if (!v.GetPathCountAndType(c, &num_paths, &path_name_type) ||
      !v.GetFillModeAndMask(c, &fill_mode, &mask) ||
      !v.GetCoverMode(c, &cover_mode) ||
      !v.GetTransformType(c, &transform_type))
    return v.error();

  if (num_paths == 0)
    return error::kNoError;

  std::unique_ptr<GLuint[]> paths;
  if (!v.GetPathNameData(c, num_paths, path_name_type, &paths))
    return v.error();

  const GLfloat* transforms = nullptr;
  if (!v.GetTransforms(c, num_paths, transform_type, &transforms))
    return v.error();

  if (!CheckBoundDrawFramebufferValid(kFunctionName))
    return error::kNoError;
  ApplyDirtyState();
  glStencilThenCoverFillPathInstancedNV(num_paths, GL_UNSIGNED_INT, paths.get(),
                                        0, fill_mode, mask, cover_mode,
                                        transform_type, transforms);
  return error::kNoError;
}

error::Error
GLES2DecoderImpl::HandleStencilThenCoverStrokePathInstancedCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  static const char kFunctionName[] =
      "glStencilThenCoverStrokeInstancedCHROMIUM";
  const volatile gles2::cmds::StencilThenCoverStrokePathInstancedCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::
                       StencilThenCoverStrokePathInstancedCHROMIUM*>(cmd_data);
  if (!features().chromium_path_rendering)
    return error::kUnknownCommand;
  PathCommandValidatorContext v(this, kFunctionName);
  GLuint num_paths = 0;
  GLenum path_name_type = GL_NONE;
  GLenum cover_mode = GL_BOUNDING_BOX_OF_BOUNDING_BOXES_CHROMIUM;
  GLenum transform_type = GL_NONE;
  if (!v.GetPathCountAndType(c, &num_paths, &path_name_type) ||
      !v.GetCoverMode(c, &cover_mode) ||
      !v.GetTransformType(c, &transform_type))
    return v.error();

  if (num_paths == 0)
    return error::kNoError;

  std::unique_ptr<GLuint[]> paths;
  if (!v.GetPathNameData(c, num_paths, path_name_type, &paths))
    return v.error();

  const GLfloat* transforms = nullptr;
  if (!v.GetTransforms(c, num_paths, transform_type, &transforms))
    return v.error();

  GLint reference = static_cast<GLint>(c.reference);
  GLuint mask = static_cast<GLuint>(c.mask);

  if (!CheckBoundDrawFramebufferValid(kFunctionName))
    return error::kNoError;
  ApplyDirtyState();
  glStencilThenCoverStrokePathInstancedNV(
      num_paths, GL_UNSIGNED_INT, paths.get(), 0, reference, mask, cover_mode,
      transform_type, transforms);
  return error::kNoError;
}

void GLES2DecoderImpl::DoBindFragmentInputLocationCHROMIUM(
    GLuint program_id,
    GLint location,
    const std::string& name) {
  static const char kFunctionName[] = "glBindFragmentInputLocationCHROMIUM";
  if (!StringIsValidForGLES(name)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, kFunctionName, "invalid character");
    return;
  }
  if (ProgramManager::HasBuiltInPrefix(name)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, kFunctionName, "reserved prefix");
    return;
  }
  Program* program = GetProgram(program_id);
  if (!program || program->IsDeleted()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, kFunctionName, "invalid program");
    return;
  }
  if (location < 0 ||
      static_cast<uint32_t>(location) >= group_->max_varying_vectors() * 4) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, kFunctionName,
                       "location out of range");
    return;
  }

  program->SetFragmentInputLocationBinding(name, location);
}

const SamplerState& GLES2DecoderImpl::GetSamplerStateForTextureUnit(
    GLenum target, GLuint unit) {
  if (features().enable_samplers) {
    Sampler* sampler = state_.sampler_units[unit].get();
    if (sampler)
      return sampler->sampler_state();
  }
  TextureUnit& texture_unit = state_.texture_units[unit];
  TextureRef* texture_ref = texture_unit.GetInfoForSamplerType(target).get();
  if (texture_ref)
    return texture_ref->texture()->sampler_state();

  return default_sampler_state_;
}

bool GLES2DecoderImpl::NeedsCopyTextureImageWorkaround(
    GLenum internal_format,
    int32_t channels_exist,
    GLuint* source_texture_service_id,
    GLenum* source_texture_target) {
  // On some OSX devices, copyTexImage2D will fail if all of these conditions
  // are met:
  //   1. The internal format of the new texture is not GL_RGB or GL_RGBA.
  //   2. The image of the read FBO is backed by an IOSurface.
  // See https://crbug.com/581777#c4 for more details.
  if (!workarounds().use_intermediary_for_copy_texture_image)
    return false;

  if (internal_format == GL_RGB || internal_format == GL_RGBA)
    return false;

  Framebuffer* framebuffer = GetBoundReadFramebuffer();
  if (!framebuffer)
    return false;

  const Framebuffer::Attachment* attachment =
      framebuffer->GetReadBufferAttachment();
  if (!attachment)
    return false;

  if (!attachment->IsTextureAttachment())
    return false;

  TextureRef* texture =
      texture_manager()->GetTexture(attachment->object_name());
  if (!texture->texture()->HasImages())
    return false;

  // The workaround only works if the source texture consists of the channels
  // kRGB or kRGBA.
  if (channels_exist != GLES2Util::kRGBA && channels_exist != GLES2Util::kRGB)
    return false;

  *source_texture_target = texture->texture()->target();
  *source_texture_service_id = texture->service_id();
  return true;
}

bool GLES2DecoderImpl::ChromiumImageNeedsRGBEmulation() {
  gpu::ImageFactory* factory = GetContextGroup()->image_factory();
  return factory ? !factory->SupportsFormatRGB() : false;
}

void GLES2DecoderImpl::ClearScheduleCALayerState() {
  ca_layer_shared_state_.reset();
}

void GLES2DecoderImpl::ClearFramebufferForWorkaround(GLbitfield mask) {
  ScopedGLErrorSuppressor suppressor("GLES2DecoderImpl::ClearWorkaround",
                                     GetErrorState());
  clear_framebuffer_blit_->ClearFramebuffer(
      this, GetBoundReadFramebufferSize(), mask, state_.color_clear_red,
      state_.color_clear_green, state_.color_clear_blue,
      state_.color_clear_alpha, state_.depth_clear, state_.stencil_clear);
}

error::Error GLES2DecoderImpl::HandleBindFragmentInputLocationCHROMIUMBucket(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BindFragmentInputLocationCHROMIUMBucket& c =
      *static_cast<
          const volatile gles2::cmds::BindFragmentInputLocationCHROMIUMBucket*>(
          cmd_data);
  if (!features().chromium_path_rendering) {
    return error::kUnknownCommand;
  }

  GLuint program = static_cast<GLuint>(c.program);
  GLint location = static_cast<GLint>(c.location);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  DoBindFragmentInputLocationCHROMIUM(program, location, name_str);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleProgramPathFragmentInputGenCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  static const char kFunctionName[] = "glProgramPathFragmentInputGenCHROMIUM";
  const volatile gles2::cmds::ProgramPathFragmentInputGenCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::ProgramPathFragmentInputGenCHROMIUM*>(
          cmd_data);
  if (!features().chromium_path_rendering) {
    return error::kUnknownCommand;
  }

  GLint program_id = static_cast<GLint>(c.program);

  Program* program = GetProgram(program_id);
  if (!program || !program->IsValid() || program->IsDeleted()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, kFunctionName, "invalid program");
    return error::kNoError;
  }

  GLenum gen_mode = static_cast<GLint>(c.genMode);
  if (!validators_->path_fragment_input_gen_mode.IsValid(gen_mode)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(kFunctionName, gen_mode, "genMode");
    return error::kNoError;
  }

  GLint components = static_cast<GLint>(c.components);
  if (components < 0 || components > 4) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, kFunctionName,
                       "components out of range");
    return error::kNoError;
  }

  if ((components != 0 && gen_mode == GL_NONE) ||
      (components == 0 && gen_mode != GL_NONE)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, kFunctionName,
                       "components and genMode do not match");
    return error::kNoError;
  }

  GLint location = static_cast<GLint>(c.location);
  if (program->IsInactiveFragmentInputLocationByFakeLocation(location))
    return error::kNoError;

  const Program::FragmentInputInfo* fragment_input_info =
      program->GetFragmentInputInfoByFakeLocation(location);
  if (!fragment_input_info) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, kFunctionName, "unknown location");
    return error::kNoError;
  }
  GLint real_location = fragment_input_info->location;

  const GLfloat* coeffs = NULL;

  if (components > 0) {
    GLint components_needed = -1;

    switch (fragment_input_info->type) {
      case GL_FLOAT:
        components_needed = 1;
        break;
      case GL_FLOAT_VEC2:
        components_needed = 2;
        break;
      case GL_FLOAT_VEC3:
        components_needed = 3;
        break;
      case GL_FLOAT_VEC4:
        components_needed = 4;
        break;
      default:
        LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, kFunctionName,
                           "fragment input type is not single-precision "
                           "floating-point scalar or vector");
        return error::kNoError;
    }

    if (components_needed != components) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, kFunctionName,
                         "components does not match fragment input type");
      return error::kNoError;
    }
    uint32_t coeffs_per_component =
        GLES2Util::GetCoefficientCountForGLPathFragmentInputGenMode(gen_mode);
    // The multiplication below will not overflow.
    DCHECK(coeffs_per_component > 0 && coeffs_per_component <= 4);
    DCHECK(components > 0 && components <= 4);
    uint32_t coeffs_size = sizeof(GLfloat) * coeffs_per_component * components;

    uint32_t coeffs_shm_id = static_cast<uint32_t>(c.coeffs_shm_id);
    uint32_t coeffs_shm_offset = static_cast<uint32_t>(c.coeffs_shm_offset);

    if (coeffs_shm_id != 0 || coeffs_shm_offset != 0) {
      coeffs = GetSharedMemoryAs<const GLfloat*>(
          coeffs_shm_id, coeffs_shm_offset, coeffs_size);
    }

    if (!coeffs) {
      return error::kOutOfBounds;
    }
  }
  glProgramPathFragmentInputGenNV(program->service_id(), real_location,
                                  gen_mode, components, coeffs);
  return error::kNoError;
}

void GLES2DecoderImpl::RestoreAllExternalTextureBindingsIfNeeded() {
  if (texture_manager()->GetServiceIdGeneration() ==
      texture_manager_service_id_generation_)
    return;

  // Texture manager's version has changed, so rebind all external textures
  // in case their service ids have changed.
  for (unsigned texture_unit_index = 0;
       texture_unit_index < state_.texture_units.size(); texture_unit_index++) {
    TextureUnit& texture_unit = state_.texture_units[texture_unit_index];
    if (texture_unit.bind_target != GL_TEXTURE_EXTERNAL_OES)
      continue;

    if (TextureRef* texture_ref =
            texture_unit.bound_texture_external_oes.get()) {
      glActiveTexture(GL_TEXTURE0 + texture_unit_index);
      glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture_ref->service_id());
    }
  }

  glActiveTexture(GL_TEXTURE0 + state_.active_texture_unit);

  texture_manager_service_id_generation_ =
      texture_manager()->GetServiceIdGeneration();
}

// Include the auto-generated part of this file. We split this because it means
// we can easily edit the non-auto generated parts right here in this file
// instead of having to edit some template or the code generator.
#include "base/macros.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_autogen.h"

}  // namespace gles2
}  // namespace gpu
