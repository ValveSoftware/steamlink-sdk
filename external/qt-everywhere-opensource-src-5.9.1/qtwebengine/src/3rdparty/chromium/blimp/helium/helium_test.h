// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_HELIUM_HELIUM_TEST_H_
#define BLIMP_HELIUM_HELIUM_TEST_H_

#include "base/at_exit.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace helium {

class HeliumTest : public ::testing::Test {
 public:
  HeliumTest() {}
  ~HeliumTest() override {}

 private:
  base::ShadowingAtExitManager at_exit_manager_;

  DISALLOW_COPY_AND_ASSIGN(HeliumTest);
};

}  // namespace helium
}  // namespace blimp

#endif  // BLIMP_HELIUM_HELIUM_TEST_H_
