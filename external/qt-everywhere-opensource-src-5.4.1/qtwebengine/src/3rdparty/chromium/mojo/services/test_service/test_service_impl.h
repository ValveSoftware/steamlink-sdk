// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_TEST_SERVICE_TEST_SERVICE_IMPL_H_
#define MOJO_SERVICES_TEST_SERVICE_TEST_SERVICE_IMPL_H_

#include "mojo/public/cpp/system/macros.h"
#include "mojo/services/test_service/test_service.mojom.h"

namespace mojo {
namespace test {

class TestServiceApplication;

class TestServiceImpl : public InterfaceImpl<ITestService> {
 public:
  explicit TestServiceImpl(TestServiceApplication* application);
  virtual ~TestServiceImpl();

  // |ITestService| methods:
  virtual void OnConnectionEstablished() MOJO_OVERRIDE;
  virtual void OnConnectionError() MOJO_OVERRIDE;
  virtual void Ping(const mojo::Callback<void()>& callback) MOJO_OVERRIDE;

 private:
  TestServiceApplication* const application_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(TestServiceImpl);
};

}  // namespace test
}  // namespace mojo

#endif  // MOJO_SERVICES_TEST_SERVICE_TEST_SERVICE_IMPL_H_
