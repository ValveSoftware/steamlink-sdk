// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file includes all the necessary GL headers and implements some useful
// utilities.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GL_UTILS_H_
#define GPU_COMMAND_BUFFER_SERVICE_GL_UTILS_H_

#include <vector>

#include "build/build_config.h"
#include "ui/gl/gl_bindings.h"

// Define this for extra GL error debugging (slower).
// #define GL_ERROR_DEBUGGING
#ifdef GL_ERROR_DEBUGGING
#define CHECK_GL_ERROR() do {                                           \
    GLenum gl_error = glGetError();                                     \
    LOG_IF(ERROR, gl_error != GL_NO_ERROR) << "GL Error :" << gl_error; \
  } while (0)
#else  // GL_ERROR_DEBUGGING
#define CHECK_GL_ERROR() void(0)
#endif  // GL_ERROR_DEBUGGING

namespace gl {
struct GLVersionInfo;
}

namespace gpu {

struct Capabilities;
class FeatureInfo;

namespace gles2 {

std::vector<int> GetAllGLErrors();

bool PrecisionMeetsSpecForHighpFloat(GLint rangeMin,
                                     GLint rangeMax,
                                     GLint precision);
void QueryShaderPrecisionFormat(const gl::GLVersionInfo& gl_version_info,
                                GLenum shader_type,
                                GLenum precision_type,
                                GLint* range,
                                GLint* precision);

// Using the provided feature info, query the numeric limits of the underlying
// GL and fill in the members of the Capabilities struct.  Does not perform any
// extension checks.
void PopulateNumericCapabilities(Capabilities* caps,
                                 const FeatureInfo* feature_info);

bool CheckUniqueAndNonNullIds(GLsizei n, const GLuint* client_ids);

const char* GetServiceVersionString(const FeatureInfo* feature_info);
const char* GetServiceShadingLanguageVersionString(
    const FeatureInfo* feature_info);
const char* GetServiceRendererString(const FeatureInfo* feature_info);
const char* GetServiceVendorString(const FeatureInfo* feature_info);

void APIENTRY LogGLDebugMessage(GLenum source,
                                GLenum type,
                                GLuint id,
                                GLenum severity,
                                GLsizei length,
                                const GLchar* message,
                                GLvoid* user_param);

void InitializeGLDebugLogging();

} // gles2
} // gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GL_UTILS_H_
