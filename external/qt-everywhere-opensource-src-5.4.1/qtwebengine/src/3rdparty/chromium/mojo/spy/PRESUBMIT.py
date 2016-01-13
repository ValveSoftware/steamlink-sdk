# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import sys

def _CommonChecks(input_api, output_api):
  results = []
  # Importing ui actually brings tvcm into the path.
  import ui
  from tvcm import presubmit_checker
  checker = presubmit_checker.PresubmitChecker(input_api, output_api)
  results += checker.RunChecks()
  return results

def GetPathsToPrepend(input_api):
  return [input_api.PresubmitLocalPath()]

def RunWithPrependedPath(prepended_path, fn, *args):
  old_path = sys.path

  try:
    sys.path = prepended_path + old_path
    return fn(*args)
  finally:
    sys.path = old_path

def CheckChangeOnUpload(input_api, output_api):
  def go():
    results = []
    results.extend(_CommonChecks(input_api, output_api))
    return results
  return RunWithPrependedPath(GetPathsToPrepend(input_api), go)

def CheckChangeOnCommit(input_api, output_api):
  def go():
    results = []
    results.extend(_CommonChecks(input_api, output_api))
    return results
  return RunWithPrependedPath(GetPathsToPrepend(input_api), go)
