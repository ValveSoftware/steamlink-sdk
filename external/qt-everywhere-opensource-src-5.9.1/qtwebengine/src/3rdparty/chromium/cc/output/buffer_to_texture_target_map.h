// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_BUFFER_TO_TEXTURE_TARGET_MAP_H_
#define CC_OUTPUT_BUFFER_TO_TEXTURE_TARGET_MAP_H_

#include <map>
#include <string>

#include "cc/base/cc_export.h"
#include "ui/gfx/buffer_types.h"

namespace cc {
// A map of GPU Memory Buffer usage/format to GL texture target.
using BufferToTextureTargetKey = std::pair<gfx::BufferUsage, gfx::BufferFormat>;
using BufferToTextureTargetMap = std::map<BufferToTextureTargetKey, uint32_t>;

// Converts a serialized ImageTextureTargetsMap back to the runtime format.
// Serialization takes the form:
//   "usage,format,target;usage,format,target;...;usage,format,target"
CC_EXPORT BufferToTextureTargetMap
StringToBufferToTextureTargetMap(const std::string& str);

// Converts an ImageTextureTargetsMap to a string representation of the format:
//   "usage,format,target;usage,format,target;...;usage,format,target"
CC_EXPORT std::string BufferToTextureTargetMapToString(
    const BufferToTextureTargetMap& map);

// Returns a default-initialized BufferToTextureTargetsMap where every entry
// maps to GL_TEXTURE_2D.
CC_EXPORT BufferToTextureTargetMap DefaultBufferToTextureTargetMapForTesting();

}  // namespace cc

#endif  // CC_OUTPUT_BUFFER_TO_TEXTURE_TARGET_MAP_H_
