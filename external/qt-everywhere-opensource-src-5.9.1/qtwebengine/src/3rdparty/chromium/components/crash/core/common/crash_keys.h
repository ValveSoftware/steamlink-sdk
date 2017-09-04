// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_COMMON_CRASH_KEYS_H_
#define COMPONENTS_CRASH_CORE_COMMON_CRASH_KEYS_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/debug/crash_logging.h"
#include "build/build_config.h"

namespace base {
class CommandLine;
}  // namespace base

namespace crash_keys {

// Sets the ID (which may either be a full GUID or a GUID that was already
// stripped from its dashes -- in either case this method will strip remaining
// dashes before setting the crash key).
void SetMetricsClientIdFromGUID(const std::string& metrics_client_guid);
void ClearMetricsClientId();

// Sets the list of active experiment/variations info.
void SetVariationsList(const std::vector<std::string>& variations);

// Adds a common set of crash keys for holding command-line switches to |keys|.
void GetCrashKeysForCommandLineSwitches(
    std::vector<base::debug::CrashKey>* keys);

// A function returning true if |flag| is a switch that should be filtered out
// of crash keys.
using SwitchFilterFunction = bool (*)(const std::string& flag);

// Sets the kNumSwitches key and a set of keys named using kSwitchFormat based
// on the given |command_line|. If |skip_filter| is not null, ignore any switch
// for which it returns true.
void SetSwitchesFromCommandLine(const base::CommandLine& command_line,
                                SwitchFilterFunction skip_filter);

// Crash Key Constants /////////////////////////////////////////////////////////

// kChunkMaxLength is the platform-specific maximum size that a value in a
// single chunk can be; see base::debug::InitCrashKeys. The maximum lengths
// specified by breakpad include the trailing NULL, so the actual length of the
// chunk is one less.
#if defined(OS_MACOSX)
const size_t kChunkMaxLength = 255;
#else  // OS_MACOSX
const size_t kChunkMaxLength = 63;
#endif  // !OS_MACOSX

// A small crash key, guaranteed to never be split into multiple pieces.
const size_t kSmallSize = 63;

// A medium crash key, which will be chunked on certain platforms but not
// others. Guaranteed to never be more than four chunks.
const size_t kMediumSize = kSmallSize * 4;

// A large crash key, which will be chunked on all platforms. This should be
// used sparingly.
const size_t kLargeSize = kSmallSize * 16;

// Crash Key Name Constants ////////////////////////////////////////////////////

// The GUID used to identify this client to the crash system.
#if defined(OS_MACOSX)
// When using Crashpad, the crash reporting client ID is the responsibility of
// Crashpad. It is not set directly by Chrome. To make the metrics client ID
// available on the server, it's stored in a distinct key.
extern const char kMetricsClientId[];
#elif defined(OS_WIN)
extern const char kMetricsClientId[];
extern const char kClientId[];
#else
// When using Breakpad instead of Crashpad, the crash reporting client ID is the
// same as the metrics client ID.
extern const char kClientId[];
#endif

// The product release/distribution channel.
extern const char kChannel[];

// The total number of experiments the instance has.
extern const char kNumVariations[];

// The experiments chunk. Hashed experiment names separated by |,|. This is
// typically set by SetExperimentList.
extern const char kVariations[];

// The maximum number of command line switches to process. |kSwitchFormat|
// should be formatted with an integer in the range [1, kSwitchesMaxCount].
const size_t kSwitchesMaxCount = 15;

// A printf-style format string naming the set of crash keys corresponding to
// at most |kSwitchesMaxCount| command line switches.
extern const char kSwitchFormat[];

// The total number of switches, used to report the total in case more than
// |kSwitchesMaxCount| are present.
extern const char kNumSwitches[];

// Used to help investigate bug 464926.
extern const char kBug464926CrashKey[];

#if defined(OS_MACOSX)
namespace mac {

// Records Cocoa zombie/used-after-freed objects that resulted in a
// deliberate crash.
extern const char kZombie[];
extern const char kZombieTrace[];

}  // namespace mac
#endif

}  // namespace crash_keys

#endif  // COMPONENTS_CRASH_CORE_COMMON_CRASH_KEYS_H_
