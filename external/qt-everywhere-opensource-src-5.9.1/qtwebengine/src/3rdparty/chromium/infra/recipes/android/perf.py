# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from recipe_engine.types import freeze


DEPS = [
    'build/adb',
    'build/chromium',
    'build/chromium_android',
    'build/chromium_tests',
    'build/test_utils',
    'depot_tools/bot_update',
    'depot_tools/gclient',
    'recipe_engine/json',
    'recipe_engine/path',
    'recipe_engine/properties',
    'recipe_engine/python',
    'recipe_engine/step',
]


REPO_URL = 'https://chromium.googlesource.com/chromium/src.git'

def _CreateTestSpec(name, perf_id, required_apks, num_device_shards=1,
                    num_host_shards=1, target_bits=64,
                    browser_name=None, remove_system_webview=False,
                    enable_platform_mode=False):
  def _CreateShardTestSpec(name, perf_id, required_apks, num_device_shards,
                           num_host_shards, shard_index, target_bits,
                           browser_name, remove_system_webview,
                           enable_platform_mode):
    spec = {
      'perf_id': perf_id,
      'required_apks': required_apks,
      'bucket': 'chrome-perf',
      'num_device_shards': num_device_shards,
      'num_host_shards': num_host_shards,
      'shard_index': shard_index,
      'test_spec_file': 'chromium.perf.json',
      'max_battery_temp': 350,
      'known_devices_file': '.known_devices',
      'browser_name': browser_name,
      'remove_system_webview': remove_system_webview,
      'enable_platform_mode': enable_platform_mode,
    }
    if target_bits == 32:
      builder_name = 'Android Builder'
    elif target_bits == 64:
      builder_name = 'Android arm64 Builder'
      spec['recipe_config'] = 'tests_arm64'
    spec['path'] = lambda api: '%s/full-build-linux_%s.zip' % (
        builder_name, api.properties['parent_revision'])
    return spec

  tester_spec = {}
  for shard_index in xrange(num_host_shards):
    builder_name = '%s (%d)' % (name, shard_index + 1)
    tester_spec[builder_name] = _CreateShardTestSpec(
        name, perf_id, required_apks, num_device_shards, num_host_shards,
        shard_index, target_bits, browser_name, remove_system_webview,
        enable_platform_mode)
  return tester_spec

def _ChromiumPerfTesters():
  testers = [
    _CreateTestSpec('Android Galaxy S5 Perf', 'android-galaxy-s5',
        required_apks=['ChromePublic.apk'], num_device_shards=7,
        num_host_shards=3, target_bits=32, enable_platform_mode=True),
    _CreateTestSpec('Android Nexus5 Perf', 'android-nexus5',
        required_apks=['ChromePublic.apk'], num_device_shards=7,
        num_host_shards=3, target_bits=32, enable_platform_mode=True),
    _CreateTestSpec('Android Nexus5X Perf', 'android-nexus5X',
        required_apks=['ChromePublic.apk'], num_device_shards=7,
        num_host_shards=3, enable_platform_mode=True),
    _CreateTestSpec('Android Nexus6 Perf', 'android-nexus6',
        required_apks=['ChromePublic.apk'], num_device_shards=7,
        num_host_shards=3, target_bits=32),
    _CreateTestSpec('Android Nexus7v2 Perf', 'android-nexus7v2',
        required_apks=['ChromePublic.apk'], num_device_shards=7,
        num_host_shards=3, target_bits=32, enable_platform_mode=True),
    _CreateTestSpec('Android Nexus9 Perf', 'android-nexus9',
        required_apks=['ChromePublic.apk'], num_device_shards=7,
        num_host_shards=3),
    _CreateTestSpec('Android One Perf', 'android-one',
        required_apks=['ChromePublic.apk'], num_device_shards=7,
        num_host_shards=3, target_bits=32),
    _CreateTestSpec('Android Nexus5X WebView Perf', 'android-webview-nexus5X',
        required_apks=['SystemWebView.apk', 'SystemWebViewShell.apk'],
        num_device_shards=7, num_host_shards=3, target_bits=64,
        browser_name='android-webview', remove_system_webview=True),
    _CreateTestSpec('Android Nexus6 WebView Perf', 'android-webview-nexus6',
        required_apks=['SystemWebView.apk', 'SystemWebViewShell.apk'],
        num_device_shards=6, num_host_shards=3, target_bits=32,
        browser_name='android-webview', remove_system_webview=True),
  ]
  master_spec = {}
  for spec in testers:
    master_spec.update(spec)
  return master_spec

def _ChromiumPerfFyiTesters():
  testers = [
    _CreateTestSpec('Android Power Nexus 5X Perf', 'android-power-nexus-5x',
        required_apks=['ChromePublic.apk'], num_device_shards=7,
        num_host_shards=1),
  ]

  master_spec = {}
  for spec in testers:
    master_spec.update(spec)
  return master_spec

BUILDERS = freeze({
  'chromium.perf': _ChromiumPerfTesters(),
  'chromium.perf.fyi': _ChromiumPerfFyiTesters(),
})


def RunSteps(api):
  mastername = api.properties['mastername']
  buildername = api.properties['buildername']
  # TODO(akuegel): Move the configs in builders.py in chromium_tests to this
  # recipe, and get rid of duplications.
  builder = dict(BUILDERS[mastername][buildername])
  builder_config = builder.get('recipe_config', 'base_config')
  kwargs = {
    'REPO_NAME':'src',
    'REPO_URL':REPO_URL,
    'INTERNAL':False,
    'BUILD_CONFIG':'Release',
    'TARGET_PLATFORM':'android',
  }

  api.chromium_android.set_config(builder_config, **kwargs)
  api.chromium.set_config(builder_config, **kwargs)
  api.gclient.set_config('perf')
  api.gclient.apply_config('android')

  bot_update_step = api.bot_update.ensure_checkout()

  test_spec_file = builder.get('test_spec_file')
  test_spec = {}
  if test_spec_file:
    test_spec = api.chromium_tests.read_test_spec(api, test_spec_file)

    scripts_compile_targets = \
        api.chromium_tests.get_compile_targets_for_scripts()

    builder['tests'] = api.chromium_tests.generate_tests_from_test_spec(
        api, test_spec, builder, buildername, mastername, False, None,
        scripts_compile_targets, [api.chromium_tests.steps.generate_script],
        bot_update_step)

  api.path['checkout'] = api.path['slave_build'].join('src')
  api.chromium_android.clean_local_files()

  # TODO(jbudorick): Remove this after resolving
  # https://github.com/catapult-project/catapult/issues/2901
  devil_path = api.path['checkout'].join('third_party', 'catapult', 'devil')
  api.python.inline(
      'initialize devil',
      """
      import sys
      sys.path.append(sys.argv[1])
      from devil import devil_env
      devil_env.config.Initialize()
      devil_env.config.PrefetchPaths(dependencies=['adb'])
      """,
      args=[devil_path])
  api.adb.set_adb_path(
      devil_path.join('bin', 'deps', 'linux2', 'x86_64', 'bin', 'adb'))

  api.chromium_android.download_build(bucket=builder['bucket'],
    path=builder['path'](api))

  api.chromium_android.common_tests_setup_steps(
      perf_setup=True,
      remove_system_webview=builder.get('remove_system_webview', False))

  required_apks = builder.get('required_apks', [])
  for apk in required_apks:
    api.chromium_android.adb_install_apk(apk)

  api.chromium_android.host_info(
      args=api.chromium_tests.get_common_args_for_scripts())

  test_runner = api.chromium_tests.create_test_runner(
      api, builder.get('tests', []))

  try:
    failures = []
    if test_runner:
      try:
        test_runner()
      except api.step.StepFailure as f:
        failures.append(f)

    dynamic_perf_tests = api.chromium_tests.steps.DynamicPerfTests(
        builder['perf_id'], 'android', None,
        max_battery_temp=builder.get('max_battery_temp'),
        num_device_shards=builder['num_device_shards'],
        num_host_shards=builder.get('num_host_shards', 1),
        shard_index=builder.get('shard_index', 0),
        override_browser_name=builder.get('browser_name'),
        enable_platform_mode=builder.get('enable_platform_mode'))
    dynamic_perf_tests.run(api, None)

    if failures:
      raise api.step.StepFailure('src-side perf tests failed %s' % failures)
  finally:
    api.chromium_android.common_tests_final_steps(
        logcat_gs_bucket='chromium-android')


def _sanitize_nonalpha(text):
  return ''.join(c if c.isalnum() else '_' for c in text)


def GenTests(api):
  for mastername, builders in BUILDERS.iteritems():
    for buildername in builders:
      yield (
          api.test('full_%s_%s' % (_sanitize_nonalpha(mastername),
                                   _sanitize_nonalpha(buildername))) +
          api.properties.generic(
              path_config='kitchen',
              repo_name='src',
              repo_url=REPO_URL,
              mastername=mastername,
              buildername=buildername,
              parent_buildername='parent_buildername',
              parent_buildnumber='1729',
              parent_revision='deadbeef',
              revision='deadbeef',
              slavename='slavename',
              target='Release'))
  yield (api.test('provision_devices') +
      api.properties.generic(
          path_config='kitchen',
          repo_name='src',
              repo_url=REPO_URL,
              mastername='chromium.perf',
              buildername='Android Nexus5 Perf (1)',
              parent_buildername='parent_buildername',
              parent_buildnumber='1729',
              parent_revision='deadbeef',
              revision='deadbeef',
              slavename='slavename',
              target='Release')
      + api.step_data('provision_devices', retcode=1))
  yield (api.test('get_perf_test_list_old_data') +
      api.properties.generic(
          path_config='kitchen',
          repo_name='src',
          repo_url=REPO_URL,
          mastername='chromium.perf',
          buildername='Android Nexus5 Perf (1)',
          parent_buildername='parent_buildername',
          parent_buildnumber='1729',
          parent_revision='deadbeef',
          revision='deadbeef',
          slavename='slavename',
          target='Release') +
      api.override_step_data(
        'get perf test list',
        api.json.output(['perf_test.foo', 'page_cycler.foo'])))
  yield (api.test('src_side_script_fails') +
      api.properties.generic(
          path_config='kitchen',
          repo_name='src',
          repo_url=REPO_URL,
          mastername='chromium.perf',
          buildername='Android Nexus5 Perf (1)',
          parent_buildername='parent_buildername',
          parent_buildnumber='1729',
          parent_revision='deadbeef',
          revision='deadbeef',
          slavename='slavename',
          target='Release') +
      api.override_step_data(
        'read test spec (chromium.perf.json)',
        api.json.output({
            "Android Nexus5 Perf (1)": {
              "scripts": [
                {
                  "name": "host_info",
                  "script": "host_info.py"
                }]}})) +
      api.step_data('host_info', retcode=1))
  yield (api.test('test_failure') +
      api.properties.generic(
          path_config='kitchen',
          repo_name='src',
          repo_url=REPO_URL,
          mastername='chromium.perf',
          buildername='Android Nexus5 Perf (1)',
          parent_buildername='parent_buildername',
          parent_buildnumber='1729',
          parent_revision='deadbeef',
          revision='deadbeef',
          slavename='slavename',
          target='Release') +
      api.override_step_data(
          'perf_test.foo', retcode=1))
  yield (api.test('missing_device') +
      api.properties.generic(
          path_config='kitchen',
          repo_name='src',
          repo_url=REPO_URL,
          mastername='chromium.perf',
          buildername='Android Nexus5 Perf (1)',
          parent_buildername='parent_buildername',
          parent_buildnumber='1729',
          parent_revision='deadbeef',
          revision='deadbeef',
          slavename='slavename',
          target='Release') +
      api.override_step_data(
          'perf_test.foo', retcode=87))
  yield (api.test('host_info_failure') +
      api.properties.generic(
          path_config='kitchen',
          repo_name='src',
              repo_url=REPO_URL,
              mastername='chromium.perf',
              buildername='Android Nexus5 Perf (1)',
              parent_buildername='parent_buildername',
              parent_buildnumber='1729',
              parent_revision='deadbeef',
              revision='deadbeef',
              slavename='slavename',
              target='Release') +
      api.override_step_data(
          'Host Info',
          api.json.output({
                'valid': True,
                'failures': ['Failure A', 'Failure B'],
                '_host_info': {
                    'os_system': 'os_system',
                    'os_release': 'os_release',
                    'processor': 'processor',
                    'num_cpus': 'num_cpus',
                    'free_disk_space': 'free_disk_space',
                    'python_version': 'python_version',
                    'python_path': 'python_path',
                    'devices': [{
                        "usb_status": True,
                        "blacklisted": None,
                        "ro.build.fingerprint": "fingerprint",
                        "battery": {
                            "status": "5",
                            "scale": "100",
                            "temperature": "240",
                            "level": "100",
                            "technology": "Li-ion",
                            "AC powered": "false",
                            "health": "2",
                            "voltage": "4302",
                            "Wireless powered": "false",
                            "USB powered": "true",
                            "Max charging current": "500000",
                            "present": "true"
                        },
                       "adb_status": "device",
                       "imei_slice": "",
                       "ro.build.product": "bullhead",
                       "ro.build.id": "MDB08Q",
                       "serial": "00d0d567893340f4",
                       "wifi_ip": ""
                    }]
                }}),
          retcode=87))

