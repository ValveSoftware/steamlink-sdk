# -*- python -*-
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'plugin.gypi',
  ],
  'target_defaults': {
    'variables': {
      'target_base': 'none',
    },
    'target_conditions': [
      ['target_base=="ppNaClPlugin"', {
        'sources': [
          '<@(common_sources)',
        ],
        'xcode_settings': {
          'WARNING_CFLAGS!': [
            # TODO(bradnelson): remove -pedantic when --std=c++98 in common.gypi
            '-pedantic',
          ],
          'WARNING_CFLAGS': [
            '-Wno-deprecated',
            '-Wno-deprecated-declarations',
          ],
        },
        'conditions': [
          ['OS=="win"', {
            'sources': [
              'win/nacl_plugin.rc',
            ],
            'msvs_settings': {
              'VCCLCompilerTool': {
                'ExceptionHandling': '2',  # /EHsc
              },
              'VCLinkerTool': {
                'AdditionalLibraryDirectories': [
                   '$(OutDir)/lib',
                ],
              },
            },
          }],
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'ppGoogleNaClPluginChrome',
      'type': 'loadable_module',
      'sources': [
        '<@(common_sources)',
      ],
      'xcode_settings': {
        'OTHER_LDFLAGS': [
          '-Wl,-exported_symbols_list <(DEPTH)/ppapi/native_client/src/trusted/plugin/ppapi.def'
        ],
      },
      'dependencies': [
        '<(DEPTH)/media/media.gyp:shared_memory_support',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio',
        '<(DEPTH)/native_client/src/shared/imc/imc.gyp:imc',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
        '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:nonnacl_srpc',
        '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer',
        '<(DEPTH)/native_client/src/trusted/nonnacl_util/nonnacl_util.gyp:nonnacl_util',
        '<(DEPTH)/native_client/src/trusted/platform_qualify/platform_qualify.gyp:platform_qual_lib',
        '<(DEPTH)/native_client/src/trusted/gio/gio_wrapped_desc.gyp:gio_wrapped_desc',
        '<(DEPTH)/native_client/src/trusted/simple_service/simple_service.gyp:simple_service',
        '<(DEPTH)/native_client/src/trusted/reverse_service/reverse_service.gyp:reverse_service',
        '<(DEPTH)/native_client/src/trusted/weak_ref/weak_ref.gyp:weak_ref',
        '<(DEPTH)/ppapi/ppapi.gyp:ppapi_cpp',
      ],
      'conditions': [
        ['OS=="mac"', {
          'mac_bundle': 1,
          'product_name': 'ppGoogleNaClPluginChrome',
          'product_extension': 'plugin',
        }],
        ['OS=="mac" and mac_breakpad==1', {
          'variables': {
            # A real .dSYM is needed for dump_syms to operate on.
            'mac_real_dsym': 1,
          },
        }],
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
  ],
}
