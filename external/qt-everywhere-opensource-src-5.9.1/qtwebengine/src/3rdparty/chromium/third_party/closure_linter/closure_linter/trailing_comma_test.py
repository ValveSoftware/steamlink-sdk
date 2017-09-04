#!/usr/bin/env python
# Copyright 2013 The Closure Linter Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS-IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
Tests for trailing commas (ES3) errors

"""


import gflags as flags
import unittest as googletest

from closure_linter import errors
from closure_linter import runner
from closure_linter.common import erroraccumulator

flags.FLAGS.check_trailing_comma = True
class TrailingCommaTest(googletest.TestCase):
  """Test case to for gjslint errorrules."""

  def testGetTrailingCommaArray(self):
    """ warning for trailing commas before closing array
    """
    original = ['q = [1,]', ]

    # Expect line too long.
    expected = errors.COMMA_AT_END_OF_LITERAL

    self._AssertInError(original, expected)

  def testGetTrailingCommaDict(self):
    """ warning for trailing commas before closing array
    """
    original = ['q = {1:1,}', ]

    # Expect line too long.
    expected = errors.COMMA_AT_END_OF_LITERAL

    self._AssertInError(original, expected)

  def _AssertInError(self, original, expected):
    """Asserts that the error fixer corrects original to expected."""

    # Trap gjslint's output parse it to get messages added.
    error_accumulator = erroraccumulator.ErrorAccumulator()
    runner.Run('testing.js', error_accumulator, source=original)
    error_nums = [e.code for e in error_accumulator.GetErrors()]

    self.assertIn(expected, error_nums)

if __name__ == '__main__':
  googletest.main()
