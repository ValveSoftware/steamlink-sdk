# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'widevine_cdm_version_h_file%': 'widevine_cdm_version.h',
    'widevine_cdm_binary_files%': [],
    'conditions': [
      [ 'branding == "Chrome"', {
        'conditions': [
          [ 'chromeos == 1', {
            'widevine_cdm_version_h_file%':
                'chromeos/<(target_arch)/widevine_cdm_version.h',
            'widevine_cdm_binary_files%': [
              'chromeos/<(target_arch)/libwidevinecdm.so',
            ],
          }],
          [ 'OS == "linux" and chromeos == 0', {
            'widevine_cdm_version_h_file%':
                'linux/<(target_arch)/widevine_cdm_version.h',
            'widevine_cdm_binary_files%': [
              'linux/<(target_arch)/libwidevinecdm.so',
            ],
          }],
          [ 'OS == "mac"', {
            'widevine_cdm_version_h_file%':
                'mac/<(target_arch)/widevine_cdm_version.h',
            'widevine_cdm_binary_files%': [
              'mac/<(target_arch)/libwidevinecdm.dylib',
            ],
          }],
          [ 'OS == "win"', {
            'widevine_cdm_version_h_file%':
                'win/<(target_arch)/widevine_cdm_version.h',
            'widevine_cdm_binary_files%': [
              'win/<(target_arch)/widevinecdm.dll',
              'win/<(target_arch)/widevinecdm.dll.lib',
            ],
          }],
        ],
      }],
      [ 'OS == "android"', {
        'widevine_cdm_version_h_file%':
            'android/widevine_cdm_version.h',
      }],
    ],
  },
  # Always provide a target, so we can put the logic about whether there's
  # anything to be done in this file (instead of a higher-level .gyp file).
  'targets': [
    {
      # GN version: //third_party/widevine/cdm:adapter
      'target_name': 'widevinecdmadapter',
      'type': 'none',
      'conditions': [
        [ 'branding == "Chrome" and enable_pepper_cdms==1', {
          'dependencies': [
            '<(DEPTH)/ppapi/ppapi.gyp:ppapi_cpp',
            '<(DEPTH)/media/media_cdm_adapter.gyp:cdmadapter',
            'widevine_cdm_version_h',
            'widevine_cdm_binaries',
          ],
          'conditions': [
            [ 'os_posix == 1 and OS != "mac"', {
              'libraries': [
                # Copied by widevine_cdm_binaries.
                '<(PRODUCT_DIR)/libwidevinecdm.so',
              ],
            }],
            [ 'OS == "win"', {
              'libraries': [
                # Copied by widevine_cdm_binaries.
                '<(PRODUCT_DIR)/widevinecdm.dll.lib',
              ],
            }],
            [ 'OS == "mac"', {
              'libraries': [
                # Copied by widevine_cdm_binaries.
                '<(PRODUCT_DIR)/libwidevinecdm.dylib',
              ],
            }],
          ],
        }],
      ],
    },
    {
      # GN version: //third_party/widevine/cdm:version_h
      'target_name': 'widevine_cdm_version_h',
      'type': 'none',
      'copies': [{
        'destination': '<(SHARED_INTERMEDIATE_DIR)',
        'files': [ '<(widevine_cdm_version_h_file)' ],
      }],
    },
    {
      # GN version: //third_party/widevine/cdm:binaries
      'target_name': 'widevine_cdm_binaries',
      'type': 'none',
      'conditions': [
        [ 'OS=="mac"', {
          'xcode_settings': {
            'COPY_PHASE_STRIP': 'NO',
          }
        }],
      ],
      'copies': [{
        # TODO(ddorwin): Do we need a sub-directory? We either need a
        # sub-directory or to rename manifest.json before we can copy it.
        'destination': '<(PRODUCT_DIR)',
        'files': [ '<@(widevine_cdm_binary_files)' ],
      }],
    },
    {
      'target_name': 'widevine_test_license_server',
      'type': 'none',
      'conditions': [
        [ 'branding == "Chrome" and OS == "linux"', {
          'dependencies': [
            '<(DEPTH)/third_party/widevine/test/license_server/license_server.gyp:test_license_server',
          ],
        }],
      ],
    },
  ],
}
