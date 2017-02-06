# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'common_constants_sources': [
      'common/chrome_constants.cc',
      'common/chrome_constants.h',
      'common/chrome_features.cc',
      'common/chrome_features.h',
      'common/chrome_icon_resources_win.cc',
      'common/chrome_icon_resources_win.h',
      'common/chrome_paths.cc',
      'common/chrome_paths.h',
      'common/chrome_paths_android.cc',
      'common/chrome_paths_internal.h',
      'common/chrome_paths_linux.cc',
      'common/chrome_paths_mac.mm',
      'common/chrome_paths_win.cc',
      'common/chrome_switches.cc',
      'common/chrome_switches.h',
      'common/env_vars.cc',
      'common/env_vars.h',
      'common/pref_font_script_names-inl.h',
      'common/pref_font_webkit_names.h',
      'common/pref_names.cc',
      'common/pref_names.h',
    ],
  },

  'includes': [
    '../build/util/version.gypi',
  ],

  'targets': [
    {
      # GN version: //chrome/common:version_header
      'target_name': 'version_header',
      'type': 'none',
      'hard_dependency': 1,
      'actions': [
        {
          'action_name': 'version_header',
          'variables': {
            'lastchange_path':
              '<(DEPTH)/build/util/LASTCHANGE',
            'branding_path': 'app/theme/<(branding_path_component)/BRANDING',
          },
          'inputs': [
            '<(version_path)',
            '<(branding_path)',
            '<(lastchange_path)',
            'common/chrome_version.h.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome/common/chrome_version.h',
          ],
          'action': [
            'python',
            '<(version_py_path)',
            '-f', '<(version_path)',
            '-f', '<(branding_path)',
            '-f', '<(lastchange_path)',
            'common/chrome_version.h.in',
            '<@(_outputs)',
          ],
          'message': 'Generating version header file: <@(_outputs)',
        },
      ],
    },
    {
      # GN version: //chrome/common:constants
      'target_name': 'common_constants',
      'type': 'static_library',
      'hard_dependency': 1,  # Because of transitive dep on version_header.
      'sources': [
        '<@(common_constants_sources)'
      ],
      'include_dirs': [
        '<(SHARED_INTERMEDIATE_DIR)',  # Needed by chrome_paths.cc.
      ],
      'dependencies': [
        'version_header',
        'chrome_features.gyp:chrome_common_features',
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../components/components.gyp:bookmarks_common',
        '../media/media.gyp:cdm_paths',  # Needed by chrome_paths.cc.
        '../third_party/widevine/cdm/widevine_cdm.gyp:widevine_cdm_version_h',
      ],
      'target_conditions': [
        ['OS=="ios"', {
          # iOS needs chrome_paths_mac, which is excluded by filename rules;
          # re-add it in target_conditionals so it's after that exclusion.
          'sources/': [
            ['include', '^common/chrome_paths_mac\\.mm$'],
          ],
        }],
      ],
      'conditions': [
        ['disable_nacl==0', {
          'dependencies': [
            '../components/nacl.gyp:nacl_switches',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'common_constants_win64',
          'hard_dependency': 1,  # Because of transitive dep on version_header.
          'type': 'static_library',
          'sources': [
            '<@(common_constants_sources)'
          ],
          'include_dirs': [
            '<(SHARED_INTERMEDIATE_DIR)',  # Needed by chrome_paths.cc.
          ],
          'dependencies': [
            'version_header',
            'chrome_features.gyp:chrome_common_features',
            '../base/base.gyp:base_win64',
            '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations_win64',
            '../components/nacl.gyp:nacl_switches_win64',
            '../third_party/widevine/cdm/widevine_cdm.gyp:widevine_cdm_version_h',
          ],
          'defines': [
            '<@(nacl_win64_defines)',
            'COMPILE_CONTENT_STATICALLY',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
  ],
}
