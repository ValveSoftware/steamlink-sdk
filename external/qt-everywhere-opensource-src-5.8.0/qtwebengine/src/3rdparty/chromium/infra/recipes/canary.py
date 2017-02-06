# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A recipe that just checks out chromium, for canarying src-side recipes."""

DEPS = [
  'recipe_engine/properties',
  'depot_tools/bot_update',
  'depot_tools/gclient',
]

def RunSteps(api):
  api.gclient.set_config('chromium')
  api.bot_update.ensure_checkout(force=True)

def GenTests(api):
  yield api.test('basic') + api.properties.generic()
