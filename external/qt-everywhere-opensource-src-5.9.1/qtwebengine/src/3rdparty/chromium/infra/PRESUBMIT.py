# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def CommonChecks(input_api, output_api):
  results = []

  results.extend(input_api.RunTests(
      input_api.canned_checks.GetUnitTestsInDirectory(
          input_api,
          output_api,
          input_api.os_path.join('unittests'),
          whitelist=[r'.+_test\.py'])))

  return results


CheckChangeOnUpload = CommonChecks
CheckChangeOnCommit = CommonChecks
