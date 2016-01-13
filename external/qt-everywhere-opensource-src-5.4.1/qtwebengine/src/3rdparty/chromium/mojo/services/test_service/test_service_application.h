// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_TEST_SERVICE_TEST_SERVICE_APPLICATION_H_
#define MOJO_SERVICES_TEST_SERVICE_TEST_SERVICE_APPLICATION_H_

#include "mojo/public/cpp/application/application.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {
namespace test {

class TestServiceApplication : public Application {
 public:
  TestServiceApplication();
  virtual ~TestServiceApplication();

  virtual void Initialize() MOJO_OVERRIDE;

  void AddRef();
  void ReleaseRef();

 private:
  int ref_count_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(TestServiceApplication);
};

}  // namespace test
}  // namespace mojo

#endif  // MOJO_SERVICES_TEST_SERVICE_TEST_SERVICE_APPLICATION_H_
