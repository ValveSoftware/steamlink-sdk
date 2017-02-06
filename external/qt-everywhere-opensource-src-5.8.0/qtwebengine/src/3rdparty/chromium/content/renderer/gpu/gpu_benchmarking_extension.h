// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_GPU_BENCHMARKING_EXTENSION_H_
#define CONTENT_RENDERER_GPU_GPU_BENCHMARKING_EXTENSION_H_

#include "base/macros.h"
#include "gin/wrappable.h"

namespace blink {
class WebFrame;
}

namespace gin {
class Arguments;
}

namespace v8 {
class Function;
class Isolate;
class Object;
}

namespace content {

// gin class for gpu benchmarking
class GpuBenchmarking : public gin::Wrappable<GpuBenchmarking> {
 public:
  static gin::WrapperInfo kWrapperInfo;
  static void Install(blink::WebFrame* frame);

 private:
  GpuBenchmarking();
  ~GpuBenchmarking() override;

  // gin::Wrappable.
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  // JavaScript handlers.
  void SetNeedsDisplayOnAllLayers();
  void SetRasterizeOnlyVisibleContent();
  void PrintToSkPicture(v8::Isolate* isolate, const std::string& dirname);
  bool GestureSourceTypeSupported(int gesture_source_type);
  bool SmoothScrollBy(gin::Arguments* args);
  bool SmoothDrag(gin::Arguments* args);
  bool Swipe(gin::Arguments* args);
  bool ScrollBounce(gin::Arguments* args);
  bool PinchBy(gin::Arguments* args);
  float VisualViewportX();
  float VisualViewportY();
  float VisualViewportHeight();
  float VisualViewportWidth();
  float PageScaleFactor();
  bool Tap(gin::Arguments* args);
  void ClearImageCache();
  int RunMicroBenchmark(gin::Arguments* args);
  bool SendMessageToMicroBenchmark(int id, v8::Local<v8::Object> message);
  bool HasGpuChannel();
  bool HasGpuProcess();
  void GetGpuDriverBugWorkarounds(gin::Arguments* args);

  DISALLOW_COPY_AND_ASSIGN(GpuBenchmarking);
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_GPU_BENCHMARKING_EXTENSION_H_
