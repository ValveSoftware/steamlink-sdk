# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
  {
    'target_name': 'shell_interfaces',
    'type': 'none',
    'variables': {
      'mojom_files': [
        '../catalog/public/interfaces/catalog.mojom',
        'public/interfaces/capabilities.mojom',
        'public/interfaces/connector.mojom',
        'public/interfaces/interface_provider.mojom',
        'public/interfaces/shell.mojom',
        'public/interfaces/shell_client.mojom',
        'public/interfaces/shell_client_factory.mojom',
        'public/interfaces/shell_resolver.mojom',
      ],
      'mojom_typemaps': [
        '<(DEPTH)/mojo/common/common_custom_types.typemap',
      ],
    },
    'includes': [ '../../mojo/mojom_bindings_generator_explicit.gypi' ],
    'dependencies': [
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_common_custom_types_mojom',
    ],
    'export_dependent_settings': [
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_common_custom_types_mojom',
    ],
  },
  {
    # GN version: //services/shell/public/cpp
    'target_name': 'shell_public',
    'type': 'static_library',
    'sources': [
      'public/cpp/application_runner.h',
      'public/cpp/capabilities.h',
      'public/cpp/connect.h',
      'public/cpp/connection.h',
      'public/cpp/connector.h',
      'public/cpp/identity.h',
      'public/cpp/interface_binder.h',
      'public/cpp/interface_factory.h',
      'public/cpp/interface_factory_impl.h',
      'public/cpp/interface_registry.h',
      'public/cpp/lib/application_runner.cc',
      'public/cpp/lib/capabilities.cc',
      'public/cpp/lib/callback_binder.cc',
      'public/cpp/lib/callback_binder.h',
      'public/cpp/lib/connection_impl.cc',
      'public/cpp/lib/connection_impl.h',
      'public/cpp/lib/connector_impl.cc',
      'public/cpp/lib/connector_impl.h',
      'public/cpp/lib/identity.cc',
      'public/cpp/lib/interface_factory_binder.h',
      'public/cpp/lib/interface_registry.cc',
      'public/cpp/lib/names.cc',
      'public/cpp/lib/interface_provider.cc',
      'public/cpp/lib/shell_client.cc',
      'public/cpp/lib/shell_connection.cc',
      'public/cpp/lib/shell_connection_ref.cc',
      'public/cpp/names.h',
      'public/cpp/interface_provider.h',
      'public/cpp/shell_client.h',
      'public/cpp/shell_connection.h',
      'public/cpp/shell_connection_ref.h',
    ],
    'dependencies': [
      'shell_interfaces',
      '<(DEPTH)/base/base.gyp:base_i18n',
      '<(DEPTH)/mojo/mojo_public.gyp:mojo_cpp_bindings',
    ],
  },
  ],
}
