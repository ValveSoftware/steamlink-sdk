// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_save_card_infobar_delegate_mobile.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/legal_message_line.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "grit/components_scaled_resources.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/vector_icons_public.h"
#include "url/gurl.h"

namespace autofill {

AutofillSaveCardInfoBarDelegateMobile::AutofillSaveCardInfoBarDelegateMobile(
    bool upload,
    const CreditCard& card,
    std::unique_ptr<base::DictionaryValue> legal_message,
    const base::Closure& save_card_callback)
    : ConfirmInfoBarDelegate(),
      upload_(upload),
      save_card_callback_(save_card_callback),
      had_user_interaction_(false),
#if defined(OS_IOS)
      // TODO(jdonnelly): Use credit card issuer images on iOS.
      // http://crbug.com/535784
      issuer_icon_id_(kNoIconID),
#else
      issuer_icon_id_(CreditCard::IconResourceId(card.type())),
#endif
      card_label_(base::string16(kMidlineEllipsis) + card.LastFourDigits()),
      card_sub_label_(card.AbbreviatedExpirationDateForDisplay()) {
  if (legal_message)
    LegalMessageLine::Parse(*legal_message, &legal_messages_);

  AutofillMetrics::LogCreditCardInfoBarMetric(AutofillMetrics::INFOBAR_SHOWN);
}

AutofillSaveCardInfoBarDelegateMobile::
    ~AutofillSaveCardInfoBarDelegateMobile() {
  if (!had_user_interaction_)
    LogUserAction(AutofillMetrics::INFOBAR_IGNORED);
}

void AutofillSaveCardInfoBarDelegateMobile::OnLegalMessageLinkClicked(
    GURL url) {
  infobar()->owner()->OpenURL(url, NEW_FOREGROUND_TAB);
}

int AutofillSaveCardInfoBarDelegateMobile::GetIconId() const {
  return IDR_INFOBAR_AUTOFILL_CC;
}

base::string16 AutofillSaveCardInfoBarDelegateMobile::GetMessageText() const {
  return l10n_util::GetStringUTF16(
      upload_ ? IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD
              : IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_LOCAL);
}

base::string16 AutofillSaveCardInfoBarDelegateMobile::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

infobars::InfoBarDelegate::Type
AutofillSaveCardInfoBarDelegateMobile::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

infobars::InfoBarDelegate::InfoBarIdentifier
AutofillSaveCardInfoBarDelegateMobile::GetIdentifier() const {
  return AUTOFILL_CC_INFOBAR_DELEGATE;
}

bool AutofillSaveCardInfoBarDelegateMobile::ShouldExpire(
    const NavigationDetails& details) const {
  // The user has submitted a form, causing the page to navigate elsewhere. We
  // don't want the infobar to be expired at this point, because the user won't
  // get a chance to answer the question.
  return false;
}

void AutofillSaveCardInfoBarDelegateMobile::InfoBarDismissed() {
  LogUserAction(AutofillMetrics::INFOBAR_DENIED);
}

base::string16 AutofillSaveCardInfoBarDelegateMobile::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(button == BUTTON_OK
                                       ? IDS_AUTOFILL_SAVE_CARD_PROMPT_ACCEPT
                                       : IDS_AUTOFILL_SAVE_CARD_PROMPT_DENY);
}

bool AutofillSaveCardInfoBarDelegateMobile::Accept() {
  save_card_callback_.Run();
  save_card_callback_.Reset();
  LogUserAction(AutofillMetrics::INFOBAR_ACCEPTED);
  return true;
}

bool AutofillSaveCardInfoBarDelegateMobile::Cancel() {
  LogUserAction(AutofillMetrics::INFOBAR_DENIED);
  return true;
}

GURL AutofillSaveCardInfoBarDelegateMobile::GetLinkURL() const {
  return GURL(kHelpURL);
}

void AutofillSaveCardInfoBarDelegateMobile::LogUserAction(
    AutofillMetrics::InfoBarMetric user_action) {
  DCHECK(!had_user_interaction_);

  AutofillMetrics::LogCreditCardInfoBarMetric(user_action);
  had_user_interaction_ = true;
}

}  // namespace autofill
