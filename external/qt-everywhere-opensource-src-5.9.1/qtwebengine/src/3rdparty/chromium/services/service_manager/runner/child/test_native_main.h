// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_RUNNER_CHILD_TEST_NATIVE_MAIN_H_
#define SERVICES_SERVICE_MANAGER_RUNNER_CHILD_TEST_NATIVE_MAIN_H_

#include <memory>

namespace service_manager {

class Service;

int TestNativeMain(std::unique_ptr<Service> service);

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_RUNNER_CHILD_TEST_NATIVE_MAIN_H_
