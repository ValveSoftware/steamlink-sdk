# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for blimp.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import re

BLIMP_SOURCE_FILES=(r'^blimp[\\/].*\.(cc|h)$',)

def CheckChangeLintsClean(input_api, output_api):
  source_filter = lambda x: input_api.FilterSourceFile(
    x, white_list=BLIMP_SOURCE_FILES, black_list=None)

  return input_api.canned_checks.CheckChangeLintsClean(
      input_api, output_api, source_filter, lint_filters=[], verbose_level=1)

def CheckNewFilesHaveTests(input_api, output_api):
  unittest_files = set()
  files_needing_unittest = set()

  for source_file in input_api.AffectedFiles():
    if source_file.Action() == 'A':
      name = source_file.LocalPath()
      if name.endswith('_unittest.cc'):
        unittest_files.add(name)
      elif name.endswith('.cc') and not name.endswith('test.cc'):
        files_needing_unittest.add(name)

  missing_unittest_files = []

  for name in files_needing_unittest:
    unittest_name = re.sub(r'\.cc$', '_unittest.cc', name)
    if unittest_name not in unittest_files:
      missing_unittest_files.append(name)

  if missing_unittest_files:
    message = 'The following new files are missing unit tests:'
    return [output_api.PresubmitPromptWarning(message, missing_unittest_files)]
  else:
    return []

def CheckChangeOnUpload(input_api, output_api):
  results = []
  results += CheckChangeLintsClean(input_api, output_api)
  results += CheckNewFilesHaveTests(input_api, output_api)
  return results
