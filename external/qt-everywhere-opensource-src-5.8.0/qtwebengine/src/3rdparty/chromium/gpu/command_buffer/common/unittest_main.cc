// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_io_thread.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "mojo/edk/embedder/embedder.h"

#if defined(OS_ANDROID)
#include "gpu/ipc/client/android/in_process_surface_texture_manager.h"
#endif

namespace {

class GpuTestSuite : public base::TestSuite {
 public:
  GpuTestSuite(int argc, char** argv);
  ~GpuTestSuite() override;

 protected:
  void Initialize() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuTestSuite);
};

GpuTestSuite::GpuTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv) {}

GpuTestSuite::~GpuTestSuite() {}

void GpuTestSuite::Initialize() {
  base::TestSuite::Initialize();
#if defined(OS_ANDROID)
  gpu::SurfaceTextureManager::SetInstance(
      gpu::InProcessSurfaceTextureManager::GetInstance());
#endif
}

}  // namespace

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  GpuTestSuite test_suite(argc, argv);

  mojo::edk::Init();

  return base::LaunchUnitTests(
      argc, argv,
      base::Bind(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
