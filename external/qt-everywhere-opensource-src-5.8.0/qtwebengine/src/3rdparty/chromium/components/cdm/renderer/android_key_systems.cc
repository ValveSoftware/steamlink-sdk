// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cdm/renderer/android_key_systems.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "components/cdm/common/cdm_messages_android.h"
#include "components/cdm/renderer/widevine_key_system_properties.h"
#include "content/public/renderer/render_thread.h"
#include "media/base/eme_constants.h"
#include "media/base/media_switches.h"

#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

using media::EmeConfigRule;
using media::EmeFeatureSupport;
using media::EmeInitDataType;
using media::EmeSessionTypeSupport;
using media::KeySystemProperties;
using media::SupportedCodecs;
using Robustness = cdm::WidevineKeySystemProperties::Robustness;

namespace cdm {

namespace {

// Implementation of KeySystemProperties for platform-supported key systems.
// Assumes that platform key systems support no features but can and will
// make use of persistence and identifiers.
class AndroidPlatformKeySystemProperties : public KeySystemProperties {
 public:
  AndroidPlatformKeySystemProperties(const std::string& name,
                                     SupportedCodecs supported_codecs)
      : name_(name), supported_codecs_(supported_codecs) {}

  std::string GetKeySystemName() const override { return name_; }

  bool IsSupportedInitDataType(EmeInitDataType init_data_type) const override {
    // Here we assume that support for a container implies support for the
    // associated initialization data type. KeySystems handles validating
    // |init_data_type| x |container| pairings.
    switch (init_data_type) {
      case EmeInitDataType::WEBM:
        return (supported_codecs_ & media::EME_CODEC_WEBM_ALL) != 0;
      case EmeInitDataType::CENC:
#if defined(USE_PROPRIETARY_CODECS)
        return (supported_codecs_ & media::EME_CODEC_MP4_ALL) != 0;
#else
        return false;
#endif  // defined(USE_PROPRIETARY_CODECS)
      case EmeInitDataType::KEYIDS:
      case EmeInitDataType::UNKNOWN:
        return false;
    }
    NOTREACHED();
    return false;
  }

  SupportedCodecs GetSupportedCodecs() const override {
    return supported_codecs_;
  }

  EmeConfigRule GetRobustnessConfigRule(
      media::EmeMediaType media_type,
      const std::string& requested_robustness) const override {
    return requested_robustness.empty() ? EmeConfigRule::SUPPORTED
                                        : EmeConfigRule::NOT_SUPPORTED;
  }

  EmeSessionTypeSupport GetPersistentLicenseSessionSupport() const override {
    return EmeSessionTypeSupport::NOT_SUPPORTED;
  }
  EmeSessionTypeSupport GetPersistentReleaseMessageSessionSupport()
      const override {
    return EmeSessionTypeSupport::NOT_SUPPORTED;
  }
  EmeFeatureSupport GetPersistentStateSupport() const override {
    return EmeFeatureSupport::ALWAYS_ENABLED;
  }
  EmeFeatureSupport GetDistinctiveIdentifierSupport() const override {
    return EmeFeatureSupport::ALWAYS_ENABLED;
  }

 private:
  const std::string name_;
  const SupportedCodecs supported_codecs_;
};

}  // namespace

static SupportedKeySystemResponse QueryKeySystemSupport(
    const std::string& key_system) {
  SupportedKeySystemRequest request;
  SupportedKeySystemResponse response;

  request.key_system = key_system;
  request.codecs = media::EME_CODEC_ALL;
  content::RenderThread::Get()->Send(
      new ChromeViewHostMsg_QueryKeySystemSupport(request, &response));
  DCHECK(!(response.compositing_codecs & ~media::EME_CODEC_ALL))
      << "unrecognized codec";
  DCHECK(!(response.non_compositing_codecs & ~media::EME_CODEC_ALL))
      << "unrecognized codec";
  return response;
}

void AddAndroidWidevine(
    std::vector<std::unique_ptr<KeySystemProperties>>* concrete_key_systems) {
  SupportedKeySystemResponse response = QueryKeySystemSupport(
      kWidevineKeySystem);

  // Since we do not control the implementation of the MediaDrm API on Android,
  // we assume that it can and will make use of persistence even though no
  // persistence-based features are supported.

  if (response.compositing_codecs != media::EME_CODEC_NONE) {
    concrete_key_systems->emplace_back(new WidevineKeySystemProperties(
        response.compositing_codecs,           // Regular codecs.
        response.non_compositing_codecs,       // Hardware-secure codecs.
        Robustness::HW_SECURE_CRYPTO,          // Max audio robustness.
        Robustness::HW_SECURE_ALL,             // Max video robustness.
        EmeSessionTypeSupport::NOT_SUPPORTED,  // persistent-license.
        EmeSessionTypeSupport::NOT_SUPPORTED,  // persistent-release-message.
        EmeFeatureSupport::ALWAYS_ENABLED,     // Persistent state.
        EmeFeatureSupport::ALWAYS_ENABLED));   // Distinctive identifier.
  } else {
    // It doesn't make sense to support secure codecs but not regular codecs.
    DCHECK(response.non_compositing_codecs == media::EME_CODEC_NONE);
  }
}

void AddAndroidPlatformKeySystems(
    std::vector<std::unique_ptr<KeySystemProperties>>* concrete_key_systems) {
  std::vector<std::string> key_system_names;
  content::RenderThread::Get()->Send(
      new ChromeViewHostMsg_GetPlatformKeySystemNames(&key_system_names));

  for (std::vector<std::string>::const_iterator it = key_system_names.begin();
       it != key_system_names.end(); ++it) {
    SupportedKeySystemResponse response = QueryKeySystemSupport(*it);
    if (response.compositing_codecs != media::EME_CODEC_NONE) {
      concrete_key_systems->emplace_back(new AndroidPlatformKeySystemProperties(
          *it, response.compositing_codecs));
    }
  }
}

}  // namespace cdm
