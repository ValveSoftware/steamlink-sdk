// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_WINDOW_CONTAINER_TYPE_H_
#define CONTENT_PUBLIC_COMMON_WINDOW_CONTAINER_TYPE_H_

#include "content/common/content_export.h"
#include "content/public/common/window_container_type.mojom.h"

namespace blink {

struct WebWindowFeatures;

}

using WindowContainerType = content::mojom::WindowContainerType;

// TODO(rockot): Remove these duplicate definitions by updating all references
// to point directly to the mojom enum values.
extern const WindowContainerType CONTENT_EXPORT WINDOW_CONTAINER_TYPE_NORMAL;

extern const WindowContainerType CONTENT_EXPORT
WINDOW_CONTAINER_TYPE_BACKGROUND;

extern const WindowContainerType CONTENT_EXPORT
WINDOW_CONTAINER_TYPE_PERSISTENT;

// Conversion function:
WindowContainerType WindowFeaturesToContainerType(
    const blink::WebWindowFeatures& window_features);

#endif  // CONTENT_PUBLIC_COMMON_WINDOW_CONTAINER_TYPE_H_
