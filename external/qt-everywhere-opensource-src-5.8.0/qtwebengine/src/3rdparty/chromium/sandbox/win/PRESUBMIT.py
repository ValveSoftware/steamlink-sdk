# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for sandbox/win.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import re

def PostUploadHook(cl, change, output_api):
  """git cl upload will call this hook after the issue is created/modified.

  This hook adds extra try bots list to the CL description in order to run
  tests on the Windows 10 try bot in addition to CQ try bots.
  """
  rietveld_obj = cl.RpcServer()
  issue = cl.issue
  description = rietveld_obj.get_description(issue)
  if re.search(r'^CQ_INCLUDE_TRYBOTS=.*', description, re.M | re.I):
    return []

  bots = [
    'tryserver.chromium.win:win10_chromium_x64_rel_ng',
  ]

  results = []
  new_description = description
  new_description += '\nCQ_INCLUDE_TRYBOTS=%s' % ';'.join(bots)
  results.append(output_api.PresubmitNotifyResult(
      'Automatically added Win10 bot to run on CQ.'))

  if new_description != description:
    rietveld_obj.update_description(issue, new_description)

  return results
