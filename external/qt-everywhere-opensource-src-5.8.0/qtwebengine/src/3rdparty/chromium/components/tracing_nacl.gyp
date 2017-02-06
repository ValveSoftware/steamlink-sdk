# Copyright 2014 The Chromium Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../build/common_untrusted.gypi',
  ],
  'conditions': [
    ['disable_nacl==0 and disable_nacl_untrusted==0', {
      'targets': [
        {
          'target_name': 'tracing_nacl',
          'type': 'none',
          'defines!': ['CONTENT_IMPLEMENTATION'],
          'dependencies': [
            '../base/base_nacl.gyp:base_nacl',
            '../base/base_nacl.gyp:base_nacl_nonsfi',
            '../ipc/ipc_nacl.gyp:ipc_nacl',
            '../ipc/ipc_nacl.gyp:ipc_nacl_nonsfi',
          ],
          'include_dirs': [
            '..',
          ],
          'variables': {
            'nacl_untrusted_build': 1,
            'nlib_target': 'libtracing_nacl.a',
            'build_glibc': 0,
            'build_newlib': 0,
            'build_irt': 1,
            'build_pnacl_newlib': 0,
            'build_nonsfi_helper': 1,
          },
          'sources': [
            'tracing/browser/trace_config_file.cc',
            'tracing/browser/trace_config_file.h',
            'tracing/child/child_memory_dump_manager_delegate_impl.cc',
            'tracing/child/child_memory_dump_manager_delegate_impl.h',
            'tracing/child/child_trace_message_filter.cc',
            'tracing/child/child_trace_message_filter.h',
            'tracing/common/tracing_messages.cc',
            'tracing/common/tracing_messages.h',
            'tracing/common/tracing_switches.cc',
            'tracing/common/tracing_switches.h',
            'tracing/tracing_export.h',
          ],
        },
      ],
    }],
  ],
}
