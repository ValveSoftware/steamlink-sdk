// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_data.h"

#include "base/guid.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

TemplateURLData::TemplateURLData()
    : show_in_default_list(false),
      safe_for_autoreplace(false),
      id(0),
      date_created(base::Time::Now()),
      last_modified(base::Time::Now()),
      created_by_policy(false),
      usage_count(0),
      prepopulate_id(0),
      sync_guid(base::GenerateGUID()),
      keyword_(base::ASCIIToUTF16("dummy")),
      url_("x") {
}

TemplateURLData::TemplateURLData(const TemplateURLData& other) = default;

TemplateURLData::~TemplateURLData() {
}

void TemplateURLData::SetShortName(const base::string16& short_name) {
  DCHECK(!short_name.empty());

  // Remove tabs, carriage returns, and the like, as they can corrupt
  // how the short name is displayed.
  short_name_ = base::CollapseWhitespace(short_name, true);
}

void TemplateURLData::SetKeyword(const base::string16& keyword) {
  DCHECK(!keyword.empty());

  // Case sensitive keyword matching is confusing. As such, we force all
  // keywords to be lower case.
  keyword_ = base::i18n::ToLower(keyword);
}

void TemplateURLData::SetURL(const std::string& url) {
  DCHECK(!url.empty());
  url_ = url;
}
