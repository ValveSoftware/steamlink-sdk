#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The frontend for the Mojo bindings system."""


import argparse
import imp
import os
import pprint
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
if "--use_chromium_bundled_pylibs" in sys.argv[1:]:
  sys.path.insert(0, os.path.join(_GetDirAbove("mojo"), "third_party"))

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "pylib"))

from mojom.error import Error
from mojom.generate.data import OrderedModuleFromData
from mojom.parse.parser import Parse
from mojom.parse.translate import Translate


def LoadGenerators(generators_string):
  if not generators_string:
    return []  # No generators.

  script_dir = os.path.dirname(os.path.abspath(__file__))
  generators = []
  for generator_name in [s.strip() for s in generators_string.split(",")]:
    # "Built-in" generators:
    if generator_name.lower() == "c++":
      generator_name = os.path.join(script_dir, "generators",
                                    "mojom_cpp_generator.py")
    elif generator_name.lower() == "javascript":
      generator_name = os.path.join(script_dir, "generators",
                                    "mojom_js_generator.py")
    elif generator_name.lower() == "java":
      generator_name = os.path.join(script_dir, "generators",
                                    "mojom_java_generator.py")
    # Specified generator python module:
    elif generator_name.endswith(".py"):
      pass
    else:
      print "Unknown generator name %s" % generator_name
      sys.exit(1)
    generator_module = imp.load_source(os.path.basename(generator_name)[:-3],
                                       generator_name)
    generators.append(generator_module)
  return generators


def MakeImportStackMessage(imported_filename_stack):
  """Make a (human-readable) message listing a chain of imports. (Returned
  string begins with a newline (if nonempty) and does not end with one.)"""
  return ''.join(
      reversed(["\n  %s was imported by %s" % (a, b) for (a, b) in \
                    zip(imported_filename_stack[1:], imported_filename_stack)]))


# Disable check for dangerous default arguments (they're "private" keyword
# arguments; note that we want |_processed_files| to memoize across invocations
# of |ProcessFile()|):
# pylint: disable=W0102
def ProcessFile(args, remaining_args, generator_modules, filename,
                _processed_files={}, _imported_filename_stack=None):
  # Memoized results.
  if filename in _processed_files:
    return _processed_files[filename]

  if _imported_filename_stack is None:
    _imported_filename_stack = []

  # Ensure we only visit each file once.
  if filename in _imported_filename_stack:
    print "%s: Error: Circular dependency" % filename + \
        MakeImportStackMessage(_imported_filename_stack + [filename])
    sys.exit(1)

  try:
    with open(filename) as f:
      source = f.read()
  except IOError as e:
    print "%s: Error: %s" % (e.filename, e.strerror) + \
        MakeImportStackMessage(_imported_filename_stack + [filename])
    sys.exit(1)

  try:
    tree = Parse(source, filename)
  except Error as e:
    print str(e) + MakeImportStackMessage(_imported_filename_stack + [filename])
    sys.exit(1)

  dirname, name = os.path.split(filename)
  mojom = Translate(tree, name)
  if args.debug_print_intermediate:
    pprint.PrettyPrinter().pprint(mojom)

  # Process all our imports first and collect the module object for each.
  # We use these to generate proper type info.
  for import_data in mojom['imports']:
    import_filename = os.path.join(dirname, import_data['filename'])
    import_data['module'] = ProcessFile(
        args, remaining_args, generator_modules, import_filename,
        _processed_files=_processed_files,
        _imported_filename_stack=_imported_filename_stack + [filename])

  module = OrderedModuleFromData(mojom)

  # Set the path as relative to the source root.
  module.path = os.path.relpath(os.path.abspath(filename),
                                os.path.abspath(args.depth))

  # Normalize to unix-style path here to keep the generators simpler.
  module.path = module.path.replace('\\', '/')

  for generator_module in generator_modules:
    generator = generator_module.Generator(module, args.output_dir)
    filtered_args = []
    if hasattr(generator_module, 'GENERATOR_PREFIX'):
      prefix = '--' + generator_module.GENERATOR_PREFIX + '_'
      filtered_args = [arg for arg in remaining_args if arg.startswith(prefix)]
    generator.GenerateFiles(filtered_args)

  # Save result.
  _processed_files[filename] = module
  return module
# pylint: enable=W0102


def main():
  parser = argparse.ArgumentParser(
      description="Generate bindings from mojom files.")
  parser.add_argument("filename", nargs="+",
                      help="mojom input file")
  parser.add_argument("-d", "--depth", dest="depth", default=".",
                      help="depth from source root")
  parser.add_argument("-o", "--output_dir", dest="output_dir", default=".",
                      help="output directory for generated files")
  parser.add_argument("-g", "--generators", dest="generators_string",
                      metavar="GENERATORS", default="c++,javascript,java",
                      help="comma-separated list of generators")
  parser.add_argument("--debug_print_intermediate", action="store_true",
                      help="print the intermediate representation")
  parser.add_argument("--use_chromium_bundled_pylibs", action="store_true",
                      help="use Python modules bundled in the Chromium source")
  (args, remaining_args) = parser.parse_known_args()

  generator_modules = LoadGenerators(args.generators_string)

  if not os.path.exists(args.output_dir):
    os.makedirs(args.output_dir)

  for filename in args.filename:
    ProcessFile(args, remaining_args, generator_modules, filename)

  return 0


if __name__ == "__main__":
  sys.exit(main())
