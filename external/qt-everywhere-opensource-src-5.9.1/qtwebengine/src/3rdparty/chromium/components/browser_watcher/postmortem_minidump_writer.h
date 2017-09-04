// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_WATCHER_POSTMORTEM_MINIDUMP_WRITER_H_
#define COMPONENTS_BROWSER_WATCHER_POSTMORTEM_MINIDUMP_WRITER_H_

#include <stdint.h>

#include <string>

#include "base/files/file.h"
#include "components/browser_watcher/stability_report.pb.h"
#include "third_party/crashpad/crashpad/util/misc/uuid.h"

namespace browser_watcher {

// Minidump information required by the Crashpad reporter.
struct MinidumpInfo {
  MinidumpInfo();
  ~MinidumpInfo();

  // Client and report identifiers, from the Crashpad database.
  crashpad::UUID client_id;
  crashpad::UUID report_id;

  // Product name, version number and channel name from the executable's version
  // resource.
  std::string product_name;
  std::string version_number;
  std::string channel_name;

  // The platform identifier (e.g. "Win32" or "Win64").
  std::string platform;
};

// Write to |minidump_file| a minimal minidump that wraps |report|. Returns
// true on success, false otherwise.
// Note: the caller owns |minidump_file| and is responsible for keeping it valid
// for this function's duration. |minidump_file| is expected to be empty
// and a binary stream.
bool WritePostmortemDump(base::PlatformFile minidump_file,
                         const StabilityReport& report,
                         const MinidumpInfo& minidump_info);

}  // namespace browser_watcher

#endif  // COMPONENTS_BROWSER_WATCHER_POSTMORTEM_MINIDUMP_WRITER_H_
