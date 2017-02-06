// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/audio_modem/audio_modem_switches.h"

namespace switches {

// Directory to dump encoded tokens to, for debugging.
// If empty (the default), tokens are not dumped.
// If invalid (not a writable directory), Chrome will crash!
const char kAudioModemDumpTokensToDir[] = "audio-modem-dump-tokens-to-dir";

// Allow broadcast of audible audio tokens. Defaults to true.
const char kAudioModemEnableAudibleBroadcast[] =
    "audio-modem-enable-audible-broadcast";

// Allow broadcast of inaudible audio tokens. Defaults to true.
const char kAudioModemEnableInaudibleBroadcast[] =
    "audio-modem-enable-inaudible-broadcast";

}  // namespace switches
