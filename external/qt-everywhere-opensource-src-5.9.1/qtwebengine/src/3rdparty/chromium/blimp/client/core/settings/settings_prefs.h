// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_SETTINGS_SETTINGS_PREFS_H_
#define BLIMP_CLIENT_CORE_SETTINGS_SETTINGS_PREFS_H_

namespace blimp {
namespace client {
namespace prefs {

// Whether or not to enable Blimp mode.
extern const char kBlimpEnabled[];

// Whether or not to download the whole page.
extern const char kRecordWholeDocument[];

}  // namespace prefs
}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_SETTINGS_SETTINGS_PREFS_H_
