// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/test_service/test_service_impl.h"

#include "mojo/services/test_service/test_service_application.h"

namespace mojo {
namespace test {

TestServiceImpl::TestServiceImpl(TestServiceApplication* application)
    : application_(application) {
}

TestServiceImpl::~TestServiceImpl() {
}

void TestServiceImpl::OnConnectionEstablished() {
  application_->AddRef();
}

void TestServiceImpl::OnConnectionError() {
  application_->ReleaseRef();
}

void TestServiceImpl::Ping(const mojo::Callback<void()>& callback) {
  callback.Run();
}

}  // namespace test
}  // namespace mojo
