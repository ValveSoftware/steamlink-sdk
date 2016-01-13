#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for the 'grit build' tool.
'''

import os
import sys
import tempfile
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

import unittest

from grit import util
from grit.tool import build


class BuildUnittest(unittest.TestCase):

  def testFindTranslationsWithSubstitutions(self):
    # This is a regression test; we had a bug where GRIT would fail to find
    # messages with substitutions e.g. "Hello [IDS_USER]" where IDS_USER is
    # another <message>.
    output_dir = tempfile.mkdtemp()
    builder = build.RcBuilder()
    class DummyOpts(object):
      def __init__(self):
        self.input = util.PathFromRoot('grit/testdata/substitute.grd')
        self.verbose = False
        self.extra_verbose = False
    builder.Run(DummyOpts(), ['-o', output_dir])

  def testGenerateDepFile(self):
    output_dir = tempfile.mkdtemp()
    builder = build.RcBuilder()
    class DummyOpts(object):
      def __init__(self):
        self.input = util.PathFromRoot('grit/testdata/substitute.grd')
        self.verbose = False
        self.extra_verbose = False
    expected_dep_file = os.path.join(output_dir, 'substitute.grd.d')
    builder.Run(DummyOpts(), ['-o', output_dir,
                              '--depdir', output_dir,
                              '--depfile', expected_dep_file])

    self.failUnless(os.path.isfile(expected_dep_file))
    with open(expected_dep_file) as f:
      line = f.readline()
      (dep_file_name, deps_string) = line.split(': ')
      deps = deps_string.split(' ')
      self.failUnlessEqual(os.path.abspath(expected_dep_file),
          os.path.abspath(os.path.join(output_dir, dep_file_name)),
          "depfile should refer to itself as the depended upon file")
      self.failUnlessEqual(1, len(deps))
      self.failUnlessEqual(deps[0],
          util.PathFromRoot('grit/testdata/substitute.xmb'))


if __name__ == '__main__':
  unittest.main()
