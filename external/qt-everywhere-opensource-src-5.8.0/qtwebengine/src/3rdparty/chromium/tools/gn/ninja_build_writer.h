// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_NINJA_BUILD_WRITER_H_
#define TOOLS_GN_NINJA_BUILD_WRITER_H_

#include <iosfwd>
#include <set>
#include <vector>

#include "base/macros.h"
#include "tools/gn/path_output.h"

class BuildSettings;
class Err;
class Pool;
class Settings;
class Target;
class Toolchain;

// Generates the toplevel "build.ninja" file. This references the individual
// toolchain files and lists all input .gn files as dependencies of the
// build itself.
class NinjaBuildWriter {
 public:
  static bool RunAndWriteFile(
      const BuildSettings* settings,
      const std::vector<const Settings*>& all_settings,
      const Toolchain* default_toolchain,
      const std::vector<const Target*>& default_toolchain_targets,
      const std::vector<const Pool*>& all_pools,
      Err* err);

  NinjaBuildWriter(const BuildSettings* settings,
                   const std::vector<const Settings*>& all_settings,
                   const Toolchain* default_toolchain,
                   const std::vector<const Target*>& default_toolchain_targets,
                   const std::vector<const Pool*>& all_pools,
                   std::ostream& out,
                   std::ostream& dep_out);
  ~NinjaBuildWriter();

  bool Run(Err* err);

 private:
  void WriteNinjaRules();
  void WriteAllPools();
  void WriteSubninjas();
  bool WritePhonyAndAllRules(Err* err);

  void WritePhonyRule(const Target* target, const std::string& phony_name);

  const BuildSettings* build_settings_;
  std::vector<const Settings*> all_settings_;
  const Toolchain* default_toolchain_;
  std::vector<const Target*> default_toolchain_targets_;
  std::vector<const Pool*> all_pools_;
  std::ostream& out_;
  std::ostream& dep_out_;
  PathOutput path_output_;

  DISALLOW_COPY_AND_ASSIGN(NinjaBuildWriter);
};

#endif  // TOOLS_GN_NINJA_BUILD_WRITER_H_

