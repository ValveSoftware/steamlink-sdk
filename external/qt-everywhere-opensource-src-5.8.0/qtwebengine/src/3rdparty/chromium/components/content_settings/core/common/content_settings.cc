// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/content_settings.h"

#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "build/build_config.h"

ContentSetting IntToContentSetting(int content_setting) {
  return ((content_setting < 0) ||
          (content_setting >= CONTENT_SETTING_NUM_SETTINGS)) ?
      CONTENT_SETTING_DEFAULT : static_cast<ContentSetting>(content_setting);
}

// WARNING: This array should not be reordered or removed as it is used for
// histogram values. If a ContentSettingsType value has been removed, the entry
// must be replaced by a placeholder. It should correspond directly to the
// ContentType enum in histograms.xml.
// TODO(raymes): We should use a sparse histogram here on the hash of the
// content settings type name instead.
ContentSettingsType kHistogramOrder[] = {
    CONTENT_SETTINGS_TYPE_COOKIES,
    CONTENT_SETTINGS_TYPE_IMAGES,
    CONTENT_SETTINGS_TYPE_JAVASCRIPT,
    CONTENT_SETTINGS_TYPE_PLUGINS,
    CONTENT_SETTINGS_TYPE_POPUPS,
    CONTENT_SETTINGS_TYPE_GEOLOCATION,
    CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
    CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE,
    CONTENT_SETTINGS_TYPE_FULLSCREEN,
    CONTENT_SETTINGS_TYPE_MOUSELOCK,
    CONTENT_SETTINGS_TYPE_MIXEDSCRIPT,
    CONTENT_SETTINGS_TYPE_DEFAULT,  // MEDIASTREAM (removed).
    CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
    CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
    CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS,
    CONTENT_SETTINGS_TYPE_PPAPI_BROKER,
    CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS,
    CONTENT_SETTINGS_TYPE_MIDI_SYSEX,
    CONTENT_SETTINGS_TYPE_PUSH_MESSAGING,
    CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS,
    CONTENT_SETTINGS_TYPE_DEFAULT,  // METRO_SWITCH_TO_DESKTOP (deprecated).
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
    CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER,
#else
    CONTENT_SETTINGS_TYPE_DEFAULT,  // PROTECTED_MEDIA_IDENTIFIER (mobile only).
#endif
    CONTENT_SETTINGS_TYPE_APP_BANNER,
    CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT,
    CONTENT_SETTINGS_TYPE_DURABLE_STORAGE,
    CONTENT_SETTINGS_TYPE_KEYGEN,
    CONTENT_SETTINGS_TYPE_BLUETOOTH_GUARD,
    CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC,
    CONTENT_SETTINGS_TYPE_AUTOPLAY,
};

int ContentSettingTypeToHistogramValue(ContentSettingsType content_setting,
                                       size_t* num_values) {
  // Translate the list above into a map for fast lookup.
  typedef base::hash_map<int, int> Map;
  CR_DEFINE_STATIC_LOCAL(Map, kMap, ());
  if (kMap.empty()) {
    for (size_t i = 0; i < arraysize(kHistogramOrder); ++i) {
      if (kHistogramOrder[i] != CONTENT_SETTINGS_TYPE_DEFAULT)
        kMap[kHistogramOrder[i]] = static_cast<int>(i);
    }
  }

  DCHECK(ContainsKey(kMap, content_setting));
  *num_values = arraysize(kHistogramOrder);
  return kMap[content_setting];
}

ContentSettingPatternSource::ContentSettingPatternSource(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSetting setting,
    const std::string& source,
    bool incognito)
    : primary_pattern(primary_pattern),
      secondary_pattern(secondary_pattern),
      setting(setting),
      source(source),
      incognito(incognito) {}

ContentSettingPatternSource::ContentSettingPatternSource()
    : setting(CONTENT_SETTING_DEFAULT), incognito(false) {
}

ContentSettingPatternSource::ContentSettingPatternSource(
    const ContentSettingPatternSource& other) = default;

RendererContentSettingRules::RendererContentSettingRules() {}

RendererContentSettingRules::~RendererContentSettingRules() {}
