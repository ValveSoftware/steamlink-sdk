# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for gpu.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import re

def PostUploadHook(cl, change, output_api):
  """git cl upload will call this hook after the issue is created/modified.

  This hook adds extra try bots list to the CL description in order to run
  Blink tests in addition to CQ try bots.
  """
  rietveld_obj = cl.RpcServer()
  issue = cl.issue
  description = rietveld_obj.get_description(issue)
  if re.search(r'^CQ_INCLUDE_TRYBOTS=.*', description, re.M | re.I):
    return []

  bots = [
    'tryserver.chromium.linux:linux_optional_gpu_tests_rel',
    'tryserver.chromium.mac:mac_optional_gpu_tests_rel',
    'tryserver.chromium.win:win_optional_gpu_tests_rel',
  ]

  results = []
  new_description = description
  new_description += '\nCQ_INCLUDE_TRYBOTS=%s' % ';'.join(bots)
  results.append(output_api.PresubmitNotifyResult(
      'Automatically added optional GPU tests to run on CQ.'))

  if new_description != description:
    rietveld_obj.update_description(issue, new_description)

  return results
