// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/app/run_as_crashpad_handler_win.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/crashpad/crashpad/handler/handler_main.h"

namespace crash_reporter {

int RunAsCrashpadHandler(const base::CommandLine& command_line) {
  std::vector<base::string16> argv = command_line.argv();
  const base::string16 process_type = L"--type=";
  argv.erase(std::remove_if(argv.begin(), argv.end(),
                            [&process_type](const base::string16& str) {
                              return base::StartsWith(
                                         str, process_type,
                                         base::CompareCase::SENSITIVE) ||
                                     (!str.empty() && str[0] == L'/');
                            }),
             argv.end());

  std::unique_ptr<char* []> argv_as_utf8(new char*[argv.size() + 1]);
  std::vector<std::string> storage;
  storage.reserve(argv.size());
  for (size_t i = 0; i < argv.size(); ++i) {
    storage.push_back(base::UTF16ToUTF8(argv[i]));
    argv_as_utf8[i] = &storage[i][0];
  }
  argv_as_utf8[argv.size()] = nullptr;
  argv.clear();
  return crashpad::HandlerMain(static_cast<int>(storage.size()),
                               argv_as_utf8.get());
}

}  // namespace crash_reporter
