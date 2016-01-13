# -*- python -*-
# Copyright (c) 2011 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'variables': {
      'src_files': ['parsing.cc',
                    'pnacl_emu_coordinator.cc',
                    'pnacl_emu_handler.cc',
                    'pnacl_emu_stream.cc',
                    'reverse_emulate.cc',
                    'replay_handler.cc',
                    'rpc_universal_system.cc',
                    'rpc_universal.cc',
                    'sel_universal.cc',
                    ],
  },
  'targets': [
    {
      'target_name': 'sel_universal',
      'type': 'executable',
      'sources': ['<@(src_files)'],
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/imc/imc.gyp:imc',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
        '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:nonnacl_srpc',
        '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer',
        '<(DEPTH)/native_client/src/trusted/reverse_service/reverse_service.gyp:reverse_service',
        '<(DEPTH)/native_client/src/trusted/service_runtime/service_runtime.gyp:nacl_error_code',
        '<(DEPTH)/native_client/src/trusted/simple_service/simple_service.gyp:simple_service',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio',
        '<(DEPTH)/native_client/src/trusted/nonnacl_util/nonnacl_util.gyp:nonnacl_util',
      ],
      'conditions': [
        ['OS=="linux"', {
          'link_settings': {
            'libraries': [
              '-ldl',
            ],
          },
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'sel_universal64',
          'type': 'executable',
          'variables': {
            'win_target': 'x64',
          },
          'sources': ['<@(src_files)'],
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/imc/imc.gyp:imc64',
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform64',
            '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:nonnacl_srpc64',
            '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer64',
            '<(DEPTH)/native_client/src/trusted/reverse_service/reverse_service.gyp:reverse_service64',
            '<(DEPTH)/native_client/src/trusted/service_runtime/service_runtime.gyp:nacl_error_code64',
            '<(DEPTH)/native_client/src/trusted/simple_service/simple_service.gyp:simple_service64',
            '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio64',
            '<(DEPTH)/native_client/src/trusted/nonnacl_util/nonnacl_util.gyp:nonnacl_util64',
          ],
        },
      ],
    }],
  ],
}
