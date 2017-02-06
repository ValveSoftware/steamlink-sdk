# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['chromeos==1', {
      'targets': [{
        'target_name': 'ownership',
        'type': '<(component)',
        'dependencies': [
          '<(DEPTH)/base/base.gyp:base',
          '<(DEPTH)/components/components.gyp:keyed_service_core',
          '<(DEPTH)/components/components.gyp:policy_component_common',
          '<(DEPTH)/crypto/crypto.gyp:crypto',
        ],
        'defines': [
          'OWNERSHIP_IMPLEMENTATION',
        ],
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
        'sources': [
          'ownership/mock_owner_key_util.cc',
          'ownership/mock_owner_key_util.h',
          'ownership/owner_key_util.cc',
          'ownership/owner_key_util.h',
          'ownership/owner_key_util_impl.cc',
          'ownership/owner_key_util_impl.h',
          'ownership/owner_settings_service.cc',
          'ownership/owner_settings_service.h',
         ],
        'conditions': [
          ['configuration_policy==1', {
            'dependencies': [
              '<(DEPTH)/components/components.gyp:cloud_policy_proto',
              '<(DEPTH)/components/components.gyp:policy',
            ],
          }],
          ['use_nss_certs==1', {
            'dependencies': [
              '../build/linux/system.gyp:nss',
            ],
          }],
        ],
      }],
    }],
  ],
}
