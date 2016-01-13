// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEMORY_BENCHMARKING_EXTENSION_H_
#define CONTENT_RENDERER_MEMORY_BENCHMARKING_EXTENSION_H_

#include "base/basictypes.h"
#include "gin/wrappable.h"

namespace blink {
class WebFrame;
}

namespace gin {
class Arguments;
}

namespace content {

class MemoryBenchmarkingExtension
    : public gin::Wrappable<MemoryBenchmarkingExtension> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static void Install(blink::WebFrame* frame);

 private:
  MemoryBenchmarkingExtension();
  virtual ~MemoryBenchmarkingExtension();

  // gin::Wrappable.
  virtual gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) OVERRIDE;

  bool IsHeapProfilerRunning();

  void HeapProfilerDump(gin::Arguments* args);

  DISALLOW_COPY_AND_ASSIGN(MemoryBenchmarkingExtension);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEMORY_BENCHMARKING_EXTENSION_H_
