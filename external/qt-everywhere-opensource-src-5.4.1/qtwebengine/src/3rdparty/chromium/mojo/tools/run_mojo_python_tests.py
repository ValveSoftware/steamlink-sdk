#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import os
import re
import sys
import unittest


def main():
  parser = optparse.OptionParser()
  parser.usage = 'run_mojo_python_tests.py [options] [tests...]'
  parser.add_option('-v', '--verbose', action='count', default=0)
  parser.add_option('--unexpected-failures', metavar='FILENAME', action='store',
                    help=('path to write a list of any tests that fail '
                          'unexpectedly.'))
  parser.epilog = ('If --unexpected-failures is passed, a list of the tests '
                   'that failed (one per line) will be written to the file. '
                   'If no tests failed, the file will be truncated (empty). '
                   'If the test run did not completely properly, or something '
                   'else weird happened, any existing file will be left '
                   'unmodified. '
                   'If --unexpected-failures is *not* passed, any existing '
                   'file will be ignored and left unmodified.')
  options, args = parser.parse_args()

  chromium_src_dir = os.path.join(os.path.dirname(__file__),
                                  os.pardir,
                                  os.pardir)

  loader = unittest.loader.TestLoader()
  print "Running Python unit tests under mojo/public/tools/bindings/pylib ..."

  pylib_dir = os.path.join(chromium_src_dir, 'mojo', 'public',
                           'tools', 'bindings', 'pylib')
  if args:
    if not pylib_dir in sys.path:
        sys.path.append(pylib_dir)
    suite = unittest.TestSuite()
    for test_name in args:
      suite.addTests(loader.loadTestsFromName(test_name))
  else:
    suite = loader.discover(pylib_dir, pattern='*_unittest.py')

  runner = unittest.runner.TextTestRunner(verbosity=(options.verbose + 1))
  result = runner.run(suite)

  if options.unexpected_failures:
    WriteUnexpectedFailures(result, options.unexpected_failures)

  return 0 if result.wasSuccessful() else 1


def WriteUnexpectedFailures(result, path):

  # This regex and UnitTestName() extracts the test_name in a way
  # that can be handed back to the loader successfully.

  test_description = re.compile("(\w+) \(([\w.]+)\)")

  def UnitTestName(test):
    m = test_description.match(str(test))
    return "%s.%s" % (m.group(2), m.group(1))

  with open(path, 'w') as fp:
    for (test, _) in result.failures + result.errors:
      fp.write(UnitTestName(test) + '\n')


if __name__ == '__main__':
  sys.exit(main())
