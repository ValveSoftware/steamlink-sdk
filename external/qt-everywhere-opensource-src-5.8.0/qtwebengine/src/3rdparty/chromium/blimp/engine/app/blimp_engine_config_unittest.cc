// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/app/blimp_engine_config.h"

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/stringprintf.h"
#include "blimp/common/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace engine {
namespace {

// Reference client token.
static const char kTestClientToken[] = "Reference client token";

class BlimpEngineConfigTest : public testing::Test {
 protected:
  void SetUp() override {
    // Create a temporary directory and populate it with all of the switches
    // If a test requires a switch's file to be missing, call
    // RemoveFileForSwitch().
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    CreateFileForSwitch(kClientTokenPath, kTestClientToken);
  }

  // Creates a file in the temp directory for a given filepath switch.
  void CreateFileForSwitch(const std::string& filepath_switch,
                           const std::string& data) const {
    ASSERT_TRUE(base::WriteFile(GetFilepathForSwitch(filepath_switch),
                                data.c_str(), data.size()));
  }

  // Removes the associated file for a given filepath switch.
  void RemoveFileForSwitch(const std::string& filepath_switch) const {
    base::DeleteFile(GetFilepathForSwitch(filepath_switch), false);
  }

  // Creates and returns a CommandLine object with specified filepath switches.
  base::CommandLine CreateCommandLine(
      const std::vector<std::string>& filepath_switches) {
    base::CommandLine::StringVector cmd_vec = {"dummy_program"};
    for (const std::string& filepath_switch : filepath_switches) {
      cmd_vec.push_back(base::StringPrintf(
          "--%s=%s", filepath_switch.c_str(),
          GetFilepathForSwitch(filepath_switch).AsUTF8Unsafe().c_str()));
    }
    return base::CommandLine(cmd_vec);
  }

  base::FilePath GetFilepathForSwitch(
      const std::string& filepath_switch) const {
    return temp_dir_.path().Append(filepath_switch);
  }

  const std::vector<std::string> all_filepath_switches_ = {kClientTokenPath};

  base::ScopedTempDir temp_dir_;
};

TEST_F(BlimpEngineConfigTest, ClientTokenCorrect) {
  auto cmd_line = CreateCommandLine(all_filepath_switches_);
  auto engine_config = BlimpEngineConfig::Create(cmd_line);
  EXPECT_NE(nullptr, engine_config);
  EXPECT_EQ(kTestClientToken, engine_config->client_token());
}

TEST_F(BlimpEngineConfigTest, ClientTokenEmpty) {
  RemoveFileForSwitch(kClientTokenPath);
  CreateFileForSwitch(kClientTokenPath, " ");
  auto cmd_line = CreateCommandLine(all_filepath_switches_);
  auto engine_config = BlimpEngineConfig::Create(cmd_line);
  EXPECT_EQ(nullptr, engine_config);
}

}  // namespace
}  // namespace engine
}  // namespace blimp
