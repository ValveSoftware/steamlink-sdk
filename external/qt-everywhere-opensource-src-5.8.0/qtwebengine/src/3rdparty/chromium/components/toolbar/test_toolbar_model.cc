// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/toolbar/test_toolbar_model.h"

#include "components/grit/components_scaled_resources.h"
#include "ui/gfx/vector_icons_public.h"

TestToolbarModel::TestToolbarModel()
    : perform_search_term_replacement_(false),
      security_level_(security_state::SecurityStateModel::NONE),
#if defined(TOOLKIT_VIEWS)
      icon_(gfx::VectorIconId::LOCATION_BAR_HTTP),
#else
      icon_(gfx::VectorIconId::VECTOR_ICON_NONE),
#endif
      should_display_url_(true) {
}

TestToolbarModel::~TestToolbarModel() {}

base::string16 TestToolbarModel::GetText() const {
  return text_;
}

base::string16 TestToolbarModel::GetFormattedURL(size_t* prefix_end) const {
  return text_;
}

GURL TestToolbarModel::GetURL() const {
  return url_;
}

bool TestToolbarModel::WouldPerformSearchTermReplacement(
    bool ignore_editing) const {
  return perform_search_term_replacement_;
}

security_state::SecurityStateModel::SecurityLevel
TestToolbarModel::GetSecurityLevel(bool ignore_editing) const {
  return security_level_;
}

int TestToolbarModel::GetIcon() const {
  // This placeholder implementation should be removed when MD is default.
  return IDR_LOCATION_BAR_HTTP;
}

gfx::VectorIconId TestToolbarModel::GetVectorIcon() const {
  return icon_;
}

base::string16 TestToolbarModel::GetEVCertName() const {
  return (security_level_ == security_state::SecurityStateModel::EV_SECURE)
             ? ev_cert_name_
             : base::string16();
}

bool TestToolbarModel::ShouldDisplayURL() const {
  return should_display_url_;
}
