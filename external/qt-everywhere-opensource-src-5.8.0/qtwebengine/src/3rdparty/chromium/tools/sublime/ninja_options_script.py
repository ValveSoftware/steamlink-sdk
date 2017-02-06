#!/usr/bin/python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Usage within SublimeClang:
#   "sublimeclang_options_script": "python
#       ${project_path}/src/tools/sublime/ninja_options_script.py \
#       ${project_path}/src \
#       ${project_path}/src/out/Debug"
#
# NOTE: ${project_path} expands to the directory of the Sublime project file,
# and SublimgClang passes the absolute file path to the current file as an
# additional argument.

import fnmatch
import logging
import os
import sys

# Change to an absolute reference if ninja is not on your path
path_to_ninja = 'ninja'

# Ninja file options to extract (add 'cflags' if you need the c flags too,
# although these usually break things)
ninja_file_options = ['defines', 'include_dirs', 'cflags_cc']

def merge_options_dicts(options_dict_1, options_dict_2):
  '''
  Given two dictionaries of options, returns one dictionary with both sets of
  options appended together.

  Both dictionaries must have the same options, even if some of them are empty
  lists.
  '''
  assert set(options_dict_1.keys()) == set(options_dict_2.keys()), \
      "Both options dicts must have the same keys"
  final_options_dict = {}
  for key in options_dict_1:
    final_options_dict[key] = options_dict_1[key] + options_dict_2[key]
  return final_options_dict

def extract_options_from_ninja_file(ninja_file_path, options_to_extract):
  '''
  Extracts the given options from the file at ninja_file_path and returns them
  as a dictionary.
  '''
  extracted_options = dict((o, []) for o in options_to_extract)
  for line in open(ninja_file_path):
    for option in options_to_extract:
      if line.strip().startswith(option):
        extracted_options[option] += line.split('=', 1)[1].split()
  return extracted_options

def find_ninja_file_options(ninja_root_path, relative_file_path_to_find,
                            options_to_extract):
  '''
  Returns a dictionary of the given extracted options for the ninja file for
  relative_file_path_to_find.

  The options are extracted from the first *.ninja file in ninja_root_path that
  contains relative_file_path_to_find. Otherwise, the first *.ninja file that
  contains relative_file_path_to_find without the file extension. Otherwise, the
  script walks up directories until it finds ninja files and then concatenates
  the found options from all of them.
  '''
  matches = []
  for root, dirnames, filenames in os.walk(ninja_root_path):
    for filename in fnmatch.filter(filenames, '*.ninja'):
      matches.append(os.path.join(root, filename))
  logging.debug("Found %d Ninja targets", len(matches))

  # First, look for a *.ninja file containing the full filename.
  for ninja_file in matches:
    for line in open(ninja_file):
      if relative_file_path_to_find in line:
        return extract_options_from_ninja_file(ninja_file, options_to_extract)

  # Next, look for a *.ninja file containing the basename (no extension).
  # This is a fix for header files with a corresponding cpp file.
  for ninja_file in matches:
    for line in open(ninja_file):
      if os.path.splitext(relative_file_path_to_find)[0] in line:
        all_options = extract_options_from_ninja_file(ninja_file,
                                                      options_to_extract)
        if all_options['include_dirs']:
          return all_options

  # Finally, open any *.ninja files in the directory or higher.
  current_path = os.path.join(ninja_root_path, 'obj',
                              os.path.dirname(relative_file_path_to_find))
  while current_path != ninja_root_path:
    if os.path.exists(current_path):
      matches = []
      for root, dirnames, filenames in os.walk(ninja_root_path):
        for filename in fnmatch.filter(filenames, '*.ninja'):
          matches.append(os.path.join(root, filename))
      logging.debug("Found %d Ninja targets", len(matches))

      matches = []
      for match in os.listdir(current_path):
        if match.endswith('.ninja'):
          matches.append(os.path.join(current_path, match))
      all_options = dict((o, []) for o in options_to_extract)
      for ninja_file in matches:
        all_options = merge_options_dicts(all_options,
            extract_options_from_ninja_file(ninja_file, options_to_extract))
      # As soon as we have some include_dirs from the ninja files, return.
      if all_options['include_dirs']:
        return all_options
    current_path = os.path.dirname(current_path)

  return None

project_path = sys.argv[1]
build_path = sys.argv[2]
file_path = sys.argv[3]

logging.debug("Compiling file %s\n", file_path)
# The file must be somewhere under the project folder...
if not file_path.lower().startswith(project_path.lower()):
  logging.error("File %s is not in current project folder %s\n",
                file_path, project_path)
  sys.exit(1)
file_relative_path = os.path.relpath(file_path, project_path)

# Look for a .ninja file that contains our current file, since the ninja
# project file name is needed to construct the full Ninja target path.
logging.debug("Searching for Ninja target")
options = find_ninja_file_options(build_path, file_relative_path,
                                  ninja_file_options)
if not options:
  logging.error("File %s is not in any Ninja file under %s",
                file_relative_path, build_path)
  sys.exit(2)

for option in ninja_file_options:
  for piece in options[option]:
    # Resolve relative includes
    if piece.startswith('-I'):
      print('-I' + os.path.join(build_path, piece[2:]))
    else:
      print(piece)
