# Copyright (C) 2012 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import errno
import json
import logging
import re

from webkitpy.layout_tests.layout_package.json_results_generator import convert_times_trie_to_flat_paths
from webkitpy.layout_tests.models import test_expectations


_log = logging.getLogger(__name__)


class LayoutTestFinder(object):

    def __init__(self, port, options):
        self._port = port
        self._options = options
        self._filesystem = self._port.host.filesystem
        self.LAYOUT_TESTS_DIRECTORIES = ('src', 'third_party', 'WebKit', 'LayoutTests')

    def find_tests(self, args, test_list=None, fastest_percentile=None):
        paths = self._strip_test_dir_prefixes(args)
        if test_list:
            paths += self._strip_test_dir_prefixes(self._read_test_names_from_file(test_list, self._port.TEST_PATH_SEPARATOR))

        all_tests = []
        if not paths or fastest_percentile:
            all_tests = self._port.tests(None)

        path_tests = []
        if paths:
            path_tests = self._port.tests(paths)

        test_files = None
        running_all_tests = False
        if fastest_percentile:
            times_trie = self._times_trie()
            if times_trie:
                fastest_tests = self._fastest_tests(times_trie, all_tests, fastest_percentile)
                test_files = list(set(fastest_tests).union(path_tests))
            else:
                _log.warning('Running all the tests the first time to generate timing data.')
                test_files = all_tests
                running_all_tests = True
        elif paths:
            test_files = path_tests
        else:
            test_files = all_tests
            running_all_tests = True

        return (paths, test_files, running_all_tests)

    def _times_trie(self):
        times_ms_path = self._port.bot_test_times_path()
        if self._filesystem.exists(times_ms_path):
            return json.loads(self._filesystem.read_text_file(times_ms_path))
        else:
            return {}

    # The following line should run the fastest 50% of tests *and*
    # the css3/flexbox tests. It should *not* run the fastest 50%
    # of the css3/flexbox tests.
    #
    # run-webkit-tests --fastest=50 css3/flexbox
    def _fastest_tests(self, times_trie, all_tests, fastest_percentile):
        times = convert_times_trie_to_flat_paths(times_trie)

        # Ignore tests with a time==0 because those are skipped tests.
        sorted_times = sorted([test for (test, time) in times.iteritems() if time],
                              key=lambda t: (times[t], t))
        clamped_percentile = max(0, min(100, fastest_percentile))
        number_of_tests_to_return = int(len(sorted_times) * clamped_percentile / 100)
        fastest_tests = set(sorted_times[:number_of_tests_to_return])

        # Don't try to run tests in the times_trie that no longer exist,
        fastest_tests = fastest_tests.intersection(all_tests)

        # For fastest tests, include any tests not in the times_ms.json so that
        # new tests get run in the fast set.
        unaccounted_tests = set(all_tests) - set(times.keys())

        # Using a set to dedupe here means that --order=None won't work, but that's
        # ok because --fastest already runs in an arbitrary order.
        return list(fastest_tests.union(unaccounted_tests))

    def _strip_test_dir_prefixes(self, paths):
        return [self._strip_test_dir_prefix(path) for path in paths if path]

    def _strip_test_dir_prefix(self, path):
        # Remove src/third_party/WebKit/LayoutTests/ from the front of the test path,
        # or any subset of these.
        for i in range(len(self.LAYOUT_TESTS_DIRECTORIES)):
            # Handle both "LayoutTests/foo/bar.html" and "LayoutTests\foo\bar.html" if
            # the filesystem uses '\\' as a directory separator
            for separator in (self._port.TEST_PATH_SEPARATOR, self._filesystem.sep):
                directory_prefix = separator.join(self.LAYOUT_TESTS_DIRECTORIES[i:]) + separator
                if path.startswith(directory_prefix):
                    return path[len(directory_prefix):]
        return path

    def _read_test_names_from_file(self, filenames, test_path_separator):
        fs = self._filesystem
        tests = []
        for filename in filenames:
            try:
                if test_path_separator != fs.sep:
                    filename = filename.replace(test_path_separator, fs.sep)
                file_contents = fs.read_text_file(filename).split('\n')
                for line in file_contents:
                    line = self._strip_comments(line)
                    if line:
                        tests.append(line)
            except IOError as e:
                if e.errno == errno.ENOENT:
                    _log.critical('')
                    _log.critical('--test-list file "%s" not found' % file)
                raise
        return tests

    @staticmethod
    def _strip_comments(line):
        commentIndex = line.find('//')
        if commentIndex is -1:
            commentIndex = len(line)

        line = re.sub(r'\s+', ' ', line[:commentIndex].strip())
        if line == '':
            return None
        else:
            return line

    def skip_tests(self, paths, all_tests_list, expectations, http_tests):
        all_tests = set(all_tests_list)

        tests_to_skip = expectations.get_tests_with_result_type(test_expectations.SKIP)
        if self._options.skip_failing_tests:
            tests_to_skip.update(expectations.get_tests_with_result_type(test_expectations.FAIL))
            tests_to_skip.update(expectations.get_tests_with_result_type(test_expectations.FLAKY))

        if self._options.skipped == 'only':
            tests_to_skip = all_tests - tests_to_skip
        elif self._options.skipped == 'ignore':
            tests_to_skip = set()
        elif self._options.skipped != 'always':
            # make sure we're explicitly running any tests passed on the command line; equivalent to 'default'.
            tests_to_skip -= set(paths)

        return tests_to_skip

    def split_into_chunks(self, test_names):
        """split into a list to run and a set to skip, based on --run-chunk and --run-part."""
        if not self._options.run_chunk and not self._options.run_part:
            return test_names, set()

        # If the user specifies they just want to run a subset of the tests,
        # just grab a subset of the non-skipped tests.
        chunk_value = self._options.run_chunk or self._options.run_part
        try:
            (chunk_num, chunk_len) = chunk_value.split(":")
            chunk_num = int(chunk_num)
            assert chunk_num >= 0
            test_size = int(chunk_len)
            assert test_size > 0
        except AssertionError:
            _log.critical("invalid chunk '%s'" % chunk_value)
            return (None, None)

        # Get the number of tests
        num_tests = len(test_names)

        # Get the start offset of the slice.
        if self._options.run_chunk:
            chunk_len = test_size
            # In this case chunk_num can be really large. We need
            # to make the slave fit in the current number of tests.
            slice_start = (chunk_num * chunk_len) % num_tests
        else:
            # Validate the data.
            assert test_size <= num_tests
            assert chunk_num <= test_size

            # To count the chunk_len, and make sure we don't skip
            # some tests, we round to the next value that fits exactly
            # all the parts.
            rounded_tests = num_tests
            if rounded_tests % test_size != 0:
                rounded_tests = (num_tests + test_size - (num_tests % test_size))

            chunk_len = rounded_tests / test_size
            slice_start = chunk_len * (chunk_num - 1)
            # It does not mind if we go over test_size.

        # Get the end offset of the slice.
        slice_end = min(num_tests, slice_start + chunk_len)

        tests_to_run = test_names[slice_start:slice_end]

        _log.debug('chunk slice [%d:%d] of %d is %d tests' % (slice_start, slice_end, num_tests, (slice_end - slice_start)))

        # If we reached the end and we don't have enough tests, we run some
        # from the beginning.
        if slice_end - slice_start < chunk_len:
            extra = chunk_len - (slice_end - slice_start)
            _log.debug('   last chunk is partial, appending [0:%d]' % extra)
            tests_to_run.extend(test_names[0:extra])

        return (tests_to_run, set(test_names) - set(tests_to_run))
