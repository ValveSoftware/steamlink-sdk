// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_GPU_BENCHMARKING_EXTENSION_H_
#define CONTENT_RENDERER_GPU_GPU_BENCHMARKING_EXTENSION_H_

namespace v8 {
class Extension;
}

namespace content {

// V8 extension for gpu benchmarking
class GpuBenchmarkingExtension {
 public:
  static v8::Extension* Get();
};

}  // namespace content

#endif // CONTENT_RENDERER_GPU_GPU_BENCHMARKING_EXTENSION_H_
