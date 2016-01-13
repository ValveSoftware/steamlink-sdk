# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Bug module that is necessary for the layout analyzer."""

import re

from webkitpy.layout_tests.models.test_expectations import *


class Bug(object):
  """A class representing a bug.

  TODO(imasaki): add more functionalities here if bug-tracker API is available.
  For example, you can get the name of a bug owner.
  """
  # Type enum for the bug.
  WEBKIT = 0
  CHROMIUM = 1
  OTHERS = 2

  def __init__(self, bug_modifier):
    """Initialize the object using raw bug text (such as BUGWK2322).

    The bug modifier used in the test expectation file.

    Args:
      bug_modifier: a string representing a bug modifier. According to
        http://www.chromium.org/developers/testing/webkit-layout-tests/\
        testexpectations
        Bug identifiers are of the form "webkit.org/b/12345", "crbug.com/12345",
         "code.google.com/p/v8/issues/detail?id=12345" or "Bug(username)"
    """
    match = re.match('Bug\((\w+)\)$', bug_modifier)
    if match:
      self.type = self.OTHERS
      self.url = 'mailto:%s@chromium.org' % match.group(1).lower()
      self.bug_txt = bug_modifier
      return

    self.type = self.GetBugType(bug_modifier)
    self.url = bug_modifier
    self.bug_txt = bug_modifier


  def GetBugType(self, bug_modifier):
    """Returns type of the bug based on URL."""
    if bug_modifier.startswith(WEBKIT_BUG_PREFIX):
      return self.WEBKIT;
    if bug_modifier.startswith(CHROMIUM_BUG_PREFIX):
      return self.CHROMIUM;
    return self.OTHERS

  def __str__(self):
    """Get a string representation of a bug object.

    Returns:
      a string for HTML link representation of a bug.
    """
    return '<a href="%s">%s</a>' % (self.url, self.bug_txt)
