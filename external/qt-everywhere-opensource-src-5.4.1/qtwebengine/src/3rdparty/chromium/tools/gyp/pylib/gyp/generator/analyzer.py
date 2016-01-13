# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This script is intended for use as a GYP_GENERATOR. It takes as input (by way of
the generator flag file_path) the list of relative file paths to consider. If
any target has at least one of the paths as a source (or input to an action or
rule) then 'Found dependency' is output, otherwise 'No dependencies' is output.
"""

import gyp.common
import gyp.ninja_syntax as ninja_syntax
import os
import posixpath

generator_supports_multiple_toolsets = True

generator_wants_static_library_dependencies_adjusted = False

generator_default_variables = {
}
for dirname in ['INTERMEDIATE_DIR', 'SHARED_INTERMEDIATE_DIR', 'PRODUCT_DIR',
                'LIB_DIR', 'SHARED_LIB_DIR']:
  generator_default_variables[dirname] = '!!!'

for unused in ['RULE_INPUT_PATH', 'RULE_INPUT_ROOT', 'RULE_INPUT_NAME',
               'RULE_INPUT_DIRNAME', 'RULE_INPUT_EXT',
               'EXECUTABLE_PREFIX', 'EXECUTABLE_SUFFIX',
               'STATIC_LIB_PREFIX', 'STATIC_LIB_SUFFIX',
               'SHARED_LIB_PREFIX', 'SHARED_LIB_SUFFIX',
               'CONFIGURATION_NAME']:
  generator_default_variables[unused] = ''

def __MakeRelativeTargetName(path):
  """Converts a gyp target name into a relative name. For example, the path to a
  gyp file may be something like c:\foo\bar.gyp:target, this converts it to
  bar.gyp.
  """
  prune_path = os.getcwd()
  if path.startswith(prune_path):
    path = path[len(prune_path):]
  # Gyp paths are always posix style.
  path = path.replace('\\', '/')
  if path.endswith('#target'):
    path = path[0:len(path) - len('#target')]
  return path

def __ExtractBasePath(target):
  """Extracts the path components of the specified gyp target path."""
  last_index = target.rfind('/')
  if last_index == -1:
    return ''
  return target[0:(last_index + 1)]

def __AddSources(sources, base_path, base_path_components, result):
  """Extracts valid sources from |sources| and adds them to |result|. Each
  source file is relative to |base_path|, but may contain '..'. To make
  resolving '..' easier |base_path_components| contains each of the
  directories in |base_path|. Additionally each source may contain variables.
  Such sources are ignored as it is assumed dependencies on them are expressed
  and tracked in some other means."""
  # NOTE: gyp paths are always posix style.
  for source in sources:
    if not len(source) or source.startswith('!!!') or source.startswith('$'):
      continue
    # variable expansion may lead to //.
    source = source[0] + source[1:].replace('//', '/')
    if source.startswith('../'):
      path_components = base_path_components[:]
      # Resolve relative paths.
      while source.startswith('../'):
        path_components.pop(len(path_components) - 1)
        source = source[3:]
      result.append('/'.join(path_components) + source)
      continue
    result.append(base_path + source)

def __ExtractSourcesFromAction(action, base_path, base_path_components,
                               results):
  if 'inputs' in action:
    __AddSources(action['inputs'], base_path, base_path_components, results)

def __ExtractSources(target, target_dict):
  base_path = posixpath.dirname(target)
  base_path_components = base_path.split('/')
  # Add a trailing '/' so that __AddSources() can easily build paths.
  if len(base_path):
    base_path += '/'
  results = []
  if 'sources' in target_dict:
    __AddSources(target_dict['sources'], base_path, base_path_components,
                 results)
  # Include the inputs from any actions. Any changes to these effect the
  # resulting output.
  if 'actions' in target_dict:
    for action in target_dict['actions']:
      __ExtractSourcesFromAction(action, base_path, base_path_components,
                                 results)
  if 'rules' in target_dict:
    for rule in target_dict['rules']:
      __ExtractSourcesFromAction(rule, base_path, base_path_components, results)

  return results

class Target(object):
  """Holds information about a particular target:
  sources: set of source files defined by this target. This includes inputs to
           actions and rules.
  deps: list of direct dependencies."""
  def __init__(self):
    self.sources = []
    self.deps = []

def __GenerateTargets(target_list, target_dicts):
  """Generates a dictionary with the key the name of a target and the value a
  Target."""
  targets = {}

  # Queue of targets to visit.
  targets_to_visit = target_list[:]

  while len(targets_to_visit) > 0:
    absolute_target_name = targets_to_visit.pop()
    # |absolute_target| may be an absolute path and may include #target.
    # References to targets are relative, so we need to clean the name.
    relative_target_name = __MakeRelativeTargetName(absolute_target_name)
    if relative_target_name in targets:
      continue

    target = Target()
    targets[relative_target_name] = target
    target.sources.extend(__ExtractSources(relative_target_name,
                                           target_dicts[absolute_target_name]))

    for dep in target_dicts[absolute_target_name].get('dependencies', []):
      targets[relative_target_name].deps.append(__MakeRelativeTargetName(dep))
      targets_to_visit.append(dep)

  return targets

def __GetFiles(params):
  """Returns the list of files to analyze, or None if none specified."""
  generator_flags = params.get('generator_flags', {})
  file_path = generator_flags.get('file_path', None)
  if not file_path:
    return None
  try:
    f = open(file_path, 'r')
    result = []
    for file_name in f:
      if file_name.endswith('\n'):
        file_name = file_name[0:len(file_name) - 1]
      if len(file_name):
        result.append(file_name)
    f.close()
    return result
  except IOError:
    print 'Unable to open file', file_path
  return None

def CalculateVariables(default_variables, params):
  """Calculate additional variables for use in the build (called by gyp)."""
  flavor = gyp.common.GetFlavor(params)
  if flavor == 'mac':
    default_variables.setdefault('OS', 'mac')
  elif flavor == 'win':
    default_variables.setdefault('OS', 'win')
  else:
    operating_system = flavor
    if flavor == 'android':
      operating_system = 'linux'  # Keep this legacy behavior for now.
    default_variables.setdefault('OS', operating_system)

def GenerateOutput(target_list, target_dicts, data, params):
  """Called by gyp as the final stage. Outputs results."""
  files = __GetFiles(params)
  if not files:
    print 'Must specify files to analyze via file_path generator flag'
    return

  targets = __GenerateTargets(target_list, target_dicts)

  files_set = frozenset(files)
  found_in_all_sources = 0
  for target_name, target in targets.iteritems():
    sources = files_set.intersection(target.sources)
    if len(sources):
      print 'Found dependency'
      return

  print 'No dependencies'
