// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/ssl_error_ui.h"

#include "base/i18n/time_formatting.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/ssl_errors/error_classification.h"
#include "components/ssl_errors/error_info.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace security_interstitials {
namespace {

// URL for help page.
const char kHelpURL[] = "https://support.google.com/chrome/answer/4454607";

bool IsMasked(int options, SSLErrorUI::SSLErrorOptionsMask mask) {
  return ((options & mask) != 0);
}

}  // namespace

SSLErrorUI::SSLErrorUI(const GURL& request_url,
                       int cert_error,
                       const net::SSLInfo& ssl_info,
                       int display_options,
                       const base::Time& time_triggered,
                       ControllerClient* controller)
    : request_url_(request_url),
      cert_error_(cert_error),
      ssl_info_(ssl_info),
      time_triggered_(time_triggered),
      requested_strict_enforcement_(
          IsMasked(display_options, STRICT_ENFORCEMENT)),
      soft_override_enabled_(IsMasked(display_options, SOFT_OVERRIDE_ENABLED)),
      hard_override_enabled_(
          !IsMasked(display_options, HARD_OVERRIDE_DISABLED)),
      controller_(controller),
      user_made_decision_(false) {
  controller_->metrics_helper()->RecordUserDecision(MetricsHelper::SHOW);
  controller_->metrics_helper()->RecordUserInteraction(
      MetricsHelper::TOTAL_VISITS);
  ssl_errors::RecordUMAStatistics(soft_override_enabled_, time_triggered_,
                                  request_url, cert_error_,
                                  *ssl_info_.cert.get());
}

SSLErrorUI::~SSLErrorUI() {
  // If the page is closing without an explicit decision, record it as not
  // proceeding.
  if (!user_made_decision_) {
    controller_->metrics_helper()->RecordUserDecision(
        MetricsHelper::DONT_PROCEED);
  }
  controller_->metrics_helper()->RecordShutdownMetrics();
}

void SSLErrorUI::PopulateStringsForHTML(base::DictionaryValue* load_time_data) {
  DCHECK(load_time_data);

  // Shared with other errors.
  common_string_util::PopulateSSLLayoutStrings(cert_error_, load_time_data);
  common_string_util::PopulateSSLDebuggingStrings(ssl_info_, time_triggered_,
                                                  load_time_data);

  // Shared values for both the overridable and non-overridable versions.
  load_time_data->SetBoolean("bad_clock", false);
  load_time_data->SetString("tabTitle",
                            l10n_util::GetStringUTF16(IDS_SSL_V2_TITLE));
  load_time_data->SetString("heading",
                            l10n_util::GetStringUTF16(IDS_SSL_V2_HEADING));
  load_time_data->SetString(
      "primaryParagraph",
      l10n_util::GetStringFUTF16(
          IDS_SSL_V2_PRIMARY_PARAGRAPH,
          common_string_util::GetFormattedHostName(request_url_)));

  if (soft_override_enabled_)
    PopulateOverridableStrings(load_time_data);
  else
    PopulateNonOverridableStrings(load_time_data);
}

void SSLErrorUI::PopulateOverridableStrings(
    base::DictionaryValue* load_time_data) {
  DCHECK(soft_override_enabled_);

  base::string16 url(common_string_util::GetFormattedHostName(request_url_));
  ssl_errors::ErrorInfo error_info = ssl_errors::ErrorInfo::CreateError(
      ssl_errors::ErrorInfo::NetErrorToErrorType(cert_error_),
      ssl_info_.cert.get(), request_url_);

  load_time_data->SetBoolean("overridable", true);
  load_time_data->SetString("explanationParagraph", error_info.details());
  load_time_data->SetString(
      "primaryButtonText",
      l10n_util::GetStringUTF16(IDS_SSL_OVERRIDABLE_SAFETY_BUTTON));
  load_time_data->SetString(
      "finalParagraph",
      l10n_util::GetStringFUTF16(IDS_SSL_OVERRIDABLE_PROCEED_PARAGRAPH, url));
}

void SSLErrorUI::PopulateNonOverridableStrings(
    base::DictionaryValue* load_time_data) {
  DCHECK(!soft_override_enabled_);

  base::string16 url(common_string_util::GetFormattedHostName(request_url_));
  ssl_errors::ErrorInfo::ErrorType type =
      ssl_errors::ErrorInfo::NetErrorToErrorType(cert_error_);

  load_time_data->SetBoolean("overridable", false);
  load_time_data->SetString(
      "explanationParagraph",
      l10n_util::GetStringFUTF16(IDS_SSL_NONOVERRIDABLE_MORE, url));
  load_time_data->SetString("primaryButtonText",
                            l10n_util::GetStringUTF16(IDS_SSL_RELOAD));

  // Customize the help link depending on the specific error type.
  // Only mark as HSTS if none of the more specific error types apply,
  // and use INVALID as a fallback if no other string is appropriate.
  load_time_data->SetInteger("errorType", type);
  int help_string = IDS_SSL_NONOVERRIDABLE_INVALID;
  switch (type) {
    case ssl_errors::ErrorInfo::CERT_REVOKED:
      help_string = IDS_SSL_NONOVERRIDABLE_REVOKED;
      break;
    case ssl_errors::ErrorInfo::CERT_PINNED_KEY_MISSING:
      help_string = IDS_SSL_NONOVERRIDABLE_PINNED;
      break;
    case ssl_errors::ErrorInfo::CERT_INVALID:
      help_string = IDS_SSL_NONOVERRIDABLE_INVALID;
      break;
    default:
      if (requested_strict_enforcement_)
        help_string = IDS_SSL_NONOVERRIDABLE_HSTS;
  }
  load_time_data->SetString("finalParagraph",
                            l10n_util::GetStringFUTF16(help_string, url));
}

void SSLErrorUI::HandleCommand(SecurityInterstitialCommands command) {
  switch (command) {
    case CMD_DONT_PROCEED:
      controller_->metrics_helper()->RecordUserDecision(
          MetricsHelper::DONT_PROCEED);
      user_made_decision_ = true;
      controller_->GoBack();
      break;
    case CMD_PROCEED:
      if (hard_override_enabled_) {
        controller_->metrics_helper()->RecordUserDecision(
            MetricsHelper::PROCEED);
        controller_->Proceed();
        user_made_decision_ = true;
      }
      break;
    case CMD_DO_REPORT:
      controller_->SetReportingPreference(true);
      break;
    case CMD_DONT_REPORT:
      controller_->SetReportingPreference(false);
      break;
    case CMD_SHOW_MORE_SECTION:
      controller_->metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::SHOW_ADVANCED);
      break;
    case CMD_OPEN_HELP_CENTER:
      controller_->metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::SHOW_LEARN_MORE);
      controller_->OpenUrlInCurrentTab(GURL(kHelpURL));
      break;
    case CMD_RELOAD:
      controller_->metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::RELOAD);
      controller_->Reload();
      break;
    case CMD_OPEN_REPORTING_PRIVACY:
      controller_->OpenExtendedReportingPrivacyPolicy();
      break;
    case CMD_OPEN_DATE_SETTINGS:
    case CMD_OPEN_DIAGNOSTIC:
    case CMD_OPEN_LOGIN:
    case CMD_REPORT_PHISHING_ERROR:
      // Not supported by the SSL error page.
      NOTREACHED() << "Unsupported command: " << command;
    case CMD_ERROR:
    case CMD_TEXT_FOUND:
    case CMD_TEXT_NOT_FOUND:
      // Commands are for testing.
      break;
  }
}

}  // security_interstitials
