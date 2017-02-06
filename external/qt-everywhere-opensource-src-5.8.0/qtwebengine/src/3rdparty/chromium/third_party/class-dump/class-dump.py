# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Wrapper around class-dump to allow gn build to run the tool (as action
# target can only run python scripts). Also filters the output to remove
# all cxx_destruct lines that are not understood from class-dump output.

import argparse
import errno
import os
import subprocess
import sys


def Main(args):
  parser = argparse.ArgumentParser('wrapper around class-dump')
  parser.add_argument(
      '-o', '--output', required=True,
      help='name of the output file')
  parser.add_argument(
      '-t', '--class-dump-path', required=True,
      help='path to class-dump executable')
  parser.add_argument(
      'args', nargs='*',
      help='arguments to pass to class-dump')
  args = parser.parse_args(args)

  try:
    os.makedirs(os.path.dirname(args.output))
  except OSError, e:
    if e.errno != errno.EEXIST:
      raise

  process = subprocess.Popen(
      [args.class_dump_path] + args.args,
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE)
  stdout, stderr = process.communicate()
  if process.returncode != 0:
    sys.stderr.write(stderr)
    sys.exit(process.returncode)

  with open(args.output, 'wb') as output:
    output.write("// Treat class-dump output as a system header.\n")
    output.write("#pragma clang system_header\n")
    for line in stdout.splitlines():
      if 'cxx_destruct' in line:
        continue
      output.write(line)
      output.write('\n')


if __name__ == '__main__':
  Main(sys.argv[1:])
