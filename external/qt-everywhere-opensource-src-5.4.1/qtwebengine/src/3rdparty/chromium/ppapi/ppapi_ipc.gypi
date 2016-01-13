# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'variables': {
      'nacl_win64_target': 0,
      'ppapi_ipc_target': 0,
    },
    'target_conditions': [
      # This part is shared between the targets defined below.
      ['ppapi_ipc_target==1', {
        'sources': [
          'proxy/nacl_message_scanner.cc',
          'proxy/nacl_message_scanner.h',
          'proxy/ppapi_messages.cc',
          'proxy/ppapi_messages.h',
          'proxy/ppapi_param_traits.cc',
          'proxy/ppapi_param_traits.h',
          'proxy/raw_var_data.cc',
          'proxy/raw_var_data.h',
          'proxy/resource_message_params.cc',
          'proxy/resource_message_params.h',
          'proxy/serialized_flash_menu.cc',
          'proxy/serialized_flash_menu.h',
          'proxy/serialized_handle.cc',
          'proxy/serialized_handle.h',
          'proxy/serialized_structs.cc',
          'proxy/serialized_structs.h',
          'proxy/serialized_var.cc',
          'proxy/serialized_var.h',
          'proxy/var_serialization_rules.h',
        ],
        'include_dirs': [
          '..',
        ],
        'target_conditions': [
          ['>(nacl_untrusted_build)==1 or >(nacl_win64_target)==1', {
            'sources!': [
              'proxy/serialized_flash_menu.cc',
            ],
          }],
        ],
      }],
    ],
  },
}
