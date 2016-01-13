# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    'nacl/nacl_defines.gypi',
  ],
  'target_defaults': {
    'variables': {
      'nacl_target': 0,
    },
    'target_conditions': [
      # This part is shared between the targets defined below. Only files and
      # settings relevant for building the Win64 target should be added here.
      ['nacl_target==1', {
        'include_dirs': [
          '<(INTERMEDIATE_DIR)',
        ],
        'defines': [
          '<@(nacl_defines)',
        ],
        'sources': [
          # .cc, .h, and .mm files under nacl that are used on all
          # platforms, including both 32-bit and 64-bit Windows.
          # Test files are also not included.
          'nacl/loader/nacl_ipc_adapter.cc',
          'nacl/loader/nacl_ipc_adapter.h',
          'nacl/loader/nacl_main.cc',
          'nacl/loader/nacl_main_platform_delegate.h',
          'nacl/loader/nacl_main_platform_delegate_linux.cc',
          'nacl/loader/nacl_main_platform_delegate_mac.mm',
          'nacl/loader/nacl_main_platform_delegate_win.cc',
          'nacl/loader/nacl_listener.cc',
          'nacl/loader/nacl_listener.h',
          'nacl/loader/nacl_trusted_listener.cc',
          'nacl/loader/nacl_trusted_listener.h',
          'nacl/loader/nacl_validation_db.h',
          'nacl/loader/nacl_validation_query.cc',
          'nacl/loader/nacl_validation_query.h',
        ],
        # TODO(gregoryd): consider switching NaCl to use Chrome OS defines
        'conditions': [
          ['OS=="win"', {
            'defines': [
              '__STDC_LIMIT_MACROS=1',
            ],
            'include_dirs': [
              '<(DEPTH)/third_party/wtl/include',
            ],
          },],
          ['OS=="linux"', {
            'defines': [
              '__STDC_LIMIT_MACROS=1',
            ],
            'sources': [
              '../components/nacl/common/nacl_paths.cc',
              '../components/nacl/common/nacl_paths.h',
            ],
          },],
        ],
      }],
    ],
  },
  'conditions': [
    ['disable_nacl!=1', {
      'targets': [
        {
          'target_name': 'nacl',
          'type': 'static_library',
          'variables': {
            'nacl_target': 1,
          },
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:base_static',
            '../ipc/ipc.gyp:ipc',
            '../ppapi/native_client/src/trusted/plugin/plugin.gyp:ppGoogleNaClPluginChrome',
            '../ppapi/ppapi_internal.gyp:ppapi_shared',
            '../ppapi/ppapi_internal.gyp:ppapi_ipc',
            '../native_client/src/trusted/service_runtime/service_runtime.gyp:sel_main_chrome',
          ],
          'conditions': [
            ['disable_nacl_untrusted==0', {
              'dependencies': [
                '../ppapi/native_client/native_client.gyp:nacl_irt',
                '../ppapi/native_client/src/untrusted/pnacl_support_extension/pnacl_support_extension.gyp:pnacl_support_extension',
              ],
            }],
          ],
          'direct_dependent_settings': {
            'defines': [
              '<@(nacl_defines)',
            ],
          },
        },
        {
          'target_name': 'nacl_browser',
          'type': 'static_library',
          'sources': [
            'nacl/browser/nacl_broker_host_win.cc',
            'nacl/browser/nacl_broker_host_win.h',
            'nacl/browser/nacl_broker_service_win.cc',
            'nacl/browser/nacl_broker_service_win.h',
            'nacl/browser/nacl_browser.cc',
            'nacl/browser/nacl_browser.h',
            'nacl/browser/nacl_file_host.cc',
            'nacl/browser/nacl_file_host.h',
            'nacl/browser/nacl_host_message_filter.cc',
            'nacl/browser/nacl_host_message_filter.h',
            'nacl/browser/nacl_process_host.cc',
            'nacl/browser/nacl_process_host.h',
            'nacl/browser/nacl_validation_cache.cc',
            'nacl/browser/nacl_validation_cache.h',
            'nacl/browser/pnacl_host.cc',
            'nacl/browser/pnacl_host.h',
            'nacl/browser/pnacl_translation_cache.cc',
            'nacl/browser/pnacl_translation_cache.h',
            'nacl/common/nacl_debug_exception_handler_win.cc',
            'nacl/common/nacl_debug_exception_handler_win.h',
          ],
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            'nacl_common',
            'nacl_switches',
            '../native_client/src/trusted/service_runtime/service_runtime.gyp:sel',
            '../content/content.gyp:content_browser',
          ],
          'defines': [
            '<@(nacl_defines)',
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [4267, ],
          'conditions': [
            ['OS=="linux"', {
              'sources': [
                '../components/nacl/zygote/nacl_fork_delegate_linux.cc',
                '../components/nacl/zygote/nacl_fork_delegate_linux.h',
              ],
              'dependencies': [
                # Required by nacl_fork_delegate_linux.cc.
                '../sandbox/sandbox.gyp:suid_sandbox_client',
              ]
            }],
          ],
        },
        {
          'target_name': 'nacl_renderer',
          'type': 'static_library',
          'sources': [
            'nacl/renderer/file_downloader.cc',
            'nacl/renderer/file_downloader.h',
            'nacl/renderer/histogram.cc',
            'nacl/renderer/histogram.h',
            'nacl/renderer/manifest_downloader.cc',
            'nacl/renderer/manifest_downloader.h',
            'nacl/renderer/manifest_service_channel.cc',
            'nacl/renderer/manifest_service_channel.h',
            'nacl/renderer/nacl_helper.cc',
            'nacl/renderer/nacl_helper.h',
            'nacl/renderer/json_manifest.cc',
            'nacl/renderer/json_manifest.h',
            'nacl/renderer/nexe_load_manager.cc',
            'nacl/renderer/nexe_load_manager.h',
            'nacl/renderer/pnacl_translation_resource_host.cc',
            'nacl/renderer/pnacl_translation_resource_host.h',
            'nacl/renderer/ppb_nacl_private_impl.cc',
            'nacl/renderer/ppb_nacl_private_impl.h',
            'nacl/renderer/progress_event.cc',
            'nacl/renderer/progress_event.h',
            'nacl/renderer/sandbox_arch.cc',
            'nacl/renderer/sandbox_arch.h',
            'nacl/renderer/trusted_plugin_channel.cc',
            'nacl/renderer/trusted_plugin_channel.h',
          ],
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            '../content/content.gyp:content_renderer',
            '../third_party/jsoncpp/jsoncpp.gyp:jsoncpp',
            '../third_party/WebKit/public/blink.gyp:blink',
            '../webkit/common/webkit_common.gyp:webkit_common',
          ],
          'defines': [
            '<@(nacl_defines)',
          ],
          'direct_dependent_settings': {
            'defines': [
              '<@(nacl_defines)',
            ],
          },
        },
        {
          'target_name': 'nacl_loader_unittests',
          'type': '<(gtest_target_type)',
          'sources': [
            'nacl/loader/run_all_unittests.cc',
          ],
          'dependencies': [
            'nacl',
            '../base/base.gyp:test_support_base',
            '../testing/gtest.gyp:gtest',
          ],
          'conditions': [
            ['OS=="linux"', {
              'sources': [
                # TODO(mazda): Move this to browser_tests once we have
                # established a way to run browser_tests on ARM Chrome OS
                # devices (http://crbug.com/364729).
                'nacl/loader/nonsfi/irt_icache_unittest.cc',
                # TODO(hamaji): Currently, we build them twice. Stop building
                # them for components_unittests. See crbug.com/364751
                'nacl/loader/nonsfi/nonsfi_sandbox_unittest.cc',
                'nacl/loader/nonsfi/nonsfi_sandbox_sigsys_unittest.cc',
              ],
              'dependencies': [
                'nacl_linux',
                '../sandbox/sandbox.gyp:sandbox_linux_test_utils',
              ],
            }],
          ],
        },
      ],
      'conditions': [
        ['OS=="linux"', {
          'targets': [
            {
              'target_name': 'nacl_helper',
              'type': 'executable',
              'include_dirs': [
                '..',
              ],
              'sources': [
                'nacl/loader/nacl_helper_linux.cc',
                'nacl/loader/nacl_helper_linux.h',
              ],
              'dependencies': [
                'nacl_linux',
              ],
              'cflags': ['-fPIE'],
              'ldflags!': [
                # Do not pick the default ASan options from
                # base/debug/sanitizer_options.cc to avoid a conflict with those
                # in nacl/nacl_helper_linux.cc.
                '-Wl,-u_sanitizer_options_link_helper',
              ],
              'link_settings': {
                'ldflags': ['-pie'],
              },
            }, {
              'target_name': 'nacl_linux',
              'type': 'static_library',
              'include_dirs': [
                '..',
              ],
              'defines': [
                '<@(nacl_defines)',
                # Allow .cc files to know if they're being compiled as part
                # of nacl_helper.
                'IN_NACL_HELPER=1',
              ],
              'sources': [
                'nacl/loader/nonsfi/abi_conversion.cc',
                'nacl/loader/nonsfi/abi_conversion.h',
                'nacl/loader/nonsfi/elf_loader.cc',
                'nacl/loader/nonsfi/elf_loader.h',
                'nacl/loader/nonsfi/irt_basic.cc',
                'nacl/loader/nonsfi/irt_clock.cc',
                'nacl/loader/nonsfi/irt_exception_handling.cc',
                'nacl/loader/nonsfi/irt_fdio.cc',
                'nacl/loader/nonsfi/irt_futex.cc',
                'nacl/loader/nonsfi/irt_icache.cc',
                'nacl/loader/nonsfi/irt_interfaces.cc',
                'nacl/loader/nonsfi/irt_interfaces.h',
                'nacl/loader/nonsfi/irt_memory.cc',
                'nacl/loader/nonsfi/irt_ppapi.cc',
                'nacl/loader/nonsfi/irt_random.cc',
                'nacl/loader/nonsfi/irt_resource_open.cc',
                'nacl/loader/nonsfi/irt_thread.cc',
                'nacl/loader/nonsfi/irt_util.h',
                'nacl/loader/nonsfi/nonsfi_main.cc',
                'nacl/loader/nonsfi/nonsfi_main.h',
                'nacl/loader/nonsfi/nonsfi_sandbox.cc',
                'nacl/loader/nonsfi/nonsfi_sandbox.h',
                'nacl/loader/sandbox_linux/nacl_bpf_sandbox_linux.cc',
                'nacl/loader/sandbox_linux/nacl_sandbox_linux.cc',
                '../ppapi/nacl_irt/irt_manifest.h',
                '../ppapi/nacl_irt/manifest_service.cc',
                '../ppapi/nacl_irt/manifest_service.h',
                '../ppapi/nacl_irt/plugin_main.cc',
                '../ppapi/nacl_irt/plugin_main.h',
                '../ppapi/nacl_irt/plugin_startup.cc',
                '../ppapi/nacl_irt/plugin_startup.h',
                '../ppapi/nacl_irt/ppapi_dispatcher.cc',
                '../ppapi/nacl_irt/ppapi_dispatcher.h',
              ],
              'dependencies': [
                'nacl',
                'nacl_common',
                'nacl_switches',
                '../components/tracing.gyp:tracing',
                '../crypto/crypto.gyp:crypto',
                '../sandbox/sandbox.gyp:libc_urandom_override',
                '../sandbox/sandbox.gyp:sandbox',
                '../ppapi/ppapi_internal.gyp:ppapi_proxy',
              ],
              'conditions': [
                ['use_glib == 1', {
                  'dependencies': [
                    '../build/linux/system.gyp:glib',
                  ],
                }],
                ['os_posix == 1 and OS != "mac"', {
                  'conditions': [
                    ['use_allocator!="none"', {
                      'dependencies': [
                        '../base/allocator/allocator.gyp:allocator',
                      ],
                    }],
                  ],
                }],
                ['use_seccomp_bpf == 0', {
                  'sources!': [
                    '../content/common/sandbox_linux/sandbox_bpf_base_policy_linux.cc',
                    '../content/common/sandbox_linux/sandbox_init_linux.cc',
                  ],
                }, {
                  'defines': ['USE_SECCOMP_BPF'],
                }],
              ],
              'cflags': ['-fPIE'],
            },
          ],
        }],
        ['OS=="win" and target_arch=="ia32"', {
          'targets': [
            {
              'target_name': 'nacl_win64',
              'type': 'static_library',
              'variables': {
                'nacl_target': 1,
              },
              'dependencies': [
                'nacl_common_win64',
                '../native_client/src/trusted/service_runtime/service_runtime.gyp:sel_main_chrome64',
                '../ppapi/ppapi_internal.gyp:ppapi_shared_win64',
                '../ppapi/ppapi_internal.gyp:ppapi_ipc_win64',
              ],
              'export_dependent_settings': [
                '../ppapi/ppapi_internal.gyp:ppapi_ipc_win64',
              ],
              'sources': [
                '../components/nacl/broker/nacl_broker_listener.cc',
                '../components/nacl/broker/nacl_broker_listener.h',
                '../components/nacl/common/nacl_debug_exception_handler_win.cc',
                '../components/nacl/loader/nacl_helper_win_64.cc',
                '../components/nacl/loader/nacl_helper_win_64.h',
              ],
              'include_dirs': [
                '..',
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
              'direct_dependent_settings': {
                'defines': [
                  '<@(nacl_defines)',
                ],
              },
            },
            {
              'target_name': 'nacl_switches_win64',
              'type': 'static_library',
              'sources': [
                'nacl/common/nacl_switches.cc',
                'nacl/common/nacl_switches.h',
              ],
              'include_dirs': [
                '..',
              ],
              'configurations': {
                'Common_Base': {
                  'msvs_target_platform': 'x64',
                },
              },
            },
            {
              'target_name': 'nacl_common_win64',
              'type': 'static_library',
              'defines': [
                'COMPILE_CONTENT_STATICALLY',
              ],
              'sources': [
                'nacl/common/nacl_cmd_line.cc',
                'nacl/common/nacl_cmd_line.h',
                'nacl/common/nacl_messages.cc',
                'nacl/common/nacl_messages.h',
                'nacl/common/nacl_types.cc',
                'nacl/common/nacl_types.h',
              ],
              'include_dirs': [
                '..',
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
    }, {  # else (disable_nacl==1)
      'targets': [
        {
          'target_name': 'nacl',
          'type': 'none',
          'sources': [],
        },
      ],
      'conditions': [
        ['OS=="win"', {
          'targets': [
            {
              'target_name': 'nacl_win64',
              'type': 'none',
              'sources': [],
            },
            {
              'target_name': 'nacl_switches_win64',
              'type': 'none',
              'sources': [],
            },
          ],
        }],
      ],
    }],
  ],
  'targets': [
    {
      'target_name': 'nacl_switches',
      'type': 'static_library',
      'sources': [
        'nacl/common/nacl_switches.cc',
        'nacl/common/nacl_switches.h',
    ],
      'include_dirs': [
        '..',
      ],
    },
    {
      'target_name': 'nacl_common',
      'type': 'static_library',
      'sources': [
        'nacl/common/nacl_cmd_line.cc',
        'nacl/common/nacl_cmd_line.h',
        'nacl/common/nacl_host_messages.h',
        'nacl/common/nacl_host_messages.cc',
        'nacl/common/nacl_messages.cc',
        'nacl/common/nacl_messages.h',
        'nacl/common/nacl_nonsfi_util.cc',
        'nacl/common/nacl_nonsfi_util.h',
        'nacl/common/nacl_process_type.h',
        'nacl/common/nacl_sandbox_type_mac.h',
        'nacl/common/nacl_types.cc',
        'nacl/common/nacl_types.h',
        'nacl/common/pnacl_types.cc',
        'nacl/common/pnacl_types.h',
      ],
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../content/content.gyp:content_common',
      ],
    },
  ]
}
