// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/message_pump_mojo.h"

#include "base/message_loop/message_loop_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace common {
namespace test {

scoped_ptr<base::MessagePump> CreateMojoMessagePump() {
  return scoped_ptr<base::MessagePump>(new MessagePumpMojo());
}

RUN_MESSAGE_LOOP_TESTS(Mojo, &CreateMojoMessagePump);

}  // namespace test
}  // namespace common
}  // namespace mojo
