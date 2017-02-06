#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file isn't officially supported by the Chromium project. It's maintained
# on a best-effort basis by volunteers, so some things may be broken from time
# to time. If you encounter errors, it's most often due to files in base that
# have been added or moved since somebody last tried this script. Generally
# such errors are easy to diagnose.

"""Bootstraps gn.

It is done by first building it manually in a temporary directory, then building
it with its own BUILD.gn to the final destination.
"""

import contextlib
import errno
import logging
import optparse
import os
import shutil
import subprocess
import sys
import tempfile

BOOTSTRAP_DIR = os.path.dirname(os.path.abspath(__file__))
GN_ROOT = os.path.dirname(BOOTSTRAP_DIR)
SRC_ROOT = os.path.dirname(os.path.dirname(GN_ROOT))

is_linux = sys.platform.startswith('linux')
is_mac = sys.platform.startswith('darwin')
is_posix = is_linux or is_mac

def check_call(cmd, **kwargs):
  logging.debug('Running: %s', ' '.join(cmd))
  subprocess.check_call(cmd, cwd=GN_ROOT, **kwargs)

def mkdir_p(path):
  try:
    os.makedirs(path)
  except OSError as e:
    if e.errno == errno.EEXIST and os.path.isdir(path):
      pass
    else: raise

@contextlib.contextmanager
def scoped_tempdir():
  path = tempfile.mkdtemp()
  try:
    yield path
  finally:
    shutil.rmtree(path)


def run_build(tempdir, options):
  if options.debug:
    build_rel = os.path.join('out', 'Debug')
  else:
    build_rel = os.path.join('out', 'Release')
  build_root = os.path.join(SRC_ROOT, build_rel)
  if options.shadow:
    build_root = os.getcwd()

  print 'Building gn manually in a temporary directory for bootstrapping...'
  build_gn_with_ninja_manually(tempdir, options)
  temp_gn = os.path.join(tempdir, 'gn')
  out_gn = os.path.join(build_root, 'gn')

  if options.no_rebuild:
    mkdir_p(build_root)
    shutil.copy2(temp_gn, out_gn)
  else:
    print 'Building gn using itself to %s...' % build_rel
    build_gn_with_gn(temp_gn, build_root, options)

  if options.output:
    # Preserve the executable permission bit.
    shutil.copy2(out_gn, options.output)


def main(argv):
  parser = optparse.OptionParser(description=sys.modules[__name__].__doc__)
  parser.add_option('-d', '--debug', action='store_true',
                    help='Do a debug build. Defaults to release build.')
  parser.add_option('-o', '--output',
                    help='place output in PATH', metavar='PATH')
  parser.add_option('-n', '--no-rebuild', action='store_true',
                    help='Do not rebuild GN with GN.')
  parser.add_option('--no-clean', action='store_true',
                    help='Re-used build directory instead of using new '
                         'temporary location each time')
  parser.add_option('--gn-gen-args', help='Args to pass to gn gen --args')
  parser.add_option('-v', '--verbose', action='store_true',
                    help='Log more details')
  parser.add_option('-s', '--shadow', action='store_true',
                    help='Use current dir as build dir')
  parser.add_option('-p', '--path', help='Path to ninja binary')
  options, args = parser.parse_args(argv)

  if args:
    parser.error('Unrecognized command line arguments: %s.' % ', '.join(args))

  logging.basicConfig(level=logging.DEBUG if options.verbose else logging.ERROR)

  try:
    if options.no_clean:
      build_dir = os.path.join(SRC_ROOT, 'out_bootstrap')
      if not os.path.exists(build_dir):
        os.makedirs(build_dir)
      return run_build(build_dir, options)
    elif options.shadow:
      build_dir = os.getcwd()
      return run_build(build_dir, options)
    else:
      with scoped_tempdir() as tempdir:
        return run_build(tempdir, options)
  except subprocess.CalledProcessError as e:
    print >> sys.stderr, str(e)
    return 1
  return 0


def write_buildflag_header_manually(root_gen_dir, header, flags):
  mkdir_p(os.path.join(root_gen_dir, os.path.dirname(header)))
  with tempfile.NamedTemporaryFile() as f:
    f.write('--flags')
    for name,value in flags.items():
      f.write(' ' + name + '=' + value)
    f.flush()

    check_call([
        os.path.join(SRC_ROOT, 'build', 'write_buildflag_header.py'),
        '--output', header,
        '--gen-dir', root_gen_dir,
        '--definitions', f.name,
    ])


def build_gn_with_ninja_manually(tempdir, options):
  root_gen_dir = os.path.join(tempdir, 'gen')
  mkdir_p(root_gen_dir)

  write_buildflag_header_manually(root_gen_dir, 'base/allocator/features.h',
      {'USE_EXPERIMENTAL_ALLOCATOR_SHIM': 'true' if is_linux else 'false'})

  write_buildflag_header_manually(root_gen_dir, 'base/debug/debugging_flags.h',
      {'ENABLE_PROFILING': 'false'})

  if is_mac:
    # //base/build_time.cc needs base/generated_build_date.h,
    # and this file is only included for Mac builds.
    mkdir_p(os.path.join(root_gen_dir, 'base'))
    check_call([
        os.path.join(SRC_ROOT, 'build', 'write_build_date_header.py'),
        os.path.join(root_gen_dir, 'base', 'generated_build_date.h'),
        'default'
    ])

  write_ninja(os.path.join(tempdir, 'build.ninja'), root_gen_dir, options)

  cmd = ['ninja', '-C', tempdir]
  if options.path:
      cmd[0] = options.path
  if options.verbose:
    cmd.append('-v')
  cmd.append('gn')
  check_call(cmd)

def write_ninja(path, root_gen_dir, options):
  cc = os.environ.get('CC', '')
  cxx = os.environ.get('CXX', '')
  cflags = os.environ.get('CFLAGS', '').split()
  cflags_cc = os.environ.get('CXXFLAGS', '').split()
  ld = os.environ.get('LD', cxx)
  ldflags = os.environ.get('LDFLAGS', '').split()
  include_dirs = [root_gen_dir, SRC_ROOT]
  libs = []

  # //base/allocator/allocator_extension.cc needs this macro defined,
  # otherwise there would be link errors.
  cflags.extend(['-DNO_TCMALLOC'])

  if is_posix:
    if options.debug:
      cflags.extend(['-O0', '-g'])
    else:
      cflags.extend(['-O2', '-g0'])

    cflags.extend([
        '-D_FILE_OFFSET_BITS=64',
        '-pthread',
        '-pipe',
        '-fno-exceptions'
    ])
    cflags_cc.extend(['-std=c++11', '-Wno-c++11-narrowing'])

  static_libraries = {
      'base': {'sources': [], 'tool': 'cxx', 'include_dirs': []},
      'dynamic_annotations': {'sources': [], 'tool': 'cc', 'include_dirs': []},
      'gn': {'sources': [], 'tool': 'cxx', 'include_dirs': []},
  }

  for name in os.listdir(GN_ROOT):
    if not name.endswith('.cc'):
      continue
    if name.endswith('_unittest.cc'):
      continue
    if name == 'run_all_unittests.cc':
      continue
    full_path = os.path.join(GN_ROOT, name)
    static_libraries['gn']['sources'].append(
        os.path.relpath(full_path, SRC_ROOT))

  static_libraries['dynamic_annotations']['sources'].extend([
      'base/third_party/dynamic_annotations/dynamic_annotations.c',
      'base/third_party/superfasthash/superfasthash.c',
  ])
  static_libraries['base']['sources'].extend([
      'base/allocator/allocator_check.cc',
      'base/allocator/allocator_extension.cc',
      'base/at_exit.cc',
      'base/base_paths.cc',
      'base/base_switches.cc',
      'base/callback_internal.cc',
      'base/command_line.cc',
      'base/debug/alias.cc',
      'base/debug/stack_trace.cc',
      'base/debug/task_annotator.cc',
      'base/environment.cc',
      'base/files/file.cc',
      'base/files/file_enumerator.cc',
      'base/files/file_path.cc',
      'base/files/file_path_constants.cc',
      'base/files/file_tracing.cc',
      'base/files/file_util.cc',
      'base/files/important_file_writer.cc',
      'base/files/memory_mapped_file.cc',
      'base/files/scoped_file.cc',
      'base/hash.cc',
      'base/json/json_parser.cc',
      'base/json/json_reader.cc',
      'base/json/json_string_value_serializer.cc',
      'base/json/json_writer.cc',
      'base/json/string_escape.cc',
      'base/lazy_instance.cc',
      'base/location.cc',
      'base/logging.cc',
      'base/md5.cc',
      'base/memory/ref_counted.cc',
      'base/memory/ref_counted_memory.cc',
      'base/memory/singleton.cc',
      'base/memory/weak_ptr.cc',
      'base/message_loop/incoming_task_queue.cc',
      'base/message_loop/message_loop.cc',
      'base/message_loop/message_loop_task_runner.cc',
      'base/message_loop/message_pump.cc',
      'base/message_loop/message_pump_default.cc',
      'base/metrics/bucket_ranges.cc',
      'base/metrics/histogram.cc',
      'base/metrics/histogram_base.cc',
      'base/metrics/histogram_samples.cc',
      'base/metrics/metrics_hashes.cc',
      'base/metrics/persistent_histogram_allocator.cc',
      'base/metrics/persistent_memory_allocator.cc',
      'base/metrics/persistent_sample_map.cc',
      'base/metrics/sample_map.cc',
      'base/metrics/sample_vector.cc',
      'base/metrics/sparse_histogram.cc',
      'base/metrics/statistics_recorder.cc',
      'base/path_service.cc',
      'base/pending_task.cc',
      'base/pickle.cc',
      'base/process/kill.cc',
      'base/process/process_iterator.cc',
      'base/process/process_metrics.cc',
      'base/profiler/scoped_profile.cc',
      'base/profiler/scoped_tracker.cc',
      'base/profiler/tracked_time.cc',
      'base/run_loop.cc',
      'base/sequence_checker_impl.cc',
      'base/sequenced_task_runner.cc',
      'base/sha1.cc',
      'base/strings/pattern.cc',
      'base/strings/string16.cc',
      'base/strings/string_number_conversions.cc',
      'base/strings/string_piece.cc',
      'base/strings/string_split.cc',
      'base/strings/string_util.cc',
      'base/strings/string_util_constants.cc',
      'base/strings/stringprintf.cc',
      'base/strings/utf_string_conversion_utils.cc',
      'base/strings/utf_string_conversions.cc',
      'base/synchronization/cancellation_flag.cc',
      'base/synchronization/lock.cc',
      'base/sys_info.cc',
      'base/task_runner.cc',
      'base/third_party/dmg_fp/dtoa_wrapper.cc',
      'base/third_party/dmg_fp/g_fmt.cc',
      'base/third_party/icu/icu_utf.cc',
      'base/third_party/nspr/prtime.cc',
      'base/threading/non_thread_safe_impl.cc',
      'base/threading/post_task_and_reply_impl.cc',
      'base/threading/sequenced_task_runner_handle.cc',
      'base/threading/sequenced_worker_pool.cc',
      'base/threading/simple_thread.cc',
      'base/threading/thread.cc',
      'base/threading/thread_checker_impl.cc',
      'base/threading/thread_collision_warner.cc',
      'base/threading/thread_id_name_manager.cc',
      'base/threading/thread_local_storage.cc',
      'base/threading/thread_restrictions.cc',
      'base/threading/thread_task_runner_handle.cc',
      'base/threading/worker_pool.cc',
      'base/time/time.cc',
      'base/timer/elapsed_timer.cc',
      'base/timer/timer.cc',
      'base/trace_event/heap_profiler_allocation_context.cc',
      'base/trace_event/heap_profiler_allocation_context_tracker.cc',
      'base/trace_event/heap_profiler_allocation_register.cc',
      'base/trace_event/heap_profiler_heap_dump_writer.cc',
      'base/trace_event/heap_profiler_stack_frame_deduplicator.cc',
      'base/trace_event/heap_profiler_type_name_deduplicator.cc',
      'base/trace_event/memory_allocator_dump.cc',
      'base/trace_event/memory_allocator_dump_guid.cc',
      'base/trace_event/memory_dump_manager.cc',
      'base/trace_event/memory_dump_request_args.cc',
      'base/trace_event/memory_dump_session_state.cc',
      'base/trace_event/memory_infra_background_whitelist.cc',
      'base/trace_event/process_memory_dump.cc',
      'base/trace_event/process_memory_maps.cc',
      'base/trace_event/process_memory_totals.cc',
      'base/trace_event/trace_buffer.cc',
      'base/trace_event/trace_config.cc',
      'base/trace_event/trace_event_argument.cc',
      'base/trace_event/trace_event_impl.cc',
      'base/trace_event/trace_event_memory_overhead.cc',
      'base/trace_event/trace_event_synthetic_delay.cc',
      'base/trace_event/trace_log.cc',
      'base/trace_event/trace_log_constants.cc',
      'base/trace_event/trace_sampling_thread.cc',
      'base/trace_event/tracing_agent.cc',
      'base/tracked_objects.cc',
      'base/tracking_info.cc',
      'base/values.cc',
      'base/vlog.cc',
  ])

  if is_posix:
    static_libraries['base']['sources'].extend([
        'base/base_paths_posix.cc',
        'base/debug/debugger_posix.cc',
        'base/debug/stack_trace_posix.cc',
        'base/files/file_enumerator_posix.cc',
        'base/files/file_posix.cc',
        'base/files/file_util_posix.cc',
        'base/files/memory_mapped_file_posix.cc',
        'base/message_loop/message_pump_libevent.cc',
        'base/posix/file_descriptor_shuffle.cc',
        'base/posix/safe_strerror.cc',
        'base/process/kill_posix.cc',
        'base/process/process_handle_posix.cc',
        'base/process/process_metrics_posix.cc',
        'base/process/process_posix.cc',
        'base/synchronization/condition_variable_posix.cc',
        'base/synchronization/lock_impl_posix.cc',
        'base/synchronization/read_write_lock_posix.cc',
        'base/synchronization/waitable_event_posix.cc',
        'base/sys_info_posix.cc',
        'base/threading/platform_thread_internal_posix.cc',
        'base/threading/platform_thread_posix.cc',
        'base/threading/thread_local_posix.cc',
        'base/threading/thread_local_storage_posix.cc',
        'base/threading/worker_pool_posix.cc',
        'base/time/time_posix.cc',
        'base/trace_event/heap_profiler_allocation_register_posix.cc',
    ])
    static_libraries['libevent'] = {
        'sources': [
            'base/third_party/libevent/buffer.c',
            'base/third_party/libevent/evbuffer.c',
            'base/third_party/libevent/evdns.c',
            'base/third_party/libevent/event.c',
            'base/third_party/libevent/event_tagging.c',
            'base/third_party/libevent/evrpc.c',
            'base/third_party/libevent/evutil.c',
            'base/third_party/libevent/http.c',
            'base/third_party/libevent/log.c',
            'base/third_party/libevent/poll.c',
            'base/third_party/libevent/select.c',
            'base/third_party/libevent/signal.c',
            'base/third_party/libevent/strlcpy.c',
        ],
        'tool': 'cc',
        'include_dirs': [],
        'cflags': cflags + ['-DHAVE_CONFIG_H'],
    }


  if is_linux:
    libs.extend(['-lrt'])
    ldflags.extend(['-pthread'])

    static_libraries['xdg_user_dirs'] = {
        'sources': [
            'base/third_party/xdg_user_dirs/xdg_user_dir_lookup.cc',
        ],
        'tool': 'cxx',
    }
    static_libraries['base']['sources'].extend([
        'base/allocator/allocator_shim.cc',
        'base/allocator/allocator_shim_default_dispatch_to_glibc.cc',
        'base/memory/shared_memory_posix.cc',
        'base/nix/xdg_util.cc',
        'base/process/internal_linux.cc',
        'base/process/process_handle_linux.cc',
        'base/process/process_iterator_linux.cc',
        'base/process/process_linux.cc',
        'base/process/process_metrics_linux.cc',
        'base/strings/sys_string_conversions_posix.cc',
        'base/sys_info_linux.cc',
        'base/threading/platform_thread_linux.cc',
        'base/trace_event/malloc_dump_provider.cc',
    ])
    static_libraries['libevent']['include_dirs'].extend([
        os.path.join(SRC_ROOT, 'base', 'third_party', 'libevent', 'linux')
    ])
    static_libraries['libevent']['sources'].extend([
        'base/third_party/libevent/epoll.c',
    ])


  if is_mac:
    static_libraries['base']['sources'].extend([
        'base/base_paths_mac.mm',
        'base/build_time.cc',
        'base/rand_util.cc',
        'base/rand_util_posix.cc',
        'base/files/file_util_mac.mm',
        'base/mac/bundle_locations.mm',
        'base/mac/call_with_eh_frame.cc',
        'base/mac/call_with_eh_frame_asm.S',
        'base/mac/foundation_util.mm',
        'base/mac/mach_logging.cc',
        'base/mac/scoped_mach_port.cc',
        'base/mac/scoped_mach_vm.cc',
        'base/mac/scoped_nsautorelease_pool.mm',
        'base/memory/shared_memory_handle_mac.cc',
        'base/memory/shared_memory_mac.cc',
        'base/message_loop/message_pump_mac.mm',
        'base/metrics/field_trial.cc',
        'base/process/process_handle_mac.cc',
        'base/process/process_iterator_mac.cc',
        'base/process/process_metrics_mac.cc',
        'base/strings/sys_string_conversions_mac.mm',
        'base/time/time_mac.cc',
        'base/threading/platform_thread_mac.mm',
        'base/trace_event/malloc_dump_provider.cc',
    ])
    static_libraries['libevent']['include_dirs'].extend([
        os.path.join(SRC_ROOT, 'base', 'third_party', 'libevent', 'mac')
    ])
    static_libraries['libevent']['sources'].extend([
        'base/third_party/libevent/kqueue.c',
    ])


  if is_mac:
    template_filename = 'build_mac.ninja.template'
  else:
    template_filename = 'build.ninja.template'

  with open(os.path.join(GN_ROOT, 'bootstrap', template_filename)) as f:
    ninja_template = f.read()

  def src_to_obj(path):
    return '%s' % os.path.splitext(path)[0] + '.o'

  ninja_lines = []
  for library, settings in static_libraries.iteritems():
    for src_file in settings['sources']:
      ninja_lines.extend([
          'build %s: %s %s' % (src_to_obj(src_file),
                               settings['tool'],
                               os.path.join(SRC_ROOT, src_file)),
          '  includes = %s' % ' '.join(
              ['-I' + dirname for dirname in
               include_dirs + settings.get('include_dirs', [])]),
          '  cflags = %s' % ' '.join(cflags + settings.get('cflags', [])),
          '  cflags_cc = %s' %
              ' '.join(cflags_cc + settings.get('cflags_cc', [])),
      ])
      if cc:
        ninja_lines.append('  cc = %s' % cc)
      if cxx:
        ninja_lines.append('  cxx = %s' % cxx)

    ninja_lines.append('build %s.a: alink_thin %s' % (
        library,
        ' '.join([src_to_obj(src_file) for src_file in settings['sources']])))

  if is_mac:
    libs.extend([
        '-framework', 'AppKit',
        '-framework', 'CoreFoundation',
        '-framework', 'Foundation',
        '-framework', 'Security',
    ]);

  ninja_lines.extend([
      'build gn: link %s' % (
          ' '.join(['%s.a' % library for library in static_libraries])),
      '  ldflags = %s' % ' '.join(ldflags),
      '  libs = %s' % ' '.join(libs),
  ])
  if ld:
    ninja_lines.append('  ld = %s' % ld)
  else:
    ninja_lines.append('  ld = $ldxx')

  ninja_lines.append('')  # Make sure the file ends with a newline.

  with open(path, 'w') as f:
    f.write(ninja_template + '\n'.join(ninja_lines))


def build_gn_with_gn(temp_gn, build_dir, options):
  gn_gen_args = options.gn_gen_args or ''
  if not options.debug:
    gn_gen_args += ' is_debug=false'
  cmd = [temp_gn, 'gen', build_dir, '--args=%s' % gn_gen_args]
  check_call(cmd)

  cmd = ['ninja', '-C', build_dir]
  if options.path:
    cmd[0] = options.path
  if options.verbose:
    cmd.append('-v')
  cmd.append('gn')
  check_call(cmd)

  if not options.debug:
    check_call(['strip', os.path.join(build_dir, 'gn')])


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
