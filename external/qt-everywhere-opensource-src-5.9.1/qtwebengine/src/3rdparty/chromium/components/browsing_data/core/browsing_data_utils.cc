// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/browsing_data_utils.h"

#include "components/browsing_data/core/counters/autofill_counter.h"
#include "components/browsing_data/core/counters/history_counter.h"
#include "components/browsing_data/core/counters/passwords_counter.h"
#include "components/browsing_data/core/pref_names.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace browsing_data {

base::Time CalculateBeginDeleteTime(TimePeriod time_period) {
  base::TimeDelta diff;
  base::Time delete_begin_time = base::Time::Now();
  switch (time_period) {
    case LAST_HOUR:
      diff = base::TimeDelta::FromHours(1);
      break;
    case LAST_DAY:
      diff = base::TimeDelta::FromHours(24);
      break;
    case LAST_WEEK:
      diff = base::TimeDelta::FromHours(7 * 24);
      break;
    case FOUR_WEEKS:
      diff = base::TimeDelta::FromHours(4 * 7 * 24);
      break;
    case ALL_TIME:
      delete_begin_time = base::Time();
      break;
  }
  return delete_begin_time - diff;
}

base::string16 GetCounterTextFromResult(
    const browsing_data::BrowsingDataCounter::Result* result) {
  base::string16 text;
  std::string pref_name = result->source()->GetPrefName();

  if (!result->Finished()) {
    // The counter is still counting.
    text = l10n_util::GetStringUTF16(IDS_CLEAR_BROWSING_DATA_CALCULATING);

  } else if (pref_name == browsing_data::prefs::kDeletePasswords ||
             pref_name == browsing_data::prefs::kDeleteDownloadHistory) {
    // Counters with trivially formatted result: passwords and downloads.
    browsing_data::BrowsingDataCounter::ResultInt count =
        static_cast<const browsing_data::BrowsingDataCounter::FinishedResult*>(
            result)
            ->Value();
    text = l10n_util::GetPluralStringFUTF16(
        pref_name == browsing_data::prefs::kDeletePasswords
            ? IDS_DEL_PASSWORDS_COUNTER
            : IDS_DEL_DOWNLOADS_COUNTER,
        count);
  } else if (pref_name == browsing_data::prefs::kDeleteBrowsingHistory) {
    // History counter.
    const browsing_data::HistoryCounter::HistoryResult* history_result =
        static_cast<const browsing_data::HistoryCounter::HistoryResult*>(
            result);
    browsing_data::BrowsingDataCounter::ResultInt local_item_count =
        history_result->Value();
    bool has_synced_visits = history_result->has_synced_visits();

    text = has_synced_visits
               ? l10n_util::GetPluralStringFUTF16(
                     IDS_DEL_BROWSING_HISTORY_COUNTER_SYNCED, local_item_count)
               : l10n_util::GetPluralStringFUTF16(
                     IDS_DEL_BROWSING_HISTORY_COUNTER, local_item_count);

  } else if (pref_name == browsing_data::prefs::kDeleteFormData) {
    // Autofill counter.
    const browsing_data::AutofillCounter::AutofillResult* autofill_result =
        static_cast<const browsing_data::AutofillCounter::AutofillResult*>(
            result);
    browsing_data::AutofillCounter::ResultInt num_suggestions =
        autofill_result->Value();
    browsing_data::AutofillCounter::ResultInt num_credit_cards =
        autofill_result->num_credit_cards();
    browsing_data::AutofillCounter::ResultInt num_addresses =
        autofill_result->num_addresses();

    std::vector<base::string16> displayed_strings;

    if (num_credit_cards) {
      displayed_strings.push_back(l10n_util::GetPluralStringFUTF16(
          IDS_DEL_AUTOFILL_COUNTER_CREDIT_CARDS, num_credit_cards));
    }
    if (num_addresses) {
      displayed_strings.push_back(l10n_util::GetPluralStringFUTF16(
          IDS_DEL_AUTOFILL_COUNTER_ADDRESSES, num_addresses));
    }
    if (num_suggestions) {
      // We use a different wording for autocomplete suggestions based on the
      // length of the entire string.
      switch (displayed_strings.size()) {
        case 0:
          displayed_strings.push_back(l10n_util::GetPluralStringFUTF16(
              IDS_DEL_AUTOFILL_COUNTER_SUGGESTIONS, num_suggestions));
          break;
        case 1:
          displayed_strings.push_back(l10n_util::GetPluralStringFUTF16(
              IDS_DEL_AUTOFILL_COUNTER_SUGGESTIONS_LONG, num_suggestions));
          break;
        case 2:
          displayed_strings.push_back(l10n_util::GetPluralStringFUTF16(
              IDS_DEL_AUTOFILL_COUNTER_SUGGESTIONS_SHORT, num_suggestions));
          break;
        default:
          NOTREACHED();
      }
    }

    // Construct the resulting string from the sections in |displayed_strings|.
    switch (displayed_strings.size()) {
      case 0:
        text = l10n_util::GetStringUTF16(IDS_DEL_AUTOFILL_COUNTER_EMPTY);
        break;
      case 1:
        text = displayed_strings[0];
        break;
      case 2:
        text = l10n_util::GetStringFUTF16(IDS_DEL_AUTOFILL_COUNTER_TWO_TYPES,
                                          displayed_strings[0],
                                          displayed_strings[1]);
        break;
      case 3:
        text = l10n_util::GetStringFUTF16(
            IDS_DEL_AUTOFILL_COUNTER_THREE_TYPES, displayed_strings[0],
            displayed_strings[1], displayed_strings[2]);
        break;
      default:
        NOTREACHED();
    }
  }

  return text;
}

bool GetDeletionPreferenceFromDataType(
    BrowsingDataType data_type,
    std::string* out_pref) {
  switch (data_type) {
    case HISTORY:
      *out_pref = prefs::kDeleteBrowsingHistory;
      return true;
    case CACHE:
      *out_pref = prefs::kDeleteCache;
      return true;
    case COOKIES:
      *out_pref = prefs::kDeleteCookies;
      return true;
    case PASSWORDS:
      *out_pref = prefs::kDeletePasswords;
      return true;
    case FORM_DATA:
      *out_pref = prefs::kDeleteFormData;
      return true;
    case BOOKMARKS:
      // Bookmarks are deleted on the Android side. No corresponding deletion
      // preference.
      return false;
    case NUM_TYPES:
      // This is not an actual type.
      NOTREACHED();
      return false;
  }
  NOTREACHED();
  return false;
}

}  // namespace browsing_data
