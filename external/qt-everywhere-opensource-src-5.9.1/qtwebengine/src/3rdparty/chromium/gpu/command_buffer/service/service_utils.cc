// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/service_utils.h"

#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/gpu_preferences.h"

namespace gpu {
namespace gles2 {

gl::GLContextAttribs GenerateGLContextAttribs(
    const ContextCreationAttribHelper& attribs_helper,
    const GpuPreferences& gpu_preferences) {
  gl::GLContextAttribs attribs;
  attribs.gpu_preference = attribs_helper.gpu_preference;
  if (gpu_preferences.use_passthrough_cmd_decoder) {
    attribs.bind_generates_resource = attribs_helper.bind_generates_resource;
    attribs.webgl_compatibility_context =
        IsWebGLContextType(attribs_helper.context_type);
  }
  return attribs;
}

}  // namespace gles2
}  // namespace gpu
