# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    # Intermediate target grouping the android tools needed to run native
    # unittests and instrumentation test apks.
    {
      # GN: //tools/android:android_tools
      'target_name': 'android_tools',
      'type': 'none',
      'dependencies': [
        'adb_reboot/adb_reboot.gyp:adb_reboot',
        'file_poller/file_poller.gyp:file_poller',
        'forwarder2/forwarder.gyp:forwarder2',
        'md5sum/md5sum.gyp:md5sum',
        'memtrack_helper/memtrack_helper.gyp:memtrack_helper',
        'purge_ashmem/purge_ashmem.gyp:purge_ashmem',
        '../../third_party/catapult/telemetry/telemetry.gyp:*#host',
      ],
    },
    {
      # GN: //tools/android:memdump
      'target_name': 'memdump',
      'type': 'none',
      'dependencies': [
        'memdump/memdump.gyp:memdump',
      ],
    },
    {
      # GN: //tools/android:memconsumer
      'target_name': 'memconsumer',
      'type': 'none',
      'dependencies': [
        'memconsumer/memconsumer.gyp:memconsumer',
      ],
    },
    {
      # GN: //tools/android:memtrack_helper
      'target_name': 'memtrack_helper',
      'type': 'none',
      'dependencies': [
        'memtrack_helper/memtrack_helper.gyp:memtrack_helper',
      ],
    },
    {
      # GN: //tools/android:ps_ext
      'target_name': 'ps_ext',
      'type': 'none',
      'dependencies': [
        'ps_ext/ps_ext.gyp:ps_ext',
      ],
    },
    {
      # GN: //tools/android:spnego_authenticator
      'target_name': 'spnego_authenticator',
      'type': 'none',
      'dependencies': [
        'kerberos/kerberos.gyp:spnego_authenticator_apk',
      ],
    },
    {
      # GN: //tools/android:customtabs_benchmark
      'target_name': 'customtabs_benchmark',
      'type': 'none',
      'dependencies': [
        'customtabs_benchmark/customtabs_benchmark.gyp:customtabs_benchmark_apk',
      ],
    },
    {
      # GN: //tools/android:audio_focus_grabber
      'target_name': 'audio_focus_grabber',
      'type': 'none',
      'dependencies': [
        'audio_focus_grabber/audio_focus_grabber.gyp:audio_focus_grabber_apk',
      ],
    },
    {
      # GN: //tools/android:push_apps_to_background
      'target_name': 'push_apps_to_background',
      'type': 'none',
      'dependencies': [
        'push_apps_to_background/push_apps_to_background.gyp:push_apps_to_background_apk',
      ],
    },
  ],
}
