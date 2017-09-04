// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_APP_BLIMP_ENGINE_CRASH_KEYS_H_
#define BLIMP_ENGINE_APP_BLIMP_ENGINE_CRASH_KEYS_H_

#include <stddef.h>

namespace blimp {
namespace engine {

// Registers the crash keys needed for the Blimp engine. Returns the size of the
// union of all keys.
size_t RegisterEngineCrashKeys();

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_APP_BLIMP_ENGINE_CRASH_KEYS_H_
