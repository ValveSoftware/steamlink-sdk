// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A utility for printing the contents of a postmortem stability minidump.

#include <windows.h>  // NOLINT
#include <dbghelp.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "components/browser_watcher/stability_report.pb.h"

namespace {

const char kUsage[] =
    "Usage: %ls --minidump=<minidump file>\n"
    "\n"
    "  Dumps the contents of a postmortem minidump in a human readable way.\n";

bool ParseCommandLine(const base::CommandLine* cmd,
                      base::FilePath* minidump_path) {
  *minidump_path = cmd->GetSwitchValuePath("minidump");
  if (minidump_path->empty()) {
    LOG(ERROR) << "Missing minidump file.\n";
    LOG(ERROR) << base::StringPrintf(kUsage, cmd->GetProgram().value().c_str());
    return false;
  }
  return true;
}

void PrintProcessState(FILE* out,
                       const browser_watcher::ProcessState& process) {
  fprintf(out, "%s", "Process:\n");
  for (int i = 0; i < process.threads_size(); ++i) {
    const browser_watcher::ThreadState thread = process.threads(i);
    fprintf(out, "%s\n", thread.thread_name().c_str());
  }
}

// TODO(manzagop): flesh out as StabilityReport gets fleshed out.
void PrintReport(FILE* out, const browser_watcher::StabilityReport& report) {
  for (int i = 0; i < report.process_states_size(); ++i) {
    const browser_watcher::ProcessState process = report.process_states(i);
    PrintProcessState(out, process);
  }
}

int Main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

  // Get the dump.
  base::FilePath minidump_path;
  if (!ParseCommandLine(base::CommandLine::ForCurrentProcess(), &minidump_path))
    return 1;

  // Read the minidump to extract the proto.
  base::ScopedFILE minidump_file;
  minidump_file.reset(base::OpenFile(minidump_path, "rb"));
  CHECK(minidump_file.get());

  // Read the header.
  // TODO(manzagop): leverage Crashpad to do this.
  MINIDUMP_HEADER header = {};
  CHECK_EQ(1U, fread(&header, sizeof(header), 1U, minidump_file.get()));
  CHECK_EQ(static_cast<ULONG32>(MINIDUMP_SIGNATURE), header.Signature);
  CHECK_EQ(2U, header.NumberOfStreams);
  RVA directory_rva = header.StreamDirectoryRva;

  // Read the directory entry for the stability report's stream.
  // Note: this hardcodes an expectation that the stability report is the first
  // encountered stream. This is acceptable for a debug tool.
  MINIDUMP_DIRECTORY directory = {};
  CHECK_EQ(0, fseek(minidump_file.get(), directory_rva, SEEK_SET));
  CHECK_EQ(1U, fread(&directory, sizeof(directory), 1U, minidump_file.get()));
  CHECK_EQ(static_cast<ULONG32>(0x4B6B0002), directory.StreamType);
  RVA report_rva = directory.Location.Rva;
  ULONG32 report_size_bytes = directory.Location.DataSize;

  // Read the serialized stability report.
  std::string serialized_report;
  serialized_report.resize(report_size_bytes);
  CHECK_EQ(0, fseek(minidump_file.get(), report_rva, SEEK_SET));
  CHECK_EQ(report_size_bytes, fread(&serialized_report.at(0), 1,
                                    report_size_bytes, minidump_file.get()));

  browser_watcher::StabilityReport report;
  CHECK(report.ParseFromString(serialized_report));

  // Note: we can't use the usual protocol buffer human readable API due to
  // the use of optimize_for = LITE_RUNTIME.
  PrintReport(stdout, report);

  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  return Main(argc, argv);
}
