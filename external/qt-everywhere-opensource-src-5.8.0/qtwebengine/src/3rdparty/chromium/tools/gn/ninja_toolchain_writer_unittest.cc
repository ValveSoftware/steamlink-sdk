// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/ninja_toolchain_writer.h"
#include "tools/gn/test_with_scope.h"

TEST(NinjaToolchainWriter, WriteToolRule) {
  TestWithScope setup;

  //Target target(setup.settings(), Label(SourceDir("//foo/"), "target"));
  //target.set_output_type(Target::EXECUTABLE);
  //target.SetToolchain(setup.toolchain());

  std::ostringstream stream;

  NinjaToolchainWriter writer(setup.settings(), setup.toolchain(),
                              std::vector<const Target*>(), stream);
  writer.WriteToolRule(Toolchain::TYPE_CC,
                       setup.toolchain()->GetTool(Toolchain::TYPE_CC),
                       std::string("prefix_"));

  EXPECT_EQ(
      "rule prefix_cc\n"
      "  command = cc ${in} ${cflags} ${cflags_c} ${defines} ${include_dirs} "
          "-o ${out}\n",
      stream.str());
}
