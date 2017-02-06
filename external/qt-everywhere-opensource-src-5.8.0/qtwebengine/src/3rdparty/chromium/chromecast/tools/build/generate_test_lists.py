#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper script to generate unit test lists for the Chromecast build scripts.
"""

import glob
import optparse
import sys


def CombineList(test_files_dir, list_output_file, include_filters,
                additional_runtime_options):
  """Writes a unit test file in a format compatible for Chromecast scripts.

  If include_filters is True, uses filters to create a test runner list
  and also include additional options, if any.
  Otherwise, creates a list only of the tests to build.

  Args:
    test_files_dir: Path to the intermediate directory containing tests/filters.
    list_output_file: Path to write the unit test file out to.
    include_filters: Whether or not to include the filters when generating
        the test list.
    additional_runtime_options: Arguments to be applied to all tests.  These are
        applied before filters (so test-specific filters take precedence).
  """

  # GYP targets may provide a numbered priority for the filename. Sort to
  # use that priority.
  test_files = sorted(glob.glob(test_files_dir + "/*.tests"))
  filter_files = sorted(glob.glob(test_files_dir + "/*.filters"))

  test_bin_set = set()
  for test_filename in test_files:
    with open(test_filename, "r") as test_file:
      for test_file_line in test_file:
        # Binary name may be a simple test target (cast_net_unittests) or be a
        # qualified gyp path (../base.gyp:base_unittests).
        test_binary_name = test_file_line.split(":")[-1].strip()
        test_bin_set.add(test_binary_name)

  test_filters = {}
  if include_filters:
    for filter_filename in filter_files:
      with open(filter_filename, "r") as filter_file:
        for filter_line in filter_file:
          (test_binary_name, filter) = filter_line.strip().split(" ", 1)

          if test_binary_name not in test_bin_set:
            raise Exception("Filter found for unknown target: " +
                test_binary_name)

          if test_binary_name in test_filters:
            test_filters[test_binary_name] += " " + filter
          else:
            test_filters[test_binary_name] = filter

  test_binaries = [
      binary + " " + (additional_runtime_options or "")
             + (" " + test_filters[binary] if binary in test_filters else "")
      for binary in test_bin_set]

  with open(list_output_file, "w") as f:
    f.write("\n".join(sorted(test_binaries)))


def CreateList(inputs, list_output_file):
  with open(list_output_file, "w") as f:
    f.write("\n".join(inputs))


def DoMain(argv):
  """Main method. Runs helper commands for generating unit test lists."""
  parser = optparse.OptionParser(
      """usage: %prog [<options>] <command> [<test names>]

      Valid commands:
          create_list       prints all given test names/args to a file, one line
                            per string
          pack_build        packs all test files from the given output directory
                            into a single test list file
          pack_run          packs all test and filter files from the given
                            output directory into a single test list file
      """)
  parser.add_option("-o", action="store", dest="list_output_file",
                    help="Output path in which to write the test list.")
  parser.add_option("-t", action="store", dest="test_files_dir",
                    help="Intermediate test list directory.")
  parser.add_option("-a", action="store", dest="additional_runtime_options",
                    help="Additional options applied to all tests.")
  options, inputs = parser.parse_args(argv)

  list_output_file = options.list_output_file
  test_files_dir = options.test_files_dir
  additional_runtime_options = options.additional_runtime_options

  if len(inputs) < 1:
    parser.error("No command given.\n")
  command = inputs[0]
  test_names = inputs[1:]

  if not list_output_file:
    parser.error("Output path (-o) is required.\n")

  if command == "create_list":
    return CreateList(test_names, list_output_file)

  if command == "pack_build":
    if not test_files_dir:
      parser.error("pack_build require a test files directory (-t).\n")
    return CombineList(test_files_dir, list_output_file, False, None)

  if command == "pack_run":
    if not test_files_dir:
      parser.error("pack_run require a test files directory (-t).\n")
    return CombineList(test_files_dir, list_output_file, True,
                       additional_runtime_options)

  parser.error("Invalid command specified.")


if __name__ == "__main__":
  DoMain(sys.argv[1:])
