// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/service_test.h"

namespace video_capture {

ServiceTest::ServiceTest()
    : service_manager::test::ServiceTest("video_capture_unittests") {}

ServiceTest::~ServiceTest() = default;

void ServiceTest::SetUp() {
  service_manager::test::ServiceTest::SetUp();
  connector()->ConnectToInterface("video_capture", &service_);
  service_->ConnectToFakeDeviceFactory(mojo::GetProxy(&factory_));
}

}  // namespace video_capture
