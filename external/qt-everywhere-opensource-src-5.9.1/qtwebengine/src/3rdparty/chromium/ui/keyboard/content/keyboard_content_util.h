// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_CONTENT_KEYBOARD_COTNENT_UTIL_H_
#define UI_KEYBOARD_CONTENT_KEYBOARD_COTNENT_UTIL_H_

#include <stddef.h>

#include "base/strings/string16.h"
#include "ui/keyboard/keyboard_export.h"

class GURL;

struct GritResourceMap;

namespace keyboard {

// Sets the override content url.
// This is used by for input view for extension IMEs.
KEYBOARD_EXPORT void SetOverrideContentUrl(const GURL& url);

// Gets the override content url.
KEYBOARD_EXPORT const GURL& GetOverrideContentUrl();

// Get the list of keyboard resources. |size| is populated with the number of
// resources in the returned array.
KEYBOARD_EXPORT const GritResourceMap* GetKeyboardExtensionResources(
    size_t* size);

}  // namespace keyboard

#endif  // UI_KEYBOARD_CONTENT_KEYBOARD_COTNENT_UTIL_H_
