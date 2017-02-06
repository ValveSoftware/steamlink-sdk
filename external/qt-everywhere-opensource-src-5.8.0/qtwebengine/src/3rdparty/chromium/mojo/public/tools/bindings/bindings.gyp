# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../mojom_bindings_generator_variables.gypi',
  ],
  'targets': [
    {
      'target_name': 'precompile_mojom_bindings_generator_templates',
      'type': 'none',
      'actions': [
        {
          'action_name': 'precompile_mojom_bindings_generator_templates',
          'inputs': [
            '<@(mojom_bindings_generator_sources)',
            'generators/cpp_templates/enum_macros.tmpl',
            'generators/cpp_templates/enum_serialization_declaration.tmpl',
            'generators/cpp_templates/interface_declaration.tmpl',
            'generators/cpp_templates/interface_definition.tmpl',
            'generators/cpp_templates/interface_macros.tmpl',
            'generators/cpp_templates/interface_proxy_declaration.tmpl',
            'generators/cpp_templates/interface_request_validator_declaration.tmpl',
            'generators/cpp_templates/interface_response_validator_declaration.tmpl',
            'generators/cpp_templates/interface_stub_declaration.tmpl',
            'generators/cpp_templates/module.cc.tmpl',
            'generators/cpp_templates/module.h.tmpl',
            'generators/cpp_templates/module-internal.h.tmpl',
            'generators/cpp_templates/struct_data_view_declaration.tmpl',
            'generators/cpp_templates/struct_data_view_definition.tmpl',
            'generators/cpp_templates/struct_declaration.tmpl',
            'generators/cpp_templates/struct_definition.tmpl',
            'generators/cpp_templates/struct_macros.tmpl',
            'generators/cpp_templates/struct_serialization_declaration.tmpl',
            'generators/cpp_templates/struct_serialization_definition.tmpl',
            'generators/cpp_templates/union_declaration.tmpl',
            'generators/cpp_templates/union_definition.tmpl',
            'generators/cpp_templates/union_serialization_declaration.tmpl',
            'generators/cpp_templates/union_serialization_definition.tmpl',
            'generators/cpp_templates/validation_macros.tmpl',
            'generators/cpp_templates/wrapper_class_declaration.tmpl',
            'generators/cpp_templates/wrapper_class_definition.tmpl',
            'generators/cpp_templates/wrapper_class_template_definition.tmpl',
            'generators/cpp_templates/wrapper_union_class_declaration.tmpl',
            'generators/cpp_templates/wrapper_union_class_definition.tmpl',
            'generators/cpp_templates/wrapper_union_class_template_definition.tmpl',
            'generators/java_templates/constant_definition.tmpl',
            'generators/java_templates/constants.java.tmpl',
            'generators/java_templates/data_types_definition.tmpl',
            'generators/java_templates/enum_definition.tmpl',
            'generators/java_templates/enum.java.tmpl',
            'generators/java_templates/header.java.tmpl',
            'generators/java_templates/interface_definition.tmpl',
            'generators/java_templates/interface_internal.java.tmpl',
            'generators/java_templates/interface.java.tmpl',
            'generators/java_templates/struct.java.tmpl',
            'generators/java_templates/union.java.tmpl',
            'generators/js_templates/enum_definition.tmpl',
            'generators/js_templates/interface_definition.tmpl',
            'generators/js_templates/module_definition.tmpl',
            'generators/js_templates/module.amd.tmpl',
            'generators/js_templates/struct_definition.tmpl',
            'generators/js_templates/union_definition.tmpl',
            'generators/js_templates/validation_macros.tmpl',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/mojo/public/tools/bindings/cpp_templates.zip',
            '<(SHARED_INTERMEDIATE_DIR)/mojo/public/tools/bindings/java_templates.zip',
            '<(SHARED_INTERMEDIATE_DIR)/mojo/public/tools/bindings/js_templates.zip',
          ],
          'action': [
            'python', '<@(mojom_bindings_generator)',
            '--use_bundled_pylibs', 'precompile',
            '-o', '<(SHARED_INTERMEDIATE_DIR)/mojo/public/tools/bindings',
          ],
        }
      ],
      'hard_dependency': 1,
    },
  ],
}

