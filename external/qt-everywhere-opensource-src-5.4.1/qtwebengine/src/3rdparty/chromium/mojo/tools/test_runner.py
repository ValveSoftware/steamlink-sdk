#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A "smart" test runner for gtest unit tests (that caches successes)."""

import logging
import os
import subprocess
import sys

_logging = logging.getLogger()

_script_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(_script_dir, "pylib"))

from transitive_hash import transitive_hash

def main(argv):
  logging.basicConfig()
  # Uncomment to debug:
  # _logging.setLevel(logging.DEBUG)

  if len(argv) < 3 or len(argv) > 4:
    print "Usage: %s gtest_list_file root_dir [successes_cache_file]" % \
        os.path.basename(argv[0])
    return 0 if len(argv) < 2 else 1

  _logging.debug("Test list file: %s", argv[1])
  with open(argv[1], 'rb') as f:
    gtest_list = [y for y in [x.strip() for x in f.readlines()] \
                      if y and y[0] != '#']
  _logging.debug("Test list: %s" % gtest_list)

  print "Running tests in directory: %s" % argv[2]
  os.chdir(argv[2])

  if len(argv) == 4 and argv[3]:
    successes_cache_filename = argv[3]
    print "Successes cache file: %s" % successes_cache_filename
  else:
    successes_cache_filename = None
    print "No successes cache file (will run all tests unconditionally)"

  if successes_cache_filename:
    # This file simply contains a list of transitive hashes of tests that
    # succeeded.
    try:
      _logging.debug("Trying to read successes cache file: %s",
                     successes_cache_filename)
      with open(argv[3], 'rb') as f:
        successes = set([x.strip() for x in f.readlines()])
      _logging.debug("Successes: %s", successes)
    except:
      # Just assume that it didn't exist, or whatever.
      print "Failed to read successes cache file %s (will create)" % argv[3]
      successes = set()

  # Run gtests with color if we're on a TTY (and we're not being told explicitly
  # what to do).
  if sys.stdout.isatty() and 'GTEST_COLOR' not in os.environ:
    _logging.debug("Setting GTEST_COLOR=yes")
    os.environ['GTEST_COLOR'] = 'yes'

  # TODO(vtl): We may not close this file on failure.
  successes_cache_file = open(successes_cache_filename, 'ab') \
      if successes_cache_filename else None
  for gtest in gtest_list:
    if gtest[0] == '*':
      gtest = gtest[1:]
      _logging.debug("%s is marked as non-cacheable" % gtest)
      cacheable = False
    else:
      cacheable = True

    if successes_cache_file and cacheable:
      _logging.debug("Getting transitive hash for %s ... " % gtest)
      try:
        gtest_hash = transitive_hash(gtest)
      except:
        print "Failed to get transitive hash for %s" % gtest
        return 1
      _logging.debug("  Transitive hash: %s" % gtest_hash)

      if gtest_hash in successes:
        print "Skipping %s (previously succeeded)" % gtest
        continue

    print "Running %s...." % gtest,
    sys.stdout.flush()
    try:
      subprocess.check_output(["./" + gtest], stderr=subprocess.STDOUT)
      print "Succeeded"
      # Record success.
      if successes_cache_filename and cacheable:
        successes.add(gtest_hash)
        successes_cache_file.write(gtest_hash + '\n')
        successes_cache_file.flush()
    except subprocess.CalledProcessError as e:
      print "Failed with exit code %d and output:" % e.returncode
      print 72 * '-'
      print e.output
      print 72 * '-'
      return 1
    except OSError as e:
      print "  Failed to start test"
      return 1
  print "All tests succeeded"
  if successes_cache_file:
    successes_cache_file.close()

  return 0

if __name__ == '__main__':
  sys.exit(main(sys.argv))
