# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A class for updating layout test expectations when updating w3c tests.

Specifically, this class fetches results from try bots for the current CL, and:
  1. Downloads new baseline files for any tests that can be rebaselined.
  2. Updates the generic TestExpectations file for any other failing tests.

This is used as part of the w3c test auto-import process.
"""

import argparse
import copy
import logging

from webkitpy.common.net.git_cl import GitCL
from webkitpy.common.net.rietveld import Rietveld
from webkitpy.common.webkit_finder import WebKitFinder
from webkitpy.layout_tests.models.test_expectations import TestExpectationLine
from webkitpy.w3c.test_parser import TestParser

_log = logging.getLogger(__name__)

MARKER_COMMENT = '# ====== New tests from w3c-test-autoroller added here ======'


class W3CExpectationsLineAdder(object):

    def __init__(self, host):
        self.host = host
        self.host.initialize_scm()
        self.finder = WebKitFinder(self.host.filesystem)

    def run(self, args=None):
        parser = argparse.ArgumentParser(description=__doc__)
        parser.add_argument('-v', '--verbose', action='store_true', help='More verbose logging.')
        args = parser.parse_args(args)
        log_level = logging.DEBUG if args.verbose else logging.INFO
        logging.basicConfig(level=log_level, format='%(message)s')

        issue_number = self.get_issue_number()
        if issue_number == 'None':
            _log.error('No issue on current branch.')
            return 1

        rietveld = Rietveld(self.host.web)
        builds = rietveld.latest_try_jobs(issue_number, self.get_try_bots())
        _log.debug('Latest try jobs: %r', builds)

        if not builds:
            _log.error('No try job information was collected.')
            return 1

        test_expectations = {}
        for build in builds:
            platform_results = self.get_failing_results_dict(build)
            test_expectations = self.merge_dicts(test_expectations, platform_results)

        for test_name, platform_result in test_expectations.iteritems():
            test_expectations[test_name] = self.merge_same_valued_keys(platform_result)

        test_expectations = self.get_expected_txt_files(test_expectations)
        test_expectation_lines = self.create_line_list(test_expectations)
        self.write_to_test_expectations(test_expectation_lines)
        return 0

    def get_issue_number(self):
        return GitCL(self.host).get_issue_number()

    def get_try_bots(self):
        return self.host.builders.all_try_builder_names()

    def generate_results_dict(self, platform, result_list):
        test_dict = {}
        if '-' in platform:
            platform = platform[platform.find('-') + 1:].capitalize()
        for result in result_list:
            test_dict[result.test_name()] = {
                platform: {
                    'expected': result.expected_results(),
                    'actual': result.actual_results(),
                    'bug': 'crbug.com/626703'
                }}
        return test_dict

    def get_failing_results_dict(self, build):
        """Returns a nested dict of failing test results.

        Retrieves a full list of layout test results from a builder result URL.
        Collects the builder name, platform and a list of tests that did not
        run as expected.

        Args:
            build: A Build object.

        Returns:
            A dictionary with the structure: {
                'key': {
                    'expected': 'TIMEOUT',
                    'actual': 'CRASH',
                    'bug': 'crbug.com/11111'
                }
            }
            If there are no failing results or no results could be fetched,
            this will return an empty dict.
        """
        layout_test_results = self.host.buildbot.fetch_results(build)
        if layout_test_results is None:
            _log.warning('No results for build %s', build)
            return {}
        platform = self.host.builders.port_name_for_builder_name(build.builder_name)
        result_list = layout_test_results.didnt_run_as_expected_results()
        failing_results_dict = self.generate_results_dict(platform, result_list)
        return failing_results_dict

    def merge_dicts(self, target, source, path=None):
        """Recursively merges nested dictionaries.

        Args:
            target: First dictionary, which is updated based on source.
            source: Second dictionary, not modified.

        Returns:
            An updated target dictionary.
        """
        path = path or []
        for key in source:
            if key in target:
                if (isinstance(target[key], dict)) and isinstance(source[key], dict):
                    self.merge_dicts(target[key], source[key], path + [str(key)])
                elif target[key] == source[key]:
                    pass
                else:
                    raise ValueError('The key: %s already exist in the target dictionary.' % '.'.join(path))
            else:
                target[key] = source[key]
        return target

    def merge_same_valued_keys(self, dictionary):
        """Merges keys in dictionary with same value.

        Traverses through a dict and compares the values of keys to one another.
        If the values match, the keys are combined to a tuple and the previous
        keys are removed from the dict.

        Args:
            dictionary: A dictionary with a dictionary as the value.

        Returns:
            A new dictionary with updated keys to reflect matching values of keys.
            Example: {
                'one': {'foo': 'bar'},
                'two': {'foo': 'bar'},
                'three': {'foo': 'bar'}
            }
            is converted to a new dictionary with that contains
            {('one', 'two', 'three'): {'foo': 'bar'}}
        """
        merged_dict = {}
        matching_value_keys = set()
        keys = sorted(dictionary.keys())
        while keys:
            current_key = keys[0]
            found_match = False
            if current_key == keys[-1]:
                merged_dict[current_key] = dictionary[current_key]
                keys.remove(current_key)
                break

            for next_item in keys[1:]:
                if dictionary[current_key] == dictionary[next_item]:
                    found_match = True
                    matching_value_keys.update([current_key, next_item])

                if next_item == keys[-1]:
                    if found_match:
                        merged_dict[tuple(matching_value_keys)] = dictionary[current_key]
                        keys = [k for k in keys if k not in matching_value_keys]
                    else:
                        merged_dict[current_key] = dictionary[current_key]
                        keys.remove(current_key)
            matching_value_keys = set()
        return merged_dict

    def get_expectations(self, results):
        """Returns a set of test expectations for a given test dict.

        Returns a set of one or more test expectations based on the expected
        and actual results of a given test name.

        Args:
            results: A dictionary that maps one test to its results. Example:
                {
                    'test_name': {
                        'expected': 'PASS',
                        'actual': 'FAIL',
                        'bug': 'crbug.com/11111'
                    }
                }

        Returns:
            A set of one or more test expectation strings with the first letter
            capitalized. Example: set(['Failure', 'Timeout']).
        """
        expectations = set()
        failure_types = ['TEXT', 'FAIL', 'IMAGE+TEXT', 'IMAGE', 'AUDIO', 'MISSING', 'LEAK']
        test_expectation_types = ['SLOW', 'TIMEOUT', 'CRASH', 'PASS', 'REBASELINE', 'NEEDSREBASELINE', 'NEEDSMANUALREBASELINE']
        for expected in results['expected'].split():
            for actual in results['actual'].split():
                if expected in test_expectation_types and actual in failure_types:
                    expectations.add('Failure')
                if expected in failure_types and actual in test_expectation_types:
                    expectations.add(actual.capitalize())
                if expected in test_expectation_types and actual in test_expectation_types:
                    expectations.add(actual.capitalize())
        return expectations

    def create_line_list(self, merged_results):
        """Creates list of test expectations lines.

        Traverses through the given |merged_results| dictionary and parses the
        value to create one test expectations line per key.

        Args:
            merged_results: A merged_results with the format:
                {
                    'test_name': {
                        'platform': {
                            'expected: 'PASS',
                            'actual': 'FAIL',
                            'bug': 'crbug.com/11111'
                        }
                    }
                }

        Returns:
            A list of test expectations lines with the format:
            ['BUG_URL [PLATFORM(S)] TEST_MAME [EXPECTATION(S)]']
        """
        line_list = []
        for test_name, platform_results in merged_results.iteritems():
            for platform in platform_results:
                if test_name.startswith('imported'):
                    platform_list = []
                    bug = []
                    expectations = []
                    if isinstance(platform, tuple):
                        platform_list = list(platform)
                    else:
                        platform_list.append(platform)
                    bug.append(platform_results[platform]['bug'])
                    expectations = self.get_expectations(platform_results[platform])
                    line = '%s [ %s ] %s [ %s ]' % (bug[0], ' '.join(platform_list), test_name, ' '.join(expectations))
                    line_list.append(str(line))
        return line_list

    def write_to_test_expectations(self, line_list):
        """Writes to TestExpectations.

        The place in the file where the new lines are inserted is after a
        marker comment line. If this marker comment line is not found, it will
        be added to the end of the file.

        Args:
            line_list: A list of lines to add to the TestExpectations file.
        """
        _log.debug('Lines to write to TestExpectations: %r', line_list)
        port = self.host.port_factory.get()
        expectations_file_path = port.path_to_generic_test_expectations_file()
        file_contents = self.host.filesystem.read_text_file(expectations_file_path)
        marker_comment_index = file_contents.find(MARKER_COMMENT)
        line_list = [line for line in line_list if self._test_name_from_expectation_string(line) not in file_contents]
        if not line_list:
            return
        if marker_comment_index == -1:
            file_contents += '\n%s\n' % MARKER_COMMENT
            file_contents += '\n'.join(line_list)
        else:
            end_of_marker_line = (file_contents[marker_comment_index:].find('\n')) + marker_comment_index
            file_contents = file_contents[:end_of_marker_line + 1] + '\n'.join(line_list) + file_contents[end_of_marker_line:]
        self.host.filesystem.write_text_file(expectations_file_path, file_contents)

    @staticmethod
    def _test_name_from_expectation_string(expectation_string):
        return TestExpectationLine.tokenize_line(filename='', expectation_string=expectation_string, line_number=0).name

    def get_expected_txt_files(self, tests_results):
        """Fetches new baseline files for tests that should be rebaselined.

        Invokes webkit-patch rebaseline-from-try-jobs in order to download new
        -expected.txt files for testharness.js tests that did not crash or time
        out. Then, the platform-specific test is removed from the overall
        failure test dictionary.

        Args:
            tests_results: A dict mapping test name to platform to test results.

        Returns:
            An updated tests_results dictionary without the platform-specific
            testharness.js tests that required new baselines to be downloaded
            from `webkit-patch rebaseline-from-try-jobs`.
        """
        modified_tests = self.get_modified_existing_tests()
        tests_to_rebaseline, tests_results = self.get_tests_to_rebaseline(modified_tests, tests_results)
        _log.debug('Tests to rebaseline: %r', tests_to_rebaseline)
        if tests_to_rebaseline:
            webkit_patch = self.host.filesystem.join(
                self.finder.chromium_base(), self.finder.webkit_base(), self.finder.path_to_script('webkit-patch'))
            self.host.executive.run_command([
                'python',
                webkit_patch,
                'rebaseline-cl',
                '--verbose',
                '--no-trigger-jobs',
            ] + tests_to_rebaseline)
        return tests_results

    def get_modified_existing_tests(self):
        """Returns a list of layout test names for layout tests that have been modified."""
        diff_output = self.host.executive.run_command(
            ['git', 'diff', 'origin/master', '--name-only', '--diff-filter=AMR'])  # Added, modified, and renamed files.
        paths_from_chromium_root = diff_output.splitlines()
        modified_tests = []
        for path in paths_from_chromium_root:
            absolute_path = self.host.filesystem.join(self.finder.chromium_base(), path)
            if not self.host.filesystem.exists(absolute_path):
                _log.warning('File does not exist: %s', absolute_path)
                continue
            test_path = self.finder.layout_test_name(path)
            if test_path:
                modified_tests.append(test_path)
        return modified_tests

    def get_tests_to_rebaseline(self, modified_tests, test_results):
        """Returns a list of tests to download new baselines for.

        Creates a list of tests to rebaseline depending on the tests' platform-
        specific results. In general, this will be non-ref tests that failed
        due to a baseline mismatch (rather than crash or timeout).

        Args:
            modified_tests: A list of paths to modified files (which should
                be added, removed or modified files in the imported w3c
                directory), relative to the LayoutTests directory.
            test_results: A dictionary of failing tests results.

        Returns:
            A pair: A set of tests to be rebaselined, and a modified copy of
            the test results dictionary. The tests to be rebaselined should include
            testharness.js tests that failed due to a baseline mismatch.
        """
        test_results = copy.deepcopy(test_results)
        tests_to_rebaseline = set()
        for test_path in modified_tests:
            if not (self.is_js_test(test_path) and test_results.get(test_path)):
                continue
            for platform in test_results[test_path].keys():
                if test_results[test_path][platform]['actual'] not in ['CRASH', 'TIMEOUT']:
                    del test_results[test_path][platform]
                    tests_to_rebaseline.add(test_path)
        return sorted(tests_to_rebaseline), test_results

    def is_js_test(self, test_path):
        """Checks whether a given file is a testharness.js test.

        Args:
            test_path: A file path relative to the layout tests directory.
                This might correspond to a deleted file or a non-test.
        """
        absolute_path = self.host.filesystem.join(self.finder.layout_tests_dir(), test_path)
        test_parser = TestParser(absolute_path, self.host)
        if not test_parser.test_doc:
            return False
        return test_parser.is_jstest()
