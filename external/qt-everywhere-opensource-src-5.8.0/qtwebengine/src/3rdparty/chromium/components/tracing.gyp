# Copyright (c) 2012 The Chromium Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is intentionally a gyp file rather than a gypi for dependencies
# reasons. The other gypi files include content.gyp and content_common depends
# on this, thus if you try to rename this to gypi and include it in
# components.gyp, you will get a circular dependency error.
{
  'variables': {
    'chromium_code': 1,
  },
  'targets' : [
    {
      'target_name': 'tracing',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../ipc/ipc.gyp:ipc',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'TRACING_IMPLEMENTATION=1',
      ],
      'sources': [
        'tracing/browser/trace_config_file.cc',
        'tracing/browser/trace_config_file.h',
        'tracing/child/child_memory_dump_manager_delegate_impl.cc',
        'tracing/child/child_memory_dump_manager_delegate_impl.h',
        'tracing/child/child_trace_message_filter.cc',
        'tracing/child/child_trace_message_filter.h',
        'tracing/common/graphics_memory_dump_provider_android.cc',
        'tracing/common/graphics_memory_dump_provider_android.h',
        'tracing/common/process_metrics_memory_dump_provider.cc',
        'tracing/common/process_metrics_memory_dump_provider.h',
        'tracing/common/trace_to_console.cc',
        'tracing/common/trace_to_console.h',
        'tracing/common/tracing_messages.cc',
        'tracing/common/tracing_messages.h',
        'tracing/common/tracing_switches.cc',
        'tracing/common/tracing_switches.h',
        'tracing/core/scattered_stream_writer.cc',
        'tracing/core/scattered_stream_writer.h',
        'tracing/core/trace_ring_buffer.cc',
        'tracing/core/trace_ring_buffer.h',
        'tracing/tracing_export.h',
      ],
      'target_conditions': [
        ['>(nacl_untrusted_build)==1', {
          'sources!': [
            'tracing/common/process_metrics_memory_dump_provider.cc',
          ],
        }],
      ]
    },
  ],
}
