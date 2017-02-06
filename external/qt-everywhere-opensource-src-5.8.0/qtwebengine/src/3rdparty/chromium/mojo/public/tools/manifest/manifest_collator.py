#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" A collator for Mojo Application Manifests """

import argparse
import json
import os
import shutil
import sys
import urlparse

eater_relative = '../../../../../tools/json_comment_eater'
eater_relative = os.path.join(os.path.abspath(__file__), eater_relative)
sys.path.insert(0, os.path.normpath(eater_relative))
try:
  import json_comment_eater
finally:
  sys.path.pop(0)

def ParseJSONFile(filename):
  with open(filename) as json_file:
    try:
      return json.loads(json_comment_eater.Nom(json_file.read()))
    except ValueError:
      print "%s is not a valid JSON document" % filename
      return None

def MergeDicts(left, right):
  for k, v in right.iteritems():
    if k not in left:
      left[k] = v
    else:
      if isinstance(v, dict):
        assert isinstance(left[k], dict)
        MergeDicts(left[k], v)
      elif isinstance(v, list):
        assert isinstance(left[k], list)
        left[k].extend(v)
      else:
        raise "Refusing to merge conflicting non-collection values."
  return left


def MergeBaseManifest(parent, base):
  MergeDicts(parent["capabilities"], base["capabilities"])

  if "applications" in base:
    if "applications" not in parent:
      parent["applications"] = []
    parent["applications"].extend(base["applications"])

  if "process-group" in base:
    parent["process-group"] = base["process-group"]


def main():
  parser = argparse.ArgumentParser(
      description="Collate Mojo application manifests.")
  parser.add_argument("--parent")
  parser.add_argument("--output")
  parser.add_argument("--application-name")
  parser.add_argument("--base-manifest", default=None)
  args, children = parser.parse_known_args()

  parent = ParseJSONFile(args.parent)
  if parent == None:
    return 1

  if args.base_manifest:
    base = ParseJSONFile(args.base_manifest)
    if base == None:
      return 1
    MergeBaseManifest(parent, base)

  app_path = parent['name'].split(':')[1]
  if app_path.startswith('//'):
    raise ValueError("Application name path component '%s' must not start " \
                     "with //" % app_path)

  if args.application_name != app_path:
    raise ValueError("Application name '%s' specified in build file does not " \
                     "match application name '%s' specified in manifest." %
                     (args.application_name, app_path))

  applications = []
  for child in children:
    application = ParseJSONFile(child)
    if application == None:
      return 1
    applications.append(application)

  if len(applications) > 0:
    parent['applications'] = applications

  with open(args.output, 'w') as output_file:
    json.dump(parent, output_file)

  return 0

if __name__ == "__main__":
  sys.exit(main())
