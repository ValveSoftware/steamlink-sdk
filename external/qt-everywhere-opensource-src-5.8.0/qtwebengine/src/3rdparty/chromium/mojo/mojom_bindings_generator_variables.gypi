# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'mojom_bindings_generator':
        '<(DEPTH)/mojo/public/tools/bindings/mojom_bindings_generator.py',
    'mojom_bindings_generator_sources': [
      '<(mojom_bindings_generator)',
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
    ]
  }
}
