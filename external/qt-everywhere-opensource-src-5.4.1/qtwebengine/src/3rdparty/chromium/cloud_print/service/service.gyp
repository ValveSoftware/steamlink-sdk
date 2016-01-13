# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'target_defaults': {
    'variables': {
      'chromium_code': 1,
      'enable_wexit_time_destructors': 1,
    },
    'include_dirs': [
      '<(DEPTH)',
      # To allow including "version.h"
      '<(SHARED_INTERMEDIATE_DIR)',
    ],
    'defines' : [
      'COMPILE_CONTENT_STATICALLY',
      'SECURITY_WIN32',
      'STRICT',
      '_ATL_APARTMENT_THREADED',
      '_ATL_CSTRING_EXPLICIT_CONSTRUCTORS',
      '_ATL_NO_COM_SUPPORT',
      '_ATL_NO_AUTOMATIC_NAMESPACE',
      '_ATL_NO_EXCEPTIONS',
    ],
    'conditions': [
      ['OS=="win"', {
        # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
        'msvs_disabled_warnings': [ 4267, ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'service_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/cloud_print',
      },
      'actions': [
        {
          'action_name': 'service_resources',
          'variables': {
            'grit_grd_file': 'win/service_resources.grd',
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../../build/grit_target.gypi' ],
    },
    {
      'target_name': 'cloud_print_service_lib',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_static',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/components/components.gyp:cloud_devices_common',
        '<(DEPTH)/google_apis/google_apis.gyp:google_apis',
        '<(DEPTH)/ipc/ipc.gyp:ipc',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/url/url.gyp:url_lib',
        'service_resources',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            '<(DEPTH)/chrome/chrome.gyp:chrome_version_header',
            '<(DEPTH)/chrome/chrome.gyp:launcher_support',
            '<(DEPTH)/chrome/common_constants.gyp:common_constants',
          ],
        }],
        ['enable_printing!=0', {
          'dependencies': [
            '<(DEPTH)/printing/printing.gyp:printing',
          ],
        }],
      ],
      'sources': [
        '<(DEPTH)/content/public/common/content_switches.h',
        '<(DEPTH)/content/public/common/content_switches.cc',
        '<(DEPTH)/cloud_print/common/win/cloud_print_utils.cc',
        '<(DEPTH)/cloud_print/common/win/cloud_print_utils.h',
        'service_constants.cc',
        'service_constants.h',
        'service_state.cc',
        'service_state.h',
        'service_switches.cc',
        'service_switches.h',
        'win/chrome_launcher.cc',
        'win/chrome_launcher.h',
        'win/local_security_policy.cc',
        'win/local_security_policy.h',
        'win/service_controller.cc',
        'win/service_controller.h',
        'win/service_listener.cc',
        'win/service_listener.h',
        'win/service_utils.cc',
        'win/service_utils.h',
        'win/setup_listener.cc',
        'win/setup_listener.h',
      ],
    },
    {
      'target_name': 'cloud_print_service',
      'type': 'executable',
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_exe_version.rc',
        'win/cloud_print_service.cc',
      ],
      'includes': [
        'win/service_resources.gypi'
      ],
      'dependencies': [
        'cloud_print_service_lib',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': '1',         # Set /SUBSYSTEM:CONSOLE
          'UACExecutionLevel': '2', # /level='requireAdministrator'
          'AdditionalDependencies': [
              'secur32.lib',
          ],
        },
      },
    },
    {
      'target_name': 'cloud_print_service_config',
      'type': 'executable',
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_config_exe_version.rc',
        'win/cloud_print_service_config.cc',
      ],
      'includes': [
        'win/service_resources.gypi'
      ],
      'dependencies': [
        '<(DEPTH)/cloud_print/common/common.gyp:cloud_print_install_lib',
        'cloud_print_service_lib',
      ],
      'msvs_settings': {
        'VCManifestTool': {
          'AdditionalManifestFiles': [
            'common-controls.manifest',
          ],
        },
        'VCLinkerTool': {
          'SubSystem': '2',         # Set /SUBSYSTEM:WINDOWS
          'UACExecutionLevel': '2', # /level='requireAdministrator'
          'AdditionalDependencies': [
              'secur32.lib',
          ],
        },
      },
    },
    {
      'target_name': 'cloud_print_service_setup',
      'type': 'executable',
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_setup_exe_version.rc',
        'win/installer.cc',
        'win/installer.h',
      ],
      'includes': [
        'win/service_resources.gypi'
      ],
      'dependencies': [
        '<(DEPTH)/cloud_print/common/common.gyp:cloud_print_install_lib',
        'cloud_print_service_lib',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': '2',         # Set /SUBSYSTEM:WINDOWS
          'UACExecutionLevel': '2', # /level='requireAdministrator'
          'AdditionalDependencies': [
              'secur32.lib',
          ],
        },
      },
    },
  ],
}
