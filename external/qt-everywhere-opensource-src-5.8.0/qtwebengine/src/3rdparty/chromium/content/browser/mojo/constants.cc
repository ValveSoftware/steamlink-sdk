// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mojo/constants.h"

namespace content {

// The default application name the browser identifies as when connecting to
// the shell. This must match the name in
// src/content/public/app/mojo/content_browser_manifest.json.
const char kBrowserMojoApplicationName[] = "exe:content_browser";

// The default application name used to identify the gpu process when
// connecting it to the shell. This must match the name in
// src/content/public/app/mojo/content_gpu_manifest.json.
const char kGpuMojoApplicationName[] = "exe:content_gpu";

// The default application name used to identify render processes when
// connecting them to the shell. This must match the name in
// src/content/public/app/mojo/content_renderer_manifest.json.
const char kRendererMojoApplicationName[] = "exe:content_renderer";

// The default application name used to identify utility processes when
// connecting them to the shell. This must match the name in
// src/content/public/app/mojo/content_utility_manifest.json.
const char kUtilityMojoApplicationName[] = "exe:content_utility";

}  // namespace content
