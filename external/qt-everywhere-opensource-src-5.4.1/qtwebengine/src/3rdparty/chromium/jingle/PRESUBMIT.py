# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromium presubmit script for src/jingle.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into gcl.
"""

def GetPreferredTryMasters(project, change):
  return {
    'tryserver.chromium': {
      'linux_chromium_rel': set(['defaulttests']),
      'mac_chromium_rel': set(['defaulttests']),
      'win_chromium_rel': set(['defaulttests']),
    }
  }
