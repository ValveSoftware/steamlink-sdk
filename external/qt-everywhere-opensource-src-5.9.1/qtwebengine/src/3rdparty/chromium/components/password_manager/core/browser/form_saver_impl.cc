// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_saver_impl.h"

#include <memory>
#include <vector>

#include "base/auto_reset.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_store.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "url/gurl.h"
#include "url/origin.h"

using autofill::PasswordForm;

namespace password_manager {

FormSaverImpl::FormSaverImpl(PasswordStore* store) : store_(store) {
  DCHECK(store);
}

FormSaverImpl::~FormSaverImpl() = default;

void FormSaverImpl::PermanentlyBlacklist(PasswordForm* observed) {
  observed->preferred = false;
  observed->blacklisted_by_user = true;
  observed->other_possible_usernames.clear();
  observed->date_created = base::Time::Now();

  store_->AddLogin(*observed);
}

void FormSaverImpl::Save(
    const PasswordForm& pending,
    const std::map<base::string16, const PasswordForm*>& best_matches,
    const PasswordForm* old_primary_key) {
  SaveImpl(pending, true, best_matches, nullptr, old_primary_key);
}

void FormSaverImpl::Update(
    const PasswordForm& pending,
    const std::map<base::string16, const PasswordForm*>& best_matches,
    const std::vector<PasswordForm>* credentials_to_update,
    const PasswordForm* old_primary_key) {
  SaveImpl(pending, false, best_matches, credentials_to_update,
           old_primary_key);
}

void FormSaverImpl::PresaveGeneratedPassword(const PasswordForm& generated) {
  if (presaved_)
    store_->UpdateLoginWithPrimaryKey(generated, *presaved_);
  else
    store_->AddLogin(generated);
  presaved_.reset(new PasswordForm(generated));
}

void FormSaverImpl::RemovePresavedPassword() {
  if (!presaved_)
    return;

  store_->RemoveLogin(*presaved_);
  presaved_ = nullptr;
}

void FormSaverImpl::WipeOutdatedCopies(
    const PasswordForm& pending,
    std::map<base::string16, const PasswordForm*>* best_matches,
    const PasswordForm** preferred_match) {
  DCHECK(preferred_match);  // Note: *preferred_match may still be null.
  DCHECK(url::Origin(GURL(pending.signon_realm))
             .IsSameOriginWith(
                 url::Origin(GaiaUrls::GetInstance()->gaia_url().GetOrigin())))
      << pending.signon_realm << " is not a GAIA origin";

  for (auto it = best_matches->begin(); it != best_matches->end();
       /* increment inside the for loop */) {
    if ((pending.password_value != it->second->password_value) &&
        gaia::AreEmailsSame(base::UTF16ToUTF8(pending.username_value),
                            base::UTF16ToUTF8(it->second->username_value))) {
      if (it->second == *preferred_match)
        *preferred_match = nullptr;
      store_->RemoveLogin(*it->second);
      it = best_matches->erase(it);
    } else {
      ++it;
    }
  }
}

void FormSaverImpl::SaveImpl(
    const PasswordForm& pending,
    bool is_new_login,
    const std::map<base::string16, const PasswordForm*>& best_matches,
    const std::vector<PasswordForm>* credentials_to_update,
    const PasswordForm* old_primary_key) {
  DCHECK(pending.preferred);
  DCHECK(!pending.blacklisted_by_user);

  base::AutoReset<const std::map<base::string16, const PasswordForm*>*> ar1(
      &best_matches_, &best_matches);
  base::AutoReset<const PasswordForm*> ar2(&pending_, &pending);

  UpdatePreferredLoginState();
  if (presaved_) {
    store_->UpdateLoginWithPrimaryKey(*pending_, *presaved_);
    presaved_ = nullptr;
  } else if (is_new_login) {
    store_->AddLogin(*pending_);
    if (!pending_->username_value.empty())
      DeleteEmptyUsernameCredentials();
  } else {
    if (old_primary_key)
      store_->UpdateLoginWithPrimaryKey(*pending_, *old_primary_key);
    else
      store_->UpdateLogin(*pending_);
  }

  if (credentials_to_update) {
    for (const PasswordForm& credential : *credentials_to_update) {
      store_->UpdateLogin(credential);
    }
  }
}

void FormSaverImpl::UpdatePreferredLoginState() {
  const base::string16& preferred_username = pending_->username_value;
  for (const auto& key_value_pair : *best_matches_) {
    const PasswordForm& form = *key_value_pair.second;
    if (form.preferred && !form.is_public_suffix_match &&
        form.username_value != preferred_username) {
      // This wasn't the selected login but it used to be preferred.
      PasswordForm update(form);
      update.preferred = false;
      store_->UpdateLogin(update);
    }
  }
}

void FormSaverImpl::DeleteEmptyUsernameCredentials() {
  DCHECK(!pending_->username_value.empty());

  for (const auto& match : *best_matches_) {
    const PasswordForm* form = match.second;
    if (!form->is_public_suffix_match && form->username_value.empty() &&
        form->password_value == pending_->password_value) {
      store_->RemoveLogin(*form);
    }
  }
}

}  // namespace password_manager
