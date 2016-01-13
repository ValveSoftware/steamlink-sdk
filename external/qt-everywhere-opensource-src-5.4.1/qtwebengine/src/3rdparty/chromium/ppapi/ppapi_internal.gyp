# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,  # Use higher warning level.
  },
  'target_defaults': {
    'conditions': [
      # Linux shared libraries should always be built -fPIC.
      #
      # TODO(ajwong): For internal pepper plugins, which are statically linked
      # into chrome, do we want to build w/o -fPIC?  If so, how can we express
      # that in the build system?
      ['os_posix==1 and OS!="android" and OS!="mac"', {
        'cflags': ['-fPIC', '-fvisibility=hidden'],

        # This is needed to make the Linux shlib build happy. Without this,
        # -fvisibility=hidden gets stripped by the exclusion in common.gypi
        # that is triggered when a shared library build is specified.
        'cflags/': [['include', '^-fvisibility=hidden$']],
      }],
    ],
  },
  'includes': [
    'ppapi_sources.gypi',
    'ppapi_host.gypi',
    'ppapi_ipc.gypi',
    'ppapi_proxy.gypi',
    'ppapi_shared.gypi',
    'ppapi_tests.gypi',
  ],
  'targets': [
    {
      'target_name': 'ppapi_shared',
      'type': '<(component)',
      'variables': {
        # Set the ppapi_shared_target variable, so that we will pull in the
        # sources from ppapi_shared.gypi (and only from there). We follow the
        # same pattern for the other targets defined within this file.
        'ppapi_shared_target': 1,
      },
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../gpu/command_buffer/command_buffer.gyp:gles2_utils',
        '../gpu/gpu.gyp:command_buffer_client',
        '../gpu/gpu.gyp:gles2_implementation',
        '../ipc/ipc.gyp:ipc',
        '../media/media.gyp:shared_memory_support',
        '../skia/skia.gyp:skia',
        '../third_party/icu/icu.gyp:icuuc',
        '../ui/events/events.gyp:events_base',
        '../ui/surface/surface.gyp:surface',
        '../url/url.gyp:url_lib',
        'ppapi.gyp:ppapi_c',
      ],
      'export_dependent_settings': [
        '../base/base.gyp:base',
      ],
      'conditions': [
        ['OS=="mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/QuartzCore.framework',
            ],
          },
        }],
        ['chrome_multiple_dll==1', {
          'dependencies': [
            '../third_party/WebKit/public/blink.gyp:blink_minimal',
          ],
          'export_dependent_settings': [
            '../third_party/WebKit/public/blink.gyp:blink_minimal',
          ],
        }, {
          'dependencies': [
            '../third_party/WebKit/public/blink.gyp:blink',
          ],
          'export_dependent_settings': [
            '../third_party/WebKit/public/blink.gyp:blink',
          ],
        }],
      ],
      # Disable c4267 warnings until we fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
  ],
  'conditions': [
    ['component=="static_library"', {
      # In a static build, build ppapi_ipc separately.
      'targets': [
        {
          'target_name': 'ppapi_ipc',
          'type': 'static_library',
          'variables': {
            'ppapi_ipc_target': 1,
          },
          'dependencies': [
            '../base/base.gyp:base',
            '../gpu/gpu.gyp:gpu_ipc',
            '../ipc/ipc.gyp:ipc',
            '../skia/skia.gyp:skia',
            '../ui/events/ipc/events_ipc.gyp:events_ipc',
            'ppapi.gyp:ppapi_c',
            'ppapi_shared',
          ],
          'all_dependent_settings': {
            'include_dirs': [
                '..',
            ],
          },
        },
        {
          'target_name': 'ppapi_proxy',
          'type': 'static_library',
          'variables': {
            'ppapi_proxy_target': 1,
          },
          'dependencies': [
            '../base/base.gyp:base',
            '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
            '../gpu/gpu.gyp:gles2_implementation',
            '../gpu/gpu.gyp:gpu_ipc',
            '../media/media.gyp:shared_memory_support',
            '../ipc/ipc.gyp:ipc',
            '../skia/skia.gyp:skia',
            '../third_party/icu/icu.gyp:icuuc',
            '../third_party/icu/icu.gyp:icui18n',
            '../ui/events/events.gyp:events_base',
            '../ui/surface/surface.gyp:surface',
            'ppapi.gyp:ppapi_c',
            'ppapi_shared',
            'ppapi_ipc',
          ],
          'all_dependent_settings': {
            'include_dirs': [
                '..',
            ],
          },
          # Disable c4267 warnings until we fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
          'conditions': [
            ['chrome_multiple_dll==1', {
              'dependencies': [
                '../third_party/WebKit/public/blink.gyp:blink_minimal',
              ],
            }, {
              'dependencies': [
                '../third_party/WebKit/public/blink.gyp:blink',
              ],
            }],
          ],
        },
      ],
    },
    { # component != static_library
      # In the component build, we'll just build ppapi_ipc in to ppapi_proxy.
      'targets': [
        {
          'target_name': 'ppapi_proxy',
          'type': 'shared_library',
          'variables': {
            # Setting both variables means we pull in the sources from both
            # ppapi_ipc.gypi and ppapi_proxy.gypi.
            'ppapi_ipc_target': 1,
            'ppapi_proxy_target': 1,
          },
          'dependencies': [
            '../base/base.gyp:base',
            '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
            '../gpu/gpu.gyp:gles2_implementation',
            '../gpu/gpu.gyp:gpu_ipc',
            '../media/media.gyp:shared_memory_support',
            '../ipc/ipc.gyp:ipc',
            '../skia/skia.gyp:skia',
            '../third_party/icu/icu.gyp:icuuc',
            '../third_party/icu/icu.gyp:icui18n',
            '../ui/events/events.gyp:events_base',
            '../ui/events/ipc/events_ipc.gyp:events_ipc',
            '../ui/surface/surface.gyp:surface',
            'ppapi.gyp:ppapi_c',
            'ppapi_shared',
          ],
          'all_dependent_settings': {
            'include_dirs': [
                '..',
            ],
          },
          # Disable c4267 warnings until we fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
          'conditions': [
            ['chrome_multiple_dll==1', {
              'dependencies': [
                '../third_party/WebKit/public/blink.gyp:blink_minimal',
              ],
            }, {
              'dependencies': [
                '../third_party/WebKit/public/blink.gyp:blink',
              ],
            }],
          ],
        },
        {
          # In component build, this is just a phony target that makes sure
          # ppapi_proxy is built, since that's where the ipc sources go in the
          # component build.
          'target_name': 'ppapi_ipc',
          'type': 'none',
          'dependencies': [
            'ppapi_proxy',
          ],
        },
      ],
    }],
    ['disable_nacl!=1 and OS=="win" and target_arch=="ia32"', {
      # In windows builds, we also want to define some targets to build in
      # 64-bit mode for use by nacl64.exe (the NaCl helper process for 64-bit
      # Windows).
      'targets': [
        {
          'target_name': 'ppapi_shared_win64',
          'type': '<(component)',
          'variables': {
            'nacl_win64_target': 1,
            'ppapi_shared_target': 1,
          },
          'dependencies': [
            'ppapi.gyp:ppapi_c',
            '../base/base.gyp:base_win64',
            '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations_win64',
            '../gpu/gpu.gyp:command_buffer_common_win64',
            '../ipc/ipc.gyp:ipc_win64',
            '../ui/events/latency_info_nacl.gyp:latency_info_nacl_win64',
          ],
          'defines': [
            '<@(nacl_win64_defines)',
          ],              
          'export_dependent_settings': [
            '../base/base.gyp:base_win64',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
        {
          'target_name': 'ppapi_ipc_win64',
          'type': 'static_library',
          'variables': {
            'nacl_win64_target': 1,
            'ppapi_ipc_target': 1,
          },
          'dependencies': [
            '../base/base.gyp:base_win64',
            '../ipc/ipc.gyp:ipc_win64',
            '../gpu/gpu.gyp:gpu_ipc_win64',
            '../ui/events/latency_info_nacl.gyp:latency_info_nacl_win64',
            'ppapi.gyp:ppapi_c',
            'ppapi_shared_win64',
          ],
          'export_dependent_settings': [
            '../gpu/gpu.gyp:gpu_ipc_win64',
          ],
          'defines': [
            '<@(nacl_win64_defines)',
          ],              
          'all_dependent_settings': {
            'include_dirs': [
               '..',
            ],
          },
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
      }],
    }],
  ],
}
