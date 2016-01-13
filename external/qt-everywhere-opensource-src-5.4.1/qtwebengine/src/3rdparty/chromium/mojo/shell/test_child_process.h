// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_TEST_CHILD_PROCESS_H_
#define MOJO_SHELL_TEST_CHILD_PROCESS_H_

#include "base/macros.h"
#include "mojo/shell/child_process.h"

namespace mojo {
namespace shell {

class TestChildProcess : public ChildProcess {
 public:
  TestChildProcess();
  virtual ~TestChildProcess();

  virtual void Main() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestChildProcess);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_TEST_CHILD_PROCESS_H_
