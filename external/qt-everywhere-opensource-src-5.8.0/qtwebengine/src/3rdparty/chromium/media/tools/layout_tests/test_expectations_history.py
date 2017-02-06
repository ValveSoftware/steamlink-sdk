# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module for the history of the test expectation file."""

from datetime import datetime
from datetime import timedelta

import os
import re
import sys
import tempfile
import time
import pysvn

TEST_EXPECTATIONS_ROOT = 'http://src.chromium.org/blink/trunk/'
# A map from earliest revision to path.
# TODO(imasaki): support multiple test expectation files.
TEST_EXPECTATIONS_LOCATIONS = {
    148348: 'LayoutTests/TestExpectations',
    119317: 'LayoutTests/platform/chromium/TestExpectations',
    0: 'LayoutTests/platform/chromium/test_expectations.txt'}
TEST_EXPECTATIONS_DEFAULT_PATH = (
    TEST_EXPECTATIONS_ROOT + TEST_EXPECTATIONS_LOCATIONS[148348])

class TestExpectationsHistory(object):
  """A class to represent history of the test expectation file.

  The history is obtained by calling PySVN.log()/diff() APIs.

  TODO(imasaki): Add more functionalities here like getting some statistics
      about the test expectation file.
  """

  @staticmethod
  def GetTestExpectationsPathForRevision(revision):
    for i in sorted(TEST_EXPECTATIONS_LOCATIONS.keys(), reverse=True):
      if revision >= i:
        return TEST_EXPECTATIONS_ROOT + TEST_EXPECTATIONS_LOCATIONS[i]

  @staticmethod
  def GetDiffBetweenTimes(start, end, testname_list,
                          te_location=TEST_EXPECTATIONS_DEFAULT_PATH):
    """Get difference between time period for the specified test names.

    Given the time period, this method first gets the revision number. Then,
    it gets the diff for each revision. Finally, it keeps the diff relating to
    the test names and returns them along with other information about
    revision.

    Args:
      start: A timestamp specifying start of the time period to be
          looked at.
      end: A timestamp object specifying end of the time period to be
          looked at.
      testname_list: A list of strings representing test names of interest.
      te_location: A location of the test expectation file.

    Returns:
      A list of tuples (old_rev, new_rev, author, date, message, lines). The
          |lines| contains the diff of the tests of interest.
    """
    temp_directory = tempfile.mkdtemp()
    test_expectations_path = os.path.join(temp_directory, 'TestExpectations')
    # Get directory name which is necesary to call PySVN.checkout().
    te_location_dir = te_location[0:te_location.rindex('/')]
    client = pysvn.Client()
    client.checkout(te_location_dir, temp_directory, recurse=False)
    # PySVN.log() (http://pysvn.tigris.org/docs/pysvn_prog_ref.html
    # #pysvn_client_log) returns the log messages (including revision
    # number in chronological order).
    logs = client.log(test_expectations_path,
                      revision_start=pysvn.Revision(
                          pysvn.opt_revision_kind.date, start),
                      revision_end=pysvn.Revision(
                          pysvn.opt_revision_kind.date, end))
    result_list = []
    gobackdays = 1
    while gobackdays < sys.maxint:
      goback_start = time.mktime(
          (datetime.fromtimestamp(start) - (
              timedelta(days=gobackdays))).timetuple())
      logs_before_time_period = (
          client.log(test_expectations_path,
                     revision_start=pysvn.Revision(
                         pysvn.opt_revision_kind.date, goback_start),
                     revision_end=pysvn.Revision(
                         pysvn.opt_revision_kind.date, start)))
      if logs_before_time_period:
        # Prepend at the beginning of logs.
        logs.insert(0, logs_before_time_period[len(logs_before_time_period)-1])
        break
      gobackdays *= 2

    for i in xrange(len(logs) - 1):
      old_rev = logs[i].revision.number
      new_rev = logs[i + 1].revision.number
      # Parsing the actual diff.

      new_path = TestExpectationsHistory.GetTestExpectationsPathForRevision(
          new_rev);
      old_path = TestExpectationsHistory.GetTestExpectationsPathForRevision(
          old_rev);

      text = client.diff(temp_directory,
                         url_or_path=old_path,
                         revision1=pysvn.Revision(
                             pysvn.opt_revision_kind.number, old_rev),
                         url_or_path2=new_path,
                         revision2=pysvn.Revision(
                             pysvn.opt_revision_kind.number, new_rev))
      lines = text.split('\n')
      target_lines = []
      for line in lines:
        for testname in testname_list:
          matches = re.findall(testname, line)
          if matches:
            if line[0] == '+' or line[0] == '-':
              target_lines.append(line)
      if target_lines:
        # Needs to convert to normal date string for presentation.
        result_list.append((
            old_rev, new_rev, logs[i + 1].author,
            datetime.fromtimestamp(
                logs[i + 1].date).strftime('%Y-%m-%d %H:%M:%S'),
            logs[i + 1].message, target_lines))
    return result_list
