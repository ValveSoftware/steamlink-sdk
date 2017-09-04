# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import fileinput
import glob
import optparse
import os
import shlex
import sys
import textwrap

def AggregateVectorIconsLegacy(working_directory, file_list, output_cc,
                               output_h):
  icon_list = []
  if file_list is None:
    # TODO(GYP_GONE): |file_list| is only None for GYP builds (where response
    # files are not supported), in which case we process all .icon files
    # contained within |working_directory| and all of its descendant
    # directories. This logic can be removed when GN is used everywhere.
    # See crbug.com/535386.
    for dirpath, dirnames, filenames in os.walk(working_directory):
      icon_list.extend(glob.glob(os.path.join(dirpath, "*.icon")))
  else:
    with open(file_list, 'r') as f:
      file_list_contents = f.read()
    icon_list = shlex.split(file_list_contents)

  input_header_template = open(os.path.join(working_directory,
                                            "vector_icons.h.template"))
  header_template_contents = input_header_template.readlines()
  input_header_template.close()
  output_header = open(output_h, "w")
  for line in header_template_contents:
    if not "TEMPLATE_PLACEHOLDER" in line:
      output_header.write(line)
      continue

    for icon_path in icon_list:
      # The icon name should be of the format "foo.icon" or "foo.1x.icon".
      (icon_name, extension) = os.path.splitext(os.path.basename(icon_path))
      (icon_name, scale_factor) = os.path.splitext(icon_name)
      if not scale_factor:
        output_header.write("  {},\n".format(icon_name.upper()))
  output_header.close()

  input_cc_template = open(
      os.path.join(working_directory, "vector_icons.cc.template"))
  cc_template_contents = input_cc_template.readlines()
  input_cc_template.close()
  output_cc = open(output_cc, "w")
  for line in cc_template_contents:
    if not "TEMPLATE_PLACEHOLDER" in line:
      output_cc.write(line)
      continue;

    for icon_path in icon_list:
      (icon_name, extension) = os.path.splitext(os.path.basename(icon_path))
      (icon_name, scale_factor) = os.path.splitext(icon_name)
      assert not scale_factor or scale_factor == ".1x"
      if (("1X" in line and scale_factor != ".1x") or
          (not "1X" in line and scale_factor == ".1x")):
        continue

      icon_file = open(icon_path)
      vector_commands = "".join(icon_file.readlines())
      icon_file.close()
      output_cc.write("ICON_TEMPLATE({}, {})\n".format(icon_name.upper(),
                                                       vector_commands))
  output_cc.close()


def Error(msg):
  print >> sys.stderr, msg
  sys.exit(1)


def CamelCase(name, suffix):
  words = name.split('_')
  words = [w.capitalize() for w in words]
  return 'k' + ''.join(words) + suffix


def AggregateVectorIcons(working_directory, file_list, output_cc, output_h,
                         use_legacy_template):
  """Compiles all .icon files in a directory into two C++ files.

  Args:
      working_directory: The path to the directory that holds the .icon files
          and C++ templates.
      file_list: A file containing the list of vector icon files to process.
          Used for GN only (this argument defaults to None for GYP).
      output_cc: The path that should be used to write the .cc file.
      output_h: The path that should be used to write the .h file.
      use_legacy_template: If True, |output_cc| and |output_h| are generated
          using .template files which make use of the VectorIconId enum.
  """

  # TODO(tdanderson): Remove this code once all vector icons map to VectorIcon
  # constants rather than VectorIconId values.
  if use_legacy_template:
    AggregateVectorIconsLegacy(working_directory, file_list, output_cc,
                               output_h)
    return

  # For each file in |file_list|, place it in |path_map| if its extension is
  # .icon or place it in |path_map_1x| if its extension is .1x.icon. The
  # two dictionaries map the icon's name to its path, e.g.,
  # path_map['cat'] = 'foo/bar/cat.icon'.
  icon_list = []
  with open(file_list, 'r') as f:
    file_list_contents = f.read()
  icon_list = shlex.split(file_list_contents)

  path_map = {}
  path_map_1x = {}

  for icon_path in icon_list:
    (icon_name, extension) = os.path.splitext(os.path.basename(icon_path))
    (icon_name, scale_factor) = os.path.splitext(icon_name)

    if (scale_factor and scale_factor != ".1x") or (extension != ".icon"):
      Error("Only filenames " + icon_name + ".icon or " + icon_name +
            ".1x.icon are allowed.")

    if not scale_factor and icon_name not in path_map:
      path_map[icon_name] = icon_path
    elif scale_factor and icon_name not in path_map_1x:
      path_map_1x[icon_name] = icon_path
    else:
      Error("A vector icon with name '" + icon_name + "' already exists.")

  for icon_1x in path_map_1x:
    if icon_1x not in path_map:
      Error("The file " + icon_1x + ".icon must be provided.")

  # Generate the file vector_icons.h which declares a variable for each
  # icon in |path_map|. The variable name is derived from the icon name by
  # converting to camel case, prepending 'k', and appending 'Icon'. For
  # example, the icon 'foo_bar' will have the variable name kFooBarIcon.
  input_header_template = open(os.path.join(working_directory,
                                            "vector_icons.h.template"))
  header_template_contents = input_header_template.readlines()
  input_header_template.close()

  output_header = open(output_h, "w")
  for line in header_template_contents:
    if not "TEMPLATE_PLACEHOLDER" in line:
      output_header.write(line)
      continue

    for icon in path_map:
      (icon_name, extension) = os.path.splitext(
                               os.path.basename(path_map[icon]))
      (icon_name, scale_factor) = os.path.splitext(icon_name)
      output_header.write(
          "VECTOR_ICON_TEMPLATE_H({})\n".format(CamelCase(icon_name, "Icon")))
  output_header.close()

  # Copy the vector icon drawing commands from the .icon and .1x.icon files
  # and use them to generate vector_icons.cc, which defines the variables
  # declared in vector_icons.h.
  input_cc_template = open(
      os.path.join(working_directory, "vector_icons.cc.template"))
  cc_template_contents = input_cc_template.readlines()
  input_cc_template.close()

  output_cc = open(output_cc, "w")
  for line in cc_template_contents:
    if not "TEMPLATE_PLACEHOLDER" in line:
      output_cc.write(line)
      continue;

    for icon in path_map:
      (icon_name, extension) = os.path.splitext(
                               os.path.basename(path_map[icon]))
      (icon_name, scale_factor) = os.path.splitext(icon_name)

      # Store the vector-drawing commands for foo_bar.icon in the temporary
      # variable kFooBarPath.
      icon_file = open(path_map[icon])
      vector_commands = "".join(icon_file.readlines())
      icon_file.close()
      output_cc.write("PATH_ELEMENT_TEMPLATE({}, {})\n".format(
          CamelCase(icon_name, "Path"), vector_commands))

      # Store the vector-drawing commands for foo_bar.1x.icon in the temporary
      # variable kFooBarPath1x, if the file exists.
      vector_commands_1x = None
      if (icon in path_map_1x):
        icon_file = open(path_map_1x[icon])
        vector_commands_1x = "".join(icon_file.readlines())
        icon_file.close()
        output_cc.write("PATH_ELEMENT_TEMPLATE({}, {})\n".format(
            CamelCase(icon_name, "Path1x"), vector_commands_1x))

      # Define the value of kFooBarIcon.
      third_arg = "nullptr" if vector_commands_1x is None else CamelCase(
                  icon_name, "Path1x")
      output_cc.write("VECTOR_ICON_TEMPLATE({}, {}, {})\n".format(CamelCase(
          icon_name, "Icon"), CamelCase(icon_name, "Path"), third_arg))

  output_cc.close()


def main():
  parser = optparse.OptionParser()
  parser.add_option("--working_directory",
                    help="The directory to look for template C++ as well as "
                         "icon files.")
  parser.add_option("--file_list",
                    help="A response file containing the list of icon files "
                         "to be processed (GN only). Defaults to None.",
                    default=None)
  parser.add_option("--output_cc",
                    help="The path to output the CC file to.")
  parser.add_option("--output_h",
                    help="The path to output the header file to.")
  parser.add_option("--use_legacy_template",
                    action="store_true",
                    help="When set, the VectorIconId enum is populated "
                         "with values corresponding to .icon files in "
                         "the current working directory.",
                    default=False)

  (options, args) = parser.parse_args()

  AggregateVectorIcons(options.working_directory,
                       options.file_list,
                       options.output_cc,
                       options.output_h,
                       options.use_legacy_template)


if __name__ == "__main__":
  main()
