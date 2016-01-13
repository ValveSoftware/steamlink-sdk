# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # The |major|, |build| and |patch| versions are inherited from Chrome.
    # Since Chrome's |minor| version is always '0', we replace it with a
    # Chromoting-specific patch version.
    # Note that we check both the |chrome_version_path| file and the
    # |remoting_version_path| so that we can override the Chrome version
    # numbers if needed.
    'version_py_path': '../build/util/version.py',
    'remoting_version_path': '../remoting/VERSION',
    'chrome_version_path': '../chrome/VERSION',
    'version_major':
      '<!(python <(version_py_path) -f <(chrome_version_path) -f <(remoting_version_path) -t "@MAJOR@")',
    'version_minor':
      '<!(python <(version_py_path) -f <(remoting_version_path) -t "@REMOTING_PATCH@")',
    'version_short':
      '<(version_major).<(version_minor).'
      '<!(python <(version_py_path) -f <(chrome_version_path) -f <(remoting_version_path) -t "@BUILD@")',
    'version_full':
      '<(version_short).'
      '<!(python <(version_py_path) -f <(chrome_version_path) -f <(remoting_version_path) -t "@PATCH@")',
  },
}
