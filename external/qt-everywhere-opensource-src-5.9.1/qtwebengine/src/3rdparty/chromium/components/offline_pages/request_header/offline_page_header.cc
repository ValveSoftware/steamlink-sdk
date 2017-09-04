// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/request_header/offline_page_header.h"

#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"

namespace offline_pages {

const char kOfflinePageHeader[] = "X-Chrome-offline";
const char kOfflinePageHeaderReasonKey[] = "reason";
const char kOfflinePageHeaderReasonValueDueToNetError[] = "error";
const char kOfflinePageHeaderReasonValueFromDownload[] = "download";
const char kOfflinePageHeaderReasonValueReload[] = "reload";
const char kOfflinePageHeaderPersistKey[] = "persist";
const char kOfflinePageHeaderIDKey[] = "id";

namespace {

bool ParseOfflineHeaderValue(const std::string& header_value,
                             bool* need_to_persist,
                             OfflinePageHeader::Reason* reason,
                             std::string* id) {
  // If the offline header is not present, treat it as not parsed successfully.
  if (header_value.empty())
    return false;

  bool token_found = false;
  base::StringTokenizer tokenizer(header_value, ", ");
  while (tokenizer.GetNext()) {
    token_found = true;
    std::string pair = tokenizer.token();
    std::size_t pos = pair.find('=');
    if (pos == std::string::npos)
      return false;
    std::string key = base::ToLowerASCII(pair.substr(0, pos));
    std::string value = base::ToLowerASCII(pair.substr(pos + 1));
    if (key == kOfflinePageHeaderPersistKey) {
      if (value == "1")
        *need_to_persist = true;
      else if (value == "0")
        *need_to_persist = false;
      else
        return false;
    } else if (key == kOfflinePageHeaderReasonKey) {
      if (value == kOfflinePageHeaderReasonValueDueToNetError)
        *reason = OfflinePageHeader::Reason::NET_ERROR;
      else if (value == kOfflinePageHeaderReasonValueFromDownload)
        *reason = OfflinePageHeader::Reason::DOWNLOAD;
      else if (value == kOfflinePageHeaderReasonValueReload)
        *reason = OfflinePageHeader::Reason::RELOAD;
      else
        return false;
    } else if (key == kOfflinePageHeaderIDKey) {
      *id = value;
    } else {
      return false;
    }
  }

  return token_found;
}

std::string ReasonToString(OfflinePageHeader::Reason reason) {
  switch (reason) {
    case OfflinePageHeader::Reason::NET_ERROR:
      return kOfflinePageHeaderReasonValueDueToNetError;
    case OfflinePageHeader::Reason::DOWNLOAD:
      return kOfflinePageHeaderReasonValueFromDownload;
    case OfflinePageHeader::Reason::RELOAD:
      return kOfflinePageHeaderReasonValueReload;
    default:
      NOTREACHED();
      return "";
  }
}

}  // namespace

OfflinePageHeader::OfflinePageHeader()
    : did_fail_parsing_for_test(false),
      need_to_persist(false),
      reason(Reason::NONE) {
}

OfflinePageHeader::OfflinePageHeader(const std::string& header_value)
    : did_fail_parsing_for_test(false),
      need_to_persist(false),
      reason(Reason::NONE) {
  if (!ParseOfflineHeaderValue(header_value, &need_to_persist, &reason, &id)) {
    did_fail_parsing_for_test = true;
    Clear();
  }
}

OfflinePageHeader::~OfflinePageHeader() {}

std::string OfflinePageHeader::GetCompleteHeaderString() const {
  if (reason == Reason::NONE)
    return std::string();

  std::string value(kOfflinePageHeader);
  value += ": ";

  value += kOfflinePageHeaderPersistKey;
  value += "=";
  value += need_to_persist ? "1" : "0";

  value += " " ;
  value += kOfflinePageHeaderReasonKey;
  value += "=";
  value += ReasonToString(reason);

  if (!id.empty()) {
    value += " " ;
    value += kOfflinePageHeaderIDKey;
    value += "=";
    value += id;
  }

  return value;
}

void OfflinePageHeader::Clear() {
  reason = Reason::NONE;
  need_to_persist = false;
  id.clear();
}

}  // namespace offline_pages
