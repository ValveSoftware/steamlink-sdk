// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user_error_page/supervised_user_error_page.h"

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "grit/components_resources.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"

namespace supervised_user_error_page {

namespace {

static const int kAvatarSize1x = 45;
static const int kAvatarSize2x = 90;

#if defined(GOOGLE_CHROME_BUILD)
bool ReasonIsAutomatic(FilteringBehaviorReason reason) {
  return reason == ASYNC_CHECKER || reason == BLACKLIST;
}
#endif

std::string BuildAvatarImageUrl(const std::string& url, int size) {
  std::string result = url;
  size_t slash = result.rfind('/');
  if (slash != std::string::npos)
    result.insert(slash, "/s" + base::IntToString(size));
  return result;
}

int GetBlockHeaderID(FilteringBehaviorReason reason) {
  switch (reason) {
    case DEFAULT:
      return IDS_SUPERVISED_USER_BLOCK_HEADER_DEFAULT;
    case BLACKLIST:
    case ASYNC_CHECKER:
      return IDS_SUPERVISED_USER_BLOCK_HEADER_SAFE_SITES;
    case WHITELIST:
      NOTREACHED();
      break;
    case MANUAL:
      return IDS_SUPERVISED_USER_BLOCK_HEADER_MANUAL;
  }
  NOTREACHED();
  return 0;
}
}  //  namespace

int GetBlockMessageID(FilteringBehaviorReason reason,
                      bool is_child_account,
                      bool single_parent) {
  switch (reason) {
    case DEFAULT:
      if (!is_child_account)
        return IDS_SUPERVISED_USER_BLOCK_MESSAGE_DEFAULT;
      if (single_parent)
        return IDS_CHILD_BLOCK_MESSAGE_DEFAULT_SINGLE_PARENT;
      return IDS_CHILD_BLOCK_MESSAGE_DEFAULT_MULTI_PARENT;
    case BLACKLIST:
    case ASYNC_CHECKER:
      return IDS_SUPERVISED_USER_BLOCK_MESSAGE_SAFE_SITES;
    case WHITELIST:
      NOTREACHED();
      break;
    case MANUAL:
      if (!is_child_account)
        return IDS_SUPERVISED_USER_BLOCK_MESSAGE_MANUAL;
      if (single_parent)
        return IDS_CHILD_BLOCK_MESSAGE_MANUAL_SINGLE_PARENT;
      return IDS_CHILD_BLOCK_MESSAGE_MANUAL_MULTI_PARENT;
  }
  NOTREACHED();
  return 0;
}

std::string BuildHtml(bool allow_access_requests,
                      const std::string& profile_image_url,
                      const std::string& profile_image_url2,
                      const base::string16& custodian,
                      const base::string16& custodian_email,
                      const base::string16& second_custodian,
                      const base::string16& second_custodian_email,
                      bool is_child_account,
                      FilteringBehaviorReason reason,
                      const std::string& app_locale) {
  base::DictionaryValue strings;
  strings.SetString("blockPageTitle",
                    l10n_util::GetStringUTF16(IDS_BLOCK_INTERSTITIAL_TITLE));
  strings.SetBoolean("allowAccessRequests", allow_access_requests);
  strings.SetString("avatarURL1x",
                    BuildAvatarImageUrl(profile_image_url, kAvatarSize1x));
  strings.SetString("avatarURL2x",
                    BuildAvatarImageUrl(profile_image_url, kAvatarSize2x));
  strings.SetString("secondAvatarURL1x",
                    BuildAvatarImageUrl(profile_image_url2, kAvatarSize1x));
  strings.SetString("secondAvatarURL2x",
                    BuildAvatarImageUrl(profile_image_url2, kAvatarSize2x));
  strings.SetString("custodianName", custodian);
  strings.SetString("custodianEmail", custodian_email);
  strings.SetString("secondCustodianName", second_custodian);
  strings.SetString("secondCustodianEmail", second_custodian_email);
  base::string16 block_message;
  if (allow_access_requests) {
    if (is_child_account) {
      block_message = l10n_util::GetStringUTF16(
          second_custodian.empty()
              ? IDS_CHILD_BLOCK_INTERSTITIAL_MESSAGE_SINGLE_PARENT
              : IDS_CHILD_BLOCK_INTERSTITIAL_MESSAGE_MULTI_PARENT);
    } else {
      block_message =
          l10n_util::GetStringFUTF16(IDS_BLOCK_INTERSTITIAL_MESSAGE, custodian);
    }
  } else {
    block_message = l10n_util::GetStringUTF16(
        IDS_BLOCK_INTERSTITIAL_MESSAGE_ACCESS_REQUESTS_DISABLED);
  }
  strings.SetString("blockPageMessage", block_message);
  strings.SetString("blockReasonMessage",
                    l10n_util::GetStringUTF16(GetBlockMessageID(
                        reason, is_child_account, second_custodian.empty())));
  strings.SetString("blockReasonHeader",
                    l10n_util::GetStringUTF16(GetBlockHeaderID(reason)));
  bool show_feedback = false;
#if defined(GOOGLE_CHROME_BUILD)
  show_feedback = is_child_account && ReasonIsAutomatic(reason);
#endif
  strings.SetBoolean("showFeedbackLink", show_feedback);
  strings.SetString("feedbackLink", l10n_util::GetStringUTF16(
                                        IDS_BLOCK_INTERSTITIAL_SEND_FEEDBACK));
  strings.SetString("backButton", l10n_util::GetStringUTF16(IDS_BACK_BUTTON));
  strings.SetString(
      "requestAccessButton",
      l10n_util::GetStringUTF16(IDS_BLOCK_INTERSTITIAL_REQUEST_ACCESS_BUTTON));
  strings.SetString(
      "showDetailsLink",
      l10n_util::GetStringUTF16(IDS_BLOCK_INTERSTITIAL_SHOW_DETAILS));
  strings.SetString(
      "hideDetailsLink",
      l10n_util::GetStringUTF16(IDS_BLOCK_INTERSTITIAL_HIDE_DETAILS));
  base::string16 request_sent_message;
  base::string16 request_failed_message;
  if (is_child_account) {
    if (second_custodian.empty()) {
      request_sent_message = l10n_util::GetStringUTF16(
          IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_SENT_MESSAGE_SINGLE_PARENT);
      request_failed_message = l10n_util::GetStringUTF16(
          IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_FAILED_MESSAGE_SINGLE_PARENT);
    } else {
      request_sent_message = l10n_util::GetStringUTF16(
          IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_SENT_MESSAGE_MULTI_PARENT);
      request_failed_message = l10n_util::GetStringUTF16(
          IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_FAILED_MESSAGE_MULTI_PARENT);
    }
  } else {
    request_sent_message = l10n_util::GetStringFUTF16(
        IDS_BLOCK_INTERSTITIAL_REQUEST_SENT_MESSAGE, custodian);
    request_failed_message = l10n_util::GetStringFUTF16(
        IDS_BLOCK_INTERSTITIAL_REQUEST_FAILED_MESSAGE, custodian);
  }
  strings.SetString("requestSentMessage", request_sent_message);
  strings.SetString("requestFailedMessage", request_failed_message);
  webui::SetLoadTimeDataDefaults(app_locale, &strings);
  std::string html =
      ResourceBundle::GetSharedInstance()
          .GetRawDataResource(IDR_SUPERVISED_USER_BLOCK_INTERSTITIAL_HTML)
          .as_string();
  webui::AppendWebUiCssTextDefaults(&html);
  return webui::GetI18nTemplateHtml(html, &strings);
}

}  //  namespace supervised_user_error_page
