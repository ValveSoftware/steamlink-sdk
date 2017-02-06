// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utilities related to password manager's UI.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_UI_UTILS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_UI_UTILS_H_

#include <string>

#include "base/strings/string_piece.h"

#include "url/gurl.h"

namespace autofill {
struct PasswordForm;
}

namespace password_manager {

// Reverses order of labels in hostname.
std::string SplitByDotAndReverse(base::StringPiece host);

// Removes 'android://' and reverses order of labels in hostname.
std::string StripAndroidAndReverse(const std::string& origin);

// Returns a string suitable for security display to the user (just like
// |FormatUrlForSecurityDisplay| with OMIT_HTTP_AND_HTTPS) based on origin of
// |password_form| and without prefixes "m.", "mobile." or "www.". Also
// returns the full URL of the origin as |link_url|. |link_url| will be also
// shown as tooltip on the password page.
// For Android forms with empty |password_form.affiliated_web_realm|,
// returns the result of GetHumanReadableOriginForAndroidUri. For other Android
// forms, returns |password_form.affiliated_web_realm|.
// |*origin_is_clickable| is set to true, except for Android forms with empty
// |password_form.affiliated_web_realm|.
// |is_android_url|, |link_url|, |origin_is_clickable| are required to non-null.
std::string GetShownOriginAndLinkUrl(
    const autofill::PasswordForm& password_form,
    bool* is_android_uri,
    GURL* link_url,
    bool* origin_is_clickable);

// Returns a string suitable for security display to the user (just like
// |FormatUrlForSecurityDisplay| with OMIT_HTTP_AND_HTTPS) based on origin of
// |password_form|) and without prefixes "m.", "mobile." or "www.".
std::string GetShownOrigin(const GURL& origin);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_UI_UTILS_H_
