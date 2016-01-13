# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Layout tests module that is necessary for the layout analyzer.

Layout tests are stored in an SVN repository and LayoutTestCaseManager collects
these layout test cases (including description).
"""

import copy
import csv
import locale
import re
import sys
import urllib2

import pysvn

# LayoutTests SVN root location.
DEFAULT_LAYOUTTEST_LOCATION = (
    'http://src.chromium.org/blink/trunk/LayoutTests/')
# LayoutTests SVN view link
DEFAULT_LAYOUTTEST_SVN_VIEW_LOCATION = (
    'http://src.chromium.org/viewvc/blink/trunk/LayoutTests/')


# When parsing the test HTML file and finding the test description,
# this script tries to find the test description using sentences
# starting with these keywords. This is adhoc but it is the only way
# since there is no standard for writing test description.
KEYWORDS_FOR_TEST_DESCRIPTION = ['This test', 'Tests that', 'Test ']

# If cannot find the keywords, this script tries to find test case
# description by the following tags.
TAGS_FOR_TEST_DESCRIPTION = ['title', 'p', 'div']

# If cannot find the tags, this script tries to find the test case
# description in the sentence containing following words.
KEYWORD_FOR_TEST_DESCRIPTION_FAIL_SAFE = ['PASSED ', 'PASS:']


class LayoutTests(object):
  """A class to store test names in layout tests.

  The test names (including regular expression patterns) are read from a CSV
  file and used for getting layout test names from repository.
  """

  def __init__(self, layouttest_root_path=DEFAULT_LAYOUTTEST_LOCATION,
               parent_location_list=None, filter_names=None,
               recursion=False):
    """Initialize LayoutTests using root and CSV file.

    Args:
      layouttest_root_path: A location string where layout tests are stored.
      parent_location_list: A list of parent directories that are needed for
          getting layout tests.
      filter_names: A list of test name patterns that are used for filtering
          test names (e.g., media/*.html).
      recursion: a boolean indicating whether the test names are sought
          recursively.
    """

    if layouttest_root_path.startswith('http://'):
      name_map = self.GetLayoutTestNamesFromSVN(parent_location_list,
                                                layouttest_root_path,
                                                recursion)
    else:
      # TODO(imasaki): support other forms such as CSV for reading test names.
      pass
    self.name_map = copy.copy(name_map)
    if filter_names:
      # Filter names.
      for lt_name in name_map.iterkeys():
        match = False
        for filter_name in filter_names:
          if re.search(filter_name, lt_name):
            match = True
            break
        if not match:
          del self.name_map[lt_name]
    # We get description only for the filtered names.
    for lt_name in self.name_map.iterkeys():
      self.name_map[lt_name] = 'No description available'

  @staticmethod
  def ExtractTestDescription(txt):
    """Extract the description description from test code in HTML.

    Currently, we have 4 rules described in the code below.
    (This example falls into rule 1):
      <p>
      This tests the intrinsic size of a video element is the default
      300,150 before metadata is loaded, and 0,0 after
      metadata is loaded for an audio-only file.
      </p>
    The strategy is very adhoc since the original test case files
    (in HTML format) do not have standard way to store test description.

    Args:
      txt: A HTML text which may or may not contain test description.

    Returns:
      A string that contains test description. Returns 'UNKNOWN' if the
          test description is not found.
    """
    # (1) Try to find test description that contains keywords such as
    #     'test that' and surrounded by p tag.
    #     This is the most common case.
    for keyword in KEYWORDS_FOR_TEST_DESCRIPTION:
      # Try to find <p> and </p>.
      pattern = r'<p>(.*' + keyword + '.*)</p>'
      matches = re.search(pattern, txt)
      if matches is not None:
        return matches.group(1).strip()

    # (2) Try to find it by using more generic keywords such as 'PASS' etc.
    for keyword in KEYWORD_FOR_TEST_DESCRIPTION_FAIL_SAFE:
      # Try to find new lines.
      pattern = r'\n(.*' + keyword + '.*)\n'
      matches = re.search(pattern, txt)
      if matches is not None:
        # Remove 'p' tag.
        text = matches.group(1).strip()
        return text.replace('<p>', '').replace('</p>', '')

    # (3) Try to find it by using HTML tag such as title.
    for tag in TAGS_FOR_TEST_DESCRIPTION:
      pattern = r'<' + tag + '>(.*)</' + tag + '>'
      matches = re.search(pattern, txt)
      if matches is not None:
        return matches.group(1).strip()

    # (4) Try to find it by using test description and remove 'p' tag.
    for keyword in KEYWORDS_FOR_TEST_DESCRIPTION:
      # Try to find <p> and </p>.
      pattern = r'\n(.*' + keyword + '.*)\n'
      matches = re.search(pattern, txt)
      if matches is not None:
        # Remove 'p' tag.
        text = matches.group(1).strip()
        return text.replace('<p>', '').replace('</p>', '')

    # (5) cannot find test description using existing rules.
    return 'UNKNOWN'

  @staticmethod
  def GetLayoutTestNamesFromSVN(parent_location_list,
                                layouttest_root_path, recursion):
    """Get LayoutTest names from SVN.

    Args:
      parent_location_list: a list of locations of parent directories. This is
          used when getting layout tests using PySVN.list().
      layouttest_root_path: the root path of layout tests directory.
      recursion: a boolean indicating whether the test names are sought
          recursively.

    Returns:
      a map containing test names as keys for de-dupe.
    """
    client = pysvn.Client()
    # Get directory structure in the repository SVN.
    name_map = {}
    for parent_location in parent_location_list:
      if parent_location.endswith('/'):
        full_path = layouttest_root_path + parent_location
        try:
          file_list = client.list(full_path, recurse=recursion)
          for file_name in file_list:
            if sys.stdout.isatty():
              default_encoding = sys.stdout.encoding
            else:
              default_encoding = locale.getpreferredencoding()
            file_name = file_name[0].repos_path.encode(default_encoding)
            # Remove the word '/truck/LayoutTests'.
            file_name = file_name.replace('/trunk/LayoutTests/', '')
            if file_name.endswith('.html'):
              name_map[file_name] = True
        except:
          print 'Unable to list tests in %s.' % full_path
    return name_map

  @staticmethod
  def GetLayoutTestNamesFromCSV(csv_file_path):
    """Get layout test names from CSV file.

    Args:
      csv_file_path: the path for the CSV file containing test names (including
          regular expression patterns). The CSV file content has one column and
          each row contains a test name.

    Returns:
       a list of test names in string.
    """
    file_object = file(csv_file_path, 'r')
    reader = csv.reader(file_object)
    names = [row[0] for row in reader]
    file_object.close()
    return names

  @staticmethod
  def GetParentDirectoryList(names):
    """Get parent directory list from test names.

    Args:
      names: a list of test names. The test names also have path information as
          well (e.g., media/video-zoom.html).

    Returns:
      a list of parent directories for the given test names.
    """
    pd_map = {}
    for name in names:
      p_dir = name[0:name.rfind('/') + 1]
      pd_map[p_dir] = True
    return list(pd_map.iterkeys())

  def JoinWithTestExpectation(self, test_expectations):
    """Join layout tests with the test expectation file using test name as key.

    Args:
      test_expectations: a test expectations object.

    Returns:
      test_info_map contains test name as key and another map as value. The
          other map contains test description and the test expectation
          information which contains keyword (e.g., 'GPU') as key (we do
          not care about values). The map data structure is used since we
          have to look up these keywords several times.
    """
    test_info_map = {}
    for (lt_name, desc) in self.name_map.items():
      test_info_map[lt_name] = {}
      test_info_map[lt_name]['desc'] = desc
      for (te_name, te_info) in (
          test_expectations.all_test_expectation_info.items()):
        if te_name == lt_name or (
            te_name in lt_name and te_name.endswith('/')):
          # Only keep the first match when found.
          test_info_map[lt_name]['te_info'] = te_info
          break
    return test_info_map
