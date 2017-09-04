// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copied from mojo/edk/test/run_all_perftests.cc.

#include "base/command_line.h"
#include "base/test/perf_test_suite.h"
#include "mojo/edk/embedder/embedder.h"

int main(int argc, char** argv) {
  base::PerfTestSuite test(argc, argv);

  mojo::edk::Init();

  return test.Run();
}
