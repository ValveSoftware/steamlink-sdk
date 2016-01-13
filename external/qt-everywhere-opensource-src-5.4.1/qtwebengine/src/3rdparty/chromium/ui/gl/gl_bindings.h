// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_BINDINGS_H_
#define UI_GL_GL_BINDINGS_H_

// Includes the platform independent and platform dependent GL headers.
// Only include this in cc files. It pulls in system headers, including
// the X11 headers on linux, which define all kinds of macros that are
// liable to cause conflicts.

#include <GL/gl.h>
#include <GL/glext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "base/logging.h"
#include "base/threading/thread_local.h"
#include "build/build_config.h"
#include "ui/gl/gl_export.h"

// The standard OpenGL native extension headers are also included.
#if defined(OS_WIN)
#include <GL/wglext.h>
#elif defined(OS_MACOSX)
#include <OpenGL/OpenGL.h>
#elif defined(USE_X11)
#include <GL/glx.h>
#include <GL/glxext.h>

// Undefine some macros defined by X headers. This is why this file should only
// be included in .cc files.
#undef Bool
#undef None
#undef Status
#endif


// GLES2 defines not part of Desktop GL
// Shader Precision-Specified Types
#define GL_LOW_FLOAT                                     0x8DF0
#define GL_MEDIUM_FLOAT                                  0x8DF1
#define GL_HIGH_FLOAT                                    0x8DF2
#define GL_LOW_INT                                       0x8DF3
#define GL_MEDIUM_INT                                    0x8DF4
#define GL_HIGH_INT                                      0x8DF5
#define GL_IMPLEMENTATION_COLOR_READ_TYPE                0x8B9A
#define GL_IMPLEMENTATION_COLOR_READ_FORMAT              0x8B9B
#define GL_MAX_FRAGMENT_UNIFORM_VECTORS                  0x8DFD
#define GL_MAX_VERTEX_UNIFORM_VECTORS                    0x8DFB
#define GL_MAX_VARYING_VECTORS                           0x8DFC
#define GL_SHADER_BINARY_FORMATS                         0x8DF8
#define GL_NUM_SHADER_BINARY_FORMATS                     0x8DF9
#define GL_SHADER_COMPILER                               0x8DFA
#define GL_RGB565                                        0x8D62
#define GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES           0x8B8B
#define GL_RGB8_OES                                      0x8051
#define GL_RGBA8_OES                                     0x8058
#define GL_HALF_FLOAT_OES                                0x8D61

// GL_OES_EGL_image_external
#define GL_TEXTURE_EXTERNAL_OES                          0x8D65
#define GL_SAMPLER_EXTERNAL_OES                          0x8D66
#define GL_TEXTURE_BINDING_EXTERNAL_OES                  0x8D67
#define GL_REQUIRED_TEXTURE_IMAGE_UNITS_OES              0x8D68

// GL_ANGLE_translated_shader_source
#define GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE         0x93A0

// GL_CHROMIUM_flipy
#define GL_UNPACK_FLIP_Y_CHROMIUM                        0x9240

#define GL_UNPACK_PREMULTIPLY_ALPHA_CHROMIUM             0x9241
#define GL_UNPACK_UNPREMULTIPLY_ALPHA_CHROMIUM           0x9242
#define GL_UNPACK_COLORSPACE_CONVERSION_CHROMIUM         0x9243
#define GL_BIND_GENERATES_RESOURCE_CHROMIUM              0x9244

// GL_CHROMIUM_gpu_memory_manager
#define GL_TEXTURE_POOL_CHROMIUM                         0x6000
#define GL_TEXTURE_POOL_MANAGED_CHROMIUM                 0x6001
#define GL_TEXTURE_POOL_UNMANAGED_CHROMIUM               0x6002

// GL_ANGLE_pack_reverse_row_order
#define GL_PACK_REVERSE_ROW_ORDER_ANGLE                  0x93A4

// GL_ANGLE_texture_usage
#define GL_TEXTURE_USAGE_ANGLE                           0x93A2
#define GL_FRAMEBUFFER_ATTACHMENT_ANGLE                  0x93A3

// GL_EXT_texture_storage
#define GL_TEXTURE_IMMUTABLE_FORMAT_EXT                  0x912F
#define GL_ALPHA8_EXT                                    0x803C
#define GL_LUMINANCE8_EXT                                0x8040
#define GL_LUMINANCE8_ALPHA8_EXT                         0x8045
#define GL_RGBA32F_EXT                                   0x8814
#define GL_RGB32F_EXT                                    0x8815
#define GL_ALPHA32F_EXT                                  0x8816
#define GL_LUMINANCE32F_EXT                              0x8818
#define GL_LUMINANCE_ALPHA32F_EXT                        0x8819
#define GL_RGBA16F_EXT                                   0x881A
#define GL_RGB16F_EXT                                    0x881B
#define GL_ALPHA16F_EXT                                  0x881C
#define GL_LUMINANCE16F_EXT                              0x881E
#define GL_LUMINANCE_ALPHA16F_EXT                        0x881F
#define GL_BGRA8_EXT                                     0x93A1

// GL_ANGLE_instanced_arrays
#define GL_VERTEX_ATTRIB_ARRAY_DIVISOR_ANGLE             0x88FE

// GL_EXT_occlusion_query_boolean
#define GL_ANY_SAMPLES_PASSED_EXT                        0x8C2F
#define GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT           0x8D6A
#define GL_CURRENT_QUERY_EXT                             0x8865
#define GL_QUERY_RESULT_EXT                              0x8866
#define GL_QUERY_RESULT_AVAILABLE_EXT                    0x8867

// GL_CHROMIUM_command_buffer_query
#define GL_COMMANDS_ISSUED_CHROMIUM                      0x84F2

/* GL_CHROMIUM_get_error_query */
#define GL_GET_ERROR_QUERY_CHROMIUM                      0x84F3

/* GL_CHROMIUM_command_buffer_latency_query */
#define GL_LATENCY_QUERY_CHROMIUM                        0x84F4

/* GL_CHROMIUM_async_pixel_transfers */
#define GL_ASYNC_PIXEL_UNPACK_COMPLETED_CHROMIUM         0x84F5
#define GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM           0x84F6

// GL_CHROMIUM_sync_query
#define GL_COMMANDS_COMPLETED_CHROMIUM                   0x84F7

// GL_CHROMIUM_image
#define GL_IMAGE_ROWBYTES_CHROMIUM                       0x78F0
#define GL_IMAGE_MAP_CHROMIUM                            0x78F1
#define GL_IMAGE_SCANOUT_CHROMIUM                        0x78F2

// GL_OES_texure_3D
#define GL_SAMPLER_3D_OES                                0x8B5F

// GL_OES_depth24
#define GL_DEPTH_COMPONENT24_OES                         0x81A6

// GL_OES_depth32
#define GL_DEPTH_COMPONENT32_OES                         0x81A7

// GL_OES_packed_depth_stencil
#ifndef GL_DEPTH24_STENCIL8_OES
#define GL_DEPTH24_STENCIL8_OES                          0x88F0
#endif

#ifndef GL_DEPTH24_STENCIL8
#define GL_DEPTH24_STENCIL8                              0x88F0
#endif

// GL_OES_compressed_ETC1_RGB8_texture
#define GL_ETC1_RGB8_OES                                 0x8D64

// GL_AMD_compressed_ATC_texture
#define GL_ATC_RGB_AMD                                   0x8C92
#define GL_ATC_RGBA_EXPLICIT_ALPHA_AMD                   0x8C93
#define GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD               0x87EE

// GL_IMG_texture_compression_pvrtc
#define GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG               0x8C00
#define GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG               0x8C01
#define GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG              0x8C02
#define GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG              0x8C03

// GL_OES_vertex_array_object
#define GL_VERTEX_ARRAY_BINDING_OES                      0x85B5

// GL_CHROMIUM_pixel_transfer_buffer_object
#define GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM         0x78EC
#define GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM           0x78ED
#define GL_PIXEL_PACK_TRANSFER_BUFFER_BINDING_CHROMIUM   0x78EE
#define GL_PIXEL_UNPACK_TRANSFER_BUFFER_BINDING_CHROMIUM 0x78EF

/* GL_EXT_discard_framebuffer */
#ifndef GL_EXT_discard_framebuffer
#define GL_COLOR_EXT                                     0x1800
#define GL_DEPTH_EXT                                     0x1801
#define GL_STENCIL_EXT                                   0x1802
#endif

// GL_ARB_get_program_binary
#define PROGRAM_BINARY_RETRIEVABLE_HINT                  0x8257
// GL_OES_get_program_binary
#define GL_PROGRAM_BINARY_LENGTH_OES                     0x8741
#define GL_NUM_PROGRAM_BINARY_FORMATS_OES                0x87FE
#define GL_PROGRAM_BINARY_FORMATS_OES                    0x87FF

#ifndef GL_EXT_multisampled_render_to_texture
#define GL_RENDERBUFFER_SAMPLES_EXT                      0x8CAB
#define GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT        0x8D56
#define GL_MAX_SAMPLES_EXT                               0x8D57
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_SAMPLES_EXT    0x8D6C
#endif

#ifndef GL_IMG_multisampled_render_to_texture
#define GL_RENDERBUFFER_SAMPLES_IMG                      0x9133
#define GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_IMG        0x9134
#define GL_MAX_SAMPLES_IMG                               0x9135
#define GL_TEXTURE_SAMPLES_IMG                           0x9136
#endif

#define GL_GLEXT_PROTOTYPES 1

#if defined(OS_WIN)
#define GL_BINDING_CALL WINAPI
#else
#define GL_BINDING_CALL
#endif

#define GL_SERVICE_LOG(args) DLOG(INFO) << args;
#if defined(NDEBUG)
  #define GL_SERVICE_LOG_CODE_BLOCK(code)
#else
  #define GL_SERVICE_LOG_CODE_BLOCK(code) code
#endif

// Forward declare OSMesa types.
typedef struct osmesa_context *OSMesaContext;
typedef void (*OSMESAproc)();

// Forward declare EGL types.
typedef uint64 EGLuint64CHROMIUM;

#include "gl_bindings_autogen_gl.h"
#include "gl_bindings_autogen_osmesa.h"

#if defined(OS_WIN)
#include "gl_bindings_autogen_egl.h"
#include "gl_bindings_autogen_wgl.h"
#elif defined(USE_X11)
#include "gl_bindings_autogen_egl.h"
#include "gl_bindings_autogen_glx.h"
#elif defined(USE_OZONE)
#include "gl_bindings_autogen_egl.h"
#elif defined(OS_ANDROID)
#include "gl_bindings_autogen_egl.h"
#endif

namespace gfx {

struct GL_EXPORT DriverGL {
  void InitializeStaticBindings();
  void InitializeCustomDynamicBindings(GLContext* context);
  void InitializeDebugBindings();
  void InitializeNullDrawBindings();
  // TODO(danakj): Remove this when all test suites are using null-draw.
  bool HasInitializedNullDrawBindings();
  bool SetNullDrawBindingsEnabled(bool enabled);
  void ClearBindings();

  ProcsGL fn;
  ProcsGL orig_fn;
  ProcsGL debug_fn;
  ExtensionsGL ext;
  bool null_draw_bindings_enabled;

 private:
  void InitializeDynamicBindings(GLContext* context);
};

struct GL_EXPORT DriverOSMESA {
  void InitializeStaticBindings();
  void InitializeDynamicBindings(GLContext* context);
  void InitializeDebugBindings();
  void ClearBindings();

  ProcsOSMESA fn;
  ProcsOSMESA debug_fn;
  ExtensionsOSMESA ext;
};

#if defined(OS_WIN)
struct GL_EXPORT DriverWGL {
  void InitializeStaticBindings();
  void InitializeDynamicBindings(GLContext* context);
  void InitializeDebugBindings();
  void ClearBindings();

  ProcsWGL fn;
  ProcsWGL debug_fn;
  ExtensionsWGL ext;
};
#endif

#if defined(OS_WIN) || defined(USE_X11) || defined(OS_ANDROID) || defined(USE_OZONE)
struct GL_EXPORT DriverEGL {
  void InitializeStaticBindings();
  void InitializeDynamicBindings(GLContext* context);
  void InitializeDebugBindings();
  void ClearBindings();

  ProcsEGL fn;
  ProcsEGL debug_fn;
  ExtensionsEGL ext;
};
#endif

#if defined(USE_X11)
struct GL_EXPORT DriverGLX {
  void InitializeStaticBindings();
  void InitializeDynamicBindings(GLContext* context);
  void InitializeDebugBindings();
  void ClearBindings();

  ProcsGLX fn;
  ProcsGLX debug_fn;
  ExtensionsGLX ext;
};
#endif

// This #define is here to support autogenerated code.
#define g_current_gl_context g_current_gl_context_tls->Get()
GL_EXPORT extern base::ThreadLocalPointer<GLApi>* g_current_gl_context_tls;

GL_EXPORT extern OSMESAApi* g_current_osmesa_context;
GL_EXPORT extern DriverGL g_driver_gl;
GL_EXPORT extern DriverOSMESA g_driver_osmesa;

#if defined(OS_WIN)

GL_EXPORT extern EGLApi* g_current_egl_context;
GL_EXPORT extern WGLApi* g_current_wgl_context;
GL_EXPORT extern DriverEGL g_driver_egl;
GL_EXPORT extern DriverWGL g_driver_wgl;

#elif defined(USE_X11)

GL_EXPORT extern EGLApi* g_current_egl_context;
GL_EXPORT extern GLXApi* g_current_glx_context;
GL_EXPORT extern DriverEGL g_driver_egl;
GL_EXPORT extern DriverGLX g_driver_glx;

#elif defined(USE_OZONE)

GL_EXPORT extern EGLApi* g_current_egl_context;
GL_EXPORT extern DriverEGL g_driver_egl;

#elif defined(OS_ANDROID)

GL_EXPORT extern EGLApi* g_current_egl_context;
GL_EXPORT extern DriverEGL g_driver_egl;

#endif

}  // namespace gfx

#endif  // UI_GL_GL_BINDINGS_H_
