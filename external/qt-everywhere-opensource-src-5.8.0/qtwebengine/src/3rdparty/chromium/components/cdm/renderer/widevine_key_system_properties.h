// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CDM_RENDERER_WIDEVINE_KEY_SYSTEM_PROPERTIES_H_
#define COMPONENTS_CDM_RENDERER_WIDEVINE_KEY_SYSTEM_PROPERTIES_H_

#include "build/build_config.h"
#include "media/base/key_system_properties.h"

namespace cdm {

// Implementation of KeySystemProperties for Widevine key system.
class WidevineKeySystemProperties : public media::KeySystemProperties {
 public:
  // Robustness values understood by the Widevine key system.
  // Note: GetRobustnessConfigRule is dependent on the order of these.
  enum class Robustness {
    INVALID,
    EMPTY,
    SW_SECURE_CRYPTO,
    SW_SECURE_DECODE,
    HW_SECURE_CRYPTO,
    HW_SECURE_DECODE,
    HW_SECURE_ALL,
  };

  WidevineKeySystemProperties(
      media::SupportedCodecs supported_codecs,
#if defined(OS_ANDROID)
      media::SupportedCodecs supported_secure_codecs,
#endif  // defined(OS_ANDROID)
      Robustness max_audio_robustness,
      Robustness max_video_robustness,
      media::EmeSessionTypeSupport persistent_license_support,
      media::EmeSessionTypeSupport persistent_release_message_support,
      media::EmeFeatureSupport persistent_state_support,
      media::EmeFeatureSupport distinctive_identifier_support);

  std::string GetKeySystemName() const override;
  bool IsSupportedInitDataType(
      media::EmeInitDataType init_data_type) const override;

  media::SupportedCodecs GetSupportedCodecs() const override;
#if defined(OS_ANDROID)
  media::SupportedCodecs GetSupportedSecureCodecs() const override;
#endif

  media::EmeConfigRule GetRobustnessConfigRule(
      media::EmeMediaType media_type,
      const std::string& requested_robustness) const override;
  media::EmeSessionTypeSupport GetPersistentLicenseSessionSupport()
      const override;
  media::EmeSessionTypeSupport GetPersistentReleaseMessageSessionSupport()
      const override;
  media::EmeFeatureSupport GetPersistentStateSupport() const override;
  media::EmeFeatureSupport GetDistinctiveIdentifierSupport() const override;

#if defined(ENABLE_PEPPER_CDMS)
  std::string GetPepperType() const override;
#endif

 private:
  const media::SupportedCodecs supported_codecs_;
#if defined(OS_ANDROID)
  const media::SupportedCodecs supported_secure_codecs_;
#endif  // defined(OS_ANDROID)
  const Robustness max_audio_robustness_;
  const Robustness max_video_robustness_;
  const media::EmeSessionTypeSupport persistent_license_support_;
  const media::EmeSessionTypeSupport persistent_release_message_support_;
  const media::EmeFeatureSupport persistent_state_support_;
  const media::EmeFeatureSupport distinctive_identifier_support_;
};

}  // namespace cdm

#endif  // COMPONENTS_CDM_RENDERER_WIDEVINE_KEY_SYSTEM_PROPERTIES_H_
