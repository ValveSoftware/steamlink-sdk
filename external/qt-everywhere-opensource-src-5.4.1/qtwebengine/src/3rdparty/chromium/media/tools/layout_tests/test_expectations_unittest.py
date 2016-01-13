#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from test_expectations import TestExpectations


class TestTestExpectations(unittest.TestCase):

  def testParseLine(self):
    line = ('crbug.com/86714 [ Mac Gpu ] media/video-zoom.html [ Crash '
            'ImageOnlyFailure ]')
    expected_map = {'CRASH': True, 'IMAGE': True, 'Bugs': ['BUGCR86714'],
                    'Comments': '', 'MAC': True, 'Gpu': True,
                    'Platforms': ['MAC', 'Gpu']}
    self.assertEquals(TestExpectations.ParseLine(line),
                      ('media/video-zoom.html', expected_map))

  def testParseLineWithLineComments(self):
    line = ('crbug.com/86714 [ Mac Gpu ] media/video-zoom.html [ Crash '
            'ImageOnlyFailure ] # foo')
    expected_map = {'CRASH': True, 'IMAGE': True, 'Bugs': ['BUGCR86714'],
                    'Comments': ' foo', 'MAC': True, 'Gpu': True,
                    'Platforms': ['MAC', 'Gpu']}
    self.assertEquals(TestExpectations.ParseLine(line),
                      ('media/video-zoom.html', expected_map))

  def testParseLineWithLineGPUComments(self):
    line = ('crbug.com/86714 [ Mac ] media/video-zoom.html [ Crash '
            'ImageOnlyFailure ] # Gpu')
    expected_map = {'CRASH': True, 'IMAGE': True, 'Bugs': ['BUGCR86714'],
                    'Comments': ' Gpu', 'MAC': True,
                    'Platforms': ['MAC']}
    self.assertEquals(TestExpectations.ParseLine(line),
                      ('media/video-zoom.html', expected_map))


if __name__ == '__main__':
  unittest.main()
