// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/ninja_build_writer.h"
#include "tools/gn/pool.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/target.h"
#include "tools/gn/test_with_scope.h"

TEST(NinjaBuildWriter, TwoTargets) {
  Scheduler scheduler;
  TestWithScope setup;
  Err err;

  Target target_foo(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target_foo.set_output_type(Target::ACTION);
  target_foo.action_values().set_script(SourceFile("//foo/script.py"));
  target_foo.action_values().outputs() = SubstitutionList::MakeForTest(
      "//out/Debug/out1.out", "//out/Debug/out2.out");
  target_foo.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target_foo.OnResolved(&err));

  Target target_bar(setup.settings(), Label(SourceDir("//bar/"), "bar"));
  target_bar.set_output_type(Target::ACTION);
  target_bar.action_values().set_script(SourceFile("//bar/script.py"));
  target_bar.action_values().outputs() = SubstitutionList::MakeForTest(
      "//out/Debug/out3.out", "//out/Debug/out4.out");
  target_bar.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target_bar.OnResolved(&err));

  Pool swiming_pool(setup.settings(),
                    Label(SourceDir("//swiming/"), "pool",
                          SourceDir("//other/"), "toolchain"));
  swiming_pool.set_depth(42);

  std::ostringstream ninja_out;
  std::ostringstream depfile_out;
  std::vector<const Settings*> all_settings = {setup.settings()};
  std::vector<const Target*> targets = {&target_foo, &target_bar};
  std::vector<const Pool*> all_pools = {&swiming_pool};
  NinjaBuildWriter writer(setup.build_settings(), all_settings,
                          setup.toolchain(), targets, all_pools, ninja_out,
                          depfile_out);
  ASSERT_TRUE(writer.Run(&err));

  const char expected_rule_gn[] = "rule gn\n";
  const char expected_build_ninja[] =
      "build build.ninja: gn\n"
      "  generator = 1\n"
      "  depfile = build.ninja.d\n"
      "\n";
  const char expected_link_pool[] =
      "pool link_pool\n"
      "  depth = 0\n"
      "\n"
      "pool other_toolchain_swiming_pool\n"
      "  depth = 42\n"
      "\n";
  const char expected_toolchain[] =
      "subninja toolchain.ninja\n"
      "\n";
  const char expected_targets[] =
      "build bar: phony obj/bar/bar.stamp\n"
      "build foo$:bar: phony obj/foo/bar.stamp\n"
      "build bar$:bar: phony obj/bar/bar.stamp\n"
      "\n";
  const char expected_root_target[] =
      "build all: phony $\n"
      "    obj/foo/bar.stamp $\n"
      "    obj/bar/bar.stamp\n"
      "\n"
      "default all\n";
  std::string out_str = ninja_out.str();
#define EXPECT_SNIPPET(expected) \
    EXPECT_NE(std::string::npos, out_str.find(expected)) << \
        "Expected to find: " << expected << std::endl << \
        "Within: " << out_str
  EXPECT_SNIPPET(expected_rule_gn);
  EXPECT_SNIPPET(expected_build_ninja);
  EXPECT_SNIPPET(expected_link_pool);
  EXPECT_SNIPPET(expected_toolchain);
  EXPECT_SNIPPET(expected_targets);
  EXPECT_SNIPPET(expected_root_target);
#undef EXPECT_SNIPPET
}

TEST(NinjaBuildWriter, DuplicateOutputs) {
  Scheduler scheduler;
  TestWithScope setup;
  Err err;

  Target target_foo(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target_foo.set_output_type(Target::ACTION);
  target_foo.action_values().set_script(SourceFile("//foo/script.py"));
  target_foo.action_values().outputs() = SubstitutionList::MakeForTest(
      "//out/Debug/out1.out", "//out/Debug/out2.out");
  target_foo.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target_foo.OnResolved(&err));

  Target target_bar(setup.settings(), Label(SourceDir("//bar/"), "bar"));
  target_bar.set_output_type(Target::ACTION);
  target_bar.action_values().set_script(SourceFile("//bar/script.py"));
  target_bar.action_values().outputs() = SubstitutionList::MakeForTest(
      "//out/Debug/out3.out", "//out/Debug/out2.out");
  target_bar.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target_bar.OnResolved(&err));

  std::ostringstream ninja_out;
  std::ostringstream depfile_out;
  std::vector<const Settings*> all_settings = { setup.settings() };
  std::vector<const Target*> targets = { &target_foo, &target_bar };
  std::vector<const Pool*> all_pools;
  NinjaBuildWriter writer(setup.build_settings(), all_settings,
                          setup.toolchain(), targets, all_pools, ninja_out,
                          depfile_out);
  ASSERT_FALSE(writer.Run(&err));

  const char expected_help_test[] =
      "Two or more targets generate the same output:\n"
      "  out2.out\n"
      "\n"
      "This is can often be fixed by changing one of the target names, or by \n"
      "setting an output_name on one of them.\n"
      "\n"
      "Collisions:\n"
      "  //foo:bar\n"
      "  //bar:bar\n";

  EXPECT_EQ(expected_help_test, err.help_text());
}
