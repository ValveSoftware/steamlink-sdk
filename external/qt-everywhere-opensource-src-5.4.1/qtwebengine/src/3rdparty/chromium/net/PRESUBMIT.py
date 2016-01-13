# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromium presubmit script for src/net.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into gcl.
"""

def GetPreferredTryMasters(project, change):
  masters = {
    'tryserver.chromium': {
      'linux_chromium_rel': set(['defaulttests']),
      'mac_chromium_rel': set(['defaulttests']),
      'win_chromium_rel': set(['defaulttests']),
    }
  }
  # Changes that touch NSS files will likely need a corresponding OpenSSL edit.
  # Conveniently, this one glob also matches _openssl.* changes too.
  if any('nss' in f.LocalPath() for f in change.AffectedFiles()):
    masters['tryserver.chromium'].setdefault(
      'linux_redux', set()).add('defaulttests')
  return masters
