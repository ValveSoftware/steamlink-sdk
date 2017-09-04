/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/qmake_link_writer.h"
#include "tools/gn/deps_iterator.h"
#include "tools/gn/ninja_binary_target_writer.h"
#include "tools/gn/output_file.h"
#include "tools/gn/settings.h"
#include "tools/gn/target.h"
#include "tools/gn/config_values_extractors.h"
#include "base/logging.h"
#include "base/strings/string_util.h"

QMakeLinkWriter::QMakeLinkWriter(const NinjaBinaryTargetWriter* writer, const Target* target, std::ostream& out)
    : target_(target),
      nwriter_(writer),
      out_(out),
      path_output_(target->settings()->build_settings()->build_dir(),
                   target->settings()->build_settings()->root_path_utf8(),
                   ESCAPE_NONE) {
}

QMakeLinkWriter::~QMakeLinkWriter() {
}

// Based on similar function in qt_creator_writer.cc
void CollectDeps(std::set<const Target*> &deps, const Target* target) {
  for (const auto& dep : target->GetDeps(Target::DEPS_ALL)) {
    const Target* dep_target = dep.ptr;
    if (deps.count(dep_target))
      continue;
    deps.insert(dep_target);
    CollectDeps(deps, dep_target);
  }
}

void PrintSourceFile(std::ostream& out, PathOutput& path_output, const SourceFile& file) {
  out << " \\\n    \"";
  if (file.is_source_absolute()) {
    out << "$$PWD/";
    path_output.WriteFile(out, file);
  } else {
    out << file.value();
  }
  out << "\"";
}

void QMakeLinkWriter::Run() {

  CHECK(target_->output_type() == Target::SHARED_LIBRARY)
         << "QMakeLinkWriter only supports SHARED_LIBRARY";

  std::vector<SourceFile> object_files;
  std::vector<SourceFile> other_files;
  std::vector<OutputFile> tool_outputs;

  const Settings* settings = target_->settings();
  object_files.reserve(target_->sources().size());

  for (const auto& source : target_->sources()) {
      Toolchain::ToolType tool_type = Toolchain::TYPE_NONE;
      if (!target_->GetOutputFilesForSource(source, &tool_type, &tool_outputs)) {
        if (GetSourceFileType(source) == SOURCE_DEF)
          other_files.push_back(source);
        continue;  // No output for this source.
      }
      object_files.push_back(tool_outputs[0].AsSourceFile(settings->build_settings()));
  }

  UniqueVector<OutputFile> extra_object_files;
  UniqueVector<const Target*> linkable_deps;
  UniqueVector<const Target*> non_linkable_deps;
  nwriter_->GetDeps(&extra_object_files, &linkable_deps, &non_linkable_deps);

  std::set<const Target*> deps;
  deps.insert(target_);
  CollectDeps(deps, target_);

  // sources files.
  out_ << "NINJA_SOURCES =";
  for (const auto& target : deps) {
    for (const auto& file : target->sources()) {
      PrintSourceFile(out_, path_output_, file);
    }
  }
  out_ << std::endl;

  // headers files.
  out_ << "NINJA_HEADERS =";
  for (const auto& target : deps) {
    for (const auto& file : target->public_headers()) {
      PrintSourceFile(out_, path_output_, file);
    }
  }
  out_ << std::endl;

  std::set<std::string> defines;
  for (const auto& target : deps) {
    for (ConfigValuesIterator it(target); !it.done(); it.Next()) {
      for (std::string define : it.cur().defines()) {
        defines.insert(define);
      }
    }
  }
  out_ << "NINJA_DEFINES =";
  for (const auto& define : defines) {
    out_ << " \\\n    " << define;
  }
  out_ << std::endl;

  // object files.
  out_ << "NINJA_OBJECTS =";
  for (const auto& file : object_files) {
    out_ << " \\\n    \"$$PWD/";
    path_output_.WriteFile(out_, file);
    out_ << "\"";
  }
  for (const auto& file : extra_object_files) {
    out_ << " \\\n    \"$$PWD/";
    path_output_.WriteFile(out_, file);
    out_ << "\"";
  }
  out_ << std::endl;

  // linker flags
  out_ << "NINJA_LFLAGS =";
  EscapeOptions opts;
  opts.mode = ESCAPE_COMMAND;
  // First the ldflags from the target and its config.
  RecursiveTargetConfigStringsToStream(target_, &ConfigValues::ldflags,
                                       opts, out_);
  out_ << std::endl;

  // archives
  out_ << "NINJA_ARCHIVES =";

  std::vector<OutputFile> solibs;
  for (const Target* cur : linkable_deps) {
    if (cur->dependency_output_file().value() !=
        cur->link_output_file().value()) {
        solibs.push_back(cur->link_output_file());
    } else {
      out_ << " \\\n    \"$$PWD/";
      path_output_.WriteFile(out_, cur->link_output_file());
      out_ << "\"";
    }
  }
  out_ << std::endl;

  // library dirs
  const OrderedSet<SourceDir> all_lib_dirs = target_->all_lib_dirs();
  const Tool* tool = target_->toolchain()->GetToolForTargetFinalOutput(target_);

  if (!all_lib_dirs.empty()) {
    out_ << "NINJA_LIB_DIRS =";
    PathOutput lib_path_output(path_output_.current_dir(),
                               settings->build_settings()->root_path_utf8(),
                               ESCAPE_COMMAND);
    for (size_t i = 0; i < all_lib_dirs.size(); i++) {
      out_ << " " << tool->lib_dir_switch();
      lib_path_output.WriteDir(out_, all_lib_dirs[i],
                               PathOutput::DIR_NO_LAST_SLASH);
    }
  }
  out_ << std::endl;

  //libs
  out_ << "NINJA_LIBS =";

  EscapeOptions lib_escape_opts;
  lib_escape_opts.mode = ESCAPE_COMMAND;

  const OrderedSet<LibFile> all_libs = target_->all_libs();
  const std::string framework_ending(".framework");
  for (size_t i = 0; i < all_libs.size(); i++) {
    const LibFile& lib_file = all_libs[i];
    const std::string& lib_value = lib_file.value();
    if (lib_file.is_source_file()) {
      out_ << " ";
      PathOutput lib_path_output(settings->build_settings()->build_dir(),
                                 settings->build_settings()->root_path_utf8(),
                                 ESCAPE_COMMAND);
      lib_path_output.WriteFile(out_, lib_file.source_file());
    } else if (base::EndsWith(lib_value, framework_ending,
                              base::CompareCase::INSENSITIVE_ASCII)) {
      out_ << " -framework ";
      EscapeStringToStream(
          out_, lib_value.substr(0, lib_value.size() - framework_ending.size()),
          lib_escape_opts);
    } else {
      out_ << " " << tool->lib_switch();
      EscapeStringToStream(out_, lib_value, lib_escape_opts);
    }
  }
  out_ << std::endl;

  // solibs
  if (!solibs.empty()) {
    out_ << "NINJA_SOLIBS =";
    for (const auto& file : solibs) {
      out_ << " \"$$PWD/";
      path_output_.WriteFile(out_, file);
      out_ << "\"";
    }
    out_ << std::endl;
  }

  //targetdeps
  out_ << "NINJA_TARGETDEPS = ";
  path_output_.WriteFile(out_, OutputFile("\"$$PWD/" + target_->label().name() + ".stamp\""));
  out_ << std::endl;
}
