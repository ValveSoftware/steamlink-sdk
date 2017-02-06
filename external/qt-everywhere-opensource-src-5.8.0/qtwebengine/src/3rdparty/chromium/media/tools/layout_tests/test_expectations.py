# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module to analyze test expectations for Webkit layout tests."""

import urllib2

from webkitpy.layout_tests.models.test_expectations import *

# Default location for chromium test expectation file.
# TODO(imasaki): support multiple test expectations files.
DEFAULT_TEST_EXPECTATIONS_LOCATION = (
    'http://src.chromium.org/blink/trunk/LayoutTests/TestExpectations')

# The following is from test expectation syntax. The detail can be found in
# http://www.chromium.org/developers/testing/webkit-layout-tests#TOC-Test-Expectations
# <decision> ::== [SKIP] [WONTFIX] [SLOW]
DECISION_NAMES = ['SKIP', 'WONTFIX', 'SLOW']
# <config> ::== RELEASE | DEBUG
CONFIG_NAMES = ['RELEASE', 'DEBUG']
# Only hard code keywords we don't expect to change.  Determine the rest from
# the format of the status line.
KNOWN_TE_KEYWORDS = DECISION_NAMES + CONFIG_NAMES


class TestExpectations(object):
  """A class to model the content of test expectation file for analysis.

  This class retrieves the TestExpectations file via HTTP from WebKit and uses
  the WebKit layout test processor to process each line.

  The resulting dictionary is stored in |all_test_expectation_info| and looks
  like:

    {'<test name>': [{'<modifier0>': True, '<modifier1>': True, ...,
                     'Platforms: ['<platform0>', ... ], 'Bugs': ['....']}]}

  Duplicate keys are merged (though technically they shouldn't exist).

  Example:
    crbug.com/145590 [ Android ] \
        platform/chromium/media/video-frame-size-change.html [ Timeout ]
    webkit.org/b/84724 [ SnowLeopard ] \
        platform/chromium/media/video-frame-size-change.html \
        [ ImageOnlyFailure Pass ]

  {'platform/chromium/media/video-frame-size-change.html': [{'IMAGE': True,
   'Bugs': ['BUGWK84724', 'BUGCR145590'], 'Comments': '',
   'Platforms': ['SNOWLEOPARD', 'ANDROID'], 'TIMEOUT': True, 'PASS': True}]}
  """

  def __init__(self, url=DEFAULT_TEST_EXPECTATIONS_LOCATION):
    """Read the test expectation file from the specified URL and parse it.

    Args:
      url: A URL string for the test expectation file.

    Raises:
      NameError when the test expectation file cannot be retrieved from |url|.
    """
    self.all_test_expectation_info = {}
    resp = urllib2.urlopen(url)
    if resp.code != 200:
      raise NameError('Test expectation file does not exist in %s' % url)
    # Start parsing each line.
    for line in resp.read().split('\n'):
      line = line.strip()
      # Skip comments.
      if line.startswith('#'):
        continue
      testname, te_info = self.ParseLine(line)
      if not testname or not te_info:
        continue
      if testname in self.all_test_expectation_info:
        # Merge keys if entry already exists.
        for k in te_info.keys():
          if (isinstance(te_info[k], list) and
              k in self.all_test_expectation_info[testname]):
            self.all_test_expectation_info[testname][0][k] += te_info[k]
          else:
            self.all_test_expectation_info[testname][0][k] = te_info[k]
      else:
        self.all_test_expectation_info[testname] = [te_info]

  @staticmethod
  def ParseLine(line):
    """Parses the provided line using WebKit's TextExpecations parser.

    Returns:
      Tuple of test name, test expectations dictionary.  See class documentation
      for the format of the dictionary
    """
    test_expectation_info = {}
    parsed = TestExpectationParser._tokenize_line('TestExpectations', line, 0)
    if parsed.is_invalid():
      return None, None

    test_expectation_info['Comments'] = parsed.comment or ''
    test_expectation_info['Bugs'] = parsed.bugs or [];
    test_expectation_info['Platforms'] =  parsed.specifiers or []
    # Shovel the expectations and modifiers in as "<key>: True" entries.  Ugly,
    # but required by the rest of the pipeline for parsing.
    for m in parsed.expectations:
      test_expectation_info[m] = True

    return parsed.name, test_expectation_info
