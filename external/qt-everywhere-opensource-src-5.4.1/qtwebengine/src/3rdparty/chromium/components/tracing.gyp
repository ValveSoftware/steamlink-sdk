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
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../ipc/ipc.gyp:ipc',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'tracing/child_trace_message_filter.cc',
        'tracing/child_trace_message_filter.h',
        'tracing/tracing_messages.cc',
        'tracing/tracing_messages.h',
      ],
    },
  ],
}
