#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generator for C++ structs from api json files.

The purpose of this tool is to remove the need for hand-written code that
converts to and from base::Value types when receiving javascript api calls.
Originally written for generating code for extension apis. Reference schemas
are in chrome/common/extensions/api.

Usage example:
  compiler.py --root /home/Work/src --namespace extensions windows.json
    tabs.json
  compiler.py --destdir gen --root /home/Work/src
    --namespace extensions windows.json tabs.json
"""

import optparse
import os
import sys

from cpp_bundle_generator import CppBundleGenerator
from cpp_generator import CppGenerator
from cpp_type_generator import CppTypeGenerator
from dart_generator import DartGenerator
import json_schema
from model import Model
from ppapi_generator import PpapiGenerator
from schema_loader import SchemaLoader

# Names of supported code generators, as specified on the command-line.
# First is default.
GENERATORS = ['cpp', 'cpp-bundle', 'dart', 'ppapi']

def GenerateSchema(generator,
                   filenames,
                   root,
                   destdir,
                   root_namespace,
                   dart_overrides_dir,
                   impl_dir):
  # Merge the source files into a single list of schemas.
  api_defs = []
  for filename in filenames:
    schema = os.path.normpath(filename)
    schema_loader = SchemaLoader(
        os.path.dirname(os.path.relpath(os.path.normpath(filename), root)),
        os.path.dirname(filename))
    api_def = schema_loader.LoadSchema(os.path.split(schema)[1])

    # If compiling the C++ model code, delete 'nocompile' nodes.
    if generator == 'cpp':
      api_def = json_schema.DeleteNodes(api_def, 'nocompile')
    api_defs.extend(api_def)

  api_model = Model()

  # For single-schema compilation make sure that the first (i.e. only) schema
  # is the default one.
  default_namespace = None

  # If we have files from multiple source paths, we'll use the common parent
  # path as the source directory.
  src_path = None

  # Load the actual namespaces into the model.
  for target_namespace, schema_filename in zip(api_defs, filenames):
    relpath = os.path.relpath(os.path.normpath(schema_filename), root)
    namespace = api_model.AddNamespace(target_namespace,
                                       relpath,
                                       include_compiler_options=True)

    if default_namespace is None:
      default_namespace = namespace

    if src_path is None:
      src_path = namespace.source_file_dir
    else:
      src_path = os.path.commonprefix((src_path, namespace.source_file_dir))

    path, filename = os.path.split(schema_filename)
    short_filename, extension = os.path.splitext(filename)

  # Construct the type generator with all the namespaces in this model.
  type_generator = CppTypeGenerator(api_model,
                                    schema_loader,
                                    default_namespace=default_namespace)
  if generator == 'cpp-bundle':
    cpp_bundle_generator = CppBundleGenerator(root,
                                              api_model,
                                              api_defs,
                                              type_generator,
                                              root_namespace,
                                              src_path,
                                              impl_dir)
    generators = [
      ('generated_api.cc', cpp_bundle_generator.api_cc_generator),
      ('generated_api.h', cpp_bundle_generator.api_h_generator),
      ('generated_schemas.cc', cpp_bundle_generator.schemas_cc_generator),
      ('generated_schemas.h', cpp_bundle_generator.schemas_h_generator)
    ]
  elif generator == 'cpp':
    cpp_generator = CppGenerator(type_generator, root_namespace)
    generators = [
      ('%s.h' % short_filename, cpp_generator.h_generator),
      ('%s.cc' % short_filename, cpp_generator.cc_generator)
    ]
  elif generator == 'dart':
    generators = [
      ('%s.dart' % namespace.unix_name, DartGenerator(
          dart_overrides_dir))
    ]
  elif generator == 'ppapi':
    generator = PpapiGenerator()
    generators = [
      (os.path.join('api', 'ppb_%s.idl' % namespace.unix_name),
       generator.idl_generator),
    ]
  else:
    raise Exception('Unrecognised generator %s' % generator)

  output_code = []
  for filename, generator in generators:
    code = generator.Generate(namespace).Render()
    if destdir:
      output_dir = os.path.join(destdir, src_path)
      if not os.path.exists(output_dir):
        os.makedirs(output_dir)
      with open(os.path.join(output_dir, filename), 'w') as f:
        f.write(code)
    output_code += [filename, '', code, '']

  return '\n'.join(output_code)


if __name__ == '__main__':
  parser = optparse.OptionParser(
      description='Generates a C++ model of an API from JSON schema',
      usage='usage: %prog [option]... schema')
  parser.add_option('-r', '--root', default='.',
      help='logical include root directory. Path to schema files from specified'
      ' dir will be the include path.')
  parser.add_option('-d', '--destdir',
      help='root directory to output generated files.')
  parser.add_option('-n', '--namespace', default='generated_api_schemas',
      help='C++ namespace for generated files. e.g extensions::api.')
  parser.add_option('-g', '--generator', default=GENERATORS[0],
      choices=GENERATORS,
      help='The generator to use to build the output code. Supported values are'
      ' %s' % GENERATORS)
  parser.add_option('-D', '--dart-overrides-dir', dest='dart_overrides_dir',
      help='Adds custom dart from files in the given directory (Dart only).')
  parser.add_option('-i', '--impl-dir', dest='impl_dir',
      help='The root path of all API implementations')

  (opts, filenames) = parser.parse_args()

  if not filenames:
    sys.exit(0) # This is OK as a no-op

  # Unless in bundle mode, only one file should be specified.
  if opts.generator != 'cpp-bundle' and len(filenames) > 1:
    # TODO(sashab): Could also just use filenames[0] here and not complain.
    raise Exception(
        "Unless in bundle mode, only one file can be specified at a time.")

  result = GenerateSchema(opts.generator, filenames, opts.root, opts.destdir,
                          opts.namespace, opts.dart_overrides_dir,
                          opts.impl_dir)
  if not opts.destdir:
    print result
