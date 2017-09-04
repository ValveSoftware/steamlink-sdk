# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import time

def CheckChangeOnUpload(input_api, output_api):
  """Warn when changing md_history without vulcanizing."""

  def _is_md_history_file(path):
    return (path.startswith('chrome/browser/resources/md_history') and
            (not path.endswith('externs.js')) and
            (not path.endswith('crisper.js')) and
            (not path.endswith('vulcanized.html')) and
            (path.endswith('js') or path.endswith('html')))

  paths = [x.LocalPath() for x in input_api.change.AffectedFiles()]
  earliest_vulcanize_change = min(os.path.getmtime(x) for x in
                                  ['app.vulcanized.html',
                                   'app.crisper.js',
                                   'lazy_load.vulcanized.html',
                                   'lazy_load.crisper.js'])
  history_changes = filter(_is_md_history_file, paths)
  latest_history_change = 0
  if history_changes:
    latest_history_change = max(os.path.getmtime(os.path.split(x)[1]) for x in
                                history_changes)

  if latest_history_change > earliest_vulcanize_change:
    return [output_api.PresubmitPromptWarning(
        'Vulcanize must be run when changing files in md_history. See '
        'docs/vulcanize.md.')]
  return []
