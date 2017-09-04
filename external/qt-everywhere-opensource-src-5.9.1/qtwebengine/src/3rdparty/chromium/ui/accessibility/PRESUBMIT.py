# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for ui/accessibility."""

import os, re

AX_IDL = 'ui/accessibility/ax_enums.idl'
AUTOMATION_IDL = 'chrome/common/extensions/api/automation.idl'

def InitialLowerCamelCase(unix_name):
  words = unix_name.split('_')
  return words[0] + ''.join(word.capitalize() for word in words[1:])

# Given a full path to an IDL file containing enum definitions,
# parse the file for enums and return a dict mapping the enum name
# to a list of values for that enum.
def GetEnumsFromFile(fullpath):
  enum_name = None
  enums = {}
  for line in open(fullpath).readlines():
    # Strip out comments
    line = re.sub('//.*', '', line)

    # Look for lines of the form "enum ENUM_NAME {" and get the enum_name
    m = re.search('enum ([\w]+) {', line)
    if m:
      enum_name = m.group(1)
      continue

    # Look for a "}" character signifying the end of an enum
    if line.find('}') >= 0:
      enum_name = None
      continue

    if not enum_name:
      continue

    # If we're inside an enum definition, add the first string consisting of
    # alphanumerics plus underscore ("\w") to the list of values for that enum.
    m = re.search('([\w]+)', line)
    if m:
      enums.setdefault(enum_name, [])
      enums[enum_name].append(m.group(1))

  return enums

def CheckMatchingEnum(ax_enums,
                      ax_enum_name,
                      automation_enums,
                      automation_enum_name,
                      errs,
                      output_api):
  if ax_enum_name not in ax_enums:
    errs.append(output_api.PresubmitError(
        'Expected %s to have an enum named %s' % (AX_IDL, ax_enum_name)))
    return
  if automation_enum_name not in automation_enums:
    errs.append(output_api.PresubmitError(
        'Expected %s to have an enum named %s' % (
            AUTOMATION_IDL, automation_enum_name)))
    return
  src = ax_enums[ax_enum_name]
  dst = automation_enums[automation_enum_name]
  for value in src:
    if InitialLowerCamelCase(value) not in dst:
      errs.append(output_api.PresubmitError(
          'Found %s.%s in %s, but did not find %s.%s in %s' % (
              ax_enum_name, value, AX_IDL,
              automation_enum_name, InitialLowerCamelCase(value),
              AUTOMATION_IDL)))

def CheckEnumsMatch(input_api, output_api):
  repo_root = input_api.change.RepositoryRoot()
  ax_enums = GetEnumsFromFile(os.path.join(repo_root, AX_IDL))
  automation_enums = GetEnumsFromFile(os.path.join(repo_root, AUTOMATION_IDL))
  errs = []
  CheckMatchingEnum(ax_enums, 'AXRole', automation_enums, 'RoleType', errs,
                    output_api)
  CheckMatchingEnum(ax_enums, 'AXState', automation_enums, 'StateType', errs,
                    output_api)
  CheckMatchingEnum(ax_enums, 'AXEvent', automation_enums, 'EventType', errs,
                    output_api)
  return errs

def CheckChangeOnUpload(input_api, output_api):
  if AX_IDL not in input_api.LocalPaths():
    return []
  return CheckEnumsMatch(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  if AX_IDL not in input_api.LocalPaths():
    return []
  return CheckEnumsMatch(input_api, output_api)
