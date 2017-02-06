// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_DELEGATE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list_threadsafe.h"
#include "chrome/browser/ui/passwords/password_manager_presenter.h"
#include "chrome/browser/ui/passwords/password_ui_view.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_function.h"

class Profile;

namespace base {
class Value;
}

namespace content {
class WebContents;
}

namespace extensions {

// Delegate used by the chrome.passwordsPrivate API to facilitate removing saved
// passwords and password exceptions and to notify listeners when these values
// have changed.
class PasswordsPrivateDelegate : public KeyedService {
 public:
  ~PasswordsPrivateDelegate() override {}

  // Sends the saved passwords list to the event router.
  virtual void SendSavedPasswordsList() = 0;

  // Gets the saved passwords list.
  virtual const std::vector<api::passwords_private::PasswordUiEntry>*
  GetSavedPasswordsList() const = 0;

  // Sends the password exceptions list to the event router.
  virtual void SendPasswordExceptionsList() = 0;

  // Gets the password exceptions list.
  virtual const std::vector<api::passwords_private::ExceptionPair>*
  GetPasswordExceptionsList() const = 0;

  // Removes the saved password entry corresponding to |origin_url| and
  // |username|.
  // |origin_url| The human-readable origin for the URL where the password is
  //     used/ should be obtained using GetHumanReadableOrigin().
  // |username| The username used in conjunction with the saved password.
  virtual void RemoveSavedPassword(
      const std::string& origin_url, const std::string& username) = 0;

  // Removes the saved password exception entry corresponding to
  // |exception_url|.
  // |exception_url| The URL corresponding to the exception to remove; should
  //     be obtained using GetHumanReadableOrigin().
  virtual void RemovePasswordException(const std::string& exception_url) = 0;

  // Requests the plain text password for entry corresponding to |origin_url|
  // and |username|.
  // |origin_url| The human-readable origin for the URL where the password is
  //     used; should be obtained using GetHumanReadableOrigin().
  // |username| The username used in conjunction with the saved password.
  // |native_window| The Chrome host window; will be used to show an OS-level
  //     authentication dialog if necessary.
  virtual void RequestShowPassword(const std::string& origin_url,
                                   const std::string& username,
                                   content::WebContents* web_contents) = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_DELEGATE_H_
