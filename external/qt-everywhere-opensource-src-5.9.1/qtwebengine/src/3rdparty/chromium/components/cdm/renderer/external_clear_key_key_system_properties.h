// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CDM_RENDERER_EXTERNAL_CLEAR_KEY_KEY_SYSTEM_PROPERTIES_H_
#define COMPONENTS_CDM_RENDERER_EXTERNAL_CLEAR_KEY_KEY_SYSTEM_PROPERTIES_H_

#include <string>

#include "build/build_config.h"
#include "media/base/key_system_properties.h"
#include "ppapi/features/features.h"

namespace cdm {

#if BUILDFLAG(ENABLE_PEPPER_CDMS)
extern const char kExternalClearKeyPepperType[];
#endif

// KeySystemProperties implementation for external Clear Key key systems.
class ExternalClearKeyProperties : public media::KeySystemProperties {
 public:
  explicit ExternalClearKeyProperties(const std::string& key_system_name);
  ~ExternalClearKeyProperties() override;

  std::string GetKeySystemName() const override;
  bool IsSupportedInitDataType(
      media::EmeInitDataType init_data_type) const override;
  media::SupportedCodecs GetSupportedCodecs() const override;
  media::EmeConfigRule GetRobustnessConfigRule(
      media::EmeMediaType media_type,
      const std::string& requested_robustness) const override;
  media::EmeSessionTypeSupport GetPersistentLicenseSessionSupport()
      const override;
  media::EmeSessionTypeSupport GetPersistentReleaseMessageSessionSupport()
      const override;
  media::EmeFeatureSupport GetPersistentStateSupport() const override;
  media::EmeFeatureSupport GetDistinctiveIdentifierSupport() const override;
#if BUILDFLAG(ENABLE_PEPPER_CDMS)
  std::string GetPepperType() const override;
#endif

 private:
  const std::string key_system_name_;
};

}  // namespace cdm

#endif  // COMPONENTS_CDM_RENDERER_EXTERNAL_CLEAR_KEY_KEY_SYSTEM_PROPERTIES_H_
