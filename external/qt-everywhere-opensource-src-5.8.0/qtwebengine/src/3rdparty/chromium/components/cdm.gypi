# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN verison: //components/cdm/common
      'target_name': 'cdm_common',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_common',
        '../ipc/ipc.gyp:ipc',
      ],
      'sources': [
        'cdm/common/cdm_message_generator.cc',
        'cdm/common/cdm_message_generator.h',
        'cdm/common/cdm_messages_android.h',
      ],
      'conditions': [
        ['OS == "android"', {
          'sources': [
            'cdm/common/widevine_drm_delegate_android.cc',
            'cdm/common/widevine_drm_delegate_android.h',
          ],
        }],
      ],
    },
    {
      # GN version: //components/cdm/renderer
      'target_name': 'cdm_renderer',
      'type': 'static_library',
      'dependencies': [
        'cdm_common',
        '../base/base.gyp:base',
        '../content/content.gyp:content_renderer',
        '../third_party/widevine/cdm/widevine_cdm.gyp:widevine_cdm_version_h',
      ],
      'include_dirs': [
        # Needed by widevine_key_system_properties.cc.
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'sources': [
        'cdm/renderer/widevine_key_system_properties.cc',
        'cdm/renderer/widevine_key_system_properties.h',
      ],
      'conditions': [
        ['OS == "android"', {
          'sources': [
            'cdm/renderer/android_key_systems.cc',
            'cdm/renderer/android_key_systems.h',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          # GN version: //components/cdm/browser
          'target_name': 'cdm_browser',
          'type': 'static_library',
          'dependencies': [
            'cdm_common',
            '../base/base.gyp:base',
            '../content/content.gyp:content_browser',
            '../content/content.gyp:content_common',
            '../media/media.gyp:media',
          ],
          'sources': [
            'cdm/browser/cdm_message_filter_android.cc',
            'cdm/browser/cdm_message_filter_android.h',
          ],
        },
      ],
    }],
  ],
}
