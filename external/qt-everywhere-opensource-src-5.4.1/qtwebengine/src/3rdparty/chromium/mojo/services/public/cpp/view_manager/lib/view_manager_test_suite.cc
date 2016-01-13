// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/lib/view_manager_test_suite.h"

#include "ui/gl/gl_surface.h"

namespace mojo {
namespace view_manager {

ViewManagerTestSuite::ViewManagerTestSuite(int argc, char** argv)
    : TestSuite(argc, argv) {}

ViewManagerTestSuite::~ViewManagerTestSuite() {
}

void ViewManagerTestSuite::Initialize() {
  base::TestSuite::Initialize();
  gfx::GLSurface::InitializeOneOffForTests();
}

}  // namespace view_manager
}  // namespace mojo
