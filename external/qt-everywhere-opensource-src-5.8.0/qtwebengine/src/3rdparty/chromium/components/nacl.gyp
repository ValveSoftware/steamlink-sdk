# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
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
        'sources': [
          # .cc, .h, and .mm files under nacl that are used on all
          # platforms, including both 32-bit and 64-bit Windows.
          # Test files are also not included.
          'nacl/loader/nacl_ipc_adapter.cc',
          'nacl/loader/nacl_ipc_adapter.h',
          'nacl/loader/nacl_listener.cc',
          'nacl/loader/nacl_listener.h',
          'nacl/loader/nacl_main.cc',
          'nacl/loader/nacl_main_platform_delegate.h',
          'nacl/loader/nacl_main_platform_delegate_linux.cc',
          'nacl/loader/nacl_main_platform_delegate_mac.mm',
          'nacl/loader/nacl_main_platform_delegate_win.cc',
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
          # GN version: //components/nacl/loader
          'target_name': 'nacl',
          'type': 'static_library',
          'variables': {
            'nacl_target': 1,
          },
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:base_static',
            '../crypto/crypto.gyp:crypto',
            '../ipc/ipc.gyp:ipc',
            '../mojo/mojo_edk.gyp:mojo_system_impl',
            '../native_client/src/trusted/service_runtime/service_runtime.gyp:sel_main_chrome',
            '../ppapi/ppapi_internal.gyp:ppapi_ipc',
            '../ppapi/ppapi_internal.gyp:ppapi_shared',
          ],
          'conditions': [
            ['disable_nacl_untrusted==0', {
              'dependencies': [
                '../ppapi/native_client/native_client.gyp:nacl_irt',
                '../ppapi/native_client/src/untrusted/pnacl_support_extension/pnacl_support_extension.gyp:pnacl_support_extension',
              ],
            }],
          ],
        },
        {
          # GN version: //components/nacl/browser
          'target_name': 'nacl_browser',
          'type': 'static_library',
          'sources': [
            'nacl/browser/bad_message.cc',
            'nacl/browser/bad_message.h',
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
            'url_formatter/url_formatter.gyp:url_formatter',
            '../native_client/src/trusted/service_runtime/service_runtime.gyp:sel',
            '../content/content.gyp:content_browser',
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
                '../sandbox/sandbox.gyp:sandbox_services',
              ]
            }],
          ],
        },
        {
          # GN version: //components/nacl/renderer
          'target_name': 'nacl_renderer',
          'type': 'static_library',
          'sources': [
            'nacl/renderer/file_downloader.cc',
            'nacl/renderer/file_downloader.h',
            'nacl/renderer/histogram.cc',
            'nacl/renderer/histogram.h',
            'nacl/renderer/json_manifest.cc',
            'nacl/renderer/json_manifest.h',
            'nacl/renderer/manifest_downloader.cc',
            'nacl/renderer/manifest_downloader.h',
            'nacl/renderer/manifest_service_channel.cc',
            'nacl/renderer/manifest_service_channel.h',
            'nacl/renderer/nacl_helper.cc',
            'nacl/renderer/nacl_helper.h',
            'nacl/renderer/nexe_load_manager.cc',
            'nacl/renderer/nexe_load_manager.h',
            'nacl/renderer/platform_info.cc',
            'nacl/renderer/platform_info.h',
            'nacl/renderer/pnacl_translation_resource_host.cc',
            'nacl/renderer/pnacl_translation_resource_host.h',
            'nacl/renderer/ppb_nacl_private_impl.cc',
            'nacl/renderer/ppb_nacl_private_impl.h',
            'nacl/renderer/progress_event.cc',
            'nacl/renderer/progress_event.h',
            'nacl/renderer/trusted_plugin_channel.cc',
            'nacl/renderer/trusted_plugin_channel.h',
          ],
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            'nacl_common',
            '../content/content.gyp:content_renderer',
            '../components/nacl/renderer/plugin/plugin.gyp:nacl_trusted_plugin',
            '../third_party/WebKit/public/blink.gyp:blink',
          ],
        },
        {
          # GN version: //components/nacl/loader:nacl_loader_unittests
          'target_name': 'nacl_loader_unittests',
          'type': '<(gtest_target_type)',
          'sources': [
            'nacl/loader/nacl_ipc_adapter_unittest.cc',
            'nacl/loader/nacl_validation_query_unittest.cc',
            'nacl/loader/run_all_unittests.cc',
          ],
          'dependencies': [
            'nacl',
            '../base/base.gyp:test_support_base',
            '../ipc/ipc.gyp:test_support_ipc',
            '../testing/gtest.gyp:gtest',
          ],
        },
      ],
      'conditions': [
        ['OS=="linux"', {
          'targets': [
            {
              # GN version: //components/nacl/loader:nacl_helper
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
                # build/sanitizers/sanitizer_options.cc to avoid a conflict with
                # those in nacl/nacl_helper_linux.cc.
                '-Wl,-u_sanitizer_options_link_helper',
              ],
              'link_settings': {
                'ldflags': ['-pie'],
              },
            }, {
              # GN version: //components/nacl/loader/sandbox_linux
              'target_name': 'nacl_linux',
              'type': 'static_library',
              'include_dirs': [
                '..',
              ],
              'defines': [
                # Allow .cc files to know if they're being compiled as part
                # of nacl_helper.
                'IN_NACL_HELPER=1',
              ],
              'sources': [
                'nacl/loader/sandbox_linux/nacl_bpf_sandbox_linux.cc',
                'nacl/loader/sandbox_linux/nacl_sandbox_linux.cc',
              ],
              'dependencies': [
                'nacl',
                'nacl_common',
                'nacl_switches',
                '../crypto/crypto.gyp:crypto',
                '../sandbox/sandbox.gyp:sandbox',
              ],
              'conditions': [
                ['use_glib == 1', {
                  'dependencies': [
                    '../build/linux/system.gyp:glib',
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
                '../mojo/mojo_edk.gyp:mojo_system_impl_win64',
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
            },
            {
              # GN version: //components/nacl/common:switches
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
              # GN version: //components/nacl/common
              'target_name': 'nacl_common_win64',
              'type': 'static_library',
              'defines': [
                'COMPILE_CONTENT_STATICALLY',
              ],
              'sources': [
                'nacl/common/nacl_cmd_line.cc',
                'nacl/common/nacl_cmd_line.h',
                'nacl/common/nacl_constants.cc',
                'nacl/common/nacl_constants.h',
                'nacl/common/nacl_messages.cc',
                'nacl/common/nacl_messages.h',
                'nacl/common/nacl_renderer_messages.cc',
                'nacl/common/nacl_renderer_messages.h',
                'nacl/common/nacl_types.cc',
                'nacl/common/nacl_types.h',
                'nacl/common/nacl_types_param_traits.cc',
                'nacl/common/nacl_types_param_traits.h',
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
    ['disable_nacl!=1 and test_isolation_mode!="noop"', {
      'targets': [
        {
          'target_name': 'nacl_loader_unittests_run',
          'type': 'none',
          'dependencies': [
            'nacl_loader_unittests',
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'nacl_loader_unittests.isolate',
          ],
        },
      ],
    }],
    ['disable_nacl==0 and disable_nacl_untrusted==0 and enable_nacl_nonsfi_test==1', {
      'targets': [
        {
          'target_name': 'nacl_helper_nonsfi_unittests',
          'type': '<(gtest_target_type)',
          'sources': [
            'nacl/loader/nonsfi/nacl_helper_nonsfi_unittests.cc',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:test_launcher_nacl_nonsfi',
            'nacl_nonsfi.gyp:nacl_helper_nonsfi_unittests_main',
          ],
        },
      ],
    }],
  ],
  'targets': [
    {
      # GN version: //components/nacl/common:switches
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
      # GN version: //components/nacl/common
      'target_name': 'nacl_common',
      'type': 'static_library',
      'sources': [
        'nacl/common/nacl_cmd_line.cc',
        'nacl/common/nacl_cmd_line.h',
        'nacl/common/nacl_constants.cc',
        'nacl/common/nacl_constants.h',
        'nacl/common/nacl_host_messages.cc',
        'nacl/common/nacl_host_messages.h',
        'nacl/common/nacl_messages.cc',
        'nacl/common/nacl_messages.h',
        'nacl/common/nacl_nonsfi_util.cc',
        'nacl/common/nacl_nonsfi_util.h',
        'nacl/common/nacl_process_type.h',
        'nacl/common/nacl_renderer_messages.cc',
        'nacl/common/nacl_renderer_messages.h',
        'nacl/common/nacl_sandbox_type.h',
        'nacl/common/nacl_types.cc',
        'nacl/common/nacl_types.h',
        'nacl/common/nacl_types_param_traits.cc',
        'nacl/common/nacl_types_param_traits.h',
        'nacl/common/pnacl_types.cc',
        'nacl/common/pnacl_types.h',
      ],
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../content/content.gyp:content_common',
        '../url/ipc/url_ipc.gyp:url_ipc',
      ],
    },
  ]
}
