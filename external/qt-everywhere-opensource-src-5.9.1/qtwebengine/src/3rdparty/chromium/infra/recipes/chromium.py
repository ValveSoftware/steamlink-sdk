# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

DEPS = [
  'build/adb',
  'build/bisect_tester',
  'build/build',
  'build/chromium',
  'build/chromium_android',
  'build/chromium_swarming',
  'build/chromium_tests',
  'build/commit_position',
  'build/file',
  'build/gsutil',
  'build/isolate',
  'build/swarming',
  'build/test_results',
  'build/test_utils',
  'depot_tools/bot_update',
  'recipe_engine/json',
  'recipe_engine/path',
  'recipe_engine/platform',
  'recipe_engine/properties',
  'recipe_engine/python',
  'recipe_engine/raw_io',
  'recipe_engine/step',
  'recipe_engine/tempfile',
]


from recipe_engine import config_types


def RunSteps(api):
  api.chromium_tests.main_waterfall_steps(api)


def GenTests(api):
  yield (
    api.test('builder') +
    api.properties.generic(
        mastername='chromium.fyi',
        buildername='Linux remote_run Builder',
        path_config='kitchen')
  )

  yield (
    api.test('tester') +
    api.properties.generic(
        mastername='chromium.fyi',
        buildername='Linux remote_run Tester',
        parent_buildername='Linux remote_run Builder',
        path_config='kitchen')
  )
