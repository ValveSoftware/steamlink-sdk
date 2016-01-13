# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromium presubmit script for src/ui/events

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into gcl.
"""

def GetPreferredTryMasters(project, change):
  tests = set(['ash_unittests',
               'aura_unittests',
               'events_unittests',
               'keyboard_unittests',
               'views_unittests'])

  return {
    'tryserver.chromium': {
      'linux_chromium_rel': tests,
      'linux_chromium_chromeos_rel': tests,
      'linux_chromeos_asan': tests,
      'win_chromium_compile_dbg': tests,
      'win_chromium_rel': tests,
    }
  }
