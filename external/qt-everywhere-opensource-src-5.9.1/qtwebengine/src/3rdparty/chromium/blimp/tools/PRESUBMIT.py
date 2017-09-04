# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit for blimp/tools."""


def CommonChecks(input_api, output_api):
  """Presubmit checks run on both upload and commit.

  This is currently limited to pylint.
  """
  checks = []

  blimp_tools_dir = input_api.PresubmitLocalPath()

  def J(*dirs):
    """Returns a path relative to presubmit directory."""
    return input_api.os_path.join(blimp_tools_dir, *dirs)

  checks.extend(input_api.canned_checks.GetPylint(
      input_api,
      output_api,
      pylintrc='pylintrc',
      extra_paths_list=[
          J(),
          J('..', '..', 'build', 'android'),
          J('..', '..', 'third_party', 'catapult', 'devil')
      ]))

  return input_api.RunTests(checks, False)


def CheckChangeOnUpload(input_api, output_api):
  """Presubmit checks on CL upload."""
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  """Presubmit checks on commit."""
  return CommonChecks(input_api, output_api)

