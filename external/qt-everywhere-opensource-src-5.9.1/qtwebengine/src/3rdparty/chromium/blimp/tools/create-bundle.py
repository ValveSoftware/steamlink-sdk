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

def ReadDependencies(manifest, relative_path):
  """Returns a list of dependencies based on the specified manifest file.
  The returned dependencies will be made relative to |relative_path| and
  normalized.
  :raises IOError: if the manifest could not be read.
  """
  dependency_list = []
  with open(manifest) as f:
    for line in f.readlines():
      # Strip comments.
      dependency = line.partition('#')[0].strip()
      # Ignore empty strings.
      if dependency:
        dependency = os.path.normpath(os.path.join(relative_path, dependency))
        dependency_list.append(dependency)
  return dependency_list

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
  parser.add_argument('--tar-contents-rooted-in',
                      help=('optional path prefix to use inside the resulting '
                            'tar file'),
                      required=False,
                      metavar='DIR')
  args = parser.parse_args()

  dependencies_path = args.build_dir
  if args.tar_contents_rooted_in:
    dependencies_path = args.tar_contents_rooted_in
  relative_path = os.path.relpath(args.build_dir, dependencies_path)

  dependency_list = ReadDependencies(args.manifest, relative_path)

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
        "-C", dependencies_path] + dependency_list
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
