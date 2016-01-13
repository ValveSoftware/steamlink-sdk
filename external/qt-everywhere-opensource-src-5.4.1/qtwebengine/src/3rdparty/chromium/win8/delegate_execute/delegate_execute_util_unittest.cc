// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "win8/delegate_execute/delegate_execute_util.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

static const char kSomeSwitch[] = "some-switch";

}  // namespace

TEST(DelegateExecuteUtil, CommandLineFromParametersTest) {
  CommandLine cl(CommandLine::NO_PROGRAM);

  // Empty parameters means empty command-line string.
  cl = delegate_execute::CommandLineFromParameters(NULL);
  EXPECT_EQ(std::wstring(), cl.GetProgram().value());
  EXPECT_EQ(CommandLine::StringType(), cl.GetCommandLineString());

  // Parameters with a switch are parsed properly.
  cl = delegate_execute::CommandLineFromParameters(
      base::StringPrintf(L"--%ls",
                         base::ASCIIToWide(kSomeSwitch).c_str()).c_str());
  EXPECT_EQ(std::wstring(), cl.GetProgram().value());
  EXPECT_TRUE(cl.HasSwitch(kSomeSwitch));
}

TEST(DelegateExecuteUtil, MakeChromeCommandLineTest) {
  static const wchar_t kSomeArgument[] = L"http://some.url/";
  static const wchar_t kOtherArgument[] = L"http://some.other.url/";
  const base::FilePath this_exe(CommandLine::ForCurrentProcess()->GetProgram());

  CommandLine cl(CommandLine::NO_PROGRAM);

  // Empty params and argument contains only the exe.
  cl = delegate_execute::MakeChromeCommandLine(
      this_exe,
      delegate_execute::CommandLineFromParameters(NULL),
      base::string16());
  EXPECT_EQ(1, cl.argv().size());
  EXPECT_EQ(this_exe.value(), cl.GetProgram().value());

  // Empty params with arg contains the arg.
  cl = delegate_execute::MakeChromeCommandLine(
      this_exe, delegate_execute::CommandLineFromParameters(NULL),
      base::string16(kSomeArgument));
  EXPECT_EQ(2, cl.argv().size());
  EXPECT_EQ(this_exe.value(), cl.GetProgram().value());
  EXPECT_EQ(1, cl.GetArgs().size());
  EXPECT_EQ(base::string16(kSomeArgument), cl.GetArgs()[0]);

  // Params with switchs and args plus arg contains the arg.
  cl = delegate_execute::MakeChromeCommandLine(
      this_exe, delegate_execute::CommandLineFromParameters(
          base::StringPrintf(L"--%ls -- %ls",
                             base::ASCIIToWide(kSomeSwitch).c_str(),
                             kOtherArgument).c_str()),
      base::string16(kSomeArgument));
  EXPECT_EQ(5, cl.argv().size());
  EXPECT_EQ(this_exe.value(), cl.GetProgram().value());
  EXPECT_TRUE(cl.HasSwitch(kSomeSwitch));
  CommandLine::StringVector args(cl.GetArgs());
  EXPECT_EQ(2, args.size());
  EXPECT_NE(
      args.end(),
      std::find(args.begin(), args.end(), base::string16(kOtherArgument)));
  EXPECT_NE(args.end(),
            std::find(args.begin(), args.end(), base::string16(kSomeArgument)));
}

TEST(DelegateExecuteUtil, ParametersFromSwitchTest) {
  EXPECT_EQ(base::string16(), delegate_execute::ParametersFromSwitch(NULL));
  EXPECT_EQ(base::string16(L"--some-switch"),
            delegate_execute::ParametersFromSwitch(kSomeSwitch));
}
