// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_api.h"

#include "base/values.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_factory.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "components/password_manager/core/common/experiments.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function_registry.h"

namespace extensions {

////////////////////////////////////////////////////////////////////////////////
// PasswordsPrivateRemoveSavedPasswordFunction

PasswordsPrivateRemoveSavedPasswordFunction::
    ~PasswordsPrivateRemoveSavedPasswordFunction() {}

ExtensionFunction::ResponseAction
    PasswordsPrivateRemoveSavedPasswordFunction::Run() {
  std::unique_ptr<api::passwords_private::RemoveSavedPassword::Params>
      parameters =
          api::passwords_private::RemoveSavedPassword::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  PasswordsPrivateDelegate* delegate =
      PasswordsPrivateDelegateFactory::GetForBrowserContext(browser_context(),
                                                            true /* create */);
  delegate->RemoveSavedPassword(
      parameters->login_pair.origin_url,
      parameters->login_pair.username);

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// PasswordsPrivateRemovePasswordExceptionFunction

PasswordsPrivateRemovePasswordExceptionFunction::
    ~PasswordsPrivateRemovePasswordExceptionFunction() {}

ExtensionFunction::ResponseAction
    PasswordsPrivateRemovePasswordExceptionFunction::Run() {
  std::unique_ptr<api::passwords_private::RemovePasswordException::Params>
      parameters =
          api::passwords_private::RemovePasswordException::Params::Create(
              *args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  PasswordsPrivateDelegate* delegate =
      PasswordsPrivateDelegateFactory::GetForBrowserContext(browser_context(),
                                                            true /* create */);
  delegate->RemovePasswordException(parameters->exception_url);

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// PasswordsPrivateRequestPlaintextPasswordFunction

PasswordsPrivateRequestPlaintextPasswordFunction::
    ~PasswordsPrivateRequestPlaintextPasswordFunction() {}

ExtensionFunction::ResponseAction
    PasswordsPrivateRequestPlaintextPasswordFunction::Run() {
  std::unique_ptr<api::passwords_private::RequestPlaintextPassword::Params>
      parameters =
          api::passwords_private::RequestPlaintextPassword::Params::Create(
              *args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  PasswordsPrivateDelegate* delegate =
      PasswordsPrivateDelegateFactory::GetForBrowserContext(browser_context(),
                                                            true /* create */);

  delegate->RequestShowPassword(parameters->login_pair.origin_url,
                                parameters->login_pair.username,
                                GetSenderWebContents());

  // No response given from this API function; instead, listeners wait for the
  // chrome.passwordsPrivate.onPlaintextPasswordRetrieved event to fire.
  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// PasswordsPrivateGetSavedPasswordListFunction

PasswordsPrivateGetSavedPasswordListFunction::
    ~PasswordsPrivateGetSavedPasswordListFunction() {}

ExtensionFunction::ResponseAction
PasswordsPrivateGetSavedPasswordListFunction::Run() {
  PasswordsPrivateDelegate* delegate =
      PasswordsPrivateDelegateFactory::GetForBrowserContext(browser_context(),
                                                            true /* create */);
  return RespondNow(ArgumentList(
      api::passwords_private::GetSavedPasswordList::Results::Create(
          *(delegate->GetSavedPasswordsList()))));
}

////////////////////////////////////////////////////////////////////////////////
// PasswordsPrivateGetPasswordExceptionListFunction

PasswordsPrivateGetPasswordExceptionListFunction::
    ~PasswordsPrivateGetPasswordExceptionListFunction() {}

ExtensionFunction::ResponseAction
PasswordsPrivateGetPasswordExceptionListFunction::Run() {
  PasswordsPrivateDelegate* delegate =
      PasswordsPrivateDelegateFactory::GetForBrowserContext(browser_context(),
                                                            true /* create */);
  return RespondNow(ArgumentList(
      api::passwords_private::GetPasswordExceptionList::Results::Create(
          *(delegate->GetPasswordExceptionsList()))));
}

}  // namespace extensions
