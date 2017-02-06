# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      # GN version: //media/mojo/interfaces
      'target_name': 'audio_output_mojom',
      'type': 'none',
      'variables': {
        'mojom_files': [
          'audio_output.mojom',
          'audio_parameters.mojom',
        ],
      },
      'dependencies': [
       '../../../mojo/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'includes': [ '../../../mojo/mojom_bindings_generator_explicit.gypi' ],
      'mojom_typemaps': [
          'audio_parameters.typemap',
      ],
    },
    {
      'target_name': 'audio_output_mojom_bindings',
      'type': 'static_library',
      'dependencies': [
        'audio_output_mojom',
      ],
    },
    {
      # GN version: //media/mojo/interfaces
      'target_name': 'platform_verification_mojo_bindings',
      'type': 'none',
      'sources': [
        'platform_verification.mojom',
      ],
      'includes': [ '../../../mojo/mojom_bindings_generator.gypi' ],
    },
    {
      'target_name': 'platform_verification_api',
      'type': 'static_library',
      'dependencies': [
        'platform_verification_mojo_bindings',
        '../../../mojo/mojo_public.gyp:mojo_cpp_bindings',
        '../../../services/shell/shell_public.gyp:shell_public',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/media/mojo/interfaces/platform_verification.mojom.cc',
        '<(SHARED_INTERMEDIATE_DIR)/media/mojo/interfaces/platform_verification.mojom.h',
      ],
    },
    {
      # GN version: //media/mojo/interfaces
      'target_name': 'provision_fetcher_mojo_bindings',
      'type': 'none',
      'variables': {
        'mojom_files': [
          'provision_fetcher.mojom',
        ],
      },
      'includes': [ '../../../mojo/mojom_bindings_generator_explicit.gypi' ],
    },
    {
      'target_name': 'provision_fetcher_api',
      'type': 'static_library',
      'dependencies': [
        'provision_fetcher_mojo_bindings',
        '../../../mojo/mojo_public.gyp:mojo_cpp_bindings',
        '../../../services/shell/shell_public.gyp:shell_public',
      ],
    },
    {
      # GN version: //media/mojo/interfaces:image_capture
      'target_name': 'image_capture_mojo_bindings',
      'type': 'static_library',
      'includes': [
        '../../../mojo/mojom_bindings_generator.gypi',
      ],
      'sources': [
        'image_capture.mojom',
      ],
    },
    {
      # GN version: //media/mojo/interfaces:image_capture
      'target_name': 'image_capture_mojo_bindings_for_blink',
      'type': 'static_library',
      'variables': {
        'for_blink': 'true',
      },
      'includes': [
        '../../../mojo/mojom_bindings_generator.gypi',
      ],
      'sources': [
        'image_capture.mojom',
      ],
    },
  ],
}
