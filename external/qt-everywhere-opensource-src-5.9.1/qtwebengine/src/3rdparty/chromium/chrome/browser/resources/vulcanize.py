#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys
import tempfile

# See //docs/vulcanize.md for instructions on installing prerequistes and
# running the vulcanize build.

_HERE_PATH = os.path.join(os.path.dirname(__file__))
_SRC_PATH = os.path.normpath(os.path.join(_HERE_PATH, '..', '..', '..'))

_RESOURCES_PATH = os.path.join(_SRC_PATH, 'ui', 'webui', 'resources')

_CR_ELEMENTS_PATH = os.path.join(_RESOURCES_PATH, 'cr_elements')
_CSS_RESOURCES_PATH = os.path.join(_RESOURCES_PATH, 'css')
_HTML_RESOURCES_PATH = os.path.join(_RESOURCES_PATH, 'html')
_JS_RESOURCES_PATH = os.path.join(_RESOURCES_PATH, 'js')
_POLYMER_PATH = os.path.join(
    _SRC_PATH, 'third_party', 'polymer', 'v1_0', 'components-chromium')

_VULCANIZE_BASE_ARGS = [
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

  '--redirect', 'chrome://resources/cr_elements/|%s' % _CR_ELEMENTS_PATH,
  '--redirect', 'chrome://resources/css/|%s' % _CSS_RESOURCES_PATH,
  '--redirect', 'chrome://resources/html/|%s' % _HTML_RESOURCES_PATH,
  '--redirect', 'chrome://resources/js/|%s' % _JS_RESOURCES_PATH,
  '--redirect', 'chrome://resources/polymer/v1_0/|%s' % _POLYMER_PATH,

  '--strip-comments',
]

def _run_cmd(cmd_parts, stdout=None):
  cmd = "'" + "' '".join(cmd_parts) + "'"
  process = subprocess.Popen(
      cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
  stdout, stderr = process.communicate()

  if stderr:
    print >> sys.stderr, '%s failed: %s' % (cmd, stderr)
    raise

  return stdout

def _vulcanize(directory, host, html_in_file, html_out_file='vulcanized.html',
               js_out_file='crisper.js', extra_args=None):
  print 'Vulcanizing %s/%s' % (directory, html_in_file)

  target_path = os.path.join(_HERE_PATH, directory)
  html_in_path = os.path.join(target_path, html_in_file)
  html_out_path = os.path.join(target_path, html_out_file)
  js_out_path = os.path.join(target_path, js_out_file)
  extra_args = extra_args or []

  output = _run_cmd(['vulcanize'] + _VULCANIZE_BASE_ARGS + extra_args +
                    ['--redirect', 'chrome://%s/|%s' % (host, target_path),
                     html_in_path])

  with tempfile.NamedTemporaryFile(mode='wt+', delete=False) as tmp:
    # Grit includes are not supported, use HTML imports instead.
    tmp.write(output.replace(
        '<include src="', '<include src-disabled="'))

  try:
    _run_cmd(['crisper', '--source', tmp.name,
                         '--script-in-head', 'false',
                         '--html', html_out_path,
                         '--js', js_out_path])

    # TODO(tsergeant): Remove when JS resources are minified by default:
    # crbug.com/619091.
    _run_cmd(['uglifyjs', js_out_path,
                         '--comments', '/Copyright|license|LICENSE|\<\/?if/',
                         '--output', js_out_path])
  finally:
    os.remove(tmp.name)


def _css_build(directory, files):
  target_path = os.path.join(_HERE_PATH, directory)
  paths = map(lambda f: os.path.join(target_path, f), files)

  _run_cmd(['polymer-css-build'] + paths)


def main():
  _vulcanize(directory='md_downloads', host='downloads',
             html_in_file='downloads.html')
  _css_build(directory='md_downloads', files=['vulcanized.html'])

  # Already loaded by history.html:
  history_extra_args = ['--exclude', 'chrome://resources/html/util.html',
                        '--exclude', 'chrome://history/constants.html']
  _vulcanize(directory='md_history', host='history', html_in_file='app.html',
             html_out_file='app.vulcanized.html', js_out_file='app.crisper.js',
             extra_args=history_extra_args)

  # Ensures that no file transitively imported by app.vulcanized.html is
  # imported by lazy_load.vulcanized.html.
  lazy_load_extra_args = ['--exclude', 'chrome://history/app.html']
  _vulcanize(directory='md_history', host='history',
             html_in_file='lazy_load.html',
             html_out_file='lazy_load.vulcanized.html',
             js_out_file='lazy_load.crisper.js',
             extra_args=history_extra_args + lazy_load_extra_args)
  _css_build(directory='md_history', files=['app.vulcanized.html',
                                            'lazy_load.vulcanized.html'])

if __name__ == '__main__':
  main()
