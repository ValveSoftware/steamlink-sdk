// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/affiliation_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

std::string LoginPairToMapKey(
    const std::string& origin_url, const std::string& username) {
  // Concatenate origin URL and username to form a unique key.
  return origin_url + ',' + username;
}

}

namespace extensions {

PasswordsPrivateDelegateImpl::PasswordsPrivateDelegateImpl(Profile* profile)
    : profile_(profile),
      password_manager_presenter_(new PasswordManagerPresenter(this)),
      set_password_list_called_(false),
      set_password_exception_list_called_(false),
      is_initialized_(false),
      web_contents_(nullptr) {
  password_manager_presenter_->Initialize();
  password_manager_presenter_->UpdatePasswordLists();
}

PasswordsPrivateDelegateImpl::~PasswordsPrivateDelegateImpl() {}

void PasswordsPrivateDelegateImpl::SendSavedPasswordsList() {
  PasswordsPrivateEventRouter* router =
      PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
  if (router)
    router->OnSavedPasswordsListChanged(current_entries_);
}

const std::vector<api::passwords_private::PasswordUiEntry>*
PasswordsPrivateDelegateImpl::GetSavedPasswordsList() const {
  return &current_entries_;
}

void PasswordsPrivateDelegateImpl::SendPasswordExceptionsList() {
  PasswordsPrivateEventRouter* router =
      PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
  if (router)
    router->OnPasswordExceptionsListChanged(current_exceptions_);
}

const std::vector<api::passwords_private::ExceptionPair>*
PasswordsPrivateDelegateImpl::GetPasswordExceptionsList() const {
  return &current_exceptions_;
}

void PasswordsPrivateDelegateImpl::RemoveSavedPassword(
    const std::string& origin_url, const std::string& username) {
  ExecuteFunction(base::Bind(
      &PasswordsPrivateDelegateImpl::RemoveSavedPasswordInternal,
      base::Unretained(this),
      origin_url,
      username));
}

void PasswordsPrivateDelegateImpl::RemoveSavedPasswordInternal(
    const std::string& origin_url, const std::string& username) {
  std::string key = LoginPairToMapKey(origin_url, username);
  if (login_pair_to_index_map_.find(key) == login_pair_to_index_map_.end()) {
    // If the URL/username pair does not exist in the map, do nothing.
    return;
  }

  password_manager_presenter_->RemoveSavedPassword(
      login_pair_to_index_map_[key]);
}

void PasswordsPrivateDelegateImpl::RemovePasswordException(
    const std::string& exception_url) {
  ExecuteFunction(base::Bind(
      &PasswordsPrivateDelegateImpl::RemovePasswordExceptionInternal,
      base::Unretained(this),
      exception_url));
}

void PasswordsPrivateDelegateImpl::RemovePasswordExceptionInternal(
    const std::string& exception_url) {
  if (exception_url_to_index_map_.find(exception_url) ==
      exception_url_to_index_map_.end()) {
    // If the exception URL does not exist in the map, do nothing.
    return;
  }

  password_manager_presenter_->RemovePasswordException(
      exception_url_to_index_map_[exception_url]);
}

void PasswordsPrivateDelegateImpl::RequestShowPassword(
    const std::string& origin_url,
    const std::string& username,
    content::WebContents* web_contents) {
  ExecuteFunction(
      base::Bind(&PasswordsPrivateDelegateImpl::RequestShowPasswordInternal,
                 base::Unretained(this), origin_url, username, web_contents));
}

void PasswordsPrivateDelegateImpl::RequestShowPasswordInternal(
    const std::string& origin_url,
    const std::string& username,
    content::WebContents* web_contents) {
  std::string key = LoginPairToMapKey(origin_url, username);
  if (login_pair_to_index_map_.find(key) == login_pair_to_index_map_.end()) {
    // If the URL/username pair does not exist in the map, do nothing.
    return;
  }

  // Save |web_contents| so that the call to RequestShowPassword() below
  // can use this value by calling GetNativeWindow(). Note: This is safe because
  // GetNativeWindow() will only be called immediately from
  // RequestShowPassword().
  // TODO(stevenjb): Pass this directly to RequestShowPassword(); see
  // crbug.com/495290.
  web_contents_ = web_contents;

  // Request the password. When it is retrieved, ShowPassword() will be called.
  password_manager_presenter_->RequestShowPassword(
      login_pair_to_index_map_[key]);
}

Profile* PasswordsPrivateDelegateImpl::GetProfile() {
  return profile_;
}

void PasswordsPrivateDelegateImpl::ShowPassword(
    size_t index,
    const std::string& origin_url,
    const std::string& username,
    const base::string16& password_value) {
  PasswordsPrivateEventRouter* router =
      PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
  if (router) {
    router->OnPlaintextPasswordFetched(origin_url, username,
                                       base::UTF16ToUTF8(password_value));
  }
}

void PasswordsPrivateDelegateImpl::SetPasswordList(
    const std::vector<std::unique_ptr<autofill::PasswordForm>>& password_list) {
  // Rebuild |login_pair_to_index_map_| so that it reflects the contents of the
  // new list.
  login_pair_to_index_map_.clear();
  for (size_t i = 0; i < password_list.size(); i++) {
    std::string key = LoginPairToMapKey(
        password_manager::GetHumanReadableOrigin(*password_list[i]),
        base::UTF16ToUTF8(password_list[i]->username_value));
    login_pair_to_index_map_[key] = i;
  }

  // Now, create a list of PasswordUiEntry objects to send to observers.
  current_entries_.clear();
  for (const auto& form : password_list) {
    api::passwords_private::PasswordUiEntry entry;
    entry.login_pair.origin_url =
        password_manager::GetHumanReadableOrigin(*form);
    entry.login_pair.username = base::UTF16ToUTF8(form->username_value);
    entry.link_url = form->origin.spec();
    entry.num_characters_in_password = form->password_value.length();

    if (!form->federation_origin.unique()) {
      entry.federation_text.reset(new std::string(l10n_util::GetStringFUTF8(
          IDS_PASSWORDS_VIA_FEDERATION,
          base::UTF8ToUTF16(form->federation_origin.host()))));
    }

    current_entries_.push_back(std::move(entry));
  }

  SendSavedPasswordsList();

  set_password_list_called_ = true;
  InitializeIfNecessary();
}

void PasswordsPrivateDelegateImpl::SetPasswordExceptionList(
    const std::vector<std::unique_ptr<autofill::PasswordForm>>&
        password_exception_list) {
  // Rebuild |exception_url_to_index_map_| so that it reflects the contents of
  // the new list.
  exception_url_to_index_map_.clear();
  for (size_t i = 0; i < password_exception_list.size(); i++) {
    std::string key = password_manager::GetHumanReadableOrigin(
        *password_exception_list[i]);
    exception_url_to_index_map_[key] = i;
  }

  // Now, create a list of exceptions to send to observers.
  current_exceptions_.clear();
  for (const auto& form : password_exception_list) {
    api::passwords_private::ExceptionPair pair;
    pair.exception_url = password_manager::GetHumanReadableOrigin(*form);
    pair.link_url = form->origin.spec();
    current_exceptions_.push_back(std::move(pair));
  }

  SendPasswordExceptionsList();

  set_password_exception_list_called_ = true;
  InitializeIfNecessary();
}

#if !defined(OS_ANDROID)
gfx::NativeWindow PasswordsPrivateDelegateImpl::GetNativeWindow() const {
  DCHECK(web_contents_);
  return web_contents_->GetTopLevelNativeWindow();
}
#endif

void PasswordsPrivateDelegateImpl::Shutdown() {
  password_manager_presenter_.reset();
}

void PasswordsPrivateDelegateImpl::ExecuteFunction(
    const base::Callback<void()>& callback) {
  if (is_initialized_) {
    callback.Run();
    return;
  }

  pre_initialization_callbacks_.push_back(callback);
}

void PasswordsPrivateDelegateImpl::InitializeIfNecessary() {
  if (is_initialized_ ||
      !set_password_list_called_ ||
      !set_password_exception_list_called_)
    return;

  is_initialized_ = true;

  for (const base::Callback<void()>& callback : pre_initialization_callbacks_) {
    callback.Run();
  }
}

}  // namespace extensions
