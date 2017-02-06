#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The frontend for the Mojo bindings system."""


import argparse
import imp
import json
import os
import pprint
import re
import sys

# Disable lint check for finding modules:
# pylint: disable=F0401

def _GetDirAbove(dirname):
  """Returns the directory "above" this file containing |dirname| (which must
  also be "above" this file)."""
  path = os.path.abspath(__file__)
  while True:
    path, tail = os.path.split(path)
    assert tail
    if tail == dirname:
      return path

# Manually check for the command-line flag. (This isn't quite right, since it
# ignores, e.g., "--", but it's close enough.)
if "--use_bundled_pylibs" in sys.argv[1:]:
  sys.path.insert(0, os.path.join(_GetDirAbove("mojo"), "third_party"))

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "pylib"))

from mojom.error import Error
import mojom.fileutil as fileutil
from mojom.generate.data import OrderedModuleFromData
from mojom.generate import template_expander
from mojom.parse.parser import Parse
from mojom.parse.translate import Translate


_BUILTIN_GENERATORS = {
  "c++": "mojom_cpp_generator.py",
  "javascript": "mojom_js_generator.py",
  "java": "mojom_java_generator.py",
}

def LoadGenerators(generators_string):
  if not generators_string:
    return []  # No generators.

  script_dir = os.path.dirname(os.path.abspath(__file__))
  generators = {}
  for generator_name in [s.strip() for s in generators_string.split(",")]:
    language = generator_name.lower()
    if language in _BUILTIN_GENERATORS:
      generator_name = os.path.join(script_dir, "generators",
                                    _BUILTIN_GENERATORS[language])
    else:
      print "Unknown generator name %s" % generator_name
      sys.exit(1)
    generator_module = imp.load_source(os.path.basename(generator_name)[:-3],
                                       generator_name)
    generators[language] = generator_module
  return generators


def MakeImportStackMessage(imported_filename_stack):
  """Make a (human-readable) message listing a chain of imports. (Returned
  string begins with a newline (if nonempty) and does not end with one.)"""
  return ''.join(
      reversed(["\n  %s was imported by %s" % (a, b) for (a, b) in \
                    zip(imported_filename_stack[1:], imported_filename_stack)]))


def FindImportFile(dir_name, file_name, search_dirs):
  for search_dir in [dir_name] + search_dirs:
    path = os.path.join(search_dir, file_name)
    if os.path.isfile(path):
      return path
  return os.path.join(dir_name, file_name)

class MojomProcessor(object):
  def __init__(self, should_generate):
    self._should_generate = should_generate
    self._processed_files = {}
    self._parsed_files = {}
    self._typemap = {}

  def LoadTypemaps(self, typemaps):
    # Support some very simple single-line comments in typemap JSON.
    comment_expr = r"^\s*//.*$"
    def no_comments(line):
      return not re.match(comment_expr, line)
    for filename in typemaps:
      with open(filename) as f:
        typemaps = json.loads("".join(filter(no_comments, f.readlines())))
        for language, typemap in typemaps.iteritems():
          language_map = self._typemap.get(language, {})
          language_map.update(typemap)
          self._typemap[language] = language_map

  def ProcessFile(self, args, remaining_args, generator_modules, filename):
    self._ParseFileAndImports(filename, args.import_directories, [])

    return self._GenerateModule(args, remaining_args, generator_modules,
        filename)

  def _GenerateModule(self, args, remaining_args, generator_modules, filename):
    # Return the already-generated module.
    if filename in self._processed_files:
      return self._processed_files[filename]
    tree = self._parsed_files[filename]

    dirname, name = os.path.split(filename)
    mojom = Translate(tree, name)
    if args.debug_print_intermediate:
      pprint.PrettyPrinter().pprint(mojom)

    # Process all our imports first and collect the module object for each.
    # We use these to generate proper type info.
    for import_data in mojom['imports']:
      import_filename = FindImportFile(dirname,
                                       import_data['filename'],
                                       args.import_directories)
      import_data['module'] = self._GenerateModule(
          args, remaining_args, generator_modules, import_filename)

    module = OrderedModuleFromData(mojom)

    # Set the path as relative to the source root.
    module.path = os.path.relpath(os.path.abspath(filename),
                                  os.path.abspath(args.depth))

    # Normalize to unix-style path here to keep the generators simpler.
    module.path = module.path.replace('\\', '/')

    if self._should_generate(filename):
      for language, generator_module in generator_modules.iteritems():
        generator = generator_module.Generator(
            module, args.output_dir, typemap=self._typemap.get(language, {}),
            variant=args.variant, bytecode_path=args.bytecode_path,
            for_blink=args.for_blink)
        filtered_args = []
        if hasattr(generator_module, 'GENERATOR_PREFIX'):
          prefix = '--' + generator_module.GENERATOR_PREFIX + '_'
          filtered_args = [arg for arg in remaining_args
                           if arg.startswith(prefix)]
        generator.GenerateFiles(filtered_args)

    # Save result.
    self._processed_files[filename] = module
    return module

  def _ParseFileAndImports(self, filename, import_directories,
      imported_filename_stack):
    # Ignore already-parsed files.
    if filename in self._parsed_files:
      return

    if filename in imported_filename_stack:
      print "%s: Error: Circular dependency" % filename + \
          MakeImportStackMessage(imported_filename_stack + [filename])
      sys.exit(1)

    try:
      with open(filename) as f:
        source = f.read()
    except IOError as e:
      print "%s: Error: %s" % (e.filename, e.strerror) + \
          MakeImportStackMessage(imported_filename_stack + [filename])
      sys.exit(1)

    try:
      tree = Parse(source, filename)
    except Error as e:
      full_stack = imported_filename_stack + [filename]
      print str(e) + MakeImportStackMessage(full_stack)
      sys.exit(1)

    dirname = os.path.split(filename)[0]
    for imp_entry in tree.import_list:
      import_filename = FindImportFile(dirname,
          imp_entry.import_filename, import_directories)
      self._ParseFileAndImports(import_filename, import_directories,
          imported_filename_stack + [filename])

    self._parsed_files[filename] = tree


def _Generate(args, remaining_args):
  if args.variant == "none":
    args.variant = None

  generator_modules = LoadGenerators(args.generators_string)

  fileutil.EnsureDirectoryExists(args.output_dir)

  processor = MojomProcessor(lambda filename: filename in args.filename)
  processor.LoadTypemaps(set(args.typemaps))
  for filename in args.filename:
    processor.ProcessFile(args, remaining_args, generator_modules, filename)

  return 0


def _Precompile(args, _):
  generator_modules = LoadGenerators(",".join(_BUILTIN_GENERATORS.keys()))

  template_expander.PrecompileTemplates(generator_modules, args.output_dir)
  return 0



def main():
  parser = argparse.ArgumentParser(
      description="Generate bindings from mojom files.")
  parser.add_argument("--use_bundled_pylibs", action="store_true",
                      help="use Python modules bundled in the SDK")

  subparsers = parser.add_subparsers()
  generate_parser = subparsers.add_parser(
      "generate", description="Generate bindings from mojom files.")
  generate_parser.add_argument("filename", nargs="+",
                               help="mojom input file")
  generate_parser.add_argument("-d", "--depth", dest="depth", default=".",
                               help="depth from source root")
  generate_parser.add_argument("-o", "--output_dir", dest="output_dir",
                               default=".",
                               help="output directory for generated files")
  generate_parser.add_argument("--debug_print_intermediate",
                               action="store_true",
                               help="print the intermediate representation")
  generate_parser.add_argument("-g", "--generators",
                               dest="generators_string",
                               metavar="GENERATORS",
                               default="c++,javascript,java",
                               help="comma-separated list of generators")
  generate_parser.add_argument(
      "-I", dest="import_directories", action="append", metavar="directory",
      default=[], help="add a directory to be searched for import files")
  generate_parser.add_argument("--typemap", action="append", metavar="TYPEMAP",
                               default=[], dest="typemaps",
                               help="apply TYPEMAP to generated output")
  generate_parser.add_argument("--variant", dest="variant", default=None,
                               help="output a named variant of the bindings")
  generate_parser.add_argument(
      "--bytecode_path", type=str, required=True, help=(
          "the path from which to load template bytecode; to generate template "
          "bytecode, run %s precompile BYTECODE_PATH" % os.path.basename(
              sys.argv[0])))
  generate_parser.add_argument("--for_blink", action="store_true",
                               help="Use WTF types as generated types for mojo "
                               "string/array/map.")
  generate_parser.set_defaults(func=_Generate)

  precompile_parser = subparsers.add_parser("precompile",
      description="Precompile templates for the mojom bindings generator.")
  precompile_parser.add_argument(
      "-o", "--output_dir", dest="output_dir", default=".",
      help="output directory for precompiled templates")
  precompile_parser.set_defaults(func=_Precompile)

  args, remaining_args = parser.parse_known_args()
  return args.func(args, remaining_args)


if __name__ == "__main__":
  sys.exit(main())
