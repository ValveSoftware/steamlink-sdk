#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Removes bundled libraries to make sure they are not used.

See README for more details.
"""


import optparse
import os.path
import sys


def DoMain(argv):
  my_dirname = os.path.abspath(os.path.dirname(__file__))
  source_tree_root = os.path.abspath(
    os.path.join(my_dirname, '..', '..', '..'))

  if os.path.join(source_tree_root, 'build', 'linux', 'unbundle') != my_dirname:
    print ('Sanity check failed: please run this script from ' +
           'build/linux/unbundle directory.')
    return 1

  parser = optparse.OptionParser()
  parser.add_option('--do-remove', action='store_true')

  options, args = parser.parse_args(argv)

  exclusion_used = {}
  for exclusion in args:
    exclusion_used[exclusion] = False

  for root, dirs, files in os.walk(source_tree_root, topdown=False):
    # Only look at paths which contain a "third_party" component
    # (note that e.g. third_party.png doesn't count).
    root_relpath = os.path.relpath(root, source_tree_root)
    if 'third_party' not in root_relpath.split(os.sep):
      continue

    for f in files:
      path = os.path.join(root, f)
      relpath = os.path.relpath(path, source_tree_root)

      excluded = False
      for exclusion in args:
        if relpath.startswith(exclusion):
          # Multiple exclusions can match the same path. Go through all of them
          # and mark each one as used.
          exclusion_used[exclusion] = True
          excluded = True
      if excluded:
        continue

      # Deleting gyp files almost always leads to gyp failures.
      # These files come from Chromium project, and can be replaced if needed.
      if f.endswith('.gyp') or f.endswith('.gypi'):
        continue

      if options.do_remove:
        # Delete the file - best way to ensure it's not used during build.
        os.remove(path)
      else:
        # By default just print paths that would be removed.
        print path

  exit_code = 0

  # Fail if exclusion list contains stale entries - this helps keep it
  # up to date.
  for exclusion, used in exclusion_used.iteritems():
    if not used:
      print '%s does not exist' % exclusion
      exit_code = 1

  if not options.do_remove:
    print ('To actually remove files printed above, please pass ' +
           '--do-remove flag.')

  return exit_code


if __name__ == '__main__':
  sys.exit(DoMain(sys.argv[1:]))
