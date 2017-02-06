// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_COMMON_SWITCHES_H_
#define COMPONENTS_MUS_COMMON_SWITCHES_H_

#include "components/mus/common/mus_common_export.h"

namespace mus {
namespace switches {

// All args in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
extern const char MUS_COMMON_EXPORT kUseMojoGpuCommandBufferInMus[];
extern const char MUS_COMMON_EXPORT kUseTestConfig[];

}  // namespace switches
}  // namespace mus

#endif  // COMPONENTS_MUS_COMMON_SWITCHES_H_
