// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_NINJA_WRITER_H_
#define TOOLS_GN_NINJA_WRITER_H_

#include <set>
#include <string>
#include <vector>

#include "base/macros.h"

class Builder;
class BuildSettings;
class Err;
class Pool;
class Settings;
class Target;

class NinjaWriter {
 public:
  // On failure will populate |err| and will return false.
  static bool RunAndWriteFiles(const BuildSettings* build_settings,
                               Builder* builder,
                               Err* err);

  // Writes only the toolchain.ninja files, skipping the root buildfile. The
  // settings for the files written will be added to the vector.
  static bool RunAndWriteToolchainFiles(
      const BuildSettings* build_settings,
      Builder* builder,
      std::vector<const Settings*>* all_settings,
      Err* err);

 private:
  NinjaWriter(const BuildSettings* build_settings, Builder* builder);
  ~NinjaWriter();

  bool WriteToolchains(std::vector<const Settings*>* all_settings,
                       std::vector<const Target*>* default_targets,
                       std::vector<const Pool*>* all_pools,
                       Err* err);
  bool WriteRootBuildfiles(const std::vector<const Settings*>& all_settings,
                           const std::vector<const Target*>& default_targets,
                           const std::vector<const Pool*>& all_pools,
                           Err* err);

  const BuildSettings* build_settings_;
  Builder* builder_;

  DISALLOW_COPY_AND_ASSIGN(NinjaWriter);
};

#endif  // TOOLS_GN_NINJA_WRITER_H_
