// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"

#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "content/public/browser/browser_context.h"

namespace extensions {

PasswordsPrivateEventRouter::PasswordsPrivateEventRouter(
    content::BrowserContext* context)
    : context_(context),
      event_router_(nullptr) {
  event_router_ = EventRouter::Get(context_);
}

PasswordsPrivateEventRouter::~PasswordsPrivateEventRouter() {}

void PasswordsPrivateEventRouter::OnSavedPasswordsListChanged(
    const std::vector<api::passwords_private::PasswordUiEntry>& entries) {
  cached_saved_password_parameters_ =
      api::passwords_private::OnSavedPasswordsListChanged::Create(entries);
  SendSavedPasswordListToListeners();
}

void PasswordsPrivateEventRouter::SendSavedPasswordListToListeners() {
  if (!cached_saved_password_parameters_.get())
    // If there is nothing to send, return early.
    return;

  std::unique_ptr<Event> extension_event(
      new Event(events::PASSWORDS_PRIVATE_ON_SAVED_PASSWORDS_LIST_CHANGED,
                api::passwords_private::OnSavedPasswordsListChanged::kEventName,
                cached_saved_password_parameters_->CreateDeepCopy()));
  event_router_->BroadcastEvent(std::move(extension_event));
}

void PasswordsPrivateEventRouter::OnPasswordExceptionsListChanged(
    const std::vector<api::passwords_private::ExceptionPair>& exceptions) {
  cached_password_exception_parameters_ =
      api::passwords_private::OnPasswordExceptionsListChanged::Create(
          exceptions);
  SendPasswordExceptionListToListeners();
}

void PasswordsPrivateEventRouter::SendPasswordExceptionListToListeners() {
  if (!cached_password_exception_parameters_.get())
    // If there is nothing to send, return early.
    return;

  std::unique_ptr<Event> extension_event(new Event(
      events::PASSWORDS_PRIVATE_ON_PASSWORD_EXCEPTIONS_LIST_CHANGED,
      api::passwords_private::OnPasswordExceptionsListChanged::kEventName,
      cached_password_exception_parameters_->CreateDeepCopy()));
  event_router_->BroadcastEvent(std::move(extension_event));
}

void PasswordsPrivateEventRouter::OnPlaintextPasswordFetched(
        const std::string& origin_url,
        const std::string& username,
        const std::string& plaintext_password) {
  api::passwords_private::PlaintextPasswordEventParameters params;
  params.login_pair.origin_url = origin_url;
  params.login_pair.username = username;
  params.plaintext_password = plaintext_password;

  std::unique_ptr<base::ListValue> event_value(new base::ListValue);
  event_value->Append(params.ToValue());

  std::unique_ptr<Event> extension_event(new Event(
      events::PASSWORDS_PRIVATE_ON_PLAINTEXT_PASSWORD_RETRIEVED,
      api::passwords_private::OnPlaintextPasswordRetrieved::kEventName,
      std::move(event_value)));
  event_router_->BroadcastEvent(std::move(extension_event));
}

PasswordsPrivateEventRouter* PasswordsPrivateEventRouter::Create(
    content::BrowserContext* context) {
  return new PasswordsPrivateEventRouter(context);
}

}  // namespace extensions
