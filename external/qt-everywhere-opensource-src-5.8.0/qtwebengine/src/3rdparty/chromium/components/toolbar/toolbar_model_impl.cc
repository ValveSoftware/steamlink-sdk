// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/toolbar/toolbar_model_impl.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/grit/components_scaled_resources.h"
#include "components/prefs/pref_service.h"
#include "components/security_state/security_state_model.h"
#include "components/strings/grit/components_strings.h"
#include "components/toolbar/toolbar_model_delegate.h"
#include "components/url_formatter/elide_url.h"
#include "components/url_formatter/url_formatter.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/vector_icons_public.h"

using security_state::SecurityStateModel;

ToolbarModelImpl::ToolbarModelImpl(ToolbarModelDelegate* delegate,
                                   size_t max_url_display_chars)
    : delegate_(delegate), max_url_display_chars_(max_url_display_chars) {
  DCHECK(delegate_);
}

ToolbarModelImpl::~ToolbarModelImpl() {
}

// ToolbarModelImpl Implementation.
base::string16 ToolbarModelImpl::GetText() const {
  base::string16 search_terms(GetSearchTerms(false));
  if (!search_terms.empty())
    return search_terms;

  return GetFormattedURL(NULL);
}

base::string16 ToolbarModelImpl::GetFormattedURL(size_t* prefix_end) const {
  GURL url(GetURL());
  // Note that we can't unescape spaces here, because if the user copies this
  // and pastes it into another program, that program may think the URL ends at
  // the space.
  const base::string16 formatted_text =
      delegate_->FormattedStringWithEquivalentMeaning(
          url, url_formatter::FormatUrl(
                   url, url_formatter::kFormatUrlOmitAll,
                   net::UnescapeRule::NORMAL, nullptr, prefix_end, nullptr));
  if (formatted_text.length() <= max_url_display_chars_)
    return formatted_text;

  // Truncating the URL breaks editing and then pressing enter, but hopefully
  // people won't try to do much with such enormous URLs anyway. If this becomes
  // a real problem, we could perhaps try to keep some sort of different "elided
  // visible URL" where editing affects and reloads the "real underlying URL",
  // but this seems very tricky for little gain.
  return gfx::TruncateString(formatted_text, max_url_display_chars_ - 1,
                             gfx::CHARACTER_BREAK) +
         gfx::kEllipsisUTF16;
}

GURL ToolbarModelImpl::GetURL() const {
  GURL url;
  return delegate_->GetURL(&url) ? url : GURL(url::kAboutBlankURL);
}

bool ToolbarModelImpl::WouldPerformSearchTermReplacement(
    bool ignore_editing) const {
  return !GetSearchTerms(ignore_editing).empty();
}

SecurityStateModel::SecurityLevel ToolbarModelImpl::GetSecurityLevel(
    bool ignore_editing) const {
  // When editing or empty, assume no security style.
  return ((input_in_progress() && !ignore_editing) || !ShouldDisplayURL())
             ? SecurityStateModel::NONE
             : delegate_->GetSecurityLevel();
}

int ToolbarModelImpl::GetIcon() const {
  switch (GetSecurityLevel(false)) {
    case SecurityStateModel::NONE:
      return IDR_LOCATION_BAR_HTTP;
    case SecurityStateModel::EV_SECURE:
    case SecurityStateModel::SECURE:
      return IDR_OMNIBOX_HTTPS_VALID;
    case SecurityStateModel::SECURITY_WARNING:
      // Surface Dubious as Neutral.
      return IDR_LOCATION_BAR_HTTP;
    case SecurityStateModel::SECURITY_POLICY_WARNING:
      return IDR_OMNIBOX_HTTPS_POLICY_WARNING;
    case SecurityStateModel::SECURITY_ERROR:
      return IDR_OMNIBOX_HTTPS_INVALID;
  }

  NOTREACHED();
  return IDR_LOCATION_BAR_HTTP;
}

gfx::VectorIconId ToolbarModelImpl::GetVectorIcon() const {
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  switch (GetSecurityLevel(false)) {
    case SecurityStateModel::NONE:
      return gfx::VectorIconId::LOCATION_BAR_HTTP;
    case SecurityStateModel::EV_SECURE:
    case SecurityStateModel::SECURE:
      return gfx::VectorIconId::LOCATION_BAR_HTTPS_VALID;
    case SecurityStateModel::SECURITY_WARNING:
      // Surface Dubious as Neutral.
      return gfx::VectorIconId::LOCATION_BAR_HTTP;
    case SecurityStateModel::SECURITY_POLICY_WARNING:
      return gfx::VectorIconId::BUSINESS;
    case SecurityStateModel::SECURITY_ERROR:
      return gfx::VectorIconId::LOCATION_BAR_HTTPS_INVALID;
  }
#endif

  NOTREACHED();
  return gfx::VectorIconId::VECTOR_ICON_NONE;
}

base::string16 ToolbarModelImpl::GetEVCertName() const {
  if (GetSecurityLevel(false) != SecurityStateModel::EV_SECURE)
    return base::string16();

  // Note: cert is guaranteed non-NULL or the security level would be NONE.
  scoped_refptr<net::X509Certificate> cert = delegate_->GetCertificate();
  DCHECK(cert.get());

  // EV are required to have an organization name and country.
  DCHECK(!cert->subject().organization_names.empty());
  DCHECK(!cert->subject().country_name.empty());
  return l10n_util::GetStringFUTF16(
      IDS_SECURE_CONNECTION_EV,
      base::UTF8ToUTF16(cert->subject().organization_names[0]),
      base::UTF8ToUTF16(cert->subject().country_name));
}

bool ToolbarModelImpl::ShouldDisplayURL() const {
  return delegate_->ShouldDisplayURL();
}

base::string16 ToolbarModelImpl::GetSearchTerms(bool ignore_editing) const {
  if (!url_replacement_enabled() || (input_in_progress() && !ignore_editing))
    return base::string16();

  return delegate_->GetSearchTerms(GetSecurityLevel(ignore_editing));
}
