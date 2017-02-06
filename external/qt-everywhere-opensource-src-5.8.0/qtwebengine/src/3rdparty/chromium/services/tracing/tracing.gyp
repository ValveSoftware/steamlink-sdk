# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'tracing_interfaces',
      'type': 'none',
      'variables': {
        'mojom_files': [
          'public/interfaces/tracing.mojom',
        ],
        'mojom_include_path': '<(DEPTH)/mojo/services',
      },
      'includes': [
        '../../mojo/mojom_bindings_generator_explicit.gypi',
      ],
    },
    {
      # Technically, these should be in the mojo_services.gyp, but this causes
      # a cycle since the ios generator can't have gyp files refer to each
      # other, even if the targets don't form a cycle.
      #
      # GN version: //services/tracing:lib
      'target_name': 'tracing_lib',
      'type': 'static_library',
      'dependencies': [
        'tracing_public',
        '../../mojo/mojo_edk.gyp:mojo_system_impl',
        '../shell/shell_public.gyp:shell_public',
      ],
      'sources': [
        'trace_data_sink.cc',
        'trace_data_sink.h',
        'trace_recorder_impl.cc',
        'trace_recorder_impl.h',
        'tracing_app.cc',
        'tracing_app.h',
      ],
    },
    {
      # GN version: //mojo/services/public/cpp
      'target_name': 'tracing_public',
      'type': 'static_library',
      'dependencies': [
        'tracing_interfaces',
        '../../mojo/mojo_edk.gyp:mojo_system_impl',
        '../shell/shell_public.gyp:shell_public',
      ],
      'sources': [
        'public/cpp/switches.cc',
        'public/cpp/switches.h',
        'public/cpp/tracing_impl.cc',
        'public/cpp/tracing_impl.h',
        'public/cpp/trace_provider_impl.cc',
        'public/cpp/trace_provider_impl.h',
      ],
    },
  ],
}
