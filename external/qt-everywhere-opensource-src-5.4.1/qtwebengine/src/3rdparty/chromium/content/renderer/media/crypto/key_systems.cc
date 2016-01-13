// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/crypto/key_systems.h"

#include <string>

#include "base/containers/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "content/public/common/content_client.h"
#include "content/public/common/eme_codec.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/key_system_info.h"
#include "content/renderer/media/crypto/key_systems_support_uma.h"

#if defined(OS_ANDROID)
#include "media/base/android/media_codec_bridge.h"
#endif

#include "widevine_cdm_version.h" // In SHARED_INTERMEDIATE_DIR.

namespace content {

const char kClearKeyKeySystem[] = "org.w3.clearkey";
const char kPrefixedClearKeyKeySystem[] = "webkit-org.w3.clearkey";
const char kUnsupportedClearKeyKeySystem[] = "unsupported-org.w3.clearkey";

struct CodecMask {
  const char* type;
  EmeCodec mask;
};

// Mapping between container types and the masks of associated codecs.
// Only audio codec can belong to a "audio/*" container. Both audio and video
// codecs can belong to a "video/*" container.
CodecMask kContainerCodecMasks[] = {
    {"audio/webm", EME_CODEC_WEBM_AUDIO_ALL},
    {"video/webm", EME_CODEC_WEBM_ALL},
#if defined(USE_PROPRIETARY_CODECS)
    {"audio/mp4", EME_CODEC_MP4_AUDIO_ALL},
    {"video/mp4", EME_CODEC_MP4_ALL}
#endif  // defined(USE_PROPRIETARY_CODECS)
};

// Mapping between codec types and their masks.
CodecMask kCodecMasks[] = {
    {"vorbis", EME_CODEC_WEBM_VORBIS},
    {"vp8", EME_CODEC_WEBM_VP8},
    {"vp8.0", EME_CODEC_WEBM_VP8},
    {"vp9", EME_CODEC_WEBM_VP9},
    {"vp9.0", EME_CODEC_WEBM_VP9},
#if defined(USE_PROPRIETARY_CODECS)
    {"mp4a", EME_CODEC_MP4_AAC},
    {"avc1", EME_CODEC_MP4_AVC1},
    {"avc3", EME_CODEC_MP4_AVC1}
#endif  // defined(USE_PROPRIETARY_CODECS)
};

static void AddClearKey(std::vector<KeySystemInfo>* concrete_key_systems) {
  KeySystemInfo info(kClearKeyKeySystem);

  // On Android, Vorbis, VP8, AAC and AVC1 are supported in MediaCodec:
  // http://developer.android.com/guide/appendix/media-formats.html
  // VP9 support is device dependent.

  info.supported_codecs = EME_CODEC_WEBM_ALL;

#if defined(OS_ANDROID)
  // Temporarily disable VP9 support for Android.
  // TODO(xhwang): Use mime_util.h to query VP9 support on Android.
  info.supported_codecs &= ~EME_CODEC_WEBM_VP9;
#endif  // defined(OS_ANDROID)

#if defined(USE_PROPRIETARY_CODECS)
  info.supported_codecs |= EME_CODEC_MP4_ALL;
#endif  // defined(USE_PROPRIETARY_CODECS)

  info.use_aes_decryptor = true;

  concrete_key_systems->push_back(info);
}

class KeySystems {
 public:
  static KeySystems& GetInstance();

  void UpdateIfNeeded();

  bool IsConcreteSupportedKeySystem(const std::string& key_system);

  bool IsSupportedKeySystemWithMediaMimeType(
      const std::string& mime_type,
      const std::vector<std::string>& codecs,
      const std::string& key_system);

  bool UseAesDecryptor(const std::string& concrete_key_system);

#if defined(ENABLE_PEPPER_CDMS)
  std::string GetPepperType(const std::string& concrete_key_system);
#endif

  void AddContainerMask(const std::string& container, uint32 mask);
  void AddCodecMask(const std::string& codec, uint32 mask);

 private:
  void UpdateSupportedKeySystems();

  void AddConcreteSupportedKeySystems(
      const std::vector<KeySystemInfo>& concrete_key_systems);

  void AddConcreteSupportedKeySystem(
      const std::string& key_system,
      bool use_aes_decryptor,
#if defined(ENABLE_PEPPER_CDMS)
      const std::string& pepper_type,
#endif
      SupportedCodecs supported_codecs,
      const std::string& parent_key_system);

  friend struct base::DefaultLazyInstanceTraits<KeySystems>;

  struct KeySystemProperties {
    KeySystemProperties() : use_aes_decryptor(false) {}

    bool use_aes_decryptor;
#if defined(ENABLE_PEPPER_CDMS)
    std::string pepper_type;
#endif
    SupportedCodecs supported_codecs;
  };

  typedef base::hash_map<std::string, KeySystemProperties>
      KeySystemPropertiesMap;
  typedef base::hash_map<std::string, std::string> ParentKeySystemMap;
  typedef base::hash_map<std::string, EmeCodec> CodecMaskMap;

  KeySystems();
  ~KeySystems() {}

  // Returns whether a |container| type is supported by checking
  // |key_system_supported_codecs|.
  // TODO(xhwang): Update this to actually check initDataType support.
  bool IsSupportedContainer(const std::string& container,
                            SupportedCodecs key_system_supported_codecs) const;

  // Returns true if all |codecs| are supported in |container| by checking
  // |key_system_supported_codecs|.
  bool IsSupportedContainerAndCodecs(
      const std::string& container,
      const std::vector<std::string>& codecs,
      SupportedCodecs key_system_supported_codecs) const;

  // Map from key system string to capabilities.
  KeySystemPropertiesMap concrete_key_system_map_;

  // Map from parent key system to the concrete key system that should be used
  // to represent its capabilities.
  ParentKeySystemMap parent_key_system_map_;

  KeySystemsSupportUMA key_systems_support_uma_;

  CodecMaskMap container_codec_masks_;
  CodecMaskMap codec_masks_;

  bool needs_update_;
  base::Time last_update_time_;

  // Makes sure all methods are called from the same thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(KeySystems);
};

static base::LazyInstance<KeySystems> g_key_systems = LAZY_INSTANCE_INITIALIZER;

KeySystems& KeySystems::GetInstance() {
  KeySystems& key_systems = g_key_systems.Get();
  key_systems.UpdateIfNeeded();
  return key_systems;
}

// Because we use a LazyInstance, the key systems info must be populated when
// the instance is lazily initiated.
KeySystems::KeySystems() : needs_update_(true) {
  // Build container and codec masks for quick look up.
  for (size_t i = 0; i < arraysize(kContainerCodecMasks); ++i) {
    const CodecMask& container_codec_mask = kContainerCodecMasks[i];
    DCHECK(container_codec_masks_.find(container_codec_mask.type) ==
           container_codec_masks_.end());
    container_codec_masks_[container_codec_mask.type] =
        container_codec_mask.mask;
  }
  for (size_t i = 0; i < arraysize(kCodecMasks); ++i) {
    const CodecMask& codec_mask = kCodecMasks[i];
    DCHECK(codec_masks_.find(codec_mask.type) == codec_masks_.end());
    codec_masks_[codec_mask.type] = codec_mask.mask;
  }

  UpdateSupportedKeySystems();

#if defined(WIDEVINE_CDM_AVAILABLE)
  key_systems_support_uma_.AddKeySystemToReport(kWidevineKeySystem);
#endif  // defined(WIDEVINE_CDM_AVAILABLE)
}

void KeySystems::UpdateIfNeeded() {
#if defined(WIDEVINE_CDM_AVAILABLE)
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!needs_update_)
    return;

  // The update could involve a sync IPC to the browser process. Use a minimum
  // update interval to avoid unnecessary frequent IPC to the browser.
  static const int kMinUpdateIntervalInSeconds = 1;
  base::Time now = base::Time::Now();
  if (now - last_update_time_ <
      base::TimeDelta::FromSeconds(kMinUpdateIntervalInSeconds)) {
    return;
  }

  UpdateSupportedKeySystems();
#endif
}

void KeySystems::UpdateSupportedKeySystems() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(needs_update_);
  concrete_key_system_map_.clear();
  parent_key_system_map_.clear();

  // Build KeySystemInfo.
  std::vector<KeySystemInfo> key_systems_info;
  GetContentClient()->renderer()->AddKeySystems(&key_systems_info);
  // Clear Key is always supported.
  AddClearKey(&key_systems_info);

  AddConcreteSupportedKeySystems(key_systems_info);

#if defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_IS_COMPONENT)
  if (IsConcreteSupportedKeySystem(kWidevineKeySystem))
    needs_update_ = false;
#endif

  last_update_time_ = base::Time::Now();
}

void KeySystems::AddConcreteSupportedKeySystems(
    const std::vector<KeySystemInfo>& concrete_key_systems) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(concrete_key_system_map_.empty());
  DCHECK(parent_key_system_map_.empty());

  for (size_t i = 0; i < concrete_key_systems.size(); ++i) {
    const KeySystemInfo& key_system_info = concrete_key_systems[i];
    AddConcreteSupportedKeySystem(key_system_info.key_system,
                                  key_system_info.use_aes_decryptor,
#if defined(ENABLE_PEPPER_CDMS)
                                  key_system_info.pepper_type,
#endif
                                  key_system_info.supported_codecs,
                                  key_system_info.parent_key_system);
  }
}

void KeySystems::AddConcreteSupportedKeySystem(
    const std::string& concrete_key_system,
    bool use_aes_decryptor,
#if defined(ENABLE_PEPPER_CDMS)
    const std::string& pepper_type,
#endif
    SupportedCodecs supported_codecs,
    const std::string& parent_key_system) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!IsConcreteSupportedKeySystem(concrete_key_system))
      << "Key system '" << concrete_key_system << "' already registered";
  DCHECK(parent_key_system_map_.find(concrete_key_system) ==
         parent_key_system_map_.end())
      <<  "'" << concrete_key_system << " is already registered as a parent";

  KeySystemProperties properties;
  properties.use_aes_decryptor = use_aes_decryptor;
#if defined(ENABLE_PEPPER_CDMS)
  DCHECK_EQ(use_aes_decryptor, pepper_type.empty());
  properties.pepper_type = pepper_type;
#endif

  properties.supported_codecs = supported_codecs;

  concrete_key_system_map_[concrete_key_system] = properties;

  if (!parent_key_system.empty()) {
    DCHECK(!IsConcreteSupportedKeySystem(parent_key_system))
        << "Parent '" << parent_key_system << "' already registered concrete";
    DCHECK(parent_key_system_map_.find(parent_key_system) ==
           parent_key_system_map_.end())
        << "Parent '" << parent_key_system << "' already registered";
    parent_key_system_map_[parent_key_system] = concrete_key_system;
  }
}

bool KeySystems::IsConcreteSupportedKeySystem(const std::string& key_system) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return concrete_key_system_map_.find(key_system) !=
      concrete_key_system_map_.end();
}

bool KeySystems::IsSupportedContainer(
    const std::string& container,
    SupportedCodecs key_system_supported_codecs) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!container.empty());

  // When checking container support for EME, "audio/foo" should be treated the
  // same as "video/foo". Convert the |container| to achieve this.
  // TODO(xhwang): Replace this with real checks against supported initDataTypes
  // combined with supported demuxers.
  std::string canonical_container = container;
  if (container.find("audio/") == 0)
    canonical_container.replace(0, 6, "video/");

  CodecMaskMap::const_iterator container_iter =
      container_codec_masks_.find(canonical_container);
  // Unrecognized container.
  if (container_iter == container_codec_masks_.end())
    return false;

  EmeCodec container_codec_mask = container_iter->second;
  // A container is supported iif at least one codec in that container is
  // supported.
  return (container_codec_mask & key_system_supported_codecs) != 0;
}

bool KeySystems::IsSupportedContainerAndCodecs(
    const std::string& container,
    const std::vector<std::string>& codecs,
    SupportedCodecs key_system_supported_codecs) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!container.empty());
  DCHECK(!codecs.empty());
  DCHECK(IsSupportedContainer(container, key_system_supported_codecs));

  CodecMaskMap::const_iterator container_iter =
      container_codec_masks_.find(container);
  EmeCodec container_codec_mask = container_iter->second;

  for (size_t i = 0; i < codecs.size(); ++i) {
    const std::string& codec = codecs[i];
    if (codec.empty())
      continue;
    CodecMaskMap::const_iterator codec_iter = codec_masks_.find(codec);
    if (codec_iter == codec_masks_.end())  // Unrecognized codec.
      return false;

    EmeCodec codec_mask = codec_iter->second;
    if (!(codec_mask & key_system_supported_codecs))  // Unsupported codec.
      return false;

    // Unsupported codec/container combination, e.g. "video/webm" and "avc1".
    if (!(codec_mask & container_codec_mask))
      return false;
  }

  return true;
}

bool KeySystems::IsSupportedKeySystemWithMediaMimeType(
    const std::string& mime_type,
    const std::vector<std::string>& codecs,
    const std::string& key_system) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If |key_system| is a parent key_system, use its concrete child.
  // Otherwise, use |key_system|.
  std::string concrete_key_system;
  ParentKeySystemMap::iterator parent_key_system_iter =
      parent_key_system_map_.find(key_system);
  if (parent_key_system_iter != parent_key_system_map_.end())
    concrete_key_system = parent_key_system_iter->second;
  else
    concrete_key_system = key_system;

  bool has_type = !mime_type.empty();

  key_systems_support_uma_.ReportKeySystemQuery(key_system, has_type);

  // Check key system support.
  KeySystemPropertiesMap::const_iterator key_system_iter =
      concrete_key_system_map_.find(concrete_key_system);
  if (key_system_iter == concrete_key_system_map_.end())
    return false;

  key_systems_support_uma_.ReportKeySystemSupport(key_system, false);

  if (!has_type) {
    DCHECK(codecs.empty());
    return true;
  }

  SupportedCodecs key_system_supported_codecs =
      key_system_iter->second.supported_codecs;

  if (!IsSupportedContainer(mime_type, key_system_supported_codecs))
    return false;

  if (!codecs.empty() &&
      !IsSupportedContainerAndCodecs(
          mime_type, codecs, key_system_supported_codecs)) {
    return false;
  }

  key_systems_support_uma_.ReportKeySystemSupport(key_system, true);
  return true;
}

bool KeySystems::UseAesDecryptor(const std::string& concrete_key_system) {
  DCHECK(thread_checker_.CalledOnValidThread());

  KeySystemPropertiesMap::iterator key_system_iter =
      concrete_key_system_map_.find(concrete_key_system);
  if (key_system_iter == concrete_key_system_map_.end()) {
      DLOG(FATAL) << concrete_key_system << " is not a known concrete system";
      return false;
  }

  return key_system_iter->second.use_aes_decryptor;
}

#if defined(ENABLE_PEPPER_CDMS)
std::string KeySystems::GetPepperType(const std::string& concrete_key_system) {
  DCHECK(thread_checker_.CalledOnValidThread());

  KeySystemPropertiesMap::iterator key_system_iter =
      concrete_key_system_map_.find(concrete_key_system);
  if (key_system_iter == concrete_key_system_map_.end()) {
      DLOG(FATAL) << concrete_key_system << " is not a known concrete system";
      return std::string();
  }

  const std::string& type = key_system_iter->second.pepper_type;
  DLOG_IF(FATAL, type.empty()) << concrete_key_system << " is not Pepper-based";
  return type;
}
#endif

void KeySystems::AddContainerMask(const std::string& container, uint32 mask) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(container_codec_masks_.find(container) ==
         container_codec_masks_.end());

  container_codec_masks_[container] = static_cast<EmeCodec>(mask);
}

void KeySystems::AddCodecMask(const std::string& codec, uint32 mask) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(codec_masks_.find(codec) == codec_masks_.end());

  codec_masks_[codec] = static_cast<EmeCodec>(mask);
}

//------------------------------------------------------------------------------

std::string GetUnprefixedKeySystemName(const std::string& key_system) {
  if (key_system == kClearKeyKeySystem)
    return kUnsupportedClearKeyKeySystem;

  if (key_system == kPrefixedClearKeyKeySystem)
    return kClearKeyKeySystem;

  return key_system;
}

std::string GetPrefixedKeySystemName(const std::string& key_system) {
  DCHECK_NE(key_system, kPrefixedClearKeyKeySystem);

  if (key_system == kClearKeyKeySystem)
    return kPrefixedClearKeyKeySystem;

  return key_system;
}

bool IsConcreteSupportedKeySystem(const std::string& key_system) {
  return KeySystems::GetInstance().IsConcreteSupportedKeySystem(key_system);
}

bool IsSupportedKeySystemWithMediaMimeType(
    const std::string& mime_type,
    const std::vector<std::string>& codecs,
    const std::string& key_system) {
  return KeySystems::GetInstance().IsSupportedKeySystemWithMediaMimeType(
      mime_type, codecs, key_system);
}

std::string KeySystemNameForUMA(const std::string& key_system) {
  if (key_system == kClearKeyKeySystem)
    return "ClearKey";
#if defined(WIDEVINE_CDM_AVAILABLE)
  if (key_system == kWidevineKeySystem)
    return "Widevine";
#endif  // WIDEVINE_CDM_AVAILABLE
  return "Unknown";
}

bool CanUseAesDecryptor(const std::string& concrete_key_system) {
  return KeySystems::GetInstance().UseAesDecryptor(concrete_key_system);
}

#if defined(ENABLE_PEPPER_CDMS)
std::string GetPepperType(const std::string& concrete_key_system) {
  return KeySystems::GetInstance().GetPepperType(concrete_key_system);
}
#endif

// These two functions are for testing purpose only. The declaration in the
// header file is guarded by "#if defined(UNIT_TEST)" so that they can be used
// by tests but not non-test code. However, this .cc file is compiled as part of
// "content" where "UNIT_TEST" is not defined. So we need to specify
// "CONTENT_EXPORT" here again so that they are visible to tests.

CONTENT_EXPORT void AddContainerMask(const std::string& container,
                                     uint32 mask) {
  KeySystems::GetInstance().AddContainerMask(container, mask);
}

CONTENT_EXPORT void AddCodecMask(const std::string& codec, uint32 mask) {
  KeySystems::GetInstance().AddCodecMask(codec, mask);
}

}  // namespace content
