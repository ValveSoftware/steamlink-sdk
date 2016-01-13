// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: this test tests LOG_V and LOG_E since all other logs are expressed
// in forms of them. LOG is also tested for good measure.
// Also note that we are only allowed to call InitLogging() twice so the test
// cases are more dense than normal.

// The following include must be first in this file. It ensures that
// libjingle style logging is used.
#define LOGGING_INSIDE_LIBJINGLE

#include "third_party/libjingle/overrides/talk/base/logging.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
static const wchar_t* const log_file_name = L"libjingle_logging.log";
#else
static const char* const log_file_name = "libjingle_logging.log";
#endif

static const int kDefaultVerbosity = 0;

static const char* AsString(talk_base::LoggingSeverity severity) {
  switch (severity) {
    case talk_base::LS_ERROR:
      return "LS_ERROR";
    case talk_base::LS_WARNING:
      return "LS_WARNING";
    case talk_base::LS_INFO:
      return "LS_INFO";
    case talk_base::LS_VERBOSE:
      return "LS_VERBOSE";
    case talk_base::LS_SENSITIVE:
      return "LS_SENSITIVE";
    default:
      return "";
  }
}

static bool ContainsString(const std::string& original,
                           const char* string_to_match) {
  return original.find(string_to_match) != std::string::npos;
}

static bool Initialize(int verbosity_level) {
  if (verbosity_level != kDefaultVerbosity) {
    // Update the command line with specified verbosity level for this file.
    CommandLine* command_line = CommandLine::ForCurrentProcess();
    std::ostringstream value_stream;
    value_stream << "logging_unittest=" << verbosity_level;
    const std::string& value = value_stream.str();
    command_line->AppendSwitchASCII("vmodule", value);
  }

  // The command line flags are parsed here and the log file name is set.
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_FILE;
  settings.log_file = log_file_name;
  settings.lock_log = logging::DONT_LOCK_LOG_FILE;
  settings.delete_old = logging::DELETE_OLD_LOG_FILE;
  if (!logging::InitLogging(settings)) {
    return false;
  }
  EXPECT_TRUE(VLOG_IS_ON(verbosity_level));
  EXPECT_FALSE(VLOG_IS_ON(verbosity_level + 1));
  return true;
}

TEST(LibjingleLogTest, DefaultConfiguration) {
  ASSERT_TRUE(Initialize(kDefaultVerbosity));

  // In the default configuration nothing should be logged.
  LOG_V(talk_base::LS_ERROR) << AsString(talk_base::LS_ERROR);
  LOG_V(talk_base::LS_WARNING) << AsString(talk_base::LS_WARNING);
  LOG_V(talk_base::LS_INFO) << AsString(talk_base::LS_INFO);
  LOG_V(talk_base::LS_VERBOSE) << AsString(talk_base::LS_VERBOSE);
  LOG_V(talk_base::LS_SENSITIVE) << AsString(talk_base::LS_SENSITIVE);

  // Read file to string.
  base::FilePath file_path(log_file_name);
  std::string contents_of_file;
  base::ReadFileToString(file_path, &contents_of_file);

  // Make sure string contains the expected values.
  EXPECT_FALSE(ContainsString(contents_of_file, AsString(talk_base::LS_ERROR)));
  EXPECT_FALSE(ContainsString(contents_of_file,
                              AsString(talk_base::LS_WARNING)));
  EXPECT_FALSE(ContainsString(contents_of_file, AsString(talk_base::LS_INFO)));
  EXPECT_FALSE(ContainsString(contents_of_file,
                              AsString(talk_base::LS_VERBOSE)));
  EXPECT_FALSE(ContainsString(contents_of_file,
                              AsString(talk_base::LS_SENSITIVE)));
}

TEST(LibjingleLogTest, InfoConfiguration) {
  ASSERT_TRUE(Initialize(talk_base::LS_INFO));

  // In this configuration everything lower or equal to LS_INFO should be
  // logged.
  LOG_V(talk_base::LS_ERROR) << AsString(talk_base::LS_ERROR);
  LOG_V(talk_base::LS_WARNING) << AsString(talk_base::LS_WARNING);
  LOG_V(talk_base::LS_INFO) << AsString(talk_base::LS_INFO);
  LOG_V(talk_base::LS_VERBOSE) << AsString(talk_base::LS_VERBOSE);
  LOG_V(talk_base::LS_SENSITIVE) << AsString(talk_base::LS_SENSITIVE);

  // Read file to string.
  base::FilePath file_path(log_file_name);
  std::string contents_of_file;
  base::ReadFileToString(file_path, &contents_of_file);

  // Make sure string contains the expected values.
  EXPECT_TRUE(ContainsString(contents_of_file, AsString(talk_base::LS_ERROR)));
  EXPECT_TRUE(ContainsString(contents_of_file,
                             AsString(talk_base::LS_WARNING)));
  EXPECT_TRUE(ContainsString(contents_of_file, AsString(talk_base::LS_INFO)));
  EXPECT_FALSE(ContainsString(contents_of_file,
                              AsString(talk_base::LS_VERBOSE)));
  EXPECT_FALSE(ContainsString(contents_of_file,
                              AsString(talk_base::LS_SENSITIVE)));

  // Also check that the log is proper.
  EXPECT_TRUE(ContainsString(contents_of_file, "logging_unittest.cc"));
  EXPECT_FALSE(ContainsString(contents_of_file, "logging.h"));
  EXPECT_FALSE(ContainsString(contents_of_file, "logging.cc"));
}

TEST(LibjingleLogTest, LogEverythingConfiguration) {
  ASSERT_TRUE(Initialize(talk_base::LS_SENSITIVE));

  // In this configuration everything should be logged.
  LOG_V(talk_base::LS_ERROR) << AsString(talk_base::LS_ERROR);
  LOG_V(talk_base::LS_WARNING) << AsString(talk_base::LS_WARNING);
  LOG(LS_INFO) << AsString(talk_base::LS_INFO);
  static const int kFakeError = 1;
  LOG_E(LS_INFO, EN, kFakeError) << "LOG_E(" << AsString(talk_base::LS_INFO) <<
      ")";
  LOG_V(talk_base::LS_VERBOSE) << AsString(talk_base::LS_VERBOSE);
  LOG_V(talk_base::LS_SENSITIVE) << AsString(talk_base::LS_SENSITIVE);

  // Read file to string.
  base::FilePath file_path(log_file_name);
  std::string contents_of_file;
  base::ReadFileToString(file_path, &contents_of_file);

  // Make sure string contains the expected values.
  EXPECT_TRUE(ContainsString(contents_of_file, AsString(talk_base::LS_ERROR)));
  EXPECT_TRUE(ContainsString(contents_of_file,
                             AsString(talk_base::LS_WARNING)));
  EXPECT_TRUE(ContainsString(contents_of_file, AsString(talk_base::LS_INFO)));
  // LOG_E
  EXPECT_TRUE(ContainsString(contents_of_file, strerror(kFakeError)));
  EXPECT_TRUE(ContainsString(contents_of_file,
                             AsString(talk_base::LS_VERBOSE)));
  EXPECT_TRUE(ContainsString(contents_of_file,
                             AsString(talk_base::LS_SENSITIVE)));
}
