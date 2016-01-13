# -*- python -*-
#
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

# TODO(sehr): remove need for the warning flag removals below
{
  'includes': [
    '../../../build/common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'target_base': 'none',
    },
    'target_conditions': [
      ['target_base=="sel_ldr_launcher"', {
        'sources': [
          'launcher_factory.cc',
          'sel_ldr_launcher.h',
          'sel_ldr_launcher_base.cc',
          'sel_ldr_launcher_standalone.cc',
        ],
        'conditions':[
          ['OS=="win"', {
            'sources': [
              'win/sel_ldr_launcher_win.cc',
            ],
          }],
          ['OS=="linux" or OS=="android"', {
            'sources': [
              'posix/sel_ldr_launcher_posix.cc',
              'posix/sel_ldr_launcher_zygote_posix.cc',
              'posix/get_plugin_dirname.cc',
            ],
          }],
          ['OS=="mac"', {
            'sources': [
              'posix/sel_ldr_launcher_posix.cc',
              'posix/sel_ldr_launcher_zygote_posix.cc',
              'osx/get_plugin_dirname.mm',
            ],
          }]
        ],
        'cflags!': [
          '-Wextra',
        ],
        'xcode_settings': {
          'WARNING_CFLAGS!': [
            '-pedantic',  # import is a gcc extension
            '-Wextra',
          ]
        },
      }],
    ]
  },
  'targets': [
    # ----------------------------------------------------------------------
    {
      'target_name': 'sel_ldr_launcher',
      'type': 'static_library',
      'variables': {
        'target_base': 'sel_ldr_launcher',
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio',
        '<(DEPTH)/native_client/src/shared/imc/imc.gyp:imc',
        '<(DEPTH)/native_client/src/shared/serialization/serialization.gyp:serialization',
        '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:nonnacl_srpc',
        '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer',
        '<(DEPTH)/native_client/src/trusted/service_runtime/service_runtime.gyp:env_cleanser',
        '<(DEPTH)/native_client/src/trusted/service_runtime/service_runtime.gyp:nacl_error_code',
      ],
    },
    # ----------------------------------------------------------------------
    {
      'target_name': 'nonnacl_util',
      'type': 'static_library',
      'dependencies': [
        'sel_ldr_launcher',
      ],
      'includes': [
        'nonnacl_util.gypi',
      ],
    },
  ],
  # ----------------------------------------------------------------------
  'conditions': [
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        # --------------------------------------------------------------------
        {
          'target_name': 'sel_ldr_launcher64',
          'type': 'static_library',
          'variables': {
            'target_base': 'sel_ldr_launcher',
            'win_target': 'x64',
          },
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio64',
            '<(DEPTH)/native_client/src/shared/imc/imc.gyp:imc64',
            '<(DEPTH)/native_client/src/shared/serialization/serialization.gyp:serialization64',
            '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:nonnacl_srpc64',
            '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer64',
            '<(DEPTH)/native_client/src/trusted/service_runtime/service_runtime.gyp:env_cleanser64',
            '<(DEPTH)/native_client/src/trusted/service_runtime/service_runtime.gyp:nacl_error_code64',
          ],
        },
        # --------------------------------------------------------------------
        {
          'target_name': 'nonnacl_util64',
          'type': 'static_library',
          'variables': {
            'win_target': 'x64',
          },
          'dependencies': [
            'sel_ldr_launcher64',
          ],
          'includes': [
            'nonnacl_util.gypi',
          ],
        },
      ],
    }],
  ],
}
