#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Counts the number of #if or #ifdef lines containing at least one
preprocessor token that is a full match for the given pattern, in the
given directory.
"""


import optparse
import os
import re
import sys


# Filename extensions we know will be handled by the C preprocessor.
# We ignore files not matching these.
CPP_EXTENSIONS = [
  '.h',
  '.cc',
  '.m',
  '.mm',
]


def _IsTestFile(filename):
  """Does a rudimentary check to try to skip test files; this could be
  improved but is good enough for basic metrics generation.
  """
  return re.match('(test|mock|dummy)_.*|.*_[a-z]*test\.(h|cc|mm)', filename)


def CountIfdefs(token_pattern, directory, skip_tests=False):
  """Returns the number of lines in files in |directory| and its
  subdirectories that have an extension from |CPP_EXTENSIONS| and are
  an #if or #ifdef line with a preprocessor token fully matching
  the string |token_pattern|.

  If |skip_tests| is true, a best effort is made to ignore test files.
  """
  token_line_re = re.compile(r'^#if(def)?.*\b(%s)\b.*$' % token_pattern)
  count = 0
  for root, dirs, files in os.walk(directory):
    for filename in files:
      if os.path.splitext(filename)[1] in CPP_EXTENSIONS:
        if not skip_tests or not _IsTestFile(filename):
          with open(os.path.join(root, filename)) as f:
            for line in f:
              line = line.strip()
              if token_line_re.match(line):
                count += 1
  return count


def PrintUsage():
  print "Usage: %s [--skip-tests] TOKEN_PATTERN DIRECTORY" % sys.argv[0]


def main():
  option_parser = optparse.OptionParser()
  option_parser.add_option('', '--skip-tests', action='store_true',
                           dest='skip_tests', default=False,
                           help='Skip test files.')
  options, args = option_parser.parse_args()

  if len(args) < 2:
    PrintUsage()
    return 1
  else:
    print CountIfdefs(args[0], args[1], options.skip_tests)
    return 0


if __name__ == '__main__':
  sys.exit(main())
