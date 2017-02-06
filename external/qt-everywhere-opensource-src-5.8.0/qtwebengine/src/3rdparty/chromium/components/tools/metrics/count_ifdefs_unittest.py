#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for count_ifdefs.
"""

import os
import unittest

import count_ifdefs


class CountIfdefsTest(unittest.TestCase):

  def setUp(self):
    self.root = os.path.join(os.path.dirname(__file__), 'testdata')

  def testNormal(self):
    count = count_ifdefs.CountIfdefs('OS_[A-Z]+', self.root)
    self.failUnless(count == 6)

  def testSkipTests(self):
    count = count_ifdefs.CountIfdefs('OS_[A-Z]+', self.root, True)
    self.failUnless(count == 4)


if __name__ == '__main__':
  unittest.main()
