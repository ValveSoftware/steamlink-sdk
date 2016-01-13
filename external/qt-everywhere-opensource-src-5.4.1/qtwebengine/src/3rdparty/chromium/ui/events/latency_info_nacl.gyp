{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
   '../../build/common_untrusted.gypi',
  ],
  'conditions': [
    ['disable_nacl==0 and disable_nacl_untrusted==0', {
      'targets': [
        {
          'target_name': 'latency_info_nacl',
          'type': 'none',
          'defines': [
            'EVENTS_BASE_IMPLEMENTATION',
            'EVENTS_IMPLEMENTATION',
          ],
          'include_dirs': [
            '../..',
          ],
          'dependencies': [
            '<(DEPTH)/base/base_nacl.gyp:base_nacl',
            '<(DEPTH)/ipc/ipc_nacl.gyp:ipc_nacl',
            '<(DEPTH)/native_client/tools.gyp:prep_toolchain'
          ],
          'variables': {
            'nacl_untrusted_build': 1,
            'nlib_target': 'liblatency_info_nacl.a',
            'build_glibc': 0,
            'build_newlib': 0,
            'build_irt': 1,
          },
          'sources': [
            'latency_info.cc',
            'latency_info.h',
            'ipc/latency_info_param_traits.cc',
            'ipc/latency_info_param_traits.h',
          ],
        },
      ],
    }],
    ['disable_nacl!=1 and OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'latency_info_nacl_win64',
          'type' : '<(component)',
          'variables': {
            'nacl_win64_target': 1,
          },
          'dependencies': [
            '<(DEPTH)/base/base.gyp:base_win64',
            '<(DEPTH)/ipc/ipc.gyp:ipc_win64',
          ],
          'defines': [
            'EVENTS_BASE_IMPLEMENTATION',
            'EVENTS_IMPLEMENTATION',
            '<@(nacl_win64_defines)',
          ],
          'include_dirs': [
            '../..',
          ],
          'sources': [
            'latency_info.cc',
            'latency_info.h',
            'ipc/latency_info_param_traits.cc',
            'ipc/latency_info_param_traits.h',
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

