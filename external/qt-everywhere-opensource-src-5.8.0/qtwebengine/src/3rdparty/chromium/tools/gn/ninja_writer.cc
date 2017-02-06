// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_writer.h"

#include "tools/gn/builder.h"
#include "tools/gn/loader.h"
#include "tools/gn/location.h"
#include "tools/gn/ninja_build_writer.h"
#include "tools/gn/ninja_toolchain_writer.h"
#include "tools/gn/settings.h"
#include "tools/gn/target.h"

NinjaWriter::NinjaWriter(const BuildSettings* build_settings,
                         Builder* builder)
    : build_settings_(build_settings),
      builder_(builder) {
}

NinjaWriter::~NinjaWriter() {
}

// static
bool NinjaWriter::RunAndWriteFiles(const BuildSettings* build_settings,
                                   Builder* builder,
                                   Err* err) {
  NinjaWriter writer(build_settings, builder);

  std::vector<const Settings*> all_settings;
  std::vector<const Target*> default_targets;
  std::vector<const Pool*> all_pools;
  if (!writer.WriteToolchains(&all_settings, &default_targets, &all_pools, err))
    return false;
  return writer.WriteRootBuildfiles(all_settings, default_targets, all_pools,
                                    err);
}

// static
bool NinjaWriter::RunAndWriteToolchainFiles(
    const BuildSettings* build_settings,
    Builder* builder,
    std::vector<const Settings*>* all_settings,
    Err* err) {
  NinjaWriter writer(build_settings, builder);
  std::vector<const Target*> default_targets;
  std::vector<const Pool*> all_pools;
  return writer.WriteToolchains(all_settings, &default_targets, &all_pools,
                                err);
}

bool NinjaWriter::WriteToolchains(std::vector<const Settings*>* all_settings,
                                  std::vector<const Target*>* default_targets,
                                  std::vector<const Pool*>* all_pools,
                                  Err* err) {
  // Categorize all targets by toolchain.
  typedef std::map<Label, std::vector<const Target*> > CategorizedMap;
  CategorizedMap categorized;

  std::vector<const BuilderRecord*> all_records = builder_->GetAllRecords();
  for (const auto& all_record : all_records) {
    if (all_record->type() == BuilderRecord::ITEM_TARGET &&
        all_record->should_generate()) {
      categorized[all_record->label().GetToolchainLabel()].push_back(
          all_record->item()->AsTarget());
      }
  }
  if (categorized.empty()) {
    Err(Location(), "No targets.",
        "I could not find any targets to write, so I'm doing nothing.")
        .PrintToStdout();
    return false;
  }

  for (auto& i : categorized) {
    // Sort targets so that they are in a deterministic order.
    std::sort(i.second.begin(), i.second.end(),
              [](const Target* a, const Target* b) {
                return a->label() < b->label();
              });
  }

  Label default_label = builder_->loader()->GetDefaultToolchain();

  // Write out the toolchain buildfiles, and also accumulate the set of
  // all settings and find the list of targets in the default toolchain.
  UniqueVector<const Pool*> pools;
  for (const auto& i : categorized) {
    const Settings* settings =
        builder_->loader()->GetToolchainSettings(i.first);
    const Toolchain* toolchain = builder_->GetToolchain(i.first);

    all_settings->push_back(settings);

    if (!NinjaToolchainWriter::RunAndWriteFile(settings, toolchain, i.second)) {
      Err(Location(),
          "Couldn't open toolchain buildfile(s) for writing").PrintToStdout();
      return false;
    }

    for (int j = Toolchain::TYPE_NONE + 1; j < Toolchain::TYPE_NUMTYPES; j++) {
      Toolchain::ToolType tool_type = static_cast<Toolchain::ToolType>(j);
      const Tool* tool = toolchain->GetTool(tool_type);
      if (tool && tool->pool().ptr)
        pools.push_back(tool->pool().ptr);
    }
  }

  *all_pools = pools.vector();
  *default_targets = categorized[default_label];
  return true;
}

bool NinjaWriter::WriteRootBuildfiles(
    const std::vector<const Settings*>& all_settings,
    const std::vector<const Target*>& default_targets,
    const std::vector<const Pool*>& all_pools,
    Err* err) {
  // All Settings objects should have the same default toolchain, and there
  // should always be at least one settings object in the build.
  CHECK(!all_settings.empty());
  const Toolchain* default_toolchain =
      builder_->GetToolchain(all_settings[0]->default_toolchain_label());

  // Write the root buildfile.
  return NinjaBuildWriter::RunAndWriteFile(build_settings_, all_settings,
                                           default_toolchain, default_targets,
                                           all_pools, err);
}
