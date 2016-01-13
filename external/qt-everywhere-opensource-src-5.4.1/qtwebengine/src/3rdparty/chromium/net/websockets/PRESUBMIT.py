# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromium presubmit script for src/net/websockets.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into gcl.
"""


# TODO(ricea): Remove this once the old implementation has been removed and the
# list of files in the README file is no longer needed.
def _CheckReadMeComplete(input_api, output_api):
  """Verifies that any new files have been added to the README file.

  Checks that if any source files were added in this CL, that they were
  also added to the README file. We do not warn about pre-existing
  errors, as that would be annoying.

  Args:
      input_api: The InputApi object provided by the presubmit framework.
      output_api: The OutputApi object provided by the framework.

  Returns:
      A list of zero or more PresubmitPromptWarning objects.
  """
  # None passed to AffectedSourceFiles means "use the default filter", which
  # does what we want, ie. returns files in the CL with filenames that look like
  # source code.
  added_source_filenames = set(input_api.basename(af.LocalPath())
                               for af in input_api.AffectedSourceFiles(None)
                               if af.Action().startswith('A'))
  if not added_source_filenames:
    return []
  readme = input_api.AffectedSourceFiles(
      lambda af: af.LocalPath().endswith('/README'))
  if not readme:
    return [output_api.PresubmitPromptWarning(
        'One or more files were added to net/websockets without being added\n'
        'to net/websockets/README.\n', added_source_filenames)]
  readme_added_filenames = set(line.strip() for line in readme[0].NewContents()
                               if line.strip() in added_source_filenames)
  if readme_added_filenames < added_source_filenames:
    return [output_api.PresubmitPromptWarning(
        'One or more files added to net/websockets but not found in the README '
        'file.\n', added_source_filenames - readme_added_filenames)]
  else:
    return []


def CheckChangeOnUpload(input_api, output_api):
  return _CheckReadMeComplete(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CheckReadMeComplete(input_api, output_api)
