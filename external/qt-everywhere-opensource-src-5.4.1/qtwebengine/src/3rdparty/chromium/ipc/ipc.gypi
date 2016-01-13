# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'variables': {
      'ipc_target': 0,
    },
    'target_conditions': [
      # This part is shared between the targets defined below.
      ['ipc_target==1', {
        'sources': [
          'file_descriptor_set_posix.cc',
          'file_descriptor_set_posix.h',
          'ipc_channel.cc',
          'ipc_channel.h',
          'ipc_channel_common.cc',
          'ipc_channel_factory.cc',
          'ipc_channel_factory.h',
          'ipc_channel_handle.h',
          'ipc_channel_nacl.cc',
          'ipc_channel_nacl.h',
          'ipc_channel_posix.cc',
          'ipc_channel_posix.h',
          'ipc_channel_proxy.cc',
          'ipc_channel_proxy.h',
          'ipc_channel_reader.cc',
          'ipc_channel_reader.h',
          'ipc_channel_win.cc',
          'ipc_channel_win.h',
          'ipc_descriptors.h',
          'ipc_export.h',
          'ipc_forwarding_message_filter.cc',
          'ipc_forwarding_message_filter.h',
          'ipc_listener.h',
          'ipc_logging.cc',
          'ipc_logging.h',
          'ipc_message.cc',
          'ipc_message.h',
          'ipc_message_macros.h',
          'ipc_message_start.h',
          'ipc_message_utils.cc',
          'ipc_message_utils.h',
          'ipc_param_traits.h',
          'ipc_platform_file.cc',
          'ipc_platform_file.h',
          'ipc_sender.h',
          'ipc_switches.cc',
          'ipc_switches.h',
          'ipc_sync_channel.cc',
          'ipc_sync_channel.h',
          'ipc_sync_message.cc',
          'ipc_sync_message.h',
          'ipc_sync_message_filter.cc',
          'ipc_sync_message_filter.h',
          'message_filter.cc',
          'message_filter.h',
          'message_filter_router.cc',
          'message_filter_router.h',
          'param_traits_log_macros.h',
          'param_traits_macros.h',
          'param_traits_read_macros.h',
          'param_traits_write_macros.h',
          'struct_constructor_macros.h',
          'struct_destructor_macros.h',
          'unix_domain_socket_util.cc',
          'unix_domain_socket_util.h',
        ],
        'defines': [
          'IPC_IMPLEMENTATION',
        ],
        'include_dirs': [
          '..',
        ],
        'target_conditions': [
          ['>(nacl_untrusted_build)==1', {
            'sources!': [
              'ipc_channel.cc',
              'ipc_channel_factory.cc',
              'ipc_channel_posix.cc',
              'unix_domain_socket_util.cc',
            ],
          }],
          ['OS == "win" or OS == "ios"', {
            'sources!': [
              'ipc_channel_factory.cc',
              'unix_domain_socket_util.cc',
            ],
          }],
        ],
      }],
    ],
  },
}
