# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

DEPS = [
  'build/build',
  'build/chromium',
  'build/chromium_android',
  'build/chromium_checkout',
  'build/chromium_swarming',
  'build/chromium_tests',
  'build/commit_position',
  'build/file',
  'build/filter',
  'build/isolate',
  'build/swarming',
  'build/test_results',
  'build/test_utils',
  'depot_tools/bot_update',
  'depot_tools/gclient',
  'depot_tools/tryserver',
  'recipe_engine/json',
  'recipe_engine/path',
  'recipe_engine/platform',
  'recipe_engine/properties',
  'recipe_engine/python',
  'recipe_engine/raw_io',
  'recipe_engine/step',
]


def RunSteps(api):
  return api.chromium_tests.trybot_steps(api)


def GenTests(api):
  yield (
    api.test('basic') +
    api.platform('linux', 64) +
    api.properties.tryserver(
        path_config='kitchen',
        mastername='tryserver.chromium.linux',
        buildername='linux_chromium_rel_ng')
  )
