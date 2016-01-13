# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for FFmpeg repository.

Does the following:
- Warns users that changes must be submitted via Gerrit.
- Warns users when a change is made without updating the README file.
"""

import re
import subprocess


# Setup some basic ANSI colors.
BOLD_RED = '\033[1;31m'
BOLD_BLUE = '\033[1;34m'


def Color(color, string):
  return ''.join([color, string, '\033[m'])


def _WarnAboutManualSteps():
  """Warn about the manual steps required for rolling the FFmpeg repository."""
  print Color(BOLD_RED, '[ WARNING ]'.center(70, '*'))
  print Color(
      BOLD_BLUE, '\n'.join([
          'Changes to FFmpeg must uploaded to Gerrit not Rietveld.  You are',
          'seeing this message because you''re trying to upload to Rietveld.',
          'Please see README.chromium for details on using Gerrit.\n',
          'Updates to FFmpeg require the following additional manual steps:',
          '  - External DEPS roll.',
          '  - If new source files were added, generate_gyp.py must be run.\n',
          'For help, please consult the following resources:',
          '  - README.chromium help text.',
          '  - Chromium development list: chromium-dev@chromium.org',
          '  - "Communication" section of http://www.chromium.org/developers']))
  print Color(BOLD_RED, 70 * '*')


def _WarnIfReadmeIsUnchanged(input_api, output_api):
  """Warn if the README file hasn't been updated with change notes."""
  readme_re = re.compile(r'.*[/\\]?chromium[/\\]patches[/\\]README$')
  for f in input_api.AffectedFiles():
    if readme_re.match(f.LocalPath()):
      return []

  return [output_api.PresubmitPromptWarning('\n'.join([
      'FFmpeg changes detected without any update to chromium/patches/README,',
      'it\'s good practice to update this file with a note about your changes.'
  ]))]


def CheckChangeOnUpload(input_api, output_api):
  _WarnAboutManualSteps()
  results = []
  results.extend(_WarnIfReadmeIsUnchanged(input_api, output_api))
  return results
