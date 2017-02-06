# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/memory_pressure
      'target_name': 'memory_pressure',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'memory_pressure/direct_memory_pressure_calculator.h',
        'memory_pressure/direct_memory_pressure_calculator_linux.cc',
        'memory_pressure/direct_memory_pressure_calculator_linux.h',
        'memory_pressure/direct_memory_pressure_calculator_win.cc',
        'memory_pressure/direct_memory_pressure_calculator_win.h',
        'memory_pressure/filtered_memory_pressure_calculator.cc',
        'memory_pressure/filtered_memory_pressure_calculator.h',
        'memory_pressure/memory_pressure_calculator.h',
        'memory_pressure/memory_pressure_listener.cc',
        'memory_pressure/memory_pressure_listener.h',
        'memory_pressure/memory_pressure_monitor.cc',
        'memory_pressure/memory_pressure_monitor.h',
        'memory_pressure/memory_pressure_stats_collector.cc',
        'memory_pressure/memory_pressure_stats_collector.h',
      ],
    },
  ],
}
