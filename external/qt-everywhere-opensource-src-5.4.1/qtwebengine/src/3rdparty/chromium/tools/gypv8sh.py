#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script is used by chrome_tests.gypi's js2webui action to maintain the
argument lists and to generate inlinable tests.
"""

import json
import optparse
import os
import subprocess
import sys
import shutil


def main ():
  parser = optparse.OptionParser()
  parser.set_usage(
      "%prog v8_shell mock.js test_api.js js2webui.js "
      "testtype inputfile inputrelfile cxxoutfile jsoutfile")
  parser.add_option('-v', '--verbose', action='store_true')
  parser.add_option('-n', '--impotent', action='store_true',
                    help="don't execute; just print (as if verbose)")
  parser.add_option('--deps_js', action="store",
                    help=("Path to deps.js for dependency resolution, " +
                          "optional."))
  (opts, args) = parser.parse_args()

  if len(args) != 9:
    parser.error('all arguments are required.')
  (v8_shell, mock_js, test_api, js2webui, test_type,
      inputfile, inputrelfile, cxxoutfile, jsoutfile) = args
  cmd = [v8_shell]
  icudatafile = os.path.join(os.path.dirname(v8_shell), 'icudtl.dat')
  if os.path.exists(icudatafile):
    cmd.extend(['--icu-data-file=%s' % icudatafile])
  arguments = [js2webui, inputfile, inputrelfile, opts.deps_js,
               cxxoutfile, test_type]
  cmd.extend(['-e', "arguments=" + json.dumps(arguments), mock_js,
         test_api, js2webui])
  if opts.verbose or opts.impotent:
    print cmd
  if not opts.impotent:
    try:
      with open(cxxoutfile, 'w') as f:
        subprocess.check_call(cmd, stdin=subprocess.PIPE, stdout=f)
      shutil.copyfile(inputfile, jsoutfile)
    except Exception, ex:
      if os.path.exists(cxxoutfile):
        # The contents of the output file will include the error message.
        print open(cxxoutfile).read()
        os.remove(cxxoutfile)
      if os.path.exists(jsoutfile):
        os.remove(jsoutfile)
      raise


if __name__ == '__main__':
 sys.exit(main())
