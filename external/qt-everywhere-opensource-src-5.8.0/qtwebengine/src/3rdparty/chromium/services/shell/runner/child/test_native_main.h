// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_RUNNER_CHILD_TEST_NATIVE_MAIN_H_
#define SERVICES_SHELL_RUNNER_CHILD_TEST_NATIVE_MAIN_H_

namespace shell {

class ShellClient;

int TestNativeMain(ShellClient* shell_client);

}  // namespace shell

#endif  // SERVICES_SHELL_RUNNER_CHILD_TEST_NATIVE_MAIN_H_
