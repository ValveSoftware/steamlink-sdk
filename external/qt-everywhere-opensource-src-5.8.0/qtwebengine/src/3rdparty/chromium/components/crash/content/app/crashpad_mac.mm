// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/app/crashpad.h"

#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <map>
#include <vector>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "components/crash/content/app/crash_reporter_client.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"
#include "third_party/crashpad/crashpad/client/crashpad_info.h"
#include "third_party/crashpad/crashpad/client/settings.h"
#include "third_party/crashpad/crashpad/client/simple_string_dictionary.h"
#include "third_party/crashpad/crashpad/client/simulate_crash.h"

namespace crash_reporter {
namespace internal {

base::FilePath PlatformCrashpadInitialization(bool initial_client,
                                              bool browser_process,
                                              bool embedded_handler) {
  base::FilePath database_path;  // Only valid in the browser process.
  DCHECK(!embedded_handler);  // This is not used on Mac.

  if (initial_client) {
    @autoreleasepool {
      base::FilePath framework_bundle_path = base::mac::FrameworkBundlePath();
      base::FilePath handler_path =
          framework_bundle_path.Append("Helpers").Append("crashpad_handler");

      // Is there a way to recover if this fails?
      CrashReporterClient* crash_reporter_client = GetCrashReporterClient();
      crash_reporter_client->GetCrashDumpLocation(&database_path);

      // TODO(mark): Reading the Breakpad keys is temporary and transitional. At
      // the very least, they should be renamed to Crashpad. For the time being,
      // this isn't the worst thing: Crashpad is still uploading to a
      // Breakpad-type server, after all.
      NSBundle* framework_bundle = base::mac::FrameworkBundle();
      NSString* product = base::mac::ObjCCast<NSString>(
          [framework_bundle objectForInfoDictionaryKey:@"BreakpadProduct"]);
      NSString* version = base::mac::ObjCCast<NSString>(
          [framework_bundle objectForInfoDictionaryKey:@"BreakpadVersion"]);
      NSString* url_ns = base::mac::ObjCCast<NSString>(
          [framework_bundle objectForInfoDictionaryKey:@"BreakpadURL"]);
#if defined(GOOGLE_CHROME_BUILD)
      NSString* channel = base::mac::ObjCCast<NSString>(
          [base::mac::OuterBundle() objectForInfoDictionaryKey:@"KSChannelID"]);
#else
      NSString* channel = nil;
#endif

      std::string url = base::SysNSStringToUTF8(url_ns);

      std::map<std::string, std::string> process_annotations;
      process_annotations["prod"] = base::SysNSStringToUTF8(product);
      process_annotations["ver"] = base::SysNSStringToUTF8(version);
      if (channel) {
        process_annotations["channel"] = base::SysNSStringToUTF8(channel);
      }
      process_annotations["plat"] = std::string("OS X");

      crashpad::CrashpadClient crashpad_client;

      std::vector<std::string> arguments;
      if (!browser_process) {
        // If this is an initial client that's not the browser process, it's
        // important that the new Crashpad handler also not be connected to any
        // existing handler. This argument tells the new Crashpad handler to
        // sever this connection.
        arguments.push_back(
            "--reset-own-crash-exception-port-to-system-default");
      }

      bool result = crashpad_client.StartHandler(handler_path,
                                                 database_path,
                                                 url,
                                                 process_annotations,
                                                 arguments,
                                                 true);
      if (result) {
        result = crashpad_client.UseHandler();
      }

      // If this is an initial client that's not the browser process, it's
      // important to sever the connection to any existing handler. If
      // StartHandler() or UseHandler() failed, call UseSystemDefaultHandler()
      // in that case to drop the link to the existing handler.
      if (!result && !browser_process) {
        crashpad::CrashpadClient::UseSystemDefaultHandler();
      }
    }  // @autoreleasepool
  }

  return database_path;
}

}  // namespace internal
}  // namespace crash_reporter
