// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VERSION_INFO_VERSION_INFO_H_
#define COMPONENTS_VERSION_INFO_VERSION_INFO_H_

#include <string>

namespace version_info {

// The possible channels for an installation, from most fun to most stable.
enum class Channel { UNKNOWN = 0, CANARY, DEV, BETA, STABLE };

// Returns the product name and version information for UserAgent header,
// e.g. "Chrome/a.b.c.d".
std::string GetProductNameAndVersionForUserAgent();

// Returns the product name, e.g. "Chromium" or "Google Chrome".
std::string GetProductName();

// Returns the version number, e.g. "6.0.490.1".
std::string GetVersionNumber();

// Returns a version control specific identifier of this release.
std::string GetLastChange();

// Returns whether this is an "official" release of the current version, i.e.
// whether kwnowing GetVersionNumber() is enough to completely determine what
// GetLastChange() is.
bool IsOfficialBuild();

// Returns the OS type, e.g. "Windows", "Linux", "FreeBDS", ...
std::string GetOSType();

// Returns a string equivalent of |channel|, indenpendent of whether the build
// is branded or not and without any additional modifiers.
std::string GetChannelString(Channel channel);

// Returns a version string to be displayed in "About Chromium" dialog.
// |modifier| is a string representation of the channel with system specific
// information, e.g. "dev SyzyASan". It is appended to the returned version
// information if non-empty.
std::string GetVersionStringWithModifier(const std::string& modifier);

}  // namespace version_info

#endif  // COMPONENTS_VERSION_INFO_VERSION_INFO_H_
