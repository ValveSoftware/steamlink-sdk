// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/key_system_config_selector.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "media/base/cdm_config.h"
#include "media/base/key_systems.h"
#include "media/base/media_permission.h"
#include "media/base/mime_util.h"
#include "media/blink/webmediaplayer_util.h"
#include "third_party/WebKit/public/platform/URLConversion.h"
#include "third_party/WebKit/public/platform/WebMediaKeySystemConfiguration.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace media {

using EmeFeatureRequirement =
    blink::WebMediaKeySystemConfiguration::Requirement;

namespace {

static EmeConfigRule GetSessionTypeConfigRule(EmeSessionTypeSupport support) {
  switch (support) {
    case EmeSessionTypeSupport::INVALID:
      NOTREACHED();
      return EmeConfigRule::NOT_SUPPORTED;
    case EmeSessionTypeSupport::NOT_SUPPORTED:
      return EmeConfigRule::NOT_SUPPORTED;
    case EmeSessionTypeSupport::SUPPORTED_WITH_IDENTIFIER:
      return EmeConfigRule::IDENTIFIER_AND_PERSISTENCE_REQUIRED;
    case EmeSessionTypeSupport::SUPPORTED:
      return EmeConfigRule::PERSISTENCE_REQUIRED;
  }
  NOTREACHED();
  return EmeConfigRule::NOT_SUPPORTED;
}

static EmeConfigRule GetDistinctiveIdentifierConfigRule(
    EmeFeatureSupport support,
    EmeFeatureRequirement requirement) {
  if (support == EmeFeatureSupport::INVALID) {
    NOTREACHED();
    return EmeConfigRule::NOT_SUPPORTED;
  }

  // For NOT_ALLOWED and REQUIRED, the result is as expected. For OPTIONAL, we
  // return the most restrictive rule that is not more restrictive than for
  // NOT_ALLOWED or REQUIRED. Those values will be checked individually when
  // the option is resolved.
  //
  //                   NOT_ALLOWED    OPTIONAL       REQUIRED
  //    NOT_SUPPORTED  I_NOT_ALLOWED  I_NOT_ALLOWED  NOT_SUPPORTED
  //      REQUESTABLE  I_NOT_ALLOWED  SUPPORTED      I_REQUIRED
  //   ALWAYS_ENABLED  NOT_SUPPORTED  I_REQUIRED     I_REQUIRED
  DCHECK(support == EmeFeatureSupport::NOT_SUPPORTED ||
         support == EmeFeatureSupport::REQUESTABLE ||
         support == EmeFeatureSupport::ALWAYS_ENABLED);
  DCHECK(requirement == EmeFeatureRequirement::NotAllowed ||
         requirement == EmeFeatureRequirement::Optional ||
         requirement == EmeFeatureRequirement::Required);
  if ((support == EmeFeatureSupport::NOT_SUPPORTED &&
       requirement == EmeFeatureRequirement::Required) ||
      (support == EmeFeatureSupport::ALWAYS_ENABLED &&
       requirement == EmeFeatureRequirement::NotAllowed)) {
    return EmeConfigRule::NOT_SUPPORTED;
  }
  if (support == EmeFeatureSupport::REQUESTABLE &&
      requirement == EmeFeatureRequirement::Optional) {
    return EmeConfigRule::SUPPORTED;
  }
  if (support == EmeFeatureSupport::NOT_SUPPORTED ||
      requirement == EmeFeatureRequirement::NotAllowed) {
    return EmeConfigRule::IDENTIFIER_NOT_ALLOWED;
  }
  return EmeConfigRule::IDENTIFIER_REQUIRED;
}

static EmeConfigRule GetPersistentStateConfigRule(
    EmeFeatureSupport support,
    EmeFeatureRequirement requirement) {
  if (support == EmeFeatureSupport::INVALID) {
    NOTREACHED();
    return EmeConfigRule::NOT_SUPPORTED;
  }

  // For NOT_ALLOWED and REQUIRED, the result is as expected. For OPTIONAL, we
  // return the most restrictive rule that is not more restrictive than for
  // NOT_ALLOWED or REQUIRED. Those values will be checked individually when
  // the option is resolved.
  //
  // Note that even though a distinctive identifier can not be required for
  // persistent state, it may still be required for persistent sessions.
  //
  //                   NOT_ALLOWED    OPTIONAL       REQUIRED
  //    NOT_SUPPORTED  P_NOT_ALLOWED  P_NOT_ALLOWED  NOT_SUPPORTED
  //      REQUESTABLE  P_NOT_ALLOWED  SUPPORTED      P_REQUIRED
  //   ALWAYS_ENABLED  NOT_SUPPORTED  P_REQUIRED     P_REQUIRED
  DCHECK(support == EmeFeatureSupport::NOT_SUPPORTED ||
         support == EmeFeatureSupport::REQUESTABLE ||
         support == EmeFeatureSupport::ALWAYS_ENABLED);
  DCHECK(requirement == EmeFeatureRequirement::NotAllowed ||
         requirement == EmeFeatureRequirement::Optional ||
         requirement == EmeFeatureRequirement::Required);
  if ((support == EmeFeatureSupport::NOT_SUPPORTED &&
       requirement == EmeFeatureRequirement::Required) ||
      (support == EmeFeatureSupport::ALWAYS_ENABLED &&
       requirement == EmeFeatureRequirement::NotAllowed)) {
    return EmeConfigRule::NOT_SUPPORTED;
  }
  if (support == EmeFeatureSupport::REQUESTABLE &&
      requirement == EmeFeatureRequirement::Optional) {
    return EmeConfigRule::SUPPORTED;
  }
  if (support == EmeFeatureSupport::NOT_SUPPORTED ||
      requirement == EmeFeatureRequirement::NotAllowed) {
    return EmeConfigRule::PERSISTENCE_NOT_ALLOWED;
  }
  return EmeConfigRule::PERSISTENCE_REQUIRED;
}

static bool IsPersistentSessionType(
    blink::WebEncryptedMediaSessionType sessionType) {
  switch (sessionType) {
    case blink::WebEncryptedMediaSessionType::Temporary:
      return false;
    case blink::WebEncryptedMediaSessionType::PersistentLicense:
      return true;
    case blink::WebEncryptedMediaSessionType::PersistentReleaseMessage:
      return true;
    case blink::WebEncryptedMediaSessionType::Unknown:
      break;
  }

  NOTREACHED();
  return false;
}

}  // namespace

struct KeySystemConfigSelector::SelectionRequest {
  std::string key_system;
  blink::WebVector<blink::WebMediaKeySystemConfiguration>
      candidate_configurations;
  blink::WebSecurityOrigin security_origin;
  base::Callback<void(const blink::WebMediaKeySystemConfiguration&,
                      const CdmConfig&)> succeeded_cb;
  base::Callback<void(const blink::WebString&)> not_supported_cb;
  bool was_permission_requested = false;
  bool is_permission_granted = false;
  bool are_secure_codecs_supported = false;
};

// Accumulates configuration rules to determine if a feature (additional
// configuration rule) can be added to an accumulated configuration.
class KeySystemConfigSelector::ConfigState {
 public:
  ConfigState(bool was_permission_requested, bool is_permission_granted)
      : was_permission_requested_(was_permission_requested),
        is_permission_granted_(is_permission_granted) {}

  bool IsPermissionGranted() const { return is_permission_granted_; }

  // Permission is possible if it has not been denied.
  bool IsPermissionPossible() const {
    return is_permission_granted_ || !was_permission_requested_;
  }

  bool IsIdentifierRequired() const { return is_identifier_required_; }

  bool IsIdentifierRecommended() const { return is_identifier_recommended_; }

  bool AreHwSecureCodecsRequired() const {
    return are_hw_secure_codecs_required_;
  }

  // Checks whether a rule is compatible with all previously added rules.
  bool IsRuleSupported(EmeConfigRule rule) const {
    switch (rule) {
      case EmeConfigRule::NOT_SUPPORTED:
        return false;
      case EmeConfigRule::IDENTIFIER_NOT_ALLOWED:
        return !is_identifier_required_;
      case EmeConfigRule::IDENTIFIER_REQUIRED:
        // TODO(sandersd): Confirm if we should be refusing these rules when
        // permission has been denied (as the spec currently says).
        return !is_identifier_not_allowed_ && IsPermissionPossible();
      case EmeConfigRule::IDENTIFIER_RECOMMENDED:
        return true;
      case EmeConfigRule::PERSISTENCE_NOT_ALLOWED:
        return !is_persistence_required_;
      case EmeConfigRule::PERSISTENCE_REQUIRED:
        return !is_persistence_not_allowed_;
      case EmeConfigRule::IDENTIFIER_AND_PERSISTENCE_REQUIRED:
        return (!is_identifier_not_allowed_ && IsPermissionPossible() &&
                !is_persistence_not_allowed_);
      case EmeConfigRule::HW_SECURE_CODECS_NOT_ALLOWED:
        return !are_hw_secure_codecs_required_;
      case EmeConfigRule::HW_SECURE_CODECS_REQUIRED:
        return !are_hw_secure_codecs_not_allowed_;
      case EmeConfigRule::SUPPORTED:
        return true;
    }
    NOTREACHED();
    return false;
  }

  // Add a rule to the accumulated configuration state.
  void AddRule(EmeConfigRule rule) {
    DCHECK(IsRuleSupported(rule));
    switch (rule) {
      case EmeConfigRule::NOT_SUPPORTED:
        NOTREACHED();
        return;
      case EmeConfigRule::IDENTIFIER_NOT_ALLOWED:
        is_identifier_not_allowed_ = true;
        return;
      case EmeConfigRule::IDENTIFIER_REQUIRED:
        is_identifier_required_ = true;
        return;
      case EmeConfigRule::IDENTIFIER_RECOMMENDED:
        is_identifier_recommended_ = true;
        return;
      case EmeConfigRule::PERSISTENCE_NOT_ALLOWED:
        is_persistence_not_allowed_ = true;
        return;
      case EmeConfigRule::PERSISTENCE_REQUIRED:
        is_persistence_required_ = true;
        return;
      case EmeConfigRule::IDENTIFIER_AND_PERSISTENCE_REQUIRED:
        is_identifier_required_ = true;
        is_persistence_required_ = true;
        return;
      case EmeConfigRule::HW_SECURE_CODECS_NOT_ALLOWED:
        are_hw_secure_codecs_not_allowed_ = true;
        return;
      case EmeConfigRule::HW_SECURE_CODECS_REQUIRED:
        are_hw_secure_codecs_required_ = true;
        return;
      case EmeConfigRule::SUPPORTED:
        return;
    }
    NOTREACHED();
  }

 private:
  // Whether permission to use a distinctive identifier was requested. If set,
  // |is_permission_granted_| represents the final decision.
  // (Not changed by adding rules.)
  bool was_permission_requested_;

  // Whether permission to use a distinctive identifier has been granted.
  // (Not changed by adding rules.)
  bool is_permission_granted_;

  // Whether a rule has been added that requires or blocks a distinctive
  // identifier.
  bool is_identifier_required_ = false;
  bool is_identifier_not_allowed_ = false;

  // Whether a rule has been added that recommends a distinctive identifier.
  bool is_identifier_recommended_ = false;

  // Whether a rule has been added that requires or blocks persistent state.
  bool is_persistence_required_ = false;
  bool is_persistence_not_allowed_ = false;

  // Whether a rule has been added that requires or blocks hardware-secure
  // codecs.
  bool are_hw_secure_codecs_required_ = false;
  bool are_hw_secure_codecs_not_allowed_ = false;
};

KeySystemConfigSelector::KeySystemConfigSelector(
    const KeySystems* key_systems,
    MediaPermission* media_permission)
    : key_systems_(key_systems),
      media_permission_(media_permission),
      weak_factory_(this) {
  DCHECK(key_systems_);
  DCHECK(media_permission_);
}

KeySystemConfigSelector::~KeySystemConfigSelector() {
}

bool IsSupportedMediaFormat(const std::string& container_mime_type,
                            const std::string& codecs,
                            bool use_aes_decryptor) {
  std::vector<std::string> codec_vector;
  ParseCodecString(codecs, &codec_vector, false);
  // AesDecryptor decrypts the stream in the demuxer before it reaches the
  // decoder so check whether the media format is supported when clear.
  SupportsType support_result =
      use_aes_decryptor
          ? IsSupportedMediaFormat(container_mime_type, codec_vector)
          : IsSupportedEncryptedMediaFormat(container_mime_type, codec_vector);
  switch (support_result) {
    case IsSupported:
      return true;
    case MayBeSupported:
      // If no codecs were specified, the best possible result is
      // MayBeSupported, indicating support for the container.
      return codec_vector.empty();
    case IsNotSupported:
      return false;
  }
  NOTREACHED();
  return false;
}

// TODO(sandersd): Move contentType parsing from Blink to here so that invalid
// parameters can be rejected. http://crbug.com/417561
bool KeySystemConfigSelector::IsSupportedContentType(
    const std::string& key_system,
    EmeMediaType media_type,
    const std::string& container_mime_type,
    const std::string& codecs,
    KeySystemConfigSelector::ConfigState* config_state) {
  // From RFC6838: "Both top-level type and subtype names are case-insensitive."
  std::string container_lower = base::ToLowerASCII(container_mime_type);

  // contentTypes must provide a codec string unless the container implies a
  // particular codec. For EME, none of the currently supported containers
  // imply a codec, so |codecs| must be provided.
  if (codecs.empty()) {
    // Since the spec didn't initially require this, add an exemption for
    // some existing containers to give clients time to adapt.
    // TODO(jrummell): Remove this exemption once the number of contentTypes
    // without codecs drops low enough (UMA added in the blink code).
    // http://crbug.com/605661.
    if (container_lower != "audio/webm" && container_lower != "video/webm" &&
        container_lower != "audio/mp4" && container_lower != "video/mp4") {
      return false;
    }
  }

  // Check that |container_mime_type| and |codecs| are supported by Chrome. This
  // is done primarily to validate extended codecs, but it also ensures that the
  // CDM cannot support codecs that Chrome does not (which could complicate the
  // robustness algorithm).
  if (!IsSupportedMediaFormat(container_lower, codecs,
                              CanUseAesDecryptor(key_system))) {
    return false;
  }

  // Check that |container_mime_type| and |codecs| are supported by the CDM.
  // This check does not handle extended codecs, so extended codec information
  // is stripped (extended codec information was checked above).
  std::vector<std::string> stripped_codec_vector;
  ParseCodecString(codecs, &stripped_codec_vector, true);
  EmeConfigRule codecs_rule = key_systems_->GetContentTypeConfigRule(
      key_system, media_type, container_lower, stripped_codec_vector);
  if (!config_state->IsRuleSupported(codecs_rule))
    return false;
  config_state->AddRule(codecs_rule);

  return true;
}

bool KeySystemConfigSelector::GetSupportedCapabilities(
    const std::string& key_system,
    EmeMediaType media_type,
    const blink::WebVector<blink::WebMediaKeySystemMediaCapability>&
        requested_media_capabilities,
    KeySystemConfigSelector::ConfigState* config_state,
    std::vector<blink::WebMediaKeySystemMediaCapability>*
        supported_media_capabilities) {
  // From
  // https://w3c.github.io/encrypted-media/#get-supported-capabilities-for-media-type
  // 1. Let local accumulated capabilities be a local copy of partial
  //    configuration.
  //    (Skipped as we directly update |config_state|. This is safe because we
  //    only do so when at least one requested media capability is supported.)
  // 2. Let supported media capabilities be empty.
  DCHECK_EQ(supported_media_capabilities->size(), 0ul);
  // 3. For each value in requested media capabilities:
  for (size_t i = 0; i < requested_media_capabilities.size(); i++) {
    // 3.1. Let contentType be the value's contentType member.
    // 3.2. Let robustness be the value's robustness member.
    const blink::WebMediaKeySystemMediaCapability& capability =
        requested_media_capabilities[i];
    // 3.3. If contentType is the empty string, return null.
    if (capability.mimeType.isEmpty()) {
      DVLOG(2) << "Rejecting requested configuration because "
               << "a capability contentType was empty.";
      return false;
    }

    // 3.4-3.11. (Implemented by IsSupportedContentType().)
    ConfigState proposed_config_state = *config_state;
    if (!base::IsStringASCII(capability.mimeType) ||
        !base::IsStringASCII(capability.codecs) ||
        !IsSupportedContentType(
            key_system, media_type, capability.mimeType.ascii(),
            capability.codecs.ascii(), &proposed_config_state)) {
      continue;
    }
    // 3.12. If robustness is not the empty string, run the following steps:
    if (!capability.robustness.isEmpty()) {
      // 3.12.1. If robustness is an unrecognized value or not supported by
      //         implementation, continue to the next iteration. String
      //         comparison is case-sensitive.
      if (!base::IsStringASCII(capability.robustness))
        continue;
      EmeConfigRule robustness_rule = key_systems_->GetRobustnessConfigRule(
          key_system, media_type, capability.robustness.ascii());
      if (!proposed_config_state.IsRuleSupported(robustness_rule))
        continue;
      proposed_config_state.AddRule(robustness_rule);
      // 3.12.2. Add robustness to configuration.
      //         (It's already added, we use capability as configuration.)
    }
    // 3.13. If the user agent and implementation do not support playback of
    //       encrypted media data as specified by configuration, including all
    //       media types, in combination with local accumulated capabilities,
    //       continue to the next iteration.
    //       (This is handled when adding rules to |proposed_config_state|.)
    // 3.14. Add configuration to supported media capabilities.
    supported_media_capabilities->push_back(capability);
    // 3.15. Add configuration to local accumulated capabilities.
    *config_state = proposed_config_state;
  }
  // 4. If supported media capabilities is empty, return null.
  if (supported_media_capabilities->empty()) {
    DVLOG(2) << "Rejecting requested configuration because "
             << "no capabilities were supported.";
    return false;
  }
  // 5. Return media type capabilities.
  return true;
}

KeySystemConfigSelector::ConfigurationSupport
KeySystemConfigSelector::GetSupportedConfiguration(
    const std::string& key_system,
    const blink::WebMediaKeySystemConfiguration& candidate,
    ConfigState* config_state,
    blink::WebMediaKeySystemConfiguration* accumulated_configuration) {
  // From
  // http://w3c.github.io/encrypted-media/#get-supported-configuration-and-consent
  // 1. Let accumulated configuration be a new MediaKeySystemConfiguration
  //    dictionary. (Done by caller.)
  // 2. Set the label member of accumulated configuration to equal the label
  //    member of candidate configuration.
  accumulated_configuration->label = candidate.label;

  // 3. If the initDataTypes member of candidate configuration is non-empty,
  //    run the following steps:
  if (!candidate.initDataTypes.isEmpty()) {
    // 3.1. Let supported types be an empty sequence of DOMStrings.
    std::vector<blink::WebEncryptedMediaInitDataType> supported_types;

    // 3.2. For each value in candidate configuration's initDataTypes member:
    for (size_t i = 0; i < candidate.initDataTypes.size(); i++) {
      // 3.2.1. Let initDataType be the value.
      blink::WebEncryptedMediaInitDataType init_data_type =
          candidate.initDataTypes[i];

      // 3.2.2. If the implementation supports generating requests based on
      //        initDataType, add initDataType to supported types. String
      //        comparison is case-sensitive. The empty string is never
      //        supported.
      if (key_systems_->IsSupportedInitDataType(
              key_system, ConvertToEmeInitDataType(init_data_type))) {
        supported_types.push_back(init_data_type);
      }
    }

    // 3.3. If supported types is empty, return null.
    if (supported_types.empty()) {
      DVLOG(2) << "Rejecting requested configuration because "
               << "no initDataType values were supported.";
      return CONFIGURATION_NOT_SUPPORTED;
    }

    // 3.4. Set the initDataTypes member of accumulated configuration to
    //      supported types.
    accumulated_configuration->initDataTypes = supported_types;
  }

  // 4. Let distinctive identifier requirement be the value of candidate
  //    configuration's distinctiveIdentifier member.
  EmeFeatureRequirement distinctive_identifier =
      candidate.distinctiveIdentifier;

  // 5. If distinctive identifier requirement is "optional" and Distinctive
  //    Identifiers are not allowed according to restrictions, set distinctive
  //    identifier requirement to "not-allowed".
  EmeFeatureSupport distinctive_identifier_support =
      key_systems_->GetDistinctiveIdentifierSupport(key_system);
  if (distinctive_identifier == EmeFeatureRequirement::Optional) {
    if (distinctive_identifier_support == EmeFeatureSupport::INVALID ||
        distinctive_identifier_support == EmeFeatureSupport::NOT_SUPPORTED) {
      distinctive_identifier = EmeFeatureRequirement::NotAllowed;
    }
  }

  // 6. Follow the steps for distinctive identifier requirement from the
  //    following list:
  //      - "required": If the implementation does not support use of
  //         Distinctive Identifier(s) in combination with accumulated
  //         configuration and restrictions, return NotSupported.
  //      - "optional": Continue with the following steps.
  //      - "not-allowed": If the implementation requires use Distinctive
  //        Identifier(s) or Distinctive Permanent Identifier(s) in
  //        combination with accumulated configuration and restrictions,
  //        return NotSupported.
  // We also reject OPTIONAL when distinctive identifiers are ALWAYS_ENABLED and
  // permission has already been denied. This would happen anyway later.
  EmeConfigRule di_rule = GetDistinctiveIdentifierConfigRule(
      distinctive_identifier_support, distinctive_identifier);
  if (!config_state->IsRuleSupported(di_rule)) {
    DVLOG(2) << "Rejecting requested configuration because "
             << "the distinctiveIdentifier requirement was not supported.";
    return CONFIGURATION_NOT_SUPPORTED;
  }
  config_state->AddRule(di_rule);

  // 7. Set the distinctiveIdentifier member of accumulated configuration to
  //    equal distinctive identifier requirement.
  accumulated_configuration->distinctiveIdentifier = distinctive_identifier;

  // 8. Let persistent state requirement be equal to the value of candidate
  //    configuration's persistentState member.
  EmeFeatureRequirement persistent_state = candidate.persistentState;

  // 9. If persistent state requirement is "optional" and persisting state is
  //    not allowed according to restrictions, set persistent state requirement
  //    to "not-allowed".
  EmeFeatureSupport persistent_state_support =
      key_systems_->GetPersistentStateSupport(key_system);
  if (persistent_state == EmeFeatureRequirement::Optional) {
    if (persistent_state_support == EmeFeatureSupport::INVALID ||
        persistent_state_support == EmeFeatureSupport::NOT_SUPPORTED) {
      persistent_state = EmeFeatureRequirement::NotAllowed;
    }
  }

  // 10. Follow the steps for persistent state requirement from the following
  //     list:
  //       - "required": If the implementation does not support persisting
  //         state in combination with accumulated configuration and
  //         restrictions, return NotSupported.
  //       - "optional": Continue with the following steps.
  //       - "not-allowed": If the implementation requires persisting state in
  //         combination with accumulated configuration and restrictions,
  //         return NotSupported.
  EmeConfigRule ps_rule =
      GetPersistentStateConfigRule(persistent_state_support, persistent_state);
  if (!config_state->IsRuleSupported(ps_rule)) {
    DVLOG(2) << "Rejecting requested configuration because "
             << "the persistentState requirement was not supported.";
    return CONFIGURATION_NOT_SUPPORTED;
  }
  config_state->AddRule(ps_rule);

  // 11. Set the persistentState member of accumulated configuration to equal
  //     the value of persistent state requirement.
  accumulated_configuration->persistentState = persistent_state;

  // 12. Follow the steps for the first matching condition from the following
  //     list:
  //       - If the sessionTypes member is present in candidate configuration,
  //         let session types be candidate configuration's sessionTypes member.
  //       - Otherwise, let session types be [ "temporary" ].
  //         (Done in MediaKeySystemAccessInitializer.)
  blink::WebVector<blink::WebEncryptedMediaSessionType> session_types =
      candidate.sessionTypes;

  // 13. For each value in session types:
  for (size_t i = 0; i < session_types.size(); i++) {
    // 13.1. Let session type be the value.
    blink::WebEncryptedMediaSessionType session_type = session_types[i];
    if (session_type == blink::WebEncryptedMediaSessionType::Unknown) {
      DVLOG(2) << "Rejecting requested configuration because "
               << "session type was not recognized.";
      return CONFIGURATION_NOT_SUPPORTED;
    }

    // 13.2. If accumulated configuration's persistentState value is
    //       "not-allowed" and the Is persistent session type? algorithm
    //       returns true for session type return NotSupported.
    if (accumulated_configuration->persistentState ==
            EmeFeatureRequirement::NotAllowed &&
        IsPersistentSessionType(session_type)) {
      DVLOG(2) << "Rejecting requested configuration because persistent "
                  "sessions are not allowed.";
      return CONFIGURATION_NOT_SUPPORTED;
    }

    // 13.3. If the implementation does not support session type in combination
    //       with accumulated configuration and restrictions for other reasons,
    //       return NotSupported.
    EmeConfigRule session_type_rule = EmeConfigRule::NOT_SUPPORTED;
    switch (session_type) {
      case blink::WebEncryptedMediaSessionType::Unknown:
        NOTREACHED();
        return CONFIGURATION_NOT_SUPPORTED;
      case blink::WebEncryptedMediaSessionType::Temporary:
        session_type_rule = EmeConfigRule::SUPPORTED;
        break;
      case blink::WebEncryptedMediaSessionType::PersistentLicense:
        session_type_rule = GetSessionTypeConfigRule(
            key_systems_->GetPersistentLicenseSessionSupport(key_system));
        break;
      case blink::WebEncryptedMediaSessionType::PersistentReleaseMessage:
        session_type_rule = GetSessionTypeConfigRule(
            key_systems_->GetPersistentReleaseMessageSessionSupport(
                key_system));
        break;
    }

    if (!config_state->IsRuleSupported(session_type_rule)) {
      DVLOG(2) << "Rejecting requested configuration because "
               << "a required session type was not supported.";
      return CONFIGURATION_NOT_SUPPORTED;
    }
    config_state->AddRule(session_type_rule);

    // 13.4. If accumulated configuration's persistentState value is "optional"
    //       and the result of running the Is persistent session type?
    //       algorithm on session type is true, change accumulated
    //       configuration's persistentState value to "required".
    if (accumulated_configuration->persistentState ==
            EmeFeatureRequirement::Optional &&
        IsPersistentSessionType(session_type)) {
      accumulated_configuration->persistentState =
          EmeFeatureRequirement::Required;
    }
  }

  // 14. Set the sessionTypes member of accumulated configuration to
  //     session types.
  accumulated_configuration->sessionTypes = session_types;

  // 15. If the videoCapabilities and audioCapabilities members in candidate
  //     configuration are both empty, return NotSupported.
  // TODO(jrummell): Enforce this once the deprecation warning is removed.
  // See http://crbug.com/616233.

  // 16. If the videoCapabilities member in candidate configuration is
  //     non-empty:
  std::vector<blink::WebMediaKeySystemMediaCapability> video_capabilities;
  if (!candidate.videoCapabilities.isEmpty()) {
    // 16.1. Let video capabilities be the result of executing the Get
    //       Supported Capabilities for Audio/Video Type algorithm on Video,
    //       candidate configuration's videoCapabilities member, accumulated
    //       configuration, and restrictions.
    // 16.2. If video capabilities is null, return NotSupported.
    if (!GetSupportedCapabilities(key_system, EmeMediaType::VIDEO,
                                  candidate.videoCapabilities, config_state,
                                  &video_capabilities)) {
      return CONFIGURATION_NOT_SUPPORTED;
    }

    // 16.3. Set the videoCapabilities member of accumulated configuration
    //       to video capabilities.
    accumulated_configuration->videoCapabilities = video_capabilities;
  } else {
    // Otherwise set the videoCapabilities member of accumulated configuration
    // to an empty sequence.
    accumulated_configuration->videoCapabilities = video_capabilities;
  }

  // 17. If the audioCapabilities member in candidate configuration is
  //     non-empty:
  std::vector<blink::WebMediaKeySystemMediaCapability> audio_capabilities;
  if (!candidate.audioCapabilities.isEmpty()) {
    // 17.1. Let audio capabilities be the result of executing the Get
    //       Supported Capabilities for Audio/Video Type algorithm on Audio,
    //       candidate configuration's audioCapabilities member, accumulated
    //       configuration, and restrictions.
    // 17.2. If audio capabilities is null, return NotSupported.
    if (!GetSupportedCapabilities(key_system, EmeMediaType::AUDIO,
                                  candidate.audioCapabilities, config_state,
                                  &audio_capabilities)) {
      return CONFIGURATION_NOT_SUPPORTED;
    }

    // 17.3. Set the audioCapabilities member of accumulated configuration
    //       to audio capabilities.
    accumulated_configuration->audioCapabilities = audio_capabilities;
  } else {
    // Otherwise set the audioCapabilities member of accumulated configuration
    // to an empty sequence.
    accumulated_configuration->audioCapabilities = audio_capabilities;
  }

  // 18. If accumulated configuration's distinctiveIdentifier value is
  //     "optional", follow the steps for the first matching condition
  //      from the following list:
  //       - If the implementation requires use Distinctive Identifier(s) or
  //         Distinctive Permanent Identifier(s) for any of the combinations
  //         in accumulated configuration, change accumulated configuration's
  //         distinctiveIdentifier value to "required".
  //       - Otherwise, change accumulated configuration's
  //         distinctiveIdentifier value to "not-allowed".
  if (accumulated_configuration->distinctiveIdentifier ==
      EmeFeatureRequirement::Optional) {
    EmeConfigRule not_allowed_rule = GetDistinctiveIdentifierConfigRule(
        key_systems_->GetDistinctiveIdentifierSupport(key_system),
        EmeFeatureRequirement::NotAllowed);
    EmeConfigRule required_rule = GetDistinctiveIdentifierConfigRule(
        key_systems_->GetDistinctiveIdentifierSupport(key_system),
        EmeFeatureRequirement::Required);
    bool not_allowed_supported =
        config_state->IsRuleSupported(not_allowed_rule);
    bool required_supported = config_state->IsRuleSupported(required_rule);
    // If a distinctive identifier is recommend and that is a possible outcome,
    // prefer that.
    if (required_supported && config_state->IsIdentifierRecommended() &&
        config_state->IsPermissionPossible()) {
      not_allowed_supported = false;
    }
    if (not_allowed_supported) {
      accumulated_configuration->distinctiveIdentifier =
          EmeFeatureRequirement::NotAllowed;
      config_state->AddRule(not_allowed_rule);
    } else if (required_supported) {
      accumulated_configuration->distinctiveIdentifier =
          EmeFeatureRequirement::Required;
      config_state->AddRule(required_rule);
    } else {
      // We should not have passed step 6.
      NOTREACHED();
      return CONFIGURATION_NOT_SUPPORTED;
    }
  }

  // 19. If accumulated configuration's persistentState value is "optional",
  //     follow the steps for the first matching condition from the following
  //     list:
  //       - If the implementation requires persisting state for any of the
  //         combinations in accumulated configuration, change accumulated
  //         configuration's persistentState value to "required".
  //       - Otherwise, change accumulated configuration's persistentState
  //         value to "not-allowed".
  if (accumulated_configuration->persistentState ==
      EmeFeatureRequirement::Optional) {
    EmeConfigRule not_allowed_rule = GetPersistentStateConfigRule(
        key_systems_->GetPersistentStateSupport(key_system),
        EmeFeatureRequirement::NotAllowed);
    EmeConfigRule required_rule = GetPersistentStateConfigRule(
        key_systems_->GetPersistentStateSupport(key_system),
        EmeFeatureRequirement::Required);
    // |distinctiveIdentifier| should not be affected after it is decided.
    DCHECK(not_allowed_rule == EmeConfigRule::NOT_SUPPORTED ||
           not_allowed_rule == EmeConfigRule::PERSISTENCE_NOT_ALLOWED);
    DCHECK(required_rule == EmeConfigRule::NOT_SUPPORTED ||
           required_rule == EmeConfigRule::PERSISTENCE_REQUIRED);
    bool not_allowed_supported =
        config_state->IsRuleSupported(not_allowed_rule);
    bool required_supported = config_state->IsRuleSupported(required_rule);
    if (not_allowed_supported) {
      accumulated_configuration->persistentState =
          EmeFeatureRequirement::NotAllowed;
      config_state->AddRule(not_allowed_rule);
    } else if (required_supported) {
      accumulated_configuration->persistentState =
          EmeFeatureRequirement::Required;
      config_state->AddRule(required_rule);
    } else {
      // We should not have passed step 5.
      NOTREACHED();
      return CONFIGURATION_NOT_SUPPORTED;
    }
  }

  // 20. If implementation in the configuration specified by the combination of
  //     the values in accumulated configuration is not supported or not allowed
  //     in the origin, return NotSupported.
  // TODO(jrummell): can we check that the CDM can't be loaded by the origin?

  // 21. If accumulated configuration's distinctiveIdentifier value is
  //     "required" and the Distinctive Identifier(s) associated with
  //     accumulated configuration are not unique per origin and profile
  //     and clearable:
  // 21.1. Update restrictions to reflect that all configurations described
  //       by accumulated configuration do not have user consent.
  // 21.2. Return ConsentDenied and restrictions.
  // (Not required as data is unique per origin and clearable.)

  // 22. Let consent status and updated restrictions be the result of running
  //     the Get Consent Status algorithm on accumulated configuration,
  //     restrictions and origin and follow the steps for the value of consent
  //     status from the following list:
  //       - "ConsentDenied": Return ConsentDenied and updated restrictions.
  //       - "InformUser": Inform the user that accumulated configuration is
  //         in use in the origin including, specifically, the information
  //         that Distinctive Identifier(s) and/or Distinctive Permanent
  //         Identifier(s) as appropriate will be used if the
  //         distinctiveIdentifier member of accumulated configuration is
  //         "required". Continue to the next step.
  //       - "Allowed": Continue to the next step.
  // Accumulated configuration's distinctiveIdentifier should be "required" or
  // "notallowed"" due to step 18. If it is "required", prompt the user for
  // consent unless it has already been granted.
  if (accumulated_configuration->distinctiveIdentifier ==
      EmeFeatureRequirement::Required) {
    // The caller is responsible for resolving what to do if permission is
    // required but has been denied (it should treat it as NOT_SUPPORTED).
    if (!config_state->IsPermissionGranted())
      return CONFIGURATION_REQUIRES_PERMISSION;
  }

  // 23. Return accumulated configuration.
  return CONFIGURATION_SUPPORTED;
}

void KeySystemConfigSelector::SelectConfig(
    const blink::WebString& key_system,
    const blink::WebVector<blink::WebMediaKeySystemConfiguration>&
        candidate_configurations,
    const blink::WebSecurityOrigin& security_origin,
    bool are_secure_codecs_supported,
    base::Callback<void(const blink::WebMediaKeySystemConfiguration&,
                        const CdmConfig&)> succeeded_cb,
    base::Callback<void(const blink::WebString&)> not_supported_cb) {
  // Continued from requestMediaKeySystemAccess(), step 6, from
  // https://w3c.github.io/encrypted-media/#requestmediakeysystemaccess
  //
  // 6.1 If keySystem is not one of the Key Systems supported by the user
  //     agent, reject promise with a NotSupportedError. String comparison
  //     is case-sensitive.
  if (!key_system.containsOnlyASCII()) {
    not_supported_cb.Run("Only ASCII keySystems are supported");
    return;
  }

  std::string key_system_ascii = key_system.ascii();
  if (!key_systems_->IsSupportedKeySystem(key_system_ascii)) {
    not_supported_cb.Run("Unsupported keySystem");
    return;
  }

  // 6.2-6.4. Implemented by OnSelectConfig().
  // TODO(sandersd): This should be async, ideally not on the main thread.
  std::unique_ptr<SelectionRequest> request(new SelectionRequest());
  request->key_system = key_system_ascii;
  request->candidate_configurations = candidate_configurations;
  request->security_origin = security_origin;
  request->are_secure_codecs_supported = are_secure_codecs_supported;
  request->succeeded_cb = succeeded_cb;
  request->not_supported_cb = not_supported_cb;
  SelectConfigInternal(std::move(request));
}

void KeySystemConfigSelector::SelectConfigInternal(
    std::unique_ptr<SelectionRequest> request) {
  // Continued from requestMediaKeySystemAccess(), step 6, from
  // https://w3c.github.io/encrypted-media/#requestmediakeysystemaccess
  //
  // 6.2. Let implementation be the implementation of keySystem.
  //      (|key_systems_| fills this role.)
  // 6.3. For each value in supportedConfigurations:
  for (size_t i = 0; i < request->candidate_configurations.size(); i++) {
    // 6.3.1. Let candidate configuration be the value.
    // 6.3.2. Let supported configuration be the result of executing the Get
    //        Supported Configuration algorithm on implementation, candidate
    //        configuration, and origin.
    // 6.3.3. If supported configuration is not NotSupported, [initialize
    //        and return a new MediaKeySystemAccess object.]
    ConfigState config_state(request->was_permission_requested,
                             request->is_permission_granted);
    DCHECK(config_state.IsRuleSupported(
        EmeConfigRule::HW_SECURE_CODECS_NOT_ALLOWED));
    if (!request->are_secure_codecs_supported)
      config_state.AddRule(EmeConfigRule::HW_SECURE_CODECS_NOT_ALLOWED);
    blink::WebMediaKeySystemConfiguration accumulated_configuration;
    CdmConfig cdm_config;
    ConfigurationSupport support = GetSupportedConfiguration(
        request->key_system, request->candidate_configurations[i],
        &config_state, &accumulated_configuration);
    switch (support) {
      case CONFIGURATION_NOT_SUPPORTED:
        continue;
      case CONFIGURATION_REQUIRES_PERMISSION:
        if (request->was_permission_requested) {
          DVLOG(2) << "Rejecting requested configuration because "
                   << "permission was denied.";
          continue;
        }
        {
          // Note: the GURL must not be constructed inline because
          // base::Passed(&request) sets |request| to null.
          GURL security_origin(url::Origin(request->security_origin).GetURL());
          media_permission_->RequestPermission(
              MediaPermission::PROTECTED_MEDIA_IDENTIFIER, security_origin,
              base::Bind(&KeySystemConfigSelector::OnPermissionResult,
                         weak_factory_.GetWeakPtr(), base::Passed(&request)));
        }
        return;
      case CONFIGURATION_SUPPORTED:
        cdm_config.allow_distinctive_identifier =
            (accumulated_configuration.distinctiveIdentifier ==
             EmeFeatureRequirement::Required);
        cdm_config.allow_persistent_state =
            (accumulated_configuration.persistentState ==
             EmeFeatureRequirement::Required);
        cdm_config.use_hw_secure_codecs =
            config_state.AreHwSecureCodecsRequired();
        request->succeeded_cb.Run(accumulated_configuration, cdm_config);
        return;
    }
  }

  // 6.4. Reject promise with a NotSupportedError.
  request->not_supported_cb.Run(
      "None of the requested configurations were supported.");
}

void KeySystemConfigSelector::OnPermissionResult(
    std::unique_ptr<SelectionRequest> request,
    bool is_permission_granted) {
  request->was_permission_requested = true;
  request->is_permission_granted = is_permission_granted;
  SelectConfigInternal(std::move(request));
}

}  // namespace media
