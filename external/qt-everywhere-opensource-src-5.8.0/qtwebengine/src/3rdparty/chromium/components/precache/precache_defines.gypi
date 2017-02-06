# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # These values are duplicated in the GN build in:
    # //components/precache/core:precache_config
    'precache_config_settings_url%': 'https://www.gstatic.com/chrome/wifiprefetch/precache_config',
    'precache_manifest_url_prefix%': 'https://www.gstatic.com/chrome/wifiprefetch/hosts/',
  },
  'conditions': [
    ['precache_config_settings_url != ""', {
      'defines': [
        'PRECACHE_CONFIG_SETTINGS_URL="<(precache_config_settings_url)"',
      ],
    }],
    ['precache_manifest_url_prefix != ""', {
      'defines': [
        'PRECACHE_MANIFEST_URL_PREFIX="<(precache_manifest_url_prefix)"',
      ],
    }],
  ],
}
