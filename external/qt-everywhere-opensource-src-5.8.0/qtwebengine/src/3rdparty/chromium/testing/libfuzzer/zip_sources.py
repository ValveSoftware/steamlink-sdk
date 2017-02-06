#!/usr/bin/python2
#
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Archive all source files that are references in binary debug info.

Invoked by libfuzzer buildbots. Executes llvm-dwarfdump to parse debug info.
"""

from __future__ import print_function

import argparse
import os
import re
import subprocess
import zipfile

compile_unit_re = re.compile('.*DW_TAG_compile_unit.*')
at_name_re = re.compile('.*DW_AT_name.*debug_str\[.*\] = "(.*)".*')


def main():
  parser = argparse.ArgumentParser(description="Zip binary sources.")
  parser.add_argument('--binary', required=True,
          help='binary file to read')
  parser.add_argument('--workdir', required=True,
          help='working directory to use to resolve relative paths')
  parser.add_argument('--output', required=True,
          help='output zip file name')
  parser.add_argument('--llvm-dwarfdump', required=False,
          default='llvm-dwarfdump', help='path to llvm-dwarfdump utility')
  args = parser.parse_args()

  # Dump .debug_info section.
  out = subprocess.check_output(
          [args.llvm_dwarfdump, '-debug-dump=info', args.binary])

  looking_for_unit = True
  compile_units = set()

  # Look for DW_AT_name within DW_TAG_compile_unit
  for line in out.splitlines():
    if looking_for_unit and compile_unit_re.match(line):
      looking_for_unit = False
    elif not looking_for_unit:
      match = at_name_re.match(line)
      if match:
        compile_units.add(match.group(1))
        looking_for_unit = True

  # Zip sources.
  with zipfile.ZipFile(args.output, 'w') as z:
    for compile_unit in sorted(compile_units):
      src_file = os.path.abspath(os.path.join(args.workdir, compile_unit))
      print(src_file)
      z.write(src_file, compile_unit)


if __name__ == '__main__':
  main()

