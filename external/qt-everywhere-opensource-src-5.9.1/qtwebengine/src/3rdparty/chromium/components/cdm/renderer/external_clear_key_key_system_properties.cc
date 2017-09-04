// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cdm/renderer/external_clear_key_key_system_properties.h"

#include "base/logging.h"
#include "media/base/eme_constants.h"
#include "ppapi/features/features.h"

namespace cdm {

#if BUILDFLAG(ENABLE_PEPPER_CDMS)
const char kExternalClearKeyPepperType[] = "application/x-ppapi-clearkey-cdm";
#endif

ExternalClearKeyProperties::ExternalClearKeyProperties(
    const std::string& key_system_name)
    : key_system_name_(key_system_name) {}

ExternalClearKeyProperties::~ExternalClearKeyProperties() {}

std::string ExternalClearKeyProperties::GetKeySystemName() const {
  return key_system_name_;
}

bool ExternalClearKeyProperties::IsSupportedInitDataType(
    media::EmeInitDataType init_data_type) const {
  switch (init_data_type) {
    case media::EmeInitDataType::WEBM:
    case media::EmeInitDataType::KEYIDS:
      return true;

    case media::EmeInitDataType::CENC:
#if defined(USE_PROPRIETARY_CODECS)
      return true;
#else
      return false;
#endif  // defined(USE_PROPRIETARY_CODECS)

    case media::EmeInitDataType::UNKNOWN:
      return false;
  }
  NOTREACHED();
  return false;
}

media::SupportedCodecs ExternalClearKeyProperties::GetSupportedCodecs() const {
#if defined(USE_PROPRIETARY_CODECS)
  return media::EME_CODEC_MP4_ALL | media::EME_CODEC_WEBM_ALL;
#else
  return media::EME_CODEC_WEBM_ALL;
#endif
}

media::EmeConfigRule ExternalClearKeyProperties::GetRobustnessConfigRule(
    media::EmeMediaType media_type,
    const std::string& requested_robustness) const {
  return requested_robustness.empty() ? media::EmeConfigRule::SUPPORTED
                                      : media::EmeConfigRule::NOT_SUPPORTED;
}

// Persistent license sessions are faked.
media::EmeSessionTypeSupport
ExternalClearKeyProperties::GetPersistentLicenseSessionSupport() const {
  return media::EmeSessionTypeSupport::SUPPORTED;
}

media::EmeSessionTypeSupport
ExternalClearKeyProperties::GetPersistentReleaseMessageSessionSupport() const {
  return media::EmeSessionTypeSupport::NOT_SUPPORTED;
}

media::EmeFeatureSupport ExternalClearKeyProperties::GetPersistentStateSupport()
    const {
  return media::EmeFeatureSupport::REQUESTABLE;
}

media::EmeFeatureSupport
ExternalClearKeyProperties::GetDistinctiveIdentifierSupport() const {
  return media::EmeFeatureSupport::NOT_SUPPORTED;
}

#if BUILDFLAG(ENABLE_PEPPER_CDMS)
std::string ExternalClearKeyProperties::GetPepperType() const {
  return kExternalClearKeyPepperType;
}
#endif

}  // namespace cdm
