// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_api.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
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
  // GetList() can immediately call GotList() (which would Respond() before
  // RespondLater()). So we post a task to preserve order.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&PasswordsPrivateGetSavedPasswordListFunction::GetList, this));
  return RespondLater();
}

void PasswordsPrivateGetSavedPasswordListFunction::GetList() {
  PasswordsPrivateDelegate* delegate =
      PasswordsPrivateDelegateFactory::GetForBrowserContext(browser_context(),
                                                            true /* create */);
  delegate->GetSavedPasswordsList(
      base::Bind(&PasswordsPrivateGetSavedPasswordListFunction::GotList, this));
}

void PasswordsPrivateGetSavedPasswordListFunction::GotList(
    const PasswordsPrivateDelegate::UiEntries& list) {
  Respond(ArgumentList(
      api::passwords_private::GetSavedPasswordList::Results::Create(list)));
}

////////////////////////////////////////////////////////////////////////////////
// PasswordsPrivateGetPasswordExceptionListFunction

PasswordsPrivateGetPasswordExceptionListFunction::
    ~PasswordsPrivateGetPasswordExceptionListFunction() {}

ExtensionFunction::ResponseAction
PasswordsPrivateGetPasswordExceptionListFunction::Run() {
  // GetList() can immediately call GotList() (which would Respond() before
  // RespondLater()). So we post a task to preserve order.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&PasswordsPrivateGetPasswordExceptionListFunction::GetList,
                 this));
  return RespondLater();
}

void PasswordsPrivateGetPasswordExceptionListFunction::GetList() {
  PasswordsPrivateDelegate* delegate =
      PasswordsPrivateDelegateFactory::GetForBrowserContext(browser_context(),
                                                            true /* create */);
  delegate->GetPasswordExceptionsList(base::Bind(
      &PasswordsPrivateGetPasswordExceptionListFunction::GotList, this));
}

void PasswordsPrivateGetPasswordExceptionListFunction::GotList(
    const PasswordsPrivateDelegate::ExceptionPairs& list) {
  Respond(ArgumentList(
      api::passwords_private::GetPasswordExceptionList::Results::Create(list)));
}

}  // namespace extensions
