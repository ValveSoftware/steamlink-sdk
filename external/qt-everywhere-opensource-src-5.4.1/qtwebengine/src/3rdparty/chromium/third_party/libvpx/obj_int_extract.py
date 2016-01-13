#!/usr/bin/python
#
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"This script is used to run obj_int_extract and output the result to a file."

import optparse
import subprocess
import sys

parser = optparse.OptionParser()
parser.description = __doc__
parser.add_option('-e', '--executable', help='path to obj_int_extract.')
parser.add_option('-f', '--format', help='binary format (gas or rvds).')
parser.add_option('-b', '--binary', help='path to binary to parse.')
parser.add_option('-o', '--output', help='path to write extracted output to.')


options, args = parser.parse_args()
if (not options.executable or not options.format or not options.binary or
    not options.output):
  parser.error('Must specify all four arguments.')
  sys.exit(1)

with open(options.output, 'w') as fh:
  subprocess.check_call([options.executable, options.format, options.binary],
                        stdout=fh)

sys.exit(0)
