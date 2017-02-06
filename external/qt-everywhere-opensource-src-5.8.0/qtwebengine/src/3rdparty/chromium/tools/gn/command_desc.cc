// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <set>
#include <sstream>

#include "base/command_line.h"
#include "build/build_config.h"
#include "tools/gn/commands.h"
#include "tools/gn/config.h"
#include "tools/gn/config_values_extractors.h"
#include "tools/gn/deps_iterator.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/item.h"
#include "tools/gn/label.h"
#include "tools/gn/runtime_deps.h"
#include "tools/gn/setup.h"
#include "tools/gn/standard_out.h"
#include "tools/gn/substitution_writer.h"
#include "tools/gn/switches.h"
#include "tools/gn/target.h"
#include "tools/gn/variables.h"

namespace commands {

namespace {

// Desc-specific command line switches.
const char kBlame[] = "blame";
const char kTree[] = "tree";

// Prints the given directory in a nice way for the user to view.
std::string FormatSourceDir(const SourceDir& dir) {
#if defined(OS_WIN)
  // On Windows we fix up system absolute paths to look like native ones.
  // Internally, they'll look like "/C:\foo\bar/"
  if (dir.is_system_absolute()) {
    std::string buf = dir.value();
    if (buf.size() > 3 && buf[2] == ':') {
      buf.erase(buf.begin());  // Erase beginning slash.
      return buf;
    }
  }
#endif
  return dir.value();
}

void RecursiveCollectChildDeps(const Target* target,
                               std::set<const Target*>* result);

void RecursiveCollectDeps(const Target* target,
                          std::set<const Target*>* result) {
  if (result->find(target) != result->end())
    return;  // Already did this target.
  result->insert(target);

  RecursiveCollectChildDeps(target, result);
}

void RecursiveCollectChildDeps(const Target* target,
                               std::set<const Target*>* result) {
  for (const auto& pair : target->GetDeps(Target::DEPS_ALL))
    RecursiveCollectDeps(pair.ptr, result);
}

// Prints dependencies of the given target (not the target itself). If the
// set is non-null, new targets encountered will be added to the set, and if
// a dependency is in the set already, it will not be recused into. When the
// set is null, all dependencies will be printed.
void RecursivePrintDeps(const Target* target,
                        const Label& default_toolchain,
                        std::set<const Target*>* seen_targets,
                        int indent_level) {
  // Combine all deps into one sorted list.
  std::vector<LabelTargetPair> sorted_deps;
  for (const auto& pair : target->GetDeps(Target::DEPS_ALL))
    sorted_deps.push_back(pair);
  std::sort(sorted_deps.begin(), sorted_deps.end(),
            LabelPtrLabelLess<Target>());

  std::string indent(indent_level * 2, ' ');
  for (const auto& pair : sorted_deps) {
    const Target* cur_dep = pair.ptr;

    OutputString(indent +
        cur_dep->label().GetUserVisibleName(default_toolchain));
    bool print_children = true;
    if (seen_targets) {
      if (seen_targets->find(cur_dep) == seen_targets->end()) {
        // New target, mark it visited.
        seen_targets->insert(cur_dep);
      } else {
        // Already seen.
        print_children = false;
        // Only print "..." if something is actually elided, which means that
        // the current target has children.
        if (!cur_dep->public_deps().empty() ||
            !cur_dep->private_deps().empty() ||
            !cur_dep->data_deps().empty())
          OutputString("...");
      }
    }

    OutputString("\n");
    if (print_children) {
      RecursivePrintDeps(cur_dep, default_toolchain, seen_targets,
                         indent_level + 1);
    }
  }
}

void PrintDeps(const Target* target, bool display_header) {
  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  Label toolchain_label = target->label().GetToolchainLabel();

  // Tree mode is separate.
  if (cmdline->HasSwitch(kTree)) {
    if (display_header)
      OutputString("\nDependency tree\n");

    if (cmdline->HasSwitch("all")) {
      // Show all tree deps with no eliding.
      RecursivePrintDeps(target, toolchain_label, nullptr, 1);
    } else {
      // Don't recurse into duplicates.
      std::set<const Target*> seen_targets;
      RecursivePrintDeps(target, toolchain_label, &seen_targets, 1);
    }
    return;
  }

  // Collect the deps to display.
  if (cmdline->HasSwitch("all")) {
    // Show all dependencies.
    if (display_header)
      OutputString("\nAll recursive dependencies\n");

    std::set<const Target*> all_deps;
    RecursiveCollectChildDeps(target, &all_deps);
    FilterAndPrintTargetSet(display_header, all_deps);
  } else {
    std::vector<const Target*> deps;
    // Show direct dependencies only.
    if (display_header) {
      OutputString(
          "\nDirect dependencies "
          "(try also \"--all\", \"--tree\", or even \"--all --tree\")\n");
    }
    for (const auto& pair : target->GetDeps(Target::DEPS_ALL))
      deps.push_back(pair.ptr);
    std::sort(deps.begin(), deps.end());
    FilterAndPrintTargets(display_header, &deps);
  }
}

// libs and lib_dirs are special in that they're inherited. We don't currently
// implement a blame feature for this since the bottom-up inheritance makes
// this difficult.
void PrintLibDirs(const Target* target, bool display_header) {
  const OrderedSet<SourceDir>& lib_dirs = target->all_lib_dirs();
  if (lib_dirs.empty())
    return;

  if (display_header)
    OutputString("\nlib_dirs\n");

  for (size_t i = 0; i < lib_dirs.size(); i++)
    OutputString("    " + FormatSourceDir(lib_dirs[i]) + "\n");
}

void PrintLibs(const Target* target, bool display_header) {
  const OrderedSet<LibFile>& libs = target->all_libs();
  if (libs.empty())
    return;

  if (display_header)
    OutputString("\nlibs\n");

  for (size_t i = 0; i < libs.size(); i++)
    OutputString("    " + libs[i].value() + "\n");
}

void PrintPublic(const Target* target, bool display_header) {
  if (display_header)
    OutputString("\npublic\n");

  if (target->all_headers_public()) {
    OutputString("  [All headers listed in the sources are public.]\n");
    return;
  }

  Target::FileList public_headers = target->public_headers();
  std::sort(public_headers.begin(), public_headers.end());
  for (const auto& hdr : public_headers)
    OutputString("  " + hdr.value() + "\n");
}

void PrintCheckIncludes(const Target* target, bool display_header) {
  if (display_header)
    OutputString("\ncheck_includes\n");

  if (target->check_includes())
    OutputString("  true\n");
  else
    OutputString("  false\n");
}

void PrintAllowCircularIncludesFrom(const Target* target, bool display_header) {
  if (display_header)
    OutputString("\nallow_circular_includes_from\n");

  Label toolchain_label = target->label().GetToolchainLabel();
  for (const auto& cur : target->allow_circular_includes_from())
    OutputString("  " + cur.GetUserVisibleName(toolchain_label) + "\n");
}

void PrintVisibility(const Target* target, bool display_header) {
  if (display_header)
    OutputString("\nvisibility\n");

  OutputString(target->visibility().Describe(2, false));
}

void PrintTestonly(const Target* target, bool display_header) {
  if (display_header)
    OutputString("\ntestonly\n");

  if (target->testonly())
    OutputString("  true\n");
  else
    OutputString("  false\n");
}

// Recursively prints subconfigs of a config.
void PrintSubConfigs(const Config* config, int indent_level) {
  if (config->configs().empty())
    return;

  std::string indent(indent_level * 2, ' ');
  Label toolchain_label = config->label().GetToolchainLabel();
  for (const auto& pair : config->configs()) {
    OutputString(
        indent + pair.label.GetUserVisibleName(toolchain_label) + "\n");
    PrintSubConfigs(pair.ptr, indent_level + 1);
  }
}

// This allows configs stored as either std::vector<LabelConfigPair> or
// UniqueVector<LabelConfigPair> to be printed.
template <class VectorType>
void PrintConfigsVector(const Item* item,
                        const VectorType& configs,
                        const std::string& heading,
                        bool display_header) {
  if (configs.empty())
    return;

  bool tree = base::CommandLine::ForCurrentProcess()->HasSwitch(kTree);

  // Don't sort since the order determines how things are processed.
  if (display_header) {
    if (tree)
      OutputString("\n" + heading + " tree (in order applying)\n");
    else
      OutputString("\n" + heading + " (in order applying, try also --tree)\n");
  }

  Label toolchain_label = item->label().GetToolchainLabel();
  for (const auto& config : configs) {
    OutputString("  " + config.label.GetUserVisibleName(toolchain_label) +
                 "\n");
    if (tree)
      PrintSubConfigs(config.ptr, 2);  // 2 = start with double-indent.
  }
}

void PrintConfigs(const Target* target, bool display_header) {
  PrintConfigsVector(target, target->configs().vector(), "configs",
                     display_header);
}

void PrintConfigs(const Config* config, bool display_header) {
  PrintConfigsVector(config, config->configs().vector(), "configs",
                     display_header);
}

void PrintPublicConfigs(const Target* target, bool display_header) {
  PrintConfigsVector(target, target->public_configs(),
                     "public_configs", display_header);
}

void PrintAllDependentConfigs(const Target* target, bool display_header) {
  PrintConfigsVector(target, target->all_dependent_configs(),
                     "all_dependent_configs", display_header);
}

void PrintFileList(const Target::FileList& files,
                   const std::string& header,
                   bool indent_extra,
                   bool display_header) {
  if (files.empty())
    return;

  if (display_header)
    OutputString("\n" + header + "\n");

  std::string indent = indent_extra ? "    " : "  ";

  Target::FileList sorted = files;
  std::sort(sorted.begin(), sorted.end());
  for (const auto& elem : sorted)
    OutputString(indent + elem.value() + "\n");
}

void PrintSources(const Target* target, bool display_header) {
  PrintFileList(target->sources(), "sources", false, display_header);
}

void PrintInputs(const Target* target, bool display_header) {
  PrintFileList(target->inputs(), "inputs", false, display_header);
}

void PrintOutputs(const Target* target, bool display_header) {
  if (display_header)
    OutputString("\noutputs\n");

  if (target->output_type() == Target::ACTION) {
    // Action, print out outputs, don't apply sources to it.
    for (const auto& elem : target->action_values().outputs().list()) {
      OutputString("  " + elem.AsString() + "\n");
    }
  } else if (target->output_type() == Target::CREATE_BUNDLE) {
    std::vector<SourceFile> output_files;
    target->bundle_data().GetOutputsAsSourceFiles(target->settings(),
                                                  &output_files);
    PrintFileList(output_files, std::string(), true, false);
  } else {
    const SubstitutionList& outputs = target->action_values().outputs();
    if (!outputs.required_types().empty()) {
      // Display the pattern and resolved pattern separately, since there are
      // subtitutions used.
      OutputString("  Output pattern\n");
      for (const auto& elem : outputs.list())
        OutputString("    " + elem.AsString() + "\n");

      // Now display what that resolves to given the sources.
      OutputString("\n  Resolved output file list\n");
    }

    // Resolved output list.
    std::vector<SourceFile> output_files;
    SubstitutionWriter::ApplyListToSources(target->settings(), outputs,
                                           target->sources(), &output_files);
    PrintFileList(output_files, std::string(), true, false);
  }
}

void PrintScript(const Target* target, bool display_header) {
  if (display_header)
    OutputString("\nscript\n");
  OutputString("  " + target->action_values().script().value() + "\n");
}

void PrintArgs(const Target* target, bool display_header) {
  if (display_header)
    OutputString("\nargs\n");
  for (const auto& elem : target->action_values().args().list()) {
    OutputString("  " + elem.AsString() + "\n");
  }
}

void PrintDepfile(const Target* target, bool display_header) {
  if (target->action_values().depfile().empty())
    return;
  if (display_header)
    OutputString("\ndepfile\n");
  OutputString("  " + target->action_values().depfile().AsString() + "\n");
}

// Attribute the origin for attributing from where a target came from. Does
// nothing if the input is null or it does not have a location.
void OutputSourceOfDep(const ParseNode* origin, std::ostream& out) {
  if (!origin)
    return;
  Location location = origin->GetRange().begin();
  out << "       (Added by " + location.file()->name().value() << ":"
      << location.line_number() << ")\n";
}

// Templatized writer for writing out different config value types.
template<typename T> struct DescValueWriter {};
template<> struct DescValueWriter<std::string> {
  void operator()(const std::string& str, std::ostream& out) const {
    out << "    " << str << "\n";
  }
};

template<> struct DescValueWriter<SourceDir> {
  void operator()(const SourceDir& dir, std::ostream& out) const {
    out << "    " << FormatSourceDir(dir) << "\n";
  }
};

template<> struct DescValueWriter<LibFile> {
  void operator()(const LibFile& lib, std::ostream& out) const {
    if (lib.is_source_file())
      out << "    " << lib.source_file().value() << "\n";
    else
      out << "    " << lib.value() << "\n";
  }
};

// Writes a given config value type to the string, optionally with attribution.
// This should match RecursiveTargetConfigToStream in the order it traverses.
template<typename T> void OutputRecursiveTargetConfig(
    const Target* target,
    const char* header_name,
    const std::vector<T>& (ConfigValues::* getter)() const) {
  bool display_blame =
      base::CommandLine::ForCurrentProcess()->HasSwitch(kBlame);

  DescValueWriter<T> writer;
  std::ostringstream out;

  for (ConfigValuesIterator iter(target); !iter.done(); iter.Next()) {
    if ((iter.cur().*getter)().empty())
      continue;

    // Optional blame sub-head.
    if (display_blame) {
      const Config* config = iter.GetCurrentConfig();
      if (config) {
        // Source of this value is a config.
        out << "  From " << config->label().GetUserVisibleName(false) << "\n";
        OutputSourceOfDep(iter.origin(), out);
      } else {
        // Source of this value is the target itself.
        out << "  From " << target->label().GetUserVisibleName(false) << "\n";
      }
    }

    // Actual values.
    ConfigValuesToStream(iter.cur(), getter, writer, out);
  }

  std::string out_str = out.str();
  if (!out_str.empty()) {
    if (header_name)
      OutputString("\n" + std::string(header_name) + "\n");
    OutputString(out_str);
  }
}

template<typename T> void OutputConfigValueArray(
    const ConfigValues& values,
    const char* header_name,
    const std::vector<T>& (ConfigValues::* getter)() const) {
  std::ostringstream out;

  DescValueWriter<T> writer;
  for (const T& cur : (values.*getter)())
    writer(cur, out);

  std::string out_str = out.str();
  if (!out_str.empty()) {
    if (header_name)
      OutputString("\n" + std::string(header_name) + "\n");
    OutputString(out_str);
  }
}

void PrintRuntimeDeps(const Target* target) {
  bool display_blame =
      base::CommandLine::ForCurrentProcess()->HasSwitch(kBlame);
  Label toolchain = target->label().GetToolchainLabel();

  const Target* previous_from = NULL;
  for (const auto& pair : ComputeRuntimeDeps(target)) {
    if (display_blame) {
      // Generally a target's runtime deps will be listed sequentially, so
      // group them and don't duplicate the "from" label for two in a row.
      if (previous_from == pair.second) {
        OutputString("  ");  // Just indent.
      } else {
        previous_from = pair.second;
        OutputString("From ");
        OutputString(pair.second->label().GetUserVisibleName(toolchain));
        OutputString("\n  ");  // Make the file name indented.
      }
    }
    OutputString(pair.first.value());
    OutputString("\n");
  }
}

// If "what" is empty, prints all PCH info. If "what" is nonempty, prints only
// the things that match (if any). Returns true if anything was printed.
bool PrintPrecompiledHeaderInfo(const ConfigValues& values,
                                const std::string& what,
                                bool display_headers) {
  bool found_match = false;
  if (what == variables::kPrecompiledHeader || what.empty()) {
    if (!values.precompiled_header().empty()) {
      if (display_headers)
        OutputString("\nprecompiled_header\n");
      OutputString(values.precompiled_header() + "\n");
    }
    found_match = true;
  }
  if (what == variables::kPrecompiledSource || what.empty()) {
    if (!values.precompiled_source().is_null()) {
      if (display_headers)
        OutputString("\nprecompiled_source\n");
      OutputString(values.precompiled_source().value() + "\n");
    }
    found_match = true;
  }
  return found_match;
}

bool PrintTarget(const Target* target,
                 const std::string& what,
                 bool display_target_header) {
  if (display_target_header) {
    OutputString("Target: ", DECORATION_YELLOW);
    OutputString(target->label().GetUserVisibleName(false) + "\n");
    OutputString("Type: ", DECORATION_YELLOW);
    OutputString(std::string(
        Target::GetStringForOutputType(target->output_type())) + "\n");
    OutputString("Toolchain: ", DECORATION_YELLOW);
    OutputString(
        target->label().GetToolchainLabel().GetUserVisibleName(false) + "\n");
  }

  // Display headers when outputting everything.
  bool display_headers = what.empty();
  bool is_binary_output = target->IsBinary();

  bool found_match = false;

  // General target meta variables.
  if (what.empty() || what == variables::kVisibility) {
    PrintVisibility(target, display_headers);
    found_match = true;
  }
  if (what.empty() || what == variables::kTestonly) {
    PrintTestonly(target, display_headers);
    found_match  = true;
  }

  // Binary target meta variables.
  if (is_binary_output) {
    if (what.empty() || what == variables::kCheckIncludes) {
      PrintCheckIncludes(target, display_headers);
      found_match = true;
    }
    if (what.empty() || what == variables::kAllowCircularIncludesFrom) {
      PrintAllowCircularIncludesFrom(target, display_headers);
      found_match = true;
    }
  }

  // Sources and inputs.
  if (what.empty() || what == variables::kSources) {
    PrintSources(target, display_headers);
    found_match = true;
  }
  if (what.empty() || what == variables::kPublic) {
    PrintPublic(target, display_headers);
    found_match = true;
  }
  if (what.empty() || what == variables::kInputs) {
    PrintInputs(target, display_headers);
    found_match = true;
  }

  // Configs. Configs set directly on a target are only relevant for binary
  // targets
  if (is_binary_output && (what.empty() || what == variables::kConfigs)) {
    PrintConfigs(target, display_headers);
    found_match = true;
  }

  // Dependent/public configs can be applied to anything.
  if (what.empty() || what == variables::kPublicConfigs) {
    PrintPublicConfigs(target, display_headers);
    found_match = true;
  }
  if (what.empty() || what == variables::kAllDependentConfigs) {
    PrintAllDependentConfigs(target, display_headers);
    found_match = true;
  }

  // Action values.
  if (target->output_type() == Target::ACTION ||
      target->output_type() == Target::ACTION_FOREACH) {
    if (what.empty() || what == variables::kScript) {
      PrintScript(target, display_headers);
      found_match = true;
    }
    if (what.empty() || what == variables::kArgs) {
      PrintArgs(target, display_headers);
      found_match = true;
    }
    if (what.empty() || what == variables::kDepfile) {
      PrintDepfile(target, display_headers);
      found_match = true;
    }
  }

  // Outputs.
  if (target->output_type() == Target::ACTION ||
      target->output_type() == Target::ACTION_FOREACH ||
      target->output_type() == Target::COPY_FILES ||
      target->output_type() == Target::CREATE_BUNDLE) {
    if (what.empty() || what == variables::kOutputs) {
      PrintOutputs(target, display_headers);
      found_match = true;
    }
  }

  // Values from configs only apply to binary targets.
  if (is_binary_output) {
    #define CONFIG_VALUE_ARRAY_HANDLER(name, type) \
      if (what.empty() || what == #name) { \
        OutputRecursiveTargetConfig<type>( \
            target, display_headers ? #name : nullptr, &ConfigValues::name); \
        found_match = true; \
      }

    CONFIG_VALUE_ARRAY_HANDLER(arflags, std::string)
    CONFIG_VALUE_ARRAY_HANDLER(asmflags, std::string)
    CONFIG_VALUE_ARRAY_HANDLER(cflags, std::string)
    CONFIG_VALUE_ARRAY_HANDLER(cflags_c, std::string)
    CONFIG_VALUE_ARRAY_HANDLER(cflags_cc, std::string)
    CONFIG_VALUE_ARRAY_HANDLER(cflags_objc, std::string)
    CONFIG_VALUE_ARRAY_HANDLER(cflags_objcc, std::string)
    CONFIG_VALUE_ARRAY_HANDLER(defines, std::string)
    CONFIG_VALUE_ARRAY_HANDLER(include_dirs, SourceDir)
    CONFIG_VALUE_ARRAY_HANDLER(ldflags, std::string)
    // Libs and lib_dirs are handled specially below.

    #undef CONFIG_VALUE_ARRAY_HANDLER

    found_match |= PrintPrecompiledHeaderInfo(target->config_values(),
                                              what, display_headers);
  }

  // Deps
  if (what.empty() || what == "deps") {
    PrintDeps(target, display_headers);
    found_match = true;
  }

  // Runtime deps are special, print only when explicitly asked for and not in
  // overview mode.
  if (what == "runtime_deps") {
    PrintRuntimeDeps(target);
    found_match = true;
  }

  // Libs can be part of any target and get recursively pushed up the chain,
  // so display them regardless of target type.
  if (what.empty() || what == variables::kLibs) {
    PrintLibs(target, display_headers);
    found_match = true;
  }
  if (what.empty() || what == variables::kLibDirs) {
    PrintLibDirs(target, display_headers);
    found_match = true;
  }

  if (!found_match) {
    OutputString("Don't know how to display \"" + what + "\" for \"" +
        Target::GetStringForOutputType(target->output_type()) + "\".\n");
    return false;
  }
  return true;
}

bool PrintConfig(const Config* config,
                 const std::string& what,
                 bool display_config_header) {
  const ConfigValues& values = config->resolved_values();

  if (display_config_header) {
    OutputString("Config: ", DECORATION_YELLOW);
    OutputString(config->label().GetUserVisibleName(false) + "\n");
    OutputString("Toolchain: ", DECORATION_YELLOW);
    OutputString(
        config->label().GetToolchainLabel().GetUserVisibleName(false) + "\n");
    if (what.empty() && !config->configs().empty()) {
      OutputString(
          "(This is a composite config, the values below are after the\n"
          "expansion of the child configs.)\n");
    }
  }

  // Display headers when outputting everything.
  bool display_headers = what.empty();

  if (what.empty() || what == variables::kConfigs)
    PrintConfigs(config, display_headers);

#define CONFIG_VALUE_ARRAY_HANDLER(name, type) \
  if (what.empty() || what == #name) { \
    OutputConfigValueArray<type>(values, display_headers ? #name : nullptr, \
                                 &ConfigValues::name); \
    found_match = true; \
  }

  bool found_match = false;

  CONFIG_VALUE_ARRAY_HANDLER(arflags, std::string)
  CONFIG_VALUE_ARRAY_HANDLER(asmflags, std::string)
  CONFIG_VALUE_ARRAY_HANDLER(cflags, std::string)
  CONFIG_VALUE_ARRAY_HANDLER(cflags_c, std::string)
  CONFIG_VALUE_ARRAY_HANDLER(cflags_cc, std::string)
  CONFIG_VALUE_ARRAY_HANDLER(cflags_objc, std::string)
  CONFIG_VALUE_ARRAY_HANDLER(cflags_objcc, std::string)
  CONFIG_VALUE_ARRAY_HANDLER(defines, std::string)
  CONFIG_VALUE_ARRAY_HANDLER(include_dirs, SourceDir)
  CONFIG_VALUE_ARRAY_HANDLER(ldflags, std::string)
  CONFIG_VALUE_ARRAY_HANDLER(lib_dirs, SourceDir)
  CONFIG_VALUE_ARRAY_HANDLER(libs, LibFile)

#undef CONFIG_VALUE_ARRAY_HANDLER

  // Handles all PCH-related variables.
  found_match |= PrintPrecompiledHeaderInfo(config->resolved_values(),
                                            what, display_headers);

  if (!found_match) {
    OutputString("Don't know how to display \"" + what + "\" for a config.\n");
    return false;
  }
  return true;
}

}  // namespace

// desc ------------------------------------------------------------------------

const char kDesc[] = "desc";
const char kDesc_HelpShort[] =
    "desc: Show lots of insightful information about a target or config.";
const char kDesc_Help[] =
    "gn desc <out_dir> <label or pattern> [<what to show>] [--blame]\n"
    "\n"
    "  Displays information about a given target or config. The build\n"
    "  build parameters will be taken for the build in the given <out_dir>.\n"
    "\n"
    "  The <label or pattern> can be a target label, a config label, or a\n"
    "  label pattern (see \"gn help label_pattern\"). A label pattern will\n"
    "  only match targets.\n"
    "\n"
    "Possibilities for <what to show>\n"
    "\n"
    "  (If unspecified an overall summary will be displayed.)\n"
    "\n"
    "  all_dependent_configs\n"
    "  allow_circular_includes_from\n"
    "  arflags [--blame]\n"
    "  args\n"
    "  cflags [--blame]\n"
    "  cflags_cc [--blame]\n"
    "  cflags_cxx [--blame]\n"
    "  check_includes\n"
    "  configs [--tree] (see below)\n"
    "  defines [--blame]\n"
    "  depfile\n"
    "  deps [--all] [--tree] (see below)\n"
    "  include_dirs [--blame]\n"
    "  inputs\n"
    "  ldflags [--blame]\n"
    "  lib_dirs\n"
    "  libs\n"
    "  outputs\n"
    "  public_configs\n"
    "  public\n"
    "  script\n"
    "  sources\n"
    "  testonly\n"
    "  visibility\n"
    "\n"
    "  runtime_deps\n"
    "      Compute all runtime deps for the given target. This is a\n"
    "      computed list and does not correspond to any GN variable, unlike\n"
    "      most other values here.\n"
    "\n"
    "      The output is a list of file names relative to the build\n"
    "      directory. See \"gn help runtime_deps\" for how this is computed.\n"
    "      This also works with \"--blame\" to see the source of the\n"
    "      dependency.\n"
    "\n"
    "Shared flags\n"
    "\n"
    ALL_TOOLCHAINS_SWITCH_HELP
    "\n"
    "Target flags\n"
    "\n"
    "  --blame\n"
    "      Used with any value specified on a config, this will name\n"
    "      the config that cause that target to get the flag. This doesn't\n"
    "      currently work for libs and lib_dirs because those are inherited\n"
    "      and are more complicated to figure out the blame (patches\n"
    "      welcome).\n"
    "\n"
    "Configs\n"
    "\n"
    "  The \"configs\" section will list all configs that apply. For targets\n"
    "  this will include configs specified in the \"configs\" variable of\n"
    "  the target, and also configs pushed onto this target via public\n"
    "  or \"all dependent\" configs.\n"
    "\n"
    "  Configs can have child configs. Specifying --tree will show the\n"
    "  hierarchy.\n"
    "\n"
    "Printing deps\n"
    "\n"
    "  Deps will include all public, private, and data deps (TODO this could\n"
    "  be clarified and enhanced) sorted in order applying. The following\n"
    "  may be used:\n"
    "\n"
    "  --all\n"
    "      Collects all recursive dependencies and prints a sorted flat list.\n"
    "      Also usable with --tree (see below).\n"
    "\n"
    TARGET_PRINTING_MODE_COMMAND_LINE_HELP
    "\n"
    TARGET_TESTONLY_FILTER_COMMAND_LINE_HELP
    "\n"
    "  --tree\n"
    "      Print a dependency tree. By default, duplicates will be elided\n"
    "      with \"...\" but when --all and -tree are used together, no\n"
    "      eliding will be performed.\n"
    "\n"
    "      The \"deps\", \"public_deps\", and \"data_deps\" will all be\n"
    "      included in the tree.\n"
    "\n"
    "      Tree output can not be used with the filtering or output flags:\n"
    "      --as, --type, --testonly.\n"
    "\n"
    TARGET_TYPE_FILTER_COMMAND_LINE_HELP
    "\n"
    "Note\n"
    "\n"
    "  This command will show the full name of directories and source files,\n"
    "  but when directories and source paths are written to the build file,\n"
    "  they will be adjusted to be relative to the build directory. So the\n"
    "  values for paths displayed by this command won't match (but should\n"
    "  mean the same thing).\n"
    "\n"
    "Examples\n"
    "\n"
    "  gn desc out/Debug //base:base\n"
    "      Summarizes the given target.\n"
    "\n"
    "  gn desc out/Foo :base_unittests deps --tree\n"
    "      Shows a dependency tree of the \"base_unittests\" project in\n"
    "      the current directory.\n"
    "\n"
    "  gn desc out/Debug //base defines --blame\n"
    "      Shows defines set for the //base:base target, annotated by where\n"
    "      each one was set from.\n";

int RunDesc(const std::vector<std::string>& args) {
  if (args.size() != 2 && args.size() != 3) {
    Err(Location(), "You're holding it wrong.",
        "Usage: \"gn desc <out_dir> <target_name> [<what to display>]\"")
        .PrintToStdout();
    return 1;
  }
  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();

  // Deliberately leaked to avoid expensive process teardown.
  Setup* setup = new Setup;
  setup->build_settings().set_check_for_bad_items(false);
  if (!setup->DoSetup(args[0], false))
    return 1;
  if (!setup->Run())
    return 1;

  // Resolve target(s) and config from inputs.
  UniqueVector<const Target*> target_matches;
  UniqueVector<const Config*> config_matches;
  UniqueVector<const Toolchain*> toolchain_matches;
  UniqueVector<SourceFile> file_matches;

  std::vector<std::string> target_list;
  target_list.push_back(args[1]);

  if (!ResolveFromCommandLineInput(
          setup, target_list, cmdline->HasSwitch(switches::kAllToolchains),
          &target_matches, &config_matches, &toolchain_matches, &file_matches))
    return 1;

  std::string what_to_print;
  if (args.size() == 3)
    what_to_print = args[2];

  bool multiple_outputs = (target_matches.size() + config_matches.size()) > 1;

  // Display headers for each target when printing all values, or when printing
  // multiple targets or configs.
  bool display_item_header = multiple_outputs || what_to_print.empty();

  bool printed_output = false;
  for (const Target* target : target_matches) {
    if (printed_output)
      OutputString("\n\n");
    printed_output = true;

    if (!PrintTarget(target, what_to_print, display_item_header))
      return 1;
  }
  for (const Config* config : config_matches) {
    if (printed_output)
      OutputString("\n\n");
    printed_output = true;

    if (!PrintConfig(config, what_to_print, display_item_header))
      return 1;
  }

  return 0;
}

}  // namespace commands
