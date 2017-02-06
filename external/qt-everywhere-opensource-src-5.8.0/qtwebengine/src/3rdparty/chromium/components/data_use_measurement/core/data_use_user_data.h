// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_USE_MEASUREMENT_CORE_DATA_USE_USER_DATA_H_
#define COMPONENTS_DATA_USE_MEASUREMENT_CORE_DATA_USE_USER_DATA_H_

#include <string>

#include "base/macros.h"
#include "base/supports_user_data.h"

namespace net {
class URLFetcher;
}

namespace data_use_measurement {

// Used to annotate URLRequests with the service name if the URLRequest is used
// by a service.
class DataUseUserData : public base::SupportsUserData::Data {
 public:
  // This enum should be synced with DataUseServices enum in histograms.xml.
  // Please keep them synced after any updates. Also add service name to
  // GetServiceNameAsString function.
  enum ServiceName {
    NOT_TAGGED,
    SUGGESTIONS,
    TRANSLATE,
    SYNC,
    OMNIBOX,
    INVALIDATION,
    RAPPOR,
    VARIATIONS,
    UMA,
    DOMAIN_RELIABILITY,
    PROFILE_DOWNLOADER,
    GOOGLE_URL_TRACKER,
    AUTOFILL,
    POLICY,
    SPELL_CHECKER,
    NTP_SNIPPETS,
  };

  explicit DataUseUserData(ServiceName service_name);
  ~DataUseUserData() override;

  // Helper function to create DataUseUserData. The caller takes the ownership
  // of the returned object.
  static base::SupportsUserData::Data* Create(
      DataUseUserData::ServiceName service);

  // Return the service name of the ServiceName enum.
  static std::string GetServiceNameAsString(ServiceName service);

  // Services should use this function to attach their |service_name| to the
  // URLFetcher serving them.
  static void AttachToFetcher(net::URLFetcher* fetcher,
                              ServiceName service_name);

  ServiceName service_name() const { return service_name_; }

  // The key for retrieving back this type of user data.
  static const void* const kUserDataKey;

 private:
  const ServiceName service_name_;

  DISALLOW_COPY_AND_ASSIGN(DataUseUserData);
};

}  // namespace data_use_measurement

#endif  // COMPONENTS_DATA_USE_MEASUREMENT_CORE_DATA_USE_USER_DATA_H_
