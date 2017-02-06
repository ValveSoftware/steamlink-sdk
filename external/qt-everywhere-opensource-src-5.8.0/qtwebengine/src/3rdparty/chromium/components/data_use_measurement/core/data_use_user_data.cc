// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_use_measurement/core/data_use_user_data.h"

#include "net/url_request/url_fetcher.h"

namespace data_use_measurement {

DataUseUserData::DataUseUserData(ServiceName service_name)
    : service_name_(service_name) {}

DataUseUserData::~DataUseUserData() {}

// static
const void* const DataUseUserData::kUserDataKey =
    &DataUseUserData::kUserDataKey;

// static
base::SupportsUserData::Data* DataUseUserData::Create(
    ServiceName service_name) {
  return new DataUseUserData(service_name);
}

// static
std::string DataUseUserData::GetServiceNameAsString(ServiceName service_name) {
  switch (service_name) {
    case SUGGESTIONS:
      return "Suggestions";
    case NOT_TAGGED:
      return "NotTagged";
    case TRANSLATE:
      return "Translate";
    case SYNC:
      return "Sync";
    case OMNIBOX:
      return "Omnibox";
    case INVALIDATION:
      return "Invalidation";
    case RAPPOR:
      return "Rappor";
    case VARIATIONS:
      return "Variations";
    case UMA:
      return "UMA";
    case DOMAIN_RELIABILITY:
      return "DomainReliability";
    case PROFILE_DOWNLOADER:
      return "ProfileDownloader";
    case GOOGLE_URL_TRACKER:
      return "GoogleURLTracker";
    case AUTOFILL:
      return "Autofill";
    case POLICY:
      return "Policy";
    case SPELL_CHECKER:
      return "SpellChecker";
    case NTP_SNIPPETS:
      return "NTPSnippets";
  }
  return "INVALID";
}

// static
void DataUseUserData::AttachToFetcher(net::URLFetcher* fetcher,
                                      ServiceName service_name) {
  fetcher->SetURLRequestUserData(kUserDataKey,
                                 base::Bind(&Create, service_name));
}

}  // namespace data_use_measurement
