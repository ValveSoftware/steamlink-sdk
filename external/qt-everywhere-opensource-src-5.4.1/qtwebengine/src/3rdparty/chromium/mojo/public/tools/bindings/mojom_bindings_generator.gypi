# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'rules': [
    {
      'rule_name': 'Generate C++, JS and Java source files from mojom files',
      'extension': 'mojom',
      'variables': {
        'mojom_base_output_dir':
             '<!(python <(DEPTH)/build/inverse_depth.py <(DEPTH))',
        'mojom_bindings_generator':
            '<(DEPTH)/mojo/public/tools/bindings/mojom_bindings_generator.py',
        'java_out_dir': '<(PRODUCT_DIR)/java_mojo/<(_target_name)/src',
      },
      'inputs': [
        '<(mojom_bindings_generator)',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/enum_declaration.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/interface_declaration.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/interface_definition.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/interface_macros.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/interface_proxy_declaration.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/interface_request_validator_declaration.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/interface_response_validator_declaration.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/interface_stub_declaration.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/module.cc.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/module.h.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/module-internal.h.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/params_definition.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/struct_declaration.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/struct_definition.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/struct_macros.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/struct_serialization_declaration.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/struct_serialization_definition.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/wrapper_class_declaration.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/cpp_templates/wrapper_class_definition.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/java_templates/constant_definition.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/java_templates/constants.java.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/java_templates/enum_definition.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/java_templates/enum.java.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/java_templates/header.java.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/java_templates/java_macros.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/js_templates/enum_definition.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/js_templates/interface_definition.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/js_templates/module.js.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/js_templates/struct_definition.tmpl',
        '<(DEPTH)/mojo/public/tools/bindings/generators/mojom_cpp_generator.py',
        '<(DEPTH)/mojo/public/tools/bindings/generators/mojom_java_generator.py',
        '<(DEPTH)/mojo/public/tools/bindings/generators/mojom_js_generator.py',
        '<(DEPTH)/mojo/public/tools/bindings/pylib/mojom/__init__.py',
        '<(DEPTH)/mojo/public/tools/bindings/pylib/mojom/error.py',
        '<(DEPTH)/mojo/public/tools/bindings/pylib/mojom/generate/__init__.py',
        '<(DEPTH)/mojo/public/tools/bindings/pylib/mojom/generate/data.py',
        '<(DEPTH)/mojo/public/tools/bindings/pylib/mojom/generate/generator.py',
        '<(DEPTH)/mojo/public/tools/bindings/pylib/mojom/generate/module.py',
        '<(DEPTH)/mojo/public/tools/bindings/pylib/mojom/generate/pack.py',
        '<(DEPTH)/mojo/public/tools/bindings/pylib/mojom/generate/template_expander.py',
        '<(DEPTH)/mojo/public/tools/bindings/pylib/mojom/parse/__init__.py',
        '<(DEPTH)/mojo/public/tools/bindings/pylib/mojom/parse/ast.py',
        '<(DEPTH)/mojo/public/tools/bindings/pylib/mojom/parse/lexer.py',
        '<(DEPTH)/mojo/public/tools/bindings/pylib/mojom/parse/parser.py',
        '<(DEPTH)/mojo/public/tools/bindings/pylib/mojom/parse/translate.py',
      ],
      'outputs': [
        '<(SHARED_INTERMEDIATE_DIR)/<(mojom_base_output_dir)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).mojom.cc',
        '<(SHARED_INTERMEDIATE_DIR)/<(mojom_base_output_dir)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).mojom.h',
        '<(SHARED_INTERMEDIATE_DIR)/<(mojom_base_output_dir)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).mojom.js',
        '<(SHARED_INTERMEDIATE_DIR)/<(mojom_base_output_dir)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).mojom-internal.h',
      ],
      'action': [
        'python', '<@(mojom_bindings_generator)',
        './<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).mojom',
        '--use_chromium_bundled_pylibs',
        '-d', '<(DEPTH)',
        '-o', '<(SHARED_INTERMEDIATE_DIR)/<(mojom_base_output_dir)/<(RULE_INPUT_DIRNAME)',
        '--java_output_directory=<(java_out_dir)',
      ],
      'message': 'Generating Mojo bindings from <(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).mojom',
      'process_outputs_as_sources': 1,
    }
  ],
  'include_dirs': [
    '<(DEPTH)',
    '<(SHARED_INTERMEDIATE_DIR)',
  ],
  'direct_dependent_settings': {
    'include_dirs': [
      '<(DEPTH)',
      '<(SHARED_INTERMEDIATE_DIR)',
    ],
    'variables': {
      'generated_src_dirs': [
        '<(PRODUCT_DIR)/java_mojo/<(_target_name)/src',
      ],
    },
  },
  'hard_dependency': 1,
}
