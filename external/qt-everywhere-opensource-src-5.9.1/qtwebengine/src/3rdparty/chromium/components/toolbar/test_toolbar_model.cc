// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/toolbar/test_toolbar_model.h"

#include "ui/gfx/vector_icons_public.h"

TestToolbarModel::TestToolbarModel()
    : security_level_(security_state::NONE),
#if defined(TOOLKIT_VIEWS)
      icon_(gfx::VectorIconId::LOCATION_BAR_HTTP),
#else
      icon_(gfx::VectorIconId::VECTOR_ICON_NONE),
#endif
      should_display_url_(true) {
}

TestToolbarModel::~TestToolbarModel() {}

base::string16 TestToolbarModel::GetFormattedURL(size_t* prefix_end) const {
  return text_;
}

GURL TestToolbarModel::GetURL() const {
  return url_;
}

security_state::SecurityLevel TestToolbarModel::GetSecurityLevel(
    bool ignore_editing) const {
  return security_level_;
}

gfx::VectorIconId TestToolbarModel::GetVectorIcon() const {
  return icon_;
}

base::string16 TestToolbarModel::GetSecureVerboseText() const {
  return base::string16();
}

base::string16 TestToolbarModel::GetEVCertName() const {
  return (security_level_ == security_state::EV_SECURE) ? ev_cert_name_
                                                        : base::string16();
}

bool TestToolbarModel::ShouldDisplayURL() const {
  return should_display_url_;
}
