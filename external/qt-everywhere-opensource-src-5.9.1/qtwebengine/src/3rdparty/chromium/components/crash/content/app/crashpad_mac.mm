// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/app/crashpad.h"

#include <CoreFoundation/CoreFoundation.h>
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
  base::FilePath metrics_path;  // Only valid in the browser process.
  DCHECK(!embedded_handler);  // This is not used on Mac.

  if (initial_client) {
    @autoreleasepool {
      base::FilePath framework_bundle_path = base::mac::FrameworkBundlePath();
      base::FilePath handler_path =
          framework_bundle_path.Append("Helpers").Append("crashpad_handler");

      // Is there a way to recover if this fails?
      CrashReporterClient* crash_reporter_client = GetCrashReporterClient();
      crash_reporter_client->GetCrashDumpLocation(&database_path);
      crash_reporter_client->GetCrashMetricsLocation(&metrics_path);

#if defined(GOOGLE_CHROME_BUILD) && defined(OFFICIAL_BUILD)
      // Only allow the possibility of report upload in official builds. This
      // crash server won't have symbols for any other build types.
      std::string url = "https://clients2.google.com/cr/report";
#else
      std::string url;
#endif

      std::map<std::string, std::string> process_annotations;

      NSBundle* outer_bundle = base::mac::OuterBundle();
      NSString* product = base::mac::ObjCCast<NSString>([outer_bundle
          objectForInfoDictionaryKey:base::mac::CFToNSCast(kCFBundleNameKey)]);
      process_annotations["prod"] =
          base::SysNSStringToUTF8(product).append("_Mac");

#if defined(GOOGLE_CHROME_BUILD)
      NSString* channel = base::mac::ObjCCast<NSString>(
          [outer_bundle objectForInfoDictionaryKey:@"KSChannelID"]);
      if (channel) {
        process_annotations["channel"] = base::SysNSStringToUTF8(channel);
      }
#endif

      NSString* version =
          base::mac::ObjCCast<NSString>([base::mac::FrameworkBundle()
              objectForInfoDictionaryKey:@"CFBundleShortVersionString"]);
      process_annotations["ver"] = base::SysNSStringToUTF8(version);

      process_annotations["plat"] = std::string("OS X");

      std::vector<std::string> arguments;
      if (!browser_process) {
        // If this is an initial client that's not the browser process, it's
        // important that the new Crashpad handler also not be connected to any
        // existing handler. This argument tells the new Crashpad handler to
        // sever this connection.
        arguments.push_back(
            "--reset-own-crash-exception-port-to-system-default");
      }

      crashpad::CrashpadClient crashpad_client;
      bool result = crashpad_client.StartHandler(handler_path,
                                                 database_path,
                                                 metrics_path,
                                                 url,
                                                 process_annotations,
                                                 arguments,
                                                 true,
                                                 false);

      // If this is an initial client that's not the browser process, it's
      // important to sever the connection to any existing handler. If
      // StartHandler() failed, call UseSystemDefaultHandler() to drop the link
      // to the existing handler.
      if (!result && !browser_process) {
        crashpad::CrashpadClient::UseSystemDefaultHandler();
      }
    }  // @autoreleasepool
  }

  return database_path;
}

}  // namespace internal
}  // namespace crash_reporter
