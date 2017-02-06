// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/common/crash_keys.h"

#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"

namespace crash_keys {

#if defined(OS_MACOSX)
// Crashpad owns the "guid" key. Chrome's metrics client ID is a separate ID
// carried in a distinct "metrics_client_id" field.
const char kMetricsClientId[] = "metrics_client_id";
#elif defined(OS_WIN)
// TODO(scottmg): While transitioning to Crashpad, there are some executables
// that use Crashpad (which use kMetricsClientId), and some that use Breakpad
// (kClientId), and they both use this file. For now we define both, but once
// Breakpad is no longer used on Windows, we will no longer need kClientId, and
// this can be combined with the OS_MACOSX block above.
const char kMetricsClientId[] = "metrics_client_id";
const char kClientId[] = "guid";
#else
const char kClientId[] = "guid";
#endif

const char kChannel[] = "channel";

const char kNumVariations[] = "num-experiments";
const char kVariations[] = "variations";

const char kSwitchFormat[] = "switch-%" PRIuS;
const char kNumSwitches[] = "num-switches";

const char kBug464926CrashKey[] = "bug-464926-info";

#if defined(OS_MACOSX)
namespace mac {

const char kZombie[] = "zombie";
const char kZombieTrace[] = "zombie_dealloc_bt";

}  // namespace mac
#endif

void SetMetricsClientIdFromGUID(const std::string& metrics_client_guid) {
  std::string stripped_guid(metrics_client_guid);
  // Remove all instance of '-' char from the GUID. So BCD-WXY becomes BCDWXY.
  base::ReplaceSubstringsAfterOffset(
      &stripped_guid, 0, "-", base::StringPiece());
  if (stripped_guid.empty())
    return;

#if defined(OS_MACOSX) || defined(OS_WIN)
  // The crash client ID is maintained by Crashpad and is distinct from the
  // metrics client ID, which is carried in its own key.
  base::debug::SetCrashKeyValue(kMetricsClientId, stripped_guid);
#else
  // The crash client ID is set by the application when Breakpad is in use.
  // The same ID as the metrics client ID is used.
  base::debug::SetCrashKeyValue(kClientId, stripped_guid);
#endif
}

void ClearMetricsClientId() {
#if defined(OS_MACOSX) || defined(OS_WIN)
  // Crashpad always monitors for crashes, but doesn't upload them when
  // crash reporting is disabled. The preference to upload crash reports is
  // linked to the preference for metrics reporting. When metrics reporting is
  // disabled, don't put the metrics client ID into crash dumps. This way, crash
  // reports that are saved but not uploaded will not have a metrics client ID
  // from the time that metrics reporting was disabled even if they are uploaded
  // by user action at a later date.
  //
  // Breakpad cannot be enabled or disabled without an application restart, and
  // it needs to use the metrics client ID as its stable crash client ID, so
  // leave its client ID intact even when metrics reporting is disabled while
  // the application is running.
  base::debug::ClearCrashKey(kMetricsClientId);
#endif
}

void SetVariationsList(const std::vector<std::string>& variations) {
  base::debug::SetCrashKeyValue(kNumVariations,
      base::StringPrintf("%" PRIuS, variations.size()));

  std::string variations_string;
  variations_string.reserve(kLargeSize);

  for (size_t i = 0; i < variations.size(); ++i) {
    const std::string& variation = variations[i];
    // Do not truncate an individual experiment.
    if (variations_string.size() + variation.size() >= kLargeSize)
      break;
    variations_string += variation;
    variations_string += ",";
  }

  base::debug::SetCrashKeyValue(kVariations, variations_string);
}

void GetCrashKeysForCommandLineSwitches(
    std::vector<base::debug::CrashKey>* keys) {
  DCHECK(keys);
  base::debug::CrashKey crash_key = { kNumSwitches, kSmallSize };
  keys->push_back(crash_key);

  // Use static storage for formatted key names, since they will persist for
  // the duration of the program.
  static char formatted_keys[kSwitchesMaxCount][sizeof(kSwitchFormat) + 1] =
      {{ 0 }};
  const size_t formatted_key_len = sizeof(formatted_keys[0]);

  // sizeof(kSwitchFormat) is platform-dependent, so make sure formatted_keys
  // actually have space for a 2-digit switch number plus null-terminator.
  static_assert(formatted_key_len >= 10,
                "insufficient space for \"switch-NN\"");

  for (size_t i = 0; i < kSwitchesMaxCount; ++i) {
    // Name the keys using 1-based indexing.
    int n = base::snprintf(formatted_keys[i], formatted_key_len, kSwitchFormat,
                           i + 1);
    DCHECK_GT(n, 0);
    base::debug::CrashKey crash_key = { formatted_keys[i], kSmallSize };
    keys->push_back(crash_key);
  }
}

void SetSwitchesFromCommandLine(const base::CommandLine& command_line,
                                SwitchFilterFunction skip_filter) {
  const base::CommandLine::StringVector& argv = command_line.argv();

  // Set the number of switches in case size > kNumSwitches.
  base::debug::SetCrashKeyValue(kNumSwitches,
      base::StringPrintf("%" PRIuS, argv.size() - 1));

  size_t key_i = 1;  // Key names are 1-indexed.

  // Go through the argv, skipping the exec path. Stop if there are too many
  // switches to hold in crash keys.
  for (size_t i = 1; i < argv.size() && key_i <= crash_keys::kSwitchesMaxCount;
       ++i) {
#if defined(OS_WIN)
    std::string switch_str = base::WideToUTF8(argv[i]);
#else
    std::string switch_str = argv[i];
#endif

    // Skip uninteresting switches.
    if (skip_filter && (*skip_filter)(switch_str))
      continue;

    std::string key = base::StringPrintf(kSwitchFormat, key_i++);
    base::debug::SetCrashKeyValue(key, switch_str);
  }

  // Clear any remaining switches.
  for (; key_i <= kSwitchesMaxCount; ++key_i)
    base::debug::ClearCrashKey(base::StringPrintf(kSwitchFormat, key_i));
}

}  // namespace crash_keys
