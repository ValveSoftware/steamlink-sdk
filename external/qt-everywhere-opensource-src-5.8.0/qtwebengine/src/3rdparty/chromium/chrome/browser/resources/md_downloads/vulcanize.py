#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys
import tempfile


_HERE_PATH = os.path.join(os.path.dirname(__file__))

_HTML_IN_PATH = os.path.join(_HERE_PATH, 'downloads.html')
_HTML_OUT_PATH = os.path.join(_HERE_PATH, 'vulcanized.html')
_JS_OUT_PATH = os.path.join(_HERE_PATH, 'crisper.js')

_SRC_PATH = os.path.normpath(os.path.join(_HERE_PATH, '..', '..', '..', '..'))

_RESOURCES_PATH = os.path.join(_SRC_PATH, 'ui', 'webui', 'resources')

_CR_ELEMENTS_PATH = os.path.join(_RESOURCES_PATH, 'cr_elements')
_CSS_RESOURCES_PATH = os.path.join(_RESOURCES_PATH, 'css')
_HTML_RESOURCES_PATH = os.path.join(_RESOURCES_PATH, 'html')
_JS_RESOURCES_PATH = os.path.join(_RESOURCES_PATH, 'js')

_POLYMER_PATH = os.path.join(
    _SRC_PATH, 'third_party', 'polymer', 'v1_0', 'components-chromium')

_VULCANIZE_ARGS = [
  '--exclude', 'crisper.js',

  # These files are already combined and minified.
  '--exclude', 'chrome://resources/html/polymer.html',
  '--exclude', 'web-animations-next-lite.min.js',

  # These files are dynamically created by C++.
  '--exclude', 'load_time_data.js',
  '--exclude', 'strings.js',
  '--exclude', 'text_defaults.css',
  '--exclude', 'text_defaults_md.css',

  '--inline-css',
  '--inline-scripts',

  '--redirect', 'chrome://downloads/|%s' % _HERE_PATH,
  '--redirect', 'chrome://resources/cr_elements/|%s' % _CR_ELEMENTS_PATH,
  '--redirect', 'chrome://resources/css/|%s' % _CSS_RESOURCES_PATH,
  '--redirect', 'chrome://resources/html/|%s' % _HTML_RESOURCES_PATH,
  '--redirect', 'chrome://resources/js/|%s' % _JS_RESOURCES_PATH,
  '--redirect', 'chrome://resources/polymer/v1_0/|%s' % _POLYMER_PATH,

  '--strip-comments',
]

def main():
  def _run_cmd(cmd_parts, stdout=None):
    cmd = "'" + "' '".join(cmd_parts) + "'"
    process = subprocess.Popen(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
    stdout, stderr = process.communicate()

    if stderr:
      print >> sys.stderr, '%s failed: %s' % (cmd, stderr)
      raise

    return stdout

  output = _run_cmd(['vulcanize'] + _VULCANIZE_ARGS + [_HTML_IN_PATH])

  with tempfile.NamedTemporaryFile(mode='wt+', delete=False) as tmp:
    tmp.write(output.replace(
        '<include src="', '<include src="../../../../ui/webui/resources/js/'))

  try:
    _run_cmd(['crisper', '--source', tmp.name,
                         '--script-in-head', 'false',
                         '--html', _HTML_OUT_PATH,
                         '--js', _JS_OUT_PATH])
  finally:
    os.remove(tmp.name)


if __name__ == '__main__':
  main()
