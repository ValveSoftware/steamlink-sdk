#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates the metrics collected weekly for the Browser Components project.

See
http://www.chromium.org/developers/design-documents/browser-components
for details.
"""

import os
import sys


# This is done so that we can import checkdeps.  If not invoked as
# main, our user must ensure it is in PYTHONPATH.
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', '..',
                               'buildtools', 'checkdeps'))


import count_ifdefs
import checkdeps
import results


# Preprocessor pattern to find OS_XYZ defines.
PREPROCESSOR_PATTERN = 'OS_[A-Z]+'


class BrowserComponentsMetricsGenerator(object):
  def __init__(self, checkout_root):
    self.checkout_root = checkout_root
    self.chrome_browser = os.path.join(checkout_root, 'chrome', 'browser')

  def CountIfdefs(self, skip_tests):
    return count_ifdefs.CountIfdefs(
        PREPROCESSOR_PATTERN, self.chrome_browser, skip_tests)

  def CountViolations(self, skip_tests):
    deps_checker = checkdeps.DepsChecker(self.checkout_root,
                                         ignore_temp_rules=True,
                                         skip_tests=skip_tests)
    deps_checker.results_formatter = results.CountViolationsFormatter()
    deps_checker.CheckDirectory(os.path.join('chrome', 'browser'))
    return int(deps_checker.results_formatter.GetResults())


def main():
  generator = BrowserComponentsMetricsGenerator(
      os.path.join(os.path.dirname(__file__), '..', '..', '..'))

  print "All metrics are for chrome/browser.\n"
  print "OS ifdefs, all:                   %d" % generator.CountIfdefs(False)
  print "OS ifdefs, -tests:                %d" % generator.CountIfdefs(True)
  print ("Intended DEPS violations, all:    %d" %
         generator.CountViolations(False))
  print "Intended DEPS violations, -tests: %d" % generator.CountViolations(True)
  return 0


if __name__ == '__main__':
  sys.exit(main())
