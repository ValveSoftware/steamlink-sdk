// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/test_service/test_service_application.h"

#include <assert.h>

#include "mojo/public/cpp/utility/run_loop.h"
#include "mojo/services/test_service/test_service_impl.h"

namespace mojo {
namespace test {

TestServiceApplication::TestServiceApplication() : ref_count_(0) {
}

TestServiceApplication::~TestServiceApplication() {
}

void TestServiceApplication::Initialize() {
  AddService<TestServiceImpl>(this);
}

void TestServiceApplication::AddRef() {
  assert(ref_count_ >= 0);
  ref_count_++;
}

void TestServiceApplication::ReleaseRef() {
  assert(ref_count_ > 0);
  ref_count_--;
  if (ref_count_ <= 0)
    RunLoop::current()->Quit();
}

}  // namespace test

// static
Application* Application::Create() {
  return new mojo::test::TestServiceApplication();
}

}  // namespace mojo
