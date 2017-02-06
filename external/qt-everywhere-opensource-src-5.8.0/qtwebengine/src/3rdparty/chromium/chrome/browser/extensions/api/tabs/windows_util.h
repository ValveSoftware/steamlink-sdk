// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_WINDOWS_UTIL_H__
#define CHROME_BROWSER_EXTENSIONS_API_TABS_WINDOWS_UTIL_H__

#include "chrome/browser/extensions/window_controller_list.h"

class UIThreadExtensionFunction;

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
class WindowController;
}

namespace windows_util {

// Populates |controller| for given |window_id|. If the window is not found,
// returns false and sets UIThreadExtensionFunction error_.
bool GetWindowFromWindowID(UIThreadExtensionFunction* function,
                           int window_id,
                           extensions::WindowController::TypeFilter filter,
                           extensions::WindowController** controller);

// Returns true if |function| (and the profile and extension that it was
// invoked from) can operate on the window wrapped by |window_controller|.
// If |all_window_types| is set this function will return true for any
// kind of window (including app and devtools), otherwise it will
// return true only for normal browser windows as well as windows
// created by the extension.
bool CanOperateOnWindow(const UIThreadExtensionFunction* function,
                        const extensions::WindowController* controller,
                        extensions::WindowController::TypeFilter filter);

}  // namespace windows_util

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_WINDOWS_UTIL_H__
