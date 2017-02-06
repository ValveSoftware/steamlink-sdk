#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import re
import sys

USAGE = """find_used_resources.py [-h] [-i INPUT] [-o OUTPUT]

Outputs the sorted list of resource ids that are part of unknown pragma warning
in the given build log.

This script is used to find the resources that are actually compiled in Chrome
in order to only include the needed strings/images in Chrome PAK files. The
script parses out the list of used resource ids. These resource ids show up in
the build output after building Chrome with gyp variable
enable_resource_whitelist_generation set to 1. This gyp flag causes the compiler
to print out a UnknownPragma message every time a resource id is used. E.g.:
foo.cc:22:0: warning: ignoring #pragma whitelisted_resource_12345
[-Wunknown-pragmas]

On Windows, the message is simply a message via __pragma(message(...)).

"""

COMPONENTS_STRINGS_HEADER = 'gen/components/strings/grit/components_strings.h'

# We don't want the resources are different between 32-bit and 64-bit build,
# added arch related resources even they are not used.
ARCH_SPECIFIC_RESOURCES = [
  'IDS_VERSION_UI_64BIT',
  'IDS_VERSION_UI_32BIT',
]

def FindResourceIds(header, resource_names):
  """Returns the numerical resource IDs that correspond to the given resource
     names, as #defined in the given header file."
  """
  pattern = re.compile(
      r'^#define (%s) _Pragma\S+ (\d+)$' % '|'.join(resource_names))
  with open(header, 'r') as f:
    res_ids = [ int(pattern.match(line).group(2))
                 for line in f if pattern.match(line) ]
  if len(res_ids) != len(resource_names):
    raise Exception('Find resource id failed: the result is ' +
                    ', '.join(str(i) for i in res_ids))
  return set(res_ids)

def GetResourceIdsInPragmaWarnings(input):
  """Returns set of resource ids that are inside unknown pragma warnings
     for the given input.
  """
  used_resources = set()
  unknown_pragma_warning_pattern = re.compile(
      'whitelisted_resource_(?P<resource_id>[0-9]+)')
  for ln in input:
    match = unknown_pragma_warning_pattern.search(ln)
    if match:
      resource_id = int(match.group('resource_id'))
      used_resources.add(resource_id)
  return used_resources

def Main():
  parser = argparse.ArgumentParser(usage=USAGE)
  parser.add_argument(
      '-i', '--input', type=argparse.FileType('r'), default=sys.stdin,
      help='The build log to read (default stdin)')
  parser.add_argument(
      '-o', '--output', type=argparse.FileType('w'), default=sys.stdout,
      help='The resource list path to write (default stdout)')
  parser.add_argument('--out-dir', required=True,
      help='The out target directory, for example out/Release')

  args = parser.parse_args()


  used_resources = GetResourceIdsInPragmaWarnings(args.input)
  used_resources |= FindResourceIds(
      os.path.join(args.out_dir, COMPONENTS_STRINGS_HEADER),
      ARCH_SPECIFIC_RESOURCES)

  for resource_id in sorted(used_resources):
    args.output.write('%d\n' % resource_id)

if __name__ == '__main__':
  Main()
