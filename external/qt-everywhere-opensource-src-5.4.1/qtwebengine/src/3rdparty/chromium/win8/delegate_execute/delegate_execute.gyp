# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../../build/util/version.gypi',
    '../../build/win_precompile.gypi',
  ],
  'targets': [
    {
      'target_name': 'delegate_execute_version_resources',
      'type': 'none',
      'conditions': [
        ['branding == "Chrome"', {
          'variables': {
             'branding_path': '../../chrome/app/theme/google_chrome/BRANDING',
          },
        }, { # else branding!="Chrome"
          'variables': {
             'branding_path': '../../chrome/app/theme/chromium/BRANDING',
          },
        }],
      ],
      'variables': {
        'output_dir': 'delegate_execute',
        'template_input_path': '../../chrome/app/chrome_version.rc.version',
      },
      'sources': [
        'delegate_execute_exe.ver',
      ],
      'includes': [
        '../../chrome/version_resource_rules.gypi',
      ],
    },
    {
      'target_name': 'delegate_execute',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../breakpad/breakpad.gyp:breakpad_handler',
        '../../chrome/chrome.gyp:installer_util',
        '../../content/content.gyp:content_common',
        '../../google_update/google_update.gyp:google_update',
        '../../ui/base/ui_base.gyp:ui_base',
        '../../ui/gfx/gfx.gyp:gfx',
        '../../ui/gfx/gfx.gyp:gfx_geometry',
        'delegate_execute_version_resources',
      ],
      'sources': [
        'chrome_util.cc',
        'chrome_util.h',
        'command_execute_impl.cc',
        'command_execute_impl.h',
        'command_execute_impl.rgs',
        'crash_server_init.cc',
        'crash_server_init.h',
        'delegate_execute.cc',
        'delegate_execute.rc',
        'delegate_execute_operation.cc',
        'delegate_execute_operation.h',
        'delegate_execute_util.cc',
        'delegate_execute_util.h',
        'resource.h',
        '<(SHARED_INTERMEDIATE_DIR)/delegate_execute/delegate_execute_exe_version.rc',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': '2',  # Set /SUBSYSTEM:WINDOWS
        },
      },
      'conditions': [
        ['use_aura==1', {
          'dependencies': [
            '../win8.gyp:metro_viewer_constants',
          ],
        }],
      ],
    },
    {
      'target_name': 'delegate_execute_unittests',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:run_all_unittests',
        '../../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'delegate_execute_util.cc',
        'delegate_execute_util.h',
        'delegate_execute_util_unittest.cc',
      ],
    },
  ],
}
