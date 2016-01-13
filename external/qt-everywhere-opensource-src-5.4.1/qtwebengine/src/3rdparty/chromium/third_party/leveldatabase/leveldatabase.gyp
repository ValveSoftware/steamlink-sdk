# Copyright (c) 2011 The LevelDB Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file. See the AUTHORS file for names of contributors.

{
  'variables': {
    'use_snappy%': 1,
  },
  'conditions': [
    ['OS == "android" and android_webview_build == 1', {
      'variables': {
        # Snappy not used in Android WebView
        # crbug.com/236780
        'use_snappy': 0,
      },
    }],
    ['OS=="android"', {
      'targets': [{
        'target_name': 'env_chromium_unittests_apk',
        'type': 'none',
        'dependencies': [
          '<(DEPTH)/base/base.gyp:base_java',
          'env_chromium_unittests',
        ],
        'variables': {
          'test_suite_name': 'env_chromium_unittests',
        },
        'includes': [ '../../build/apk_test.gypi' ],
      }],
    }],
  ],
  'target_defaults': {
    'defines': [
      'LEVELDB_PLATFORM_CHROMIUM=1',
    ],
    'include_dirs': [
      '.',
      'src/',
      'src/include/',
    ],
    'conditions': [
      ['use_snappy', {
        'defines': [
          'USE_SNAPPY=1',
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'leveldatabase',
      'type': 'static_library',
      'dependencies': [
        '../../third_party/re2/re2.gyp:re2',
        '../../base/base.gyp:base',
        # base::LazyInstance is a template that pulls in dynamic_annotations so
        # we need to explictly link in the code for dynamic_annotations.
        '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
      ],
      'conditions': [
        ['use_snappy', {
          'dependencies': [
            '../../third_party/snappy/snappy.gyp:snappy',
          ],
        }],
        ['OS=="win"', {
          'sources': [
            'env_chromium_win.cc',
            'env_chromium_win.h',
          ],
        }],
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'src/include/',
          'src/',
          '.',
        ],
      },
      # Patch posted for upstream, can be removed once that's landed and
      # rolled into Chromium.
      # Internal link: https://mondrian.corp.google.com/#review/29997992
      'msvs_disabled_warnings': [
        # Signed/unsigned comparison.
        4018,

        # TODO(jschuh): http://crbug.com/167187 size_t -> int
        4267,
      ],
      'sources': [
        # Include and then exclude so that all files show up in IDEs, even if
        # they don't build.
        'env_chromium.cc',
        'env_chromium.h',
        'env_chromium_stdio.cc',
        'env_chromium_stdio.h',
        'env_idb.h',
        'port/port_chromium.cc',
        'port/port_chromium.h',
        'src/db/builder.cc',
        'src/db/builder.h',
        'src/db/db_impl.cc',
        'src/db/db_impl.h',
        'src/db/db_iter.cc',
        'src/db/db_iter.h',
        'src/db/filename.cc',
        'src/db/filename.h',
        'src/db/dbformat.cc',
        'src/db/dbformat.h',
        'src/db/log_format.h',
        'src/db/log_reader.cc',
        'src/db/log_reader.h',
        'src/db/log_writer.cc',
        'src/db/log_writer.h',
        'src/db/memtable.cc',
        'src/db/memtable.h',
        'src/db/repair.cc',
        'src/db/skiplist.h',
        'src/db/snapshot.h',
        'src/db/table_cache.cc',
        'src/db/table_cache.h',
        'src/db/version_edit.cc',
        'src/db/version_edit.h',
        'src/db/version_set.cc',
        'src/db/version_set.h',
        'src/db/write_batch.cc',
        'src/db/write_batch_internal.h',
        'src/helpers/memenv/memenv.cc',
        'src/helpers/memenv/memenv.h',
        'src/include/leveldb/cache.h',
        'src/include/leveldb/comparator.h',
        'src/include/leveldb/db.h',
        'src/include/leveldb/env.h',
        'src/include/leveldb/filter_policy.h',
        'src/include/leveldb/iterator.h',
        'src/include/leveldb/options.h',
        'src/include/leveldb/slice.h',
        'src/include/leveldb/status.h',
        'src/include/leveldb/table.h',
        'src/include/leveldb/table_builder.h',
        'src/include/leveldb/write_batch.h',
        'src/port/port.h',
        'src/port/port_example.h',
        'src/port/port_posix.cc',
        'src/port/port_posix.h',
        'src/table/block.cc',
        'src/table/block.h',
        'src/table/block_builder.cc',
        'src/table/block_builder.h',
        'src/table/filter_block.cc',
        'src/table/filter_block.h',
        'src/table/format.cc',
        'src/table/format.h',
        'src/table/iterator.cc',
        'src/table/iterator_wrapper.h',
        'src/table/merger.cc',
        'src/table/merger.h',
        'src/table/table.cc',
        'src/table/table_builder.cc',
        'src/table/two_level_iterator.cc',
        'src/table/two_level_iterator.h',
        'src/util/arena.cc',
        'src/util/arena.h',
        'src/util/bloom.cc',
        'src/util/cache.cc',
        'src/util/coding.cc',
        'src/util/coding.h',
        'src/util/comparator.cc',
        'src/util/crc32c.cc',
        'src/util/crc32c.h',
        'src/util/env.cc',
        'src/util/filter_policy.cc',
        'src/util/hash.cc',
        'src/util/hash.h',
        'src/util/logging.cc',
        'src/util/logging.h',
        'src/util/mutexlock.h',
        'src/util/options.cc',
        'src/util/random.h',
        'src/util/status.cc',
      ],
      'sources/': [
        ['exclude', '_(android|example|portable|posix)\\.cc$'],
      ],
    },
    {
      'target_name': 'env_chromium_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'leveldatabase',
        '../../base/base.gyp:test_support_base',
        '../../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'env_chromium_unittest.cc',
      ],
      'conditions': [
        ['OS=="android"', {
          'type': 'shared_library',
          'dependencies': [
            '../../testing/android/native_test.gyp:native_test_native_code',
            '../../tools/android/forwarder2/forwarder.gyp:forwarder2',
          ],
        }],
      ],
    },
    {
      'target_name': 'leveldb_testutil',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        'leveldatabase',
      ],
      'export_dependent_settings': [
        # The tests use include directories from these projects.
        '../../base/base.gyp:base',
        'leveldatabase',
      ],
      'sources': [
        'src/util/histogram.cc',
        'src/util/histogram.h',
        'src/util/testharness.cc',
        'src/util/testharness.h',
        'src/util/testutil.cc',
        'src/util/testutil.h',
      ],
    },
    {
      'target_name': 'leveldb_arena_test',
      'type': 'executable',
      'dependencies': [
        'leveldb_testutil',
      ],
      'sources': [
        'src/util/arena_test.cc',
      ],
    },
    {
      'target_name': 'leveldb_bloom_test',
      'type': 'executable',
      'dependencies': [
        'leveldb_testutil',
      ],
      'sources': [
        'src/util/bloom_test.cc',
      ],
    },
    {
      'target_name': 'leveldb_cache_test',
      'type': 'executable',
      'dependencies': [
        'leveldb_testutil',
      ],
      'sources': [
        'src/util/cache_test.cc',
      ],
    },
    {
      'target_name': 'leveldb_coding_test',
      'type': 'executable',
      'dependencies': [
        'leveldb_testutil',
      ],
      'sources': [
        'src/util/coding_test.cc',
      ],
    },
    {
      'target_name': 'leveldb_corruption_test',
      'type': 'executable',
      'dependencies': [
        'leveldb_testutil',
      ],
      'sources': [
        'src/db/corruption_test.cc',
      ],
    },
    {
      'target_name': 'leveldb_crc32c_test',
      'type': 'executable',
      'dependencies': [
        'leveldb_testutil',
      ],
      'sources': [
        'src/util/crc32c_test.cc',
      ],
    },
    {
      'target_name': 'leveldb_db_bench',
      'type': 'executable',
      'dependencies': [
        'leveldb_testutil',
      ],
      'sources': [
        'src/db/db_bench.cc',
      ],
    },
    {
      'target_name': 'leveldb_db_test',
      'type': 'executable',
      'dependencies': [
        'leveldb_testutil',
      ],
      'sources': [
        'src/db/db_test.cc',
      ],
    },
    {
      'target_name': 'leveldb_dbformat_test',
      'type': 'executable',
      'dependencies': [
        'leveldb_testutil',
      ],
      'sources': [
        'src/db/dbformat_test.cc',
      ],
    },
    {
      'target_name': 'leveldb_env_test',
      'type': 'executable',
      'dependencies': [
        'leveldb_testutil',
      ],
      'sources': [
        'src/util/env_test.cc',
      ],
    },
    {
      'target_name': 'leveldb_filename_test',
      'type': 'executable',
      'dependencies': [
        'leveldb_testutil',
      ],
      'sources': [
        'src/db/filename_test.cc',
      ],
    },
    {
      'target_name': 'leveldb_filter_block_test',
      'type': 'executable',
      'dependencies': [
        'leveldb_testutil',
      ],
      'sources': [
        'src/table/filter_block_test.cc',
      ],
    },
    {
      'target_name': 'leveldb_log_test',
      'type': 'executable',
      'dependencies': [
        'leveldb_testutil',
      ],
      'sources': [
        'src/db/log_test.cc',
      ],
    },
    {
      'target_name': 'leveldb_skiplist_test',
      'type': 'executable',
      'dependencies': [
        'leveldb_testutil',
      ],
      'sources': [
        'src/db/skiplist_test.cc',
      ],
    },
    {
      'target_name': 'leveldb_table_test',
      'type': 'executable',
      'dependencies': [
        'leveldb_testutil',
      ],
      'sources': [
        'src/table/table_test.cc',
      ],
    },
    {
      'target_name': 'leveldb_version_edit_test',
      'type': 'executable',
      'dependencies': [
        'leveldb_testutil',
      ],
      'sources': [
        'src/db/version_edit_test.cc',
      ],
    },
    {
      'target_name': 'leveldb_write_batch_test',
      'type': 'executable',
      'dependencies': [
        'leveldb_testutil',
      ],
      'sources': [
        'src/db/write_batch_test.cc',
      ],
    },
    {
      'target_name': 'leveldb_main',
      'type': 'executable',
      'dependencies': [
        'leveldb_testutil',
      ],
      'sources': [
        'src/db/leveldb_main.cc',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
