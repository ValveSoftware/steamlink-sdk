# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # Allow widevinecdmadapter to be built in Chromium.
    'variables': {
      'enable_widevine%': 0,
    },
    'enable_widevine%': '<(enable_widevine)',
    'widevine_cdm_version_h_file%': 'widevine_cdm_version.h',
    'widevine_cdm_binary_files%': [],
    'widevine_cdm_manifest_file%': [],
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
            'widevine_cdm_manifest_file%': [
              'mac/<(target_arch)/manifest.json',
            ],
          }],
          [ 'OS == "win"', {
            'widevine_cdm_version_h_file%':
                'win/<(target_arch)/widevine_cdm_version.h',
            'widevine_cdm_binary_files%': [
              'win/<(target_arch)/widevinecdm.dll',
              'win/<(target_arch)/widevinecdm.dll.lib',
            ],
            'widevine_cdm_manifest_file%': [
              'win/<(target_arch)/manifest.json',
            ],
          }],
        ],
      }],
      [ 'OS == "android"', {
        'widevine_cdm_version_h_file%':
            'android/widevine_cdm_version.h',
      }],
      [ 'branding != "Chrome" and OS != "android" and enable_widevine == 1', {
        # If enable_widevine==1 then create a dummy widevinecdm. On Win/Mac
        # the component updater will get the latest version and use it.
        # Other systems are not currently supported.
        'widevine_cdm_version_h_file%':
            'stub/widevine_cdm_version.h',
      }],
    ],
  },
  'includes': [
    '../../../build/util/version.gypi',
    '../../../media/cdm_paths.gypi',
  ],
  # Always provide a target, so we can put the logic about whether there's
  # anything to be done in this file (instead of a higher-level .gyp file).
  'targets': [
    {
      # GN version: //third_party/widevine/cdm:widevinecdmadapter_resources
      'target_name': 'widevinecdmadapter_resources',
      'type': 'none',
      'variables': {
        'output_dir': '.',
        'branding_path': '../../../chrome/app/theme/<(branding_path_component)/BRANDING',
        'template_input_path': '../../../chrome/app/chrome_version.rc.version',
        'extra_variable_files_arguments': [ '-f', 'BRANDING' ],
        'extra_variable_files': [ 'BRANDING' ], # NOTE: matches that above
      },
      'sources': [
        'widevinecdmadapter.ver',
      ],
      'includes': [
        '../../../chrome/version_resource_rules.gypi',
      ],
    },
    {
      'target_name': 'widevinecdmadapter_binary',
      'product_name': 'widevinecdmadapter',
      'type': 'none',
      'conditions': [
        [ '(branding == "Chrome" or enable_widevine == 1) and enable_pepper_cdms == 1', {
          'dependencies': [
            '<(DEPTH)/ppapi/ppapi.gyp:ppapi_cpp',
            '<(DEPTH)/media/media_cdm_adapter.gyp:cdmadapter',
            'widevine_cdm_version_h',
            'widevine_cdm_manifest',
            'widevinecdm',
            'widevinecdmadapter_resources',
          ],
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/widevinecdmadapter_version.rc',
          ],
          'conditions': [
            [ 'os_posix == 1 and OS != "mac"', {
              'libraries': [
                '-lrt',
                # Copied/created by widevinecdm.
                '<(PRODUCT_DIR)/<(widevine_cdm_path)/libwidevinecdm.so',
              ],
            }],
            [ 'OS == "win"', {
              'libraries': [
                # Copied/created by widevinecdm.
                '<(PRODUCT_DIR)/<(widevine_cdm_path)/widevinecdm.dll.lib',
              ],
            }],
            [ 'OS == "mac"', {
              'libraries': [
                # Copied/created by widevinecdm.
                '<(PRODUCT_DIR)/<(widevine_cdm_path)/libwidevinecdm.dylib',
              ],
            }, {
              # Put Widevine CDM adapter to the correct path directly except
              # for mac. On mac strip_save_dsym doesn't work with product_dir
              # so we rely on "widevinecdmadapter" target to copy it over.
              # See http://crbug.com/611990
              'product_dir': '<(PRODUCT_DIR)/<(widevine_cdm_path)',
            }],
          ],
        }],
      ],
    },
    {
      # GN version: //third_party/widevine/cdm:widevinecdmadapter
      # On Mac this copies the widevinecdmadapter binary to
      # <(widevine_cdm_path). On all other platforms the binary is already
      # in <(widevine_cdm_path). See "product_dir" above.
      'target_name': 'widevinecdmadapter',
      'type': 'none',
      'dependencies': [
        'widevinecdmadapter_binary',
      ],
      'conditions': [
        [ '(branding == "Chrome" or enable_widevine == 1) and enable_pepper_cdms == 1 and OS == "mac"', {
          'copies': [{
            'destination': '<(PRODUCT_DIR)/<(widevine_cdm_path)',
            'files': [ '<(PRODUCT_DIR)/widevinecdmadapter.plugin' ],
          }],
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
      # GN version: //third_party/widevine/cdm:widevine_cdm_manifest
      'target_name': 'widevine_cdm_manifest',
      'type': 'none',
      'conditions': [
        [ 'widevine_cdm_manifest_file != []', {
          'copies': [{
            'destination': '<(PRODUCT_DIR)/WidevineCdm',
            'files': [ '<(widevine_cdm_manifest_file)' ],
          }],
        }],
      ],
    },
    {
      # GN version: //third_party/widevine/cdm:widevinecdm
      'target_name': 'widevinecdm',
      'type': 'none',
      'conditions': [
        [ 'branding == "Chrome"', {
          'conditions': [
            [ 'OS=="mac"', {
              'xcode_settings': {
                'COPY_PHASE_STRIP': 'NO',
              }
            }],
          ],
          'copies': [{
            'destination': '<(PRODUCT_DIR)/<(widevine_cdm_path)',
            'files': [ '<@(widevine_cdm_binary_files)' ],
          }],
        }],
        [ 'branding != "Chrome" and enable_widevine == 1', {
          # On Mac this copies the widevinecdm binary to <(widevine_cdm_path).
          # On other platforms the binary is already in <(widevine_cdm_path).
          # See "widevinecdm_binary".
          'dependencies': [
            'widevinecdm_binary',
          ],
          'conditions': [
            ['OS == "mac"', {
              'copies': [{
                'destination': '<(PRODUCT_DIR)/<(widevine_cdm_path)',
                'files': [ '<(PRODUCT_DIR)/libwidevinecdm.dylib' ],
              }],
            }],
          ],
        }],
      ],
    },
    {
      # GN version: //third_party/widevine/cdm:widevine_test_license_server
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
  'conditions': [
    [ 'branding != "Chrome" and enable_widevine == 1', {
      'targets': [
        {
          'target_name': 'widevinecdm_binary',
          'product_name': 'widevinecdm',
          'type': 'none',
          'conditions': [
            ['os_posix == 1 and OS != "mac"', {
              'type': 'loadable_module',
            }],
            ['OS == "mac" or OS == "win"', {
              'type': 'shared_library',
            }],
            ['OS == "mac"', {
              'xcode_settings': {
                'DYLIB_INSTALL_NAME_BASE': '@loader_path',
              },
            }, {
              # Put Widevine CDM in the correct path directly except
              # for mac. On mac strip_save_dsym doesn't work with product_dir
              # so we rely on the "widevinecdm" target to copy it over.
              # See http://crbug.com/611990
              'product_dir': '<(PRODUCT_DIR)/<(widevine_cdm_path)',
            }],
          ],
          'defines': ['CDM_IMPLEMENTATION'],
          'dependencies': [
            'widevine_cdm_version_h',
            '<(DEPTH)/base/base.gyp:base',
          ],
          'sources': [
            '<(DEPTH)/media/cdm/stub/stub_cdm.cc',
            '<(DEPTH)/media/cdm/stub/stub_cdm.h',
          ],
        },
      ],
    }],
  ],
}
