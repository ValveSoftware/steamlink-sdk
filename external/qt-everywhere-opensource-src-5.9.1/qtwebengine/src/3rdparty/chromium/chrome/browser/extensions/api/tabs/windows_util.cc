// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "chrome/browser/extensions/api/tabs/windows_util.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"

namespace windows_util {

bool GetWindowFromWindowID(UIThreadExtensionFunction* function,
                           int window_id,
                           extensions::WindowController::TypeFilter filter,
                           extensions::WindowController** controller,
                           std::string* error) {
  DCHECK(error);
  if (window_id == extension_misc::kCurrentWindowId) {
    extensions::WindowController* extension_window_controller =
        function->dispatcher()->GetExtensionWindowController();
    // If there is a window controller associated with this extension, use that.
    if (extension_window_controller) {
      *controller = extension_window_controller;
    } else {
      // Otherwise get the focused or most recently added window.
      *controller = extensions::WindowControllerList::GetInstance()
                        ->CurrentWindowForFunctionWithFilter(function, filter);
    }
    if (!(*controller)) {
      *error = extensions::tabs_constants::kNoCurrentWindowError;
      return false;
    }
  } else {
    *controller =
        extensions::WindowControllerList::GetInstance()
            ->FindWindowForFunctionByIdWithFilter(function, window_id, filter);
    if (!(*controller)) {
      *error = extensions::ErrorUtils::FormatErrorMessage(
          extensions::tabs_constants::kWindowNotFoundError,
          base::IntToString(window_id));
      return false;
    }
  }
  return true;
}

bool CanOperateOnWindow(const UIThreadExtensionFunction* function,
                        const extensions::WindowController* controller,
                        extensions::WindowController::TypeFilter filter) {
  if (filter && !controller->MatchesFilter(filter))
    return false;

  if (!filter && function->extension() &&
      !controller->IsVisibleToExtension(function->extension()))
    return false;

  if (function->browser_context() == controller->profile())
    return true;

  if (!function->include_incognito())
    return false;

  Profile* profile = Profile::FromBrowserContext(function->browser_context());
  return profile->HasOffTheRecordProfile() &&
         profile->GetOffTheRecordProfile() == controller->profile();
}

}  // namespace windows_util
