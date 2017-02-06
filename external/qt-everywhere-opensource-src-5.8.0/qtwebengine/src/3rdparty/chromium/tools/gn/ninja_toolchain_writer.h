// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_NINJA_TOOLCHAIN_WRITER_H_
#define TOOLS_GN_NINJA_TOOLCHAIN_WRITER_H_

#include <iosfwd>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "tools/gn/path_output.h"
#include "tools/gn/toolchain.h"

class BuildSettings;
struct EscapeOptions;
class Settings;
class Target;
class Tool;

class NinjaToolchainWriter {
 public:
  // Takes the settings for the toolchain, as well as the list of all targets
  // associated with the toolchain.
  static bool RunAndWriteFile(const Settings* settings,
                              const Toolchain* toolchain,
                              const std::vector<const Target*>& targets);

 private:
  FRIEND_TEST_ALL_PREFIXES(NinjaToolchainWriter, WriteToolRule);

  NinjaToolchainWriter(const Settings* settings,
                       const Toolchain* toolchain,
                       const std::vector<const Target*>& targets,
                       std::ostream& out);
  ~NinjaToolchainWriter();

  void Run();

  void WriteRules();
  void WriteToolRule(Toolchain::ToolType type,
                     const Tool* tool,
                     const std::string& rule_prefix);
  void WriteRulePattern(const char* name,
                        const SubstitutionPattern& pattern,
                        const EscapeOptions& options);
  void WriteSubninjas();

  const Settings* settings_;
  const Toolchain* toolchain_;
  std::vector<const Target*> targets_;
  std::ostream& out_;
  PathOutput path_output_;

  DISALLOW_COPY_AND_ASSIGN(NinjaToolchainWriter);
};

#endif  // TOOLS_GN_NINJA_TOOLCHAIN_WRITER_H_
