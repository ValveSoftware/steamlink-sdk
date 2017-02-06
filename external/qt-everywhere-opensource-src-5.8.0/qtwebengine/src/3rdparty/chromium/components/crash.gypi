# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/crash/core/browser
      'target_name': 'crash_core_browser',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'upload_list',
        '../base/base.gyp:base',
        '../components/components_strings.gyp:components_strings',
      ],
      'sources': [
        'crash/core/browser/crashes_ui_util.cc',
        'crash/core/browser/crashes_ui_util.h',
      ],
    },
    {
      # GN version: //components/crash/core/common
      'target_name': 'crash_core_common',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        # List of dependencies is intentionally very minimal. Please avoid
        # adding extra dependencies without first checking with OWNERS.
        '../base/base.gyp:base',
      ],
      'sources': [
        'crash/core/common/crash_keys.cc',
        'crash/core/common/crash_keys.h',
      ],
      'conditions': [
        ['OS=="mac" or OS=="ios"', {
          'sources': [
            'crash/core/common/objc_zombie.h',
            'crash/core/common/objc_zombie.mm',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'crash_core_common_win64',
          'type': 'static_library',
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            # List of dependencies is intentionally very minimal. Please avoid
            # adding extra dependencies without first checking with OWNERS.
            '../base/base.gyp:base_win64',
          ],
          'sources': [
            'crash/core/common/crash_keys.cc',
            'crash/core/common/crash_keys.h',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
    ['OS!="ios"', {
      'targets': [
        {
          'target_name': 'crash_component_lib',
          'type': 'static_library',
          'sources': [
            'crash/content/app/crash_keys_win.cc',
            'crash/content/app/crash_keys_win.h',
            'crash/content/app/crash_reporter_client.cc',
            'crash/content/app/crash_reporter_client.h',
          ],
          'include_dirs': [
            '..',
            '../breakpad/src',
          ],
        },
        {
          # TODO(mark): https://crbug.com/466890: merge this target with
          # crash_component.
          #
          # This is a temporary base target that is depended on by both
          # crash_component and crash_component_breakpad_to_be_deleted. It
          # provides everything common to both of those targets. For a short
          # period, there are two Mac and Windows crash component
          # implementations. The new one uses a Crashpad implementation and is
          # used by Chrome. The old one uses a Breakpad implementation and is
          # used by content_shell. Consumers should depend on the desired
          # target. All three targets behave identically on non-Mac/-Windows.
          # When content_shell and any other consumers are migrated to the
          # Crashpad implementation on Mac/Windows, crash_component will merge
          # back into this target, crash_component_non_mac_win, which will be
          # renamed crash_component. crash_component_breakpad_to_be_deleted will
          # be deleted.
          #
          # While this situation exists:
          #
          # Do not depend on this target directly! Depend on
          # crash_component_breakpad_to_be_deleted for old Breakpad behavior on
          # all platforms, or preferably, depend on crash_component to get Breakpad
          # everywhere except for Mac and Windows, where you will get Crashpad.
          #
          # GN version: //components/crash/content/app:app_non_mac
          'target_name': 'crash_component_non_mac_win',
          'variables': {
            'conditions': [
              ['OS == "ios" or OS == "mac"', {
                # On IOS there are no files compiled into the library, and we
                # can't have libraries with zero objects.
                # For now, the same applies to Mac OS X, until this target
                # merges with crash_component.
                'crash_component_target_type%': 'none',
              }, {
                'crash_component_target_type%': 'static_library',
              }],
            ],
          },
          'type': '<(crash_component_target_type)',
          'sources': [
            'crash/content/app/breakpad_linux.cc',
            'crash/content/app/breakpad_linux.h',
            'crash/content/app/breakpad_linux_impl.h',
            'crash/content/app/hard_error_handler_win.cc',
            'crash/content/app/hard_error_handler_win.h',
          ],
          'dependencies': [
            'crash_component_lib',
            'crash_core_common',
            '../base/base.gyp:base',
          ],
          'defines': ['CRASH_IMPLEMENTATION'],
          'conditions': [
            ['OS=="win"', {
              'dependencies': [
                '../breakpad/breakpad.gyp:breakpad_handler',
                '../breakpad/breakpad.gyp:breakpad_sender',
                '../sandbox/sandbox.gyp:sandbox',
              ],
            }],
            ['os_posix == 1 and OS != "mac" and OS != "ios"', {
              'dependencies': [
                '../breakpad/breakpad.gyp:breakpad_client',
              ],
              'include_dirs': [
                '../breakpad/src',
              ],
            }],
            ['clang==1 and target_arch=="ia32"', {
              'cflags!': [
                # Clang's -mstackrealign doesn't work well with
                # linux_syscall_support.h hand written asm syscalls.
                # See https://crbug.com/556393
                '-mstackrealign',
              ],
            }],
          ],
          'target_conditions': [
            # Need 'target_conditions' to override default filename_rules to include
            # the files on Android.
            ['OS=="android"', {
              'sources/': [
                ['include', '^crash/content/app/breakpad_linux\\.cc$'],
              ],
            }],
          ],
        },
        {
          # Note: if you depend on this target, you need to either link in
          # content.gyp:content_common, or add
          # content/public/common/content_switches.cc to your sources.
          #
          # GN version: //components/crash/content/app

          # TODO(mark): https://crbug.com/466890: merge this target with
          # crash_component_non_mac_win.
          #
          # Most of this target is actually in its dependency,
          # crash_component_non_mac_win.  See the comment in that target for an
          # explanation for the split. The split is temporary and the two targets
          # will be unified again soon.
          'target_name': 'crash_component',
          'variables': {
            'conditions': [
              ['OS != "mac" and OS != "win"', {
                # There are no source files except on Mac OS X and Windows.
                'crash_component_target_type%': 'none',
              }, {
                'crash_component_target_type%': 'static_library',
              }],
            ],
          },
          'type': '<(crash_component_target_type)',
          'sources': [
            'crash/content/app/crash_switches.cc',
            'crash/content/app/crash_switches.h',
            'crash/content/app/crashpad.cc',
            'crash/content/app/crashpad.h',
            'crash/content/app/crashpad_mac.mm',
            'crash/content/app/crashpad_win.cc',
            'crash/content/app/run_as_crashpad_handler_win.cc',
            'crash/content/app/run_as_crashpad_handler_win.h',
          ],
          'dependencies': [
            'crash_component_non_mac_win',
            'crash_component_lib',
            '../base/base.gyp:base',
            '../third_party/kasko/kasko.gyp:kasko',
          ],
          'export_dependent_settings': [
            '../third_party/kasko/kasko.gyp:kasko',
          ],
          'conditions': [
            ['OS=="mac" or OS=="win"', {
              'dependencies': [
                '../third_party/crashpad/crashpad/client/client.gyp:crashpad_client',
                '../third_party/crashpad/crashpad/snapshot/snapshot.gyp:crashpad_snapshot_api',
              ],
            }],
            ['OS=="win"', {
              'dependencies': [
                '../third_party/crashpad/crashpad/handler/handler.gyp:crashpad_handler_lib',
              ],
            }],
          ],
          'defines': ['CRASH_IMPLEMENTATION'],
        },
        {
          # TODO(mark): https://crbug.com/466890: remove this target.
          #
          # This is a temporary target provided for Mac and Windows Breakpad
          # users that have not yet migrated to Crashpad (namely content_shell).
          # This target will be removed shortly and all consumers will be
          # expected to use Crashpad as the Mac and Windows crash-reporting
          # client. See the comment in the crash_component_non_mac_win target
          # for more details.
          #
          # GN version: //components/crash/content/app:app_breakpad_mac_to_be_deleted
          'target_name': 'crash_component_breakpad_to_be_deleted',
          'variables': {
            'conditions': [
              ['OS != "mac" and OS != "win"', {
                # There are no source files on any platform but Mac OS X and
                # Windows.
                'crash_component_target_type%': 'none',
              }, {
                'crash_component_target_type%': 'static_library',
              }],
            ],
          },
          'type': '<(crash_component_target_type)',
          'sources': [
            'crash/content/app/breakpad_mac.h',
            'crash/content/app/breakpad_mac.mm',
            'crash/content/app/breakpad_win.cc',
            'crash/content/app/breakpad_win.h',
          ],
          'dependencies': [
            'crash_component_non_mac_win',
            'crash_component_lib',
          ],
          'defines': ['CRASH_IMPLEMENTATION'],
          'conditions': [
            ['OS=="mac"', {
              'dependencies': [
                '../breakpad/breakpad.gyp:breakpad',
              ],
              'include_dirs': [
                '..',
                '../breakpad/src',
              ],
            }],
            ['OS=="win"', {
              'dependencies': [
                '../breakpad/breakpad.gyp:breakpad_handler',
              ],
              'include_dirs': [
                '..',
                '../breakpad/src',
              ],
              'all_dependent_settings': {
                'msvs_settings': {
                  'VCLinkerTool': {
                    'AdditionalDependencies': [
                      'userenv.lib',
                    ],
                  },
                },
              },
              'msvs_settings': {
                'VCLinkerTool': {
                  'AdditionalDependencies': [
                    'userenv.lib',
                  ],
                },
              },
            }],
          ],
        },
        {
          # GN version: //components/crash/content/app:test_support
          'target_name': 'crash_test_support',
          'type': 'none',
          'dependencies': [
            'crash_component_lib',
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
              # GN version: //components/crash/content/tools:crash_service
              'target_name': 'breakpad_crash_service',
              'type': 'static_library',
              'dependencies': [
                '../base/base.gyp:base',
                '../breakpad/breakpad.gyp:breakpad_handler',
                '../breakpad/breakpad.gyp:breakpad_sender',
              ],
              'sources': [
                'crash/content/tools/crash_service.cc',
                'crash/content/tools/crash_service.h',
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
                'crash/content/app/breakpad_linux.cc',
                'crash/content/app/breakpad_linux.h',
                'crash/content/app/breakpad_linux_impl.h',
                'crash/content/app/breakpad_mac.h',
                'crash/content/app/breakpad_mac.mm',
                'crash/content/app/breakpad_win.cc',
                'crash/content/app/breakpad_win.h',
                # TODO(siggi): test the x64 version too.
                'crash/content/app/crash_keys_win.cc',
                'crash/content/app/crash_keys_win.h',
                'crash/content/app/crash_reporter_client.cc',
                'crash/content/app/crash_reporter_client.h',
                'crash/content/app/hard_error_handler_win.cc',
                'crash/content/app/hard_error_handler_win.h',
              ],
              'defines': [
                'COMPILE_CONTENT_STATICALLY',
                'CRASH_IMPLEMENTATION',
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
              'conditions': [
                ['OS=="win"', {
                  'all_dependent_settings': {
                    'msvs_settings': {
                      'VCLinkerTool': {
                        'AdditionalDependencies': [
                          'userenv.lib',
                        ],
                      },
                    },
                  },
                  'msvs_settings': {
                    'VCLinkerTool': {
                      'AdditionalDependencies': [
                        'userenv.lib',
                      ],
                    },
                  },
                }],
              ],
            },
            {
              # GN version: //components/crash/content/tools:crash_service
              'target_name': 'breakpad_crash_service_win64',
              'type': 'static_library',
              'dependencies': [
                '../base/base.gyp:base_win64',
                '../breakpad/breakpad.gyp:breakpad_handler_win64',
                '../breakpad/breakpad.gyp:breakpad_sender_win64',
              ],
              'sources': [
                'crash/content/tools/crash_service.cc',
                'crash/content/tools/crash_service.h',
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
              # GN version: //components/crash/content/app:breakpad_stubs
              'target_name': 'breakpad_stubs',
              'type': 'static_library',
              'dependencies': [
                '../base/base.gyp:base',
              ],
              'sources': [
                'crash/content/app/breakpad_mac.h',
                'crash/content/app/breakpad_mac_stubs.mm',
                'crash/content/app/crash_reporter_client.cc',
                'crash/content/app/crash_reporter_client.h',
              ],
            },
          ],
        }],
        ['os_posix == 1 and OS != "mac"', {
          'targets': [
            {
              # GN version: //components/crash/content/browser
              'target_name': 'breakpad_host',
              'type': 'static_library',
              'dependencies': [
                'crash_component',
                '../base/base.gyp:base',
                '../breakpad/breakpad.gyp:breakpad_client',
                '../content/content.gyp:content_browser',
                '../content/content.gyp:content_common',
              ],
              'sources': [
                'crash/content/browser/crash_dump_manager_android.cc',
                'crash/content/browser/crash_dump_manager_android.h',
                'crash/content/browser/crash_handler_host_linux.cc',
                'crash/content/browser/crash_handler_host_linux.h',
                'crash/content/browser/crash_micro_dump_manager_android.cc',
                'crash/content/browser/crash_micro_dump_manager_android.h',
              ],
              'include_dirs': [
                '../breakpad/src',
              ],
              'target_conditions': [
                # Need 'target_conditions' to override default filename_rules to include
                # the files on Android.
                ['OS=="android"', {
                  'sources/': [
                    ['include', '^crash/content/browser/crash_handler_host_linux\\.cc$'],
                  ],
                }],
              ],
            },
          ],
        }],
      ],
    }],
  ],
}
