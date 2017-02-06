# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re


def PostUploadHook(cl, change, output_api):
  rietveld_obj = cl.RpcServer()
  description = rietveld_obj.get_description(cl.issue)

  existing_bots = (change.CQ_INCLUDE_TRYBOTS or '').split(';')
  clean_bots = set(filter(None, map(lambda s: s.strip(), existing_bots)))
  new_bots = clean_bots | set(['tryserver.chromium.linux:closure_compilation'])
  new_tag = 'CQ_INCLUDE_TRYBOTS=%s' % ';'.join(new_bots)

  if clean_bots:
    tag_reg = '^CQ_INCLUDE_TRYBOTS=.*$'
    new_description = re.sub(tag_reg, new_tag, description, flags=re.M | re.I)
  else:
    new_description = description + '\n' + new_tag

  if new_description == description:
    return []

  rietveld_obj.update_description(cl.issue, new_description)
  return [output_api.PresubmitNotifyResult(
      'Automatically added optional Closure bots to run on CQ.')]
