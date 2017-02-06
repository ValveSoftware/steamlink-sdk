#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Bundles the Blimp target and its runtime dependencies into a tarball.

   The created bundle can be passed as input to docker build.  E.g.
   docker build - < ../../out-linux/Debug/blimp_engine_deps.tar.gz
'''


import argparse
import os
import subprocess
import sys

def ReadDependencies(manifest):
  """Read the manifest and return the list of dependencies.
  :raises IOError: if the manifest could not be read.
  """
  deps = []
  with open(manifest) as f:
    for line in f.readlines():
      # Strip comments.
      dep = line.partition('#')[0].strip()
      # Ignore empty strings.
      if dep:
        deps.append(dep)
  return deps

def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--build-dir',
                      help=('build output directory (e.g. out/Debug)'),
                      required=True,
                      metavar='DIR')
  parser.add_argument('--filelist',
                      help=('optional space separated list of files (e.g. '
                            'Dockerfile and startup script) to add to the '
                            'bundle'),
                      required=False,
                      metavar='FILE',
                      nargs='*')
  parser.add_argument('--manifest',
                      help=('file listing the set of files to include in '
                            'the bundle'),
                      required=True)
  parser.add_argument('--output',
                      help=('name and path of bundle to create'),
                      required=True,
                      metavar='FILE')
  args = parser.parse_args()

  deps = ReadDependencies(args.manifest)

  try:
    env = os.environ.copy()
    # Use fastest possible mode when gzipping.
    env["GZIP"] = "-1"
    subprocess_args = [
        "tar",
        "-zcf", args.output,
        # Ensure tarball content group permissions are appropriately set for
        # use as part of a "docker build". That is group readable with
        # executable files also being group executable.
        "--mode=g+rX",
        "-C", args.build_dir] + deps
    for f in args.filelist:
      dirname, basename = os.path.split(f)
      subprocess_args.extend(["-C", dirname, basename])
    subprocess.check_output(
        subprocess_args,
        # Redirect stderr to stdout, so that its output is captured.
        stderr=subprocess.STDOUT,
        env=env)
  except subprocess.CalledProcessError as e:
    print >> sys.stderr, "Failed to create tarball:"
    print >> sys.stderr, e.output
    sys.exit(1)

if __name__ == "__main__":
  main()
