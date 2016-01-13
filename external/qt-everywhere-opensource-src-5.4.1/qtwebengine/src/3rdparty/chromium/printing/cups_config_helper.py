#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""cups-config wrapper.

cups-config, at least on Ubuntu Lucid and Natty, dumps all
cflags/ldflags/libs when passed the --libs argument.  gyp would like
to keep these separate: cflags are only needed when compiling files
that use cups directly, while libs are only needed on the final link
line.

This can be dramatically simplified or maybe removed (depending on GN
requirements) when this is fixed:
  https://bugs.launchpad.net/ubuntu/+source/cupsys/+bug/163704
is fixed.
"""

import subprocess
import sys

def usage():
  print 'usage: %s {--api-version|--cflags|--ldflags|--libs|--libs-for-gn}' % \
      sys.argv[0]


def run_cups_config(mode):
  """Run cups-config with all --cflags etc modes, parse out the mode we want,
  and return those flags as a list."""

  cups = subprocess.Popen(['cups-config', '--cflags', '--ldflags', '--libs'],
                          stdout=subprocess.PIPE)
  flags = cups.communicate()[0].strip()

  flags_subset = []
  for flag in flags.split():
    flag_mode = None
    if flag.startswith('-l'):
      flag_mode = '--libs'
    elif (flag.startswith('-L') or flag.startswith('-Wl,')):
      flag_mode = '--ldflags'
    elif (flag.startswith('-I') or flag.startswith('-D')):
      flag_mode = '--cflags'

    # Be conservative: for flags where we don't know which mode they
    # belong in, always include them.
    if flag_mode is None or flag_mode == mode:
      flags_subset.append(flag)

  # Note: cross build is confused by the option, and may trigger linker
  # warning causing build error.
  if '-lgnutls' in flags_subset:
    flags_subset.remove('-lgnutls')

  return flags_subset


def main():
  if len(sys.argv) != 2:
    usage()
    return 1

  mode = sys.argv[1]
  if mode == '--api-version':
    subprocess.call(['cups-config', '--api-version'])
    return 0

  # All other modes get the flags.
  if mode not in ('--cflags', '--libs', '--libs-for-gn', '--ldflags'):
    usage()
    return 1

  if mode == '--libs-for-gn':
    gn_libs_output = True
    mode = '--libs'
  else:
    gn_libs_output = False

  flags = run_cups_config(mode)

  if gn_libs_output:
    # Strip "-l" from beginning of libs, quote, and surround in [ ].
    print '['
    for lib in flags:
      if lib[:2] == "-l":
        print '"%s", ' % lib[2:]
    print ']'
  else:
    print ' '.join(flags)

  return 0


if __name__ == '__main__':
  sys.exit(main())
