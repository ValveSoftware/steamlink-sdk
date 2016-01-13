#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from layouttests import LayoutTests
from test_expectations import TestExpectations


class TestLayoutTests(unittest.TestCase):
  """Unit tests for the LayoutTests class."""

  def testJoinWithTestExpectation(self):
    layouttests = LayoutTests(parent_location_list=['media/'])
    test_expectations = TestExpectations()
    test_info_map = layouttests.JoinWithTestExpectation(test_expectations)
    # TODO(imasaki): have better assertion below. Currently, the test
    # expectation file is obtained dynamically so the strict assertion is
    # impossible.
    self.assertTrue(any(['media/' in k for k in test_info_map.keys()]),
                    msg=('Analyzer result should contain at least one '
                         'media test cases since we only retrieved media '
                         'related test'))

  def testGetTestDescriptionsFromSVN(self):
    desc1 = LayoutTests.GetTestDescriptionFromSVN(
        'media/video-play-empty-events.html')
    self.assertEquals(desc1,
                      ('Test that play() from EMPTY network state triggers '
                       'load() and async play event.'),
                      msg='Extracted test description is wrong')
    desc2 = LayoutTests.GetTestDescriptionFromSVN('jquery/data.html')
    self.assertEquals(desc2, 'UNKNOWN',
                      msg='Extracted test description is wrong')

  def testGetParentDirectoryList(self):
    testname_list = ['hoge1/hoge2/hoge3.html', 'hoge1/x.html',
                     'hoge1/hoge2/hoge4.html']
    expected_result = ['hoge1/', 'hoge1/hoge2/']
    self.assertEquals(LayoutTests.GetParentDirectoryList(testname_list),
                      expected_result)


if __name__ == '__main__':
  unittest.main()
