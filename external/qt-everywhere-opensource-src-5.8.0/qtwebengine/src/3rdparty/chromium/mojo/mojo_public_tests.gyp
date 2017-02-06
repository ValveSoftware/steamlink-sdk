# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'mojo_public_test_interfaces_mojom_files': [
      'public/interfaces/bindings/tests/math_calculator.mojom',
      'public/interfaces/bindings/tests/no_module.mojom',
      'public/interfaces/bindings/tests/ping_service.mojom',
      'public/interfaces/bindings/tests/rect.mojom',
      'public/interfaces/bindings/tests/regression_tests.mojom',
      'public/interfaces/bindings/tests/sample_factory.mojom',
      'public/interfaces/bindings/tests/sample_import.mojom',
      'public/interfaces/bindings/tests/sample_import2.mojom',
      'public/interfaces/bindings/tests/sample_interfaces.mojom',
      'public/interfaces/bindings/tests/sample_service.mojom',
      'public/interfaces/bindings/tests/scoping.mojom',
      'public/interfaces/bindings/tests/serialization_test_structs.mojom',
      'public/interfaces/bindings/tests/test_constants.mojom',
      'public/interfaces/bindings/tests/test_native_types.mojom',
      'public/interfaces/bindings/tests/test_sync_methods.mojom',
    ],
  },
  'target_defaults' : {
    'include_dirs': [
      '..',
    ],
  },
  'targets': [
    {
      'target_name': 'mojo_public_test_interfaces_mojom',
      'type': 'none',
      'variables': {
        'mojom_files': [
          'public/interfaces/bindings/tests/test_structs.mojom',
          'public/interfaces/bindings/tests/test_unions.mojom',
          'public/interfaces/bindings/tests/validation_test_interfaces.mojom',
          '<@(mojo_public_test_interfaces_mojom_files)',
        ],
        'mojom_typemaps': [
          'public/cpp/bindings/tests/rect_chromium.typemap',
          'public/cpp/bindings/tests/test_native_types_chromium.typemap',
        ],
      },
      'includes': [ 'mojom_bindings_generator_explicit.gypi' ],
    },
    {
      'target_name': 'mojo_public_test_interfaces_struct_traits',
      'type': 'static_library',
      'variables': {
        'mojom_typemaps': [
          'public/cpp/bindings/tests/struct_with_traits.typemap',
        ],
      },
      'sources': [
        'public/interfaces/bindings/tests/struct_with_traits.mojom',
        'public/cpp/bindings/tests/struct_with_traits_impl_traits.cc',
      ],
      'includes': [ 'mojom_bindings_generator.gypi' ],
    },
    {
      # GN version: //mojo/public/interfaces/bindings/tests:test_interfaces
      'target_name': 'mojo_public_test_interfaces',
      'type': 'static_library',
      'export_dependent_settings': [
        'mojo_public.gyp:mojo_cpp_bindings',
      ],
      'sources': [
        'public/cpp/bindings/tests/pickled_types_chromium.cc',
      ],
      'dependencies': [
        '../ipc/ipc.gyp:ipc',
        'mojo_public.gyp:mojo_cpp_bindings',
        'mojo_public_test_interfaces_mojom',
      ],
    },
    {
      'target_name': 'mojo_public_test_associated_interfaces_mojom',
      'type': 'none',
      'variables': {
        # These files are not included in the mojo_public_test_interfaces_mojom
        # target because associated interfaces are not supported by all bindings
        # languages yet.
        'mojom_files': [
          'public/interfaces/bindings/tests/test_associated_interfaces.mojom',
          'public/interfaces/bindings/tests/validation_test_associated_interfaces.mojom',
        ],
      },
      'includes': [ 'mojom_bindings_generator_explicit.gypi' ],
    },
    {
      # GN version: //mojo/public/interfaces/bindings/tests:test_associated_interfaces
      'target_name': 'mojo_public_test_associated_interfaces',
      'type': 'static_library',
      'export_dependent_settings': [
        'mojo_public.gyp:mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_public.gyp:mojo_cpp_bindings',
        'mojo_public_test_associated_interfaces_mojom',
      ],
    },
    {
      'target_name': 'mojo_public_test_wtf_types',
      'type': 'static_library',
      'sources': [
        'public/interfaces/bindings/tests/test_wtf_types.mojom',
      ],
      'includes': [ 'mojom_bindings_generator.gypi' ],
    },
  ],
  'conditions': [
    ['OS != "ios"', {
      'targets': [
        {
          'target_name': 'mojo_public_test_interfaces_mojom_blink',
          'type': 'none',
          'variables': {
            'for_blink': 'true',
            'mojom_typemaps': [
              'public/cpp/bindings/tests/rect_blink.typemap',
              'public/cpp/bindings/tests/test_native_types_blink.typemap',
            ],
            'mojom_files': '<(mojo_public_test_interfaces_mojom_files)',
          },
          'includes': [ 'mojom_bindings_generator_explicit.gypi' ],
        },
        {
          # GN version: //mojo/public/interfaces/bindings/tests:test_interfaces_blink
          'target_name': 'mojo_public_test_interfaces_blink',
          'type': 'static_library',
          'export_dependent_settings': [
            'mojo_public.gyp:mojo_cpp_bindings',
            'mojo_public_test_interfaces_mojom_blink',
          ],
          'sources': [
            'public/cpp/bindings/tests/pickled_types_blink.cc',
          ],
          'dependencies': [
            '../ipc/ipc.gyp:ipc',
            'mojo_public.gyp:mojo_cpp_bindings',
            'mojo_public_test_interfaces_mojom_blink',
          ],
        },
        {
          'target_name': 'mojo_public_test_wtf_types_blink',
          'type': 'static_library',
          'variables': {
            'for_blink': 'true',
          },
          'sources': [
            'public/interfaces/bindings/tests/test_wtf_types.mojom',
          ],
          'includes': [ 'mojom_bindings_generator.gypi' ],
        },
      ],
    }],
  ],
}

