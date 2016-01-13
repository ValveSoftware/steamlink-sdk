// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This command-line program dumps the contents of a set of cache files, either
// to stdout or to another set of cache files.

#include <stdio.h>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/disk_cache/blockfile/disk_format.h"
#include "net/tools/dump_cache/dump_files.h"
#include "net/tools/dump_cache/simple_cache_dumper.h"

#if defined(OS_WIN)
#include "base/process/launch.h"
#include "base/win/scoped_handle.h"
#include "net/tools/dump_cache/upgrade_win.h"
#endif

enum Errors {
  GENERIC = -1,
  ALL_GOOD = 0,
  INVALID_ARGUMENT = 1,
  FILE_ACCESS_ERROR,
  UNKNOWN_VERSION,
  TOOL_NOT_FOUND,
};

#if defined(OS_WIN)
const char kUpgradeHelp[] =
    "\nIn order to use the upgrade function, a version of this tool that\n"
    "understands the file format of the files to upgrade is needed. For\n"
    "instance, to upgrade files saved with file format 3.4 to version 5.2,\n"
    "a version of this program that was compiled with version 3.4 has to be\n"
    "located beside this executable, and named dump_cache_3.exe, and this\n"
    "executable should be compiled with version 5.2 being the current one.";
#endif  // defined(OS_WIN)

// Folders to read and write cache files.
const char kInputPath[] = "input";
const char kOutputPath[] = "output";

// Dumps the file headers to stdout.
const char kDumpHeaders[] = "dump-headers";

// Dumps all entries to stdout.
const char kDumpContents[] = "dump-contents";

// Convert the cache to files.
const char kDumpToFiles[] = "dump-to-files";

// Upgrade an old version to the current one.
const char kUpgrade[] = "upgrade";

// Internal use:
const char kSlave[] = "slave";
#if defined(OS_WIN)
const char kPipe[] = "pipe";
#endif  // defined(OS_WIN)

int Help() {
  printf("warning: input files are modified by this tool\n");
  printf("dump_cache --input=path1 [--output=path2]\n");
  printf("--dump-headers: display file headers\n");
  printf("--dump-contents: display all entries\n");
  printf("--upgrade: copy contents to the output path\n");
  printf("--dump-to-files: write the contents of the cache to files\n");
  return INVALID_ARGUMENT;
}

#if defined(OS_WIN)

// Starts a new process, to generate the files.
int LaunchSlave(CommandLine command_line,
                const base::string16& pipe_number,
                int version) {
  bool do_upgrade = command_line.HasSwitch(kUpgrade);
  bool do_convert_to_text = command_line.HasSwitch(kDumpToFiles);

  if (do_upgrade) {
    base::FilePath program(
        base::StringPrintf(L"%ls%d", L"dump_cache", version));
    command_line.SetProgram(program);
  }

  if (do_upgrade || do_convert_to_text)
    command_line.AppendSwitch(kSlave);

  command_line.AppendSwitchNative(kPipe, pipe_number);
  if (!base::LaunchProcess(command_line, base::LaunchOptions(), NULL)) {
    printf("Unable to launch the needed version of this tool: %ls\n",
           command_line.GetProgram().value().c_str());
    printf("%s", kUpgradeHelp);
    return TOOL_NOT_FOUND;
  }
  return ALL_GOOD;
}

#endif

// -----------------------------------------------------------------------

int main(int argc, const char* argv[]) {
  // Setup an AtExitManager so Singleton objects will be destroyed.
  base::AtExitManager at_exit_manager;

  base::CommandLine::Init(argc, argv);

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  base::FilePath input_path = command_line.GetSwitchValuePath(kInputPath);
  if (input_path.empty())
    return Help();

  bool dump_to_files = command_line.HasSwitch(kDumpToFiles);
  bool upgrade = command_line.HasSwitch(kUpgrade);

  base::FilePath output_path = command_line.GetSwitchValuePath(kOutputPath);
  if ((dump_to_files || upgrade) && output_path.empty())
    return Help();

  int version = GetMajorVersion(input_path);
  if (!version)
    return FILE_ACCESS_ERROR;

  bool slave_required = upgrade;
  if (version != disk_cache::kCurrentVersion >> 16) {
    if (command_line.HasSwitch(kSlave)) {
      printf("Unknown version\n");
      return UNKNOWN_VERSION;
    }
    slave_required = true;
  }

#if defined(OS_WIN)
  base::string16 pipe_number = command_line.GetSwitchValueNative(kPipe);
  if (command_line.HasSwitch(kSlave) && slave_required)
    return RunSlave(input_path, pipe_number);

  base::win::ScopedHandle server;
  if (slave_required) {
    server.Set(CreateServer(&pipe_number));
    if (!server.IsValid()) {
      printf("Unable to create the server pipe\n");
      return GENERIC;
    }

    int ret = LaunchSlave(command_line, pipe_number, version);
    if (ret)
      return ret;
  }

  if (upgrade)
    return UpgradeCache(output_path, server);

  if (slave_required) {
    // Wait until the slave starts dumping data before we quit. Lazy "fix" for a
    // console quirk.
    Sleep(500);
    return ALL_GOOD;
  }
#else  // defined(OS_WIN)
  if (slave_required) {
    printf("Unsupported operation\n");
    return INVALID_ARGUMENT;
  }
#endif

  if (dump_to_files) {
    net::SimpleCacheDumper dumper(input_path, output_path);
    dumper.Run();
    return ALL_GOOD;
  }

  if (command_line.HasSwitch(kDumpContents))
    return DumpContents(input_path);

  if (command_line.HasSwitch(kDumpHeaders))
    return DumpHeaders(input_path);

  return Help();
}
