// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/manifest_util.h"

#include "base/strings/string_util.h"

namespace content {

std::string WebDisplayModeToString(blink::WebDisplayMode display) {
  switch (display) {
    case blink::WebDisplayModeUndefined:
      return "";
    case blink::WebDisplayModeBrowser:
      return "browser";
    case blink::WebDisplayModeMinimalUi:
      return "minimal-ui";
    case blink::WebDisplayModeStandalone:
      return "standalone";
    case blink::WebDisplayModeFullscreen:
      return "fullscreen";
  }
  return "";
}

blink::WebDisplayMode WebDisplayModeFromString(const std::string& display) {
  if (base::LowerCaseEqualsASCII(display, "browser"))
    return blink::WebDisplayModeBrowser;
  else if (base::LowerCaseEqualsASCII(display, "minimal-ui"))
    return blink::WebDisplayModeMinimalUi;
  else if (base::LowerCaseEqualsASCII(display, "standalone"))
    return blink::WebDisplayModeStandalone;
  else if (base::LowerCaseEqualsASCII(display, "fullscreen"))
    return blink::WebDisplayModeFullscreen;
  return blink::WebDisplayModeUndefined;
}

std::string WebScreenOrientationLockTypeToString(
    blink::WebScreenOrientationLockType orientation) {
  switch (orientation) {
    case blink::WebScreenOrientationLockDefault:
      return "";
    case blink::WebScreenOrientationLockPortraitPrimary:
      return "portrait-primary";
    case blink::WebScreenOrientationLockPortraitSecondary:
      return "portrait-secondary";
    case blink::WebScreenOrientationLockLandscapePrimary:
      return "landscape-primary";
    case blink::WebScreenOrientationLockLandscapeSecondary:
      return "landscape-secondary";
    case blink::WebScreenOrientationLockAny:
      return "any";
    case blink::WebScreenOrientationLockLandscape:
      return "landscape";
    case blink::WebScreenOrientationLockPortrait:
      return "portrait";
    case blink::WebScreenOrientationLockNatural:
      return "natural";
  }
  return "";
}

blink::WebScreenOrientationLockType
WebScreenOrientationLockTypeFromString(const std::string& orientation) {
  if (base::LowerCaseEqualsASCII(orientation, "portrait-primary"))
    return blink::WebScreenOrientationLockPortraitPrimary;
  else if (base::LowerCaseEqualsASCII(orientation, "portrait-secondary"))
    return blink::WebScreenOrientationLockPortraitSecondary;
  else if (base::LowerCaseEqualsASCII(orientation, "landscape-primary"))
    return blink::WebScreenOrientationLockLandscapePrimary;
  else if (base::LowerCaseEqualsASCII(orientation, "landscape-secondary"))
    return blink::WebScreenOrientationLockLandscapeSecondary;
  else if (base::LowerCaseEqualsASCII(orientation, "any"))
    return blink::WebScreenOrientationLockAny;
  else if (base::LowerCaseEqualsASCII(orientation, "landscape"))
    return blink::WebScreenOrientationLockLandscape;
  else if (base::LowerCaseEqualsASCII(orientation, "portrait"))
    return blink::WebScreenOrientationLockPortrait;
  else if (base::LowerCaseEqualsASCII(orientation, "natural"))
    return blink::WebScreenOrientationLockNatural;
  return blink::WebScreenOrientationLockDefault;
}

}  // namespace content
