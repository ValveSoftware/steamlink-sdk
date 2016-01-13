# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'breakpad_component_lib',
      'type': 'static_library',
      'sources': [
        'breakpad/app/breakpad_client.cc',
        'breakpad/app/breakpad_client.h',
        'breakpad/app/crash_keys_win.cc',
        'breakpad/app/crash_keys_win.h',
      ],
      'include_dirs': [
        '..',
        '../breakpad/src',
      ],
    },
    {
      'variables': {
        'conditions': [
          ['OS == "ios" ', {
            # On IOS there are no files compiled into the library, and we
            # can't have libraries with zero objects.
            'breakpad_component_target_type%': 'none',
          }, {
            'breakpad_component_target_type%': 'static_library',
          }],
        ],
      },
      # Note: if you depend on this target, you need to either link in
      # content.gyp:content_common, or add
      # content/public/common/content_switches.cc to your sources.
      'target_name': 'breakpad_component',
      'type': '<(breakpad_component_target_type)',
      'sources': [
        'breakpad/app/breakpad_linux.cc',
        'breakpad/app/breakpad_linux.h',
        'breakpad/app/breakpad_linux_impl.h',
        'breakpad/app/breakpad_mac.h',
        'breakpad/app/breakpad_mac.mm',
        'breakpad/app/breakpad_win.cc',
        'breakpad/app/breakpad_win.h',
        'breakpad/app/hard_error_handler_win.cc',
        'breakpad/app/hard_error_handler_win.h',
      ],
      'dependencies': [
        'breakpad_component_lib',
        '../base/base.gyp:base',
      ],
      'defines': ['BREAKPAD_IMPLEMENTATION'],
      'conditions': [
        ['OS=="mac"', {
          'dependencies': [
            '../breakpad/breakpad.gyp:breakpad',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            '../breakpad/breakpad.gyp:breakpad_handler',
            '../breakpad/breakpad.gyp:breakpad_sender',
            '../sandbox/sandbox.gyp:sandbox',
          ],
        }],
        ['os_posix == 1 and OS != "mac" and OS != "ios" and android_webview_build != 1', {
          'dependencies': [
            '../breakpad/breakpad.gyp:breakpad_client',
          ],
          'include_dirs': [
            '../breakpad/src',
          ],
        }],
      ],
      'target_conditions': [
        # Need 'target_conditions' to override default filename_rules to include
        # the files on Android.
        ['OS=="android"', {
          'sources/': [
            ['include', '^breakpad/app/breakpad_linux\\.cc$'],
          ],
        }],
      ],
    },
    {
      'target_name': 'breakpad_test_support',
      'type': 'none',
      'dependencies': [
        'breakpad_component_lib',
      ],
      'direct_dependent_settings': {
        'include_dirs' : [
          '../breakpad/src',
        ],
      }
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'breakpad_crash_service',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../breakpad/breakpad.gyp:breakpad_handler',
            '../breakpad/breakpad.gyp:breakpad_sender',
          ],
          'sources': [
            'breakpad/tools/crash_service.cc',
            'breakpad/tools/crash_service.h',
          ],
        },
      ],
    }],
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          # Note: if you depend on this target, you need to either link in
          # content.gyp:content_common, or add
          # content/public/common/content_switches.cc to your sources.
          'target_name': 'breakpad_win64',
          'type': 'static_library',
          'sources': [
            'breakpad/app/breakpad_client.cc',
            'breakpad/app/breakpad_client.h',
            'breakpad/app/breakpad_linux.cc',
            'breakpad/app/breakpad_linux.h',
            'breakpad/app/breakpad_linux_impl.h',
            'breakpad/app/breakpad_mac.h',
            'breakpad/app/breakpad_mac.mm',
            'breakpad/app/breakpad_win.cc',
            'breakpad/app/breakpad_win.h',
            # TODO(siggi): test the x64 version too.
            'breakpad/app/crash_keys_win.cc',
            'breakpad/app/crash_keys_win.h',
            'breakpad/app/hard_error_handler_win.cc',
            'breakpad/app/hard_error_handler_win.h',
          ],
          'defines': [
            'COMPILE_CONTENT_STATICALLY',
            'BREAKPAD_IMPLEMENTATION',
          ],
          'dependencies': [
            '../base/base.gyp:base_win64',
            '../breakpad/breakpad.gyp:breakpad_handler_win64',
            '../breakpad/breakpad.gyp:breakpad_sender_win64',
            '../sandbox/sandbox.gyp:sandbox_win64',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
        {
          'target_name': 'breakpad_crash_service_win64',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base_win64',
            '../breakpad/breakpad.gyp:breakpad_handler_win64',
            '../breakpad/breakpad.gyp:breakpad_sender_win64',
          ],
          'sources': [
            'breakpad/tools/crash_service.cc',
            'breakpad/tools/crash_service.h',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
    ['OS=="mac"', {
      'targets': [
        {
          'target_name': 'breakpad_stubs',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
          ],
          'sources': [
            'breakpad/app/breakpad_client.cc',
            'breakpad/app/breakpad_client.h',
            'breakpad/app/breakpad_mac.h',
            'breakpad/app/breakpad_mac_stubs.mm',
          ],
        },
      ],
    }],
    ['os_posix == 1 and OS != "mac" and OS != "ios" and android_webview_build != 1', {
      'targets': [
        {
          'target_name': 'breakpad_host',
          'type': 'static_library',
          'dependencies': [
            'breakpad_component',
            '../base/base.gyp:base',
            '../breakpad/breakpad.gyp:breakpad_client',
            '../content/content.gyp:content_browser',
            '../content/content.gyp:content_common',
          ],
          'sources': [
            'breakpad/browser/crash_dump_manager_android.cc',
            'breakpad/browser/crash_dump_manager_android.h',
            'breakpad/browser/crash_handler_host_linux.cc',
            'breakpad/browser/crash_handler_host_linux.h',
          ],
          'include_dirs': [
            '../breakpad/src',
          ],
          'target_conditions': [
            # Need 'target_conditions' to override default filename_rules to include
            # the files on Android.
            ['OS=="android"', {
              'sources/': [
                ['include', '^breakpad/browser/crash_handler_host_linux\\.cc$'],
              ],
            }],
          ],
        },
      ],
    }],
  ],
}
