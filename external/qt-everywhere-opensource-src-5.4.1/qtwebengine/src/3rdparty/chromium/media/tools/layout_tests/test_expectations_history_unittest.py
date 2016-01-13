#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from datetime import datetime

import calendar
import unittest


from test_expectations_history import TestExpectationsHistory


class TestTestExpectationsHistory(unittest.TestCase):
  """Unit tests for the TestExpectationsHistory class."""

  def AssertTestName(self, result_list, testname):
    """Assert test name in the result_list.

    Args:
      result_list: a result list of tuples returned by
          |GetDiffBetweenTimesOnly1Diff()|. Each tuple consists of
          (old_rev, new_rev, author, date, message, lines) where
          |lines| are the entries in the test expectation file.
      testname: a testname string.

    Returns:
      True if the result contains the testname, False otherwise.
    """
    for (_, _, _, _, _, lines) in result_list:
      if any([testname in line for line in lines]):
        return True
    return False

  # These tests use the following commit.
  # commit 235788e3a4fc71342a5c9fefe67ce9537706ce35
  # Author: rniwa@webkit.org
  # Date:   Sat Aug 20 06:19:11 2011 +0000

  def testGetDiffBetweenTimes(self):
    ptime = calendar.timegm((2011, 8, 20, 0, 0, 0, 0, 0, 0))
    ctime = calendar.timegm((2011, 8, 21, 0, 0, 0, 0, 0, 0))
    testname = 'fast/css/getComputedStyle/computed-style-without-renderer.html'
    testname_list = [testname]
    result_list = TestExpectationsHistory.GetDiffBetweenTimes(
        ptime, ctime, testname_list)
    self.assertTrue(self.AssertTestName(result_list, testname))

  def testGetDiffBetweenTimesOnly1Diff(self):
    ptime = calendar.timegm((2011, 8, 20, 6, 0, 0, 0, 0, 0))
    ctime = calendar.timegm((2011, 8, 20, 7, 0, 0, 0, 0, 0))
    testname = 'fast/css/getComputedStyle/computed-style-without-renderer.html'
    testname_list = [testname]
    result_list = TestExpectationsHistory.GetDiffBetweenTimes(
        ptime, ctime, testname_list)
    self.assertTrue(self.AssertTestName(result_list, testname))

  def testGetDiffBetweenTimesOnly1DiffWithGobackSeveralDays(self):
    ptime = calendar.timegm((2011, 9, 12, 1, 0, 0, 0, 0, 0))
    ctime = calendar.timegm((2011, 9, 12, 2, 0, 0, 0, 0, 0))
    testname = 'media/video-zoom-controls.html'
    testname_list = [testname]
    result_list = TestExpectationsHistory.GetDiffBetweenTimes(
        ptime, ctime, testname_list)
    self.assertTrue(self.AssertTestName(result_list, testname))


if __name__ == '__main__':
  unittest.main()
