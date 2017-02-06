#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Generates a list of Blimp target runtime dependencies.'''

import argparse
import fnmatch

# Returns True if |entry| matches any of the patterns in |blacklist|.
def IsBlacklisted(entry, blacklist):
  return any([next_pat for next_pat in blacklist
              if fnmatch.fnmatch(entry, next_pat)])

def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--runtime-deps-file',
                      help=('name and path of runtime deps file, '
                            'if available'),
                      required=True,
                      metavar='FILE')
  parser.add_argument('--output',
                      help=('name and path of manifest file to create '
                            '(required)'),
                      required=True,
                      metavar='FILE')
  parser.add_argument('--blacklist',
                      help=('name and path of the blacklist file to use'),
                      required=True)
  args = parser.parse_args()

  with open(args.runtime_deps_file) as f:
    deps = f.read().splitlines()

  header = [
      '# Runtime dependencies for: ' + args.runtime_deps_file,
      '#',
      '# Note: Any unnecessary dependencies should be added to',
      '#       the appropriate blacklist and this file should be regenerated.',
      '',
  ]

  blacklist_patterns = []
  with open(args.blacklist, 'r') as blacklist_file:
    blacklist_patterns = \
        [entry.partition('#')[0].strip() for entry \
         in blacklist_file.readlines()]

  with open(args.output, 'w') as manifest:
    manifest.write('\n'.join(header))
    manifest.write('\n'.join([dep for dep in deps
                              if not IsBlacklisted(dep, blacklist_patterns)]))
    manifest.write('\n')

if __name__ == "__main__":
  main()
