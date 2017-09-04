// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CERTIFICATE_TRANSPARENCY_PREF_NAMES_H_
#define COMPONENTS_CERTIFICATE_TRANSPARENCY_PREF_NAMES_H_

namespace certificate_transparency {
namespace prefs {

// The set of hosts (as URLBlacklist-syntax filters) for which Certificate
// Transparency is required to be present.
extern const char kCTRequiredHosts[];

// The set of hosts (as URLBlacklist-syntax filters) for which Certificate
// Transparency information is allowed to be absent, even if it would
// otherwise be required (e.g. as part of security policy).
extern const char kCTExcludedHosts[];

}  // namespace prefs
}  // namespace certificate_transparency

#endif  // COMPONENTS_CERTIFICATE_TRANSPARENCY_PREF_NAMES_H_
