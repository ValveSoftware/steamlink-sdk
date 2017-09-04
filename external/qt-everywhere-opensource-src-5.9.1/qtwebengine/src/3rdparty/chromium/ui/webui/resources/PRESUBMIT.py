# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re


def PostUploadHook(cl, change, output_api):
  rietveld_obj = cl.RpcServer()
  description = rietveld_obj.get_description(cl.issue)

  existing_bots = (change.CQ_INCLUDE_TRYBOTS or '').split(';')
  clean_bots = set(filter(None, map(lambda s: s.strip(), existing_bots)))
  new_bots = clean_bots | set(
      ['master.tryserver.chromium.linux:closure_compilation'])
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


def CheckChangeOnUpload(input_api, output_api):
  return _CheckForTranslations(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CheckForTranslations(input_api, output_api)


def _CheckForTranslations(input_api, output_api):
  shared_keywords = ['i18n(']
  html_keywords = shared_keywords + ['$118n{']
  js_keywords = shared_keywords + ['I18nBehavior', 'loadTimeData.']

  errors = []

  for f in input_api.AffectedFiles():
    local_path = f.LocalPath()

    keywords = None
    if local_path.endswith('.js'):
      keywords = js_keywords
    elif local_path.endswith('.html'):
      keywords = html_keywords

    if not keywords:
      continue

    for lnum, line in f.ChangedContents():
      if any(line for keyword in keywords if keyword in line):
        errors.append("%s:%d\n%s" % (f.LocalPath(), lnum, line))

  if not errors:
    return []

  return [output_api.PresubmitError("\n".join(errors) + """

Don't embed translations directly in shared UI code. Instead, inject your
translation from the place using the shared code. For an example: see
<cr-dialog>#closeText (http://bit.ly/2eLEsqh).""")]
