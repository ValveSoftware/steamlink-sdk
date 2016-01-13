#!/usr/bin/python
#
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script is used to find and copy a file that may be found in one or more
places. The sources are searched in order and (only) the first one found will
be copied to the destination."""

import shutil
import optparse
import os
import sys

parser = optparse.OptionParser()
parser.description = __doc__
parser.add_option('-d', '--destination')
parser.add_option('-s', '--source', default=[], action='append',
                  help='Specify multiple times for multiple sources.')
options, args = parser.parse_args()
if (not options.destination or not options.source):
  parser.error('Must specify both a destination and one or more sources.')
  sys.exit(1)

for src in options.source:
  if os.path.exists(src):
    shutil.copyfile(src, options.destination)
    sys.exit(0)

print "Unable to locate file"
sys.exit(1)
