# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'precache_config_settings_url%': 'http://www.gstatic.com/chrome/wifiprefetch/precache_config',
    'precache_manifest_url_prefix%': 'http://www.gstatic.com/chrome/wifiprefetch/precache_manifest_',
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
