// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/image_transport_factory.h"

#include "base/command_line.h"
#include "content/browser/compositor/gpu_process_transport_factory.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gl/gl_implementation.h"

namespace content {

namespace {
ImageTransportFactory* g_factory = NULL;
bool g_initialized_for_unit_tests = false;
static gl::DisableNullDrawGLBindings* g_disable_null_draw = NULL;

void SetFactory(ImageTransportFactory* factory) {
  g_factory = factory;
}

}

// static
void ImageTransportFactory::Initialize() {
  DCHECK(!g_factory || g_initialized_for_unit_tests);
  if (g_initialized_for_unit_tests)
    return;
  SetFactory(new GpuProcessTransportFactory);
}

void ImageTransportFactory::InitializeForUnitTests(
    std::unique_ptr<ImageTransportFactory> factory) {
  DCHECK(!g_factory);
  DCHECK(!g_initialized_for_unit_tests);
  g_initialized_for_unit_tests = true;

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnablePixelOutputInTests))
    g_disable_null_draw = new gl::DisableNullDrawGLBindings;

  SetFactory(factory.release());
}

// static
void ImageTransportFactory::Terminate() {
  delete g_factory;
  g_factory = NULL;
  delete g_disable_null_draw;
  g_disable_null_draw = NULL;
  g_initialized_for_unit_tests = false;
}

// static
ImageTransportFactory* ImageTransportFactory::GetInstance() {
  return g_factory;
}

}  // namespace content
