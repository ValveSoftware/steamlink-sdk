// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/controller_client.h"

#include <utility>

#include "components/google/core/browser/google_util.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace security_interstitials {

const char kBoxChecked[] = "boxchecked";
const char kDisplayCheckBox[] = "displaycheckbox";
const char kOptInLink[] = "optInLink";
const char kPrivacyLinkHtml[] =
    "<a id=\"privacy-link\" href=\"\" onclick=\"sendCommand(%d); "
    "return false;\" onmousedown=\"return false;\">%s</a>";

ControllerClient::ControllerClient() {}
ControllerClient::~ControllerClient() {}

MetricsHelper* ControllerClient::metrics_helper() const {
  return metrics_helper_.get();
}

void ControllerClient::set_metrics_helper(
    std::unique_ptr<MetricsHelper> metrics_helper) {
  metrics_helper_ = std::move(metrics_helper);
}

void ControllerClient::SetReportingPreference(bool report) {
  GetPrefService()->SetBoolean(GetExtendedReportingPrefName(), report);
  metrics_helper_->RecordUserInteraction(
      report ? MetricsHelper::SET_EXTENDED_REPORTING_ENABLED
             : MetricsHelper::SET_EXTENDED_REPORTING_DISABLED);
}

void ControllerClient::OpenExtendedReportingPrivacyPolicy() {
  metrics_helper_->RecordUserInteraction(MetricsHelper::SHOW_PRIVACY_POLICY);
  GURL privacy_url(
      l10n_util::GetStringUTF8(IDS_SAFE_BROWSING_PRIVACY_POLICY_URL));
  privacy_url =
      google_util::AppendGoogleLocaleParam(privacy_url, GetApplicationLocale());
  OpenUrlInCurrentTab(privacy_url);
}

}  // namespace security_interstitials
