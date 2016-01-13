# Copyright (C) 2010 Google Inc. All rights reserved.
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

try:
    import jsonresults
    from jsonresults import *
except ImportError:
    print "ERROR: Add the TestResultServer, google_appengine and yaml/lib directories to your PYTHONPATH"
    raise

import json
import logging
import unittest

FULL_RESULT_EXAMPLE = """ADD_RESULTS({
    "seconds_since_epoch": 1368146629,
    "tests": {
        "media": {
            "encrypted-media": {
                "encrypted-media-v2-events.html": {
                    "bugs": ["crbug.com/1234"],
                    "expected": "TIMEOUT",
                    "actual": "TIMEOUT",
                    "time": 6.0
                },
                "encrypted-media-v2-syntax.html": {
                    "expected": "TIMEOUT",
                    "actual": "TIMEOUT"
                }
            },
            "progress-events-generated-correctly.html": {
                "expected": "PASS FAIL IMAGE TIMEOUT CRASH MISSING",
                "actual": "TIMEOUT",
                "time": 6.0
            },
            "W3C": {
                "audio": {
                    "src": {
                        "src_removal_does_not_trigger_loadstart.html": {
                            "expected": "PASS",
                            "actual": "PASS",
                            "time": 3.5
                        }
                    }
                },
                "video": {
                    "src": {
                        "src_removal_does_not_trigger_loadstart.html": {
                            "expected": "PASS",
                            "actual": "PASS",
                            "time": 1.1
                        },
                        "notrun.html": {
                            "expected": "NOTRUN",
                            "actual": "SKIP",
                            "time": 1.1
                        }
                    }
                }
            },
            "unexpected-skip.html": {
                "expected": "PASS",
                "actual": "SKIP"
            },
            "unexpected-fail.html": {
                "expected": "PASS",
                "actual": "FAIL"
            },
            "flaky-failed.html": {
                "expected": "PASS FAIL",
                "actual": "FAIL"
            },
            "media-document-audio-repaint.html": {
                "expected": "IMAGE",
                "actual": "IMAGE",
                "time": 0.1
            },
            "unexpected-leak.html": {
                "expected": "PASS",
                "actual": "LEAK"
            }
        }
    },
    "skipped": 2,
    "num_regressions": 0,
    "build_number": "3",
    "interrupted": false,
    "layout_tests_dir": "\/tmp\/cr\/src\/third_party\/WebKit\/LayoutTests",
    "version": 3,
    "builder_name": "Webkit",
    "num_passes": 10,
    "pixel_tests_enabled": true,
    "blink_revision": "1234",
    "has_pretty_patch": true,
    "fixable": 25,
    "num_flaky": 0,
    "num_failures_by_type": {
        "CRASH": 3,
        "MISSING": 0,
        "TEXT": 3,
        "IMAGE": 1,
        "PASS": 10,
        "SKIP": 2,
        "TIMEOUT": 16,
        "IMAGE+TEXT": 0,
        "FAIL": 2,
        "AUDIO": 0,
        "LEAK": 1
    },
    "has_wdiff": true,
    "chromium_revision": "5678"
});"""

JSON_RESULTS_OLD_TEMPLATE = (
    '{"[BUILDER_NAME]":{'
    '"allFixableCount":[[TESTDATA_COUNT]],'
    '"blinkRevision":[[TESTDATA_WEBKITREVISION]],'
    '"buildNumbers":[[TESTDATA_BUILDNUMBERS]],'
    '"chromeRevision":[[TESTDATA_CHROMEREVISION]],'
    '"failure_map": %s,'
    '"fixableCount":[[TESTDATA_COUNT]],'
    '"fixableCounts":[[TESTDATA_COUNTS]],'
    '"secondsSinceEpoch":[[TESTDATA_TIMES]],'
    '"tests":{[TESTDATA_TESTS]}'
    '},'
    '"version":[VERSION]'
    '}') % json.dumps(CHAR_TO_FAILURE)

JSON_RESULTS_COUNTS = '{"' + '":[[TESTDATA_COUNT]],"'.join([char for char in CHAR_TO_FAILURE.values()]) + '":[[TESTDATA_COUNT]]}'

JSON_RESULTS_TEMPLATE = (
    '{"[BUILDER_NAME]":{'
    '"blinkRevision":[[TESTDATA_WEBKITREVISION]],'
    '"buildNumbers":[[TESTDATA_BUILDNUMBERS]],'
    '"chromeRevision":[[TESTDATA_CHROMEREVISION]],'
    '"failure_map": %s,'
    '"num_failures_by_type":%s,'
    '"secondsSinceEpoch":[[TESTDATA_TIMES]],'
    '"tests":{[TESTDATA_TESTS]}'
    '},'
    '"version":[VERSION]'
    '}') % (json.dumps(CHAR_TO_FAILURE), JSON_RESULTS_COUNTS)

JSON_RESULTS_COUNTS_TEMPLATE = '{"' + '":[TESTDATA],"'.join([char for char in CHAR_TO_FAILURE]) + '":[TESTDATA]}'

JSON_RESULTS_TEST_LIST_TEMPLATE = '{"Webkit":{"tests":{[TESTDATA_TESTS]}}}'


class MockFile(object):
    def __init__(self, name='results.json', data=''):
        self.master = 'MockMasterName'
        self.builder = 'MockBuilderName'
        self.test_type = 'MockTestType'
        self.name = name
        self.data = data

    def save(self, data):
        self.data = data
        return True


class JsonResultsTest(unittest.TestCase):
    def setUp(self):
        self._builder = "Webkit"
        self.old_log_level = logging.root.level
        logging.root.setLevel(logging.ERROR)

    def tearDown(self):
        logging.root.setLevel(self.old_log_level)

    # Use this to get better error messages than just string compare gives.
    def assert_json_equal(self, a, b):
        self.maxDiff = None
        a = json.loads(a) if isinstance(a, str) else a
        b = json.loads(b) if isinstance(b, str) else b
        self.assertEqual(a, b)

    def test_strip_prefix_suffix(self):
        json = "['contents']"
        self.assertEqual(JsonResults._strip_prefix_suffix("ADD_RESULTS(" + json + ");"), json)
        self.assertEqual(JsonResults._strip_prefix_suffix(json), json)

    def _make_test_json(self, test_data, json_string=JSON_RESULTS_TEMPLATE, builder_name="Webkit"):
        if not test_data:
            return ""

        builds = test_data["builds"]
        tests = test_data["tests"]
        if not builds or not tests:
            return ""

        counts = []
        build_numbers = []
        webkit_revision = []
        chrome_revision = []
        times = []
        for build in builds:
            counts.append(JSON_RESULTS_COUNTS_TEMPLATE.replace("[TESTDATA]", build))
            build_numbers.append("1000%s" % build)
            webkit_revision.append("2000%s" % build)
            chrome_revision.append("3000%s" % build)
            times.append("100000%s000" % build)

        json_string = json_string.replace("[BUILDER_NAME]", builder_name)
        json_string = json_string.replace("[TESTDATA_COUNTS]", ",".join(counts))
        json_string = json_string.replace("[TESTDATA_COUNT]", ",".join(builds))
        json_string = json_string.replace("[TESTDATA_BUILDNUMBERS]", ",".join(build_numbers))
        json_string = json_string.replace("[TESTDATA_WEBKITREVISION]", ",".join(webkit_revision))
        json_string = json_string.replace("[TESTDATA_CHROMEREVISION]", ",".join(chrome_revision))
        json_string = json_string.replace("[TESTDATA_TIMES]", ",".join(times))

        version = str(test_data["version"]) if "version" in test_data else "4"
        json_string = json_string.replace("[VERSION]", version)
        json_string = json_string.replace("{[TESTDATA_TESTS]}", json.dumps(tests, separators=(',', ':'), sort_keys=True))
        return json_string

    def _test_merge(self, aggregated_data, incremental_data, expected_data, max_builds=jsonresults.JSON_RESULTS_MAX_BUILDS):
        aggregated_results = self._make_test_json(aggregated_data, builder_name=self._builder)
        incremental_json, _ = JsonResults._get_incremental_json(self._builder, self._make_test_json(incremental_data, builder_name=self._builder), is_full_results_format=False)
        merged_results, status_code = JsonResults.merge(self._builder, aggregated_results, incremental_json, num_runs=max_builds, sort_keys=True)

        if expected_data:
            expected_results = self._make_test_json(expected_data, builder_name=self._builder)
            self.assert_json_equal(merged_results, expected_results)
            self.assertEqual(status_code, 200)
        else:
            self.assertTrue(status_code != 200)

    def _test_get_test_list(self, input_data, expected_data):
        input_results = self._make_test_json(input_data)
        expected_results = JSON_RESULTS_TEST_LIST_TEMPLATE.replace("{[TESTDATA_TESTS]}", json.dumps(expected_data, separators=(',', ':')))
        actual_results = JsonResults.get_test_list(self._builder, input_results)
        self.assert_json_equal(actual_results, expected_results)

    def test_update_files_empty_aggregate_data(self):
        small_file = MockFile(name='results-small.json')
        large_file = MockFile(name='results.json')

        incremental_data = {
            "builds": ["2", "1"],
            "tests": {
                "001.html": {
                    "results": [[200, TEXT]],
                    "times": [[200, 0]],
                }
            }
        }
        incremental_string = self._make_test_json(incremental_data, builder_name=small_file.builder)

        self.assertTrue(JsonResults.update_files(small_file.builder, incremental_string, small_file, large_file, is_full_results_format=False))
        self.assert_json_equal(small_file.data, incremental_string)
        self.assert_json_equal(large_file.data, incremental_string)

    def test_update_files_null_incremental_data(self):
        small_file = MockFile(name='results-small.json')
        large_file = MockFile(name='results.json')

        aggregated_data = {
            "builds": ["2", "1"],
            "tests": {
                "001.html": {
                    "results": [[200, TEXT]],
                    "times": [[200, 0]],
                }
            }
        }
        aggregated_string = self._make_test_json(aggregated_data, builder_name=small_file.builder)

        small_file.data = large_file.data = aggregated_string

        incremental_string = ""

        self.assertEqual(JsonResults.update_files(small_file.builder, incremental_string, small_file, large_file, is_full_results_format=False),
            ('No incremental JSON data to merge.', 403))
        self.assert_json_equal(small_file.data, aggregated_string)
        self.assert_json_equal(large_file.data, aggregated_string)

    def test_update_files_empty_incremental_data(self):
        small_file = MockFile(name='results-small.json')
        large_file = MockFile(name='results.json')

        aggregated_data = {
            "builds": ["2", "1"],
            "tests": {
                "001.html": {
                    "results": [[200, TEXT]],
                    "times": [[200, 0]],
                }
            }
        }
        aggregated_string = self._make_test_json(aggregated_data, builder_name=small_file.builder)

        small_file.data = large_file.data = aggregated_string

        incremental_data = {
            "builds": [],
            "tests": {}
        }
        incremental_string = self._make_test_json(incremental_data, builder_name=small_file.builder)

        self.assertEqual(JsonResults.update_files(small_file.builder, incremental_string, small_file, large_file, is_full_results_format=False),
            ('No incremental JSON data to merge.', 403))
        self.assert_json_equal(small_file.data, aggregated_string)
        self.assert_json_equal(large_file.data, aggregated_string)

    def test_merge_with_empty_aggregated_results(self):
        incremental_data = {
            "builds": ["2", "1"],
            "tests": {
                "001.html": {
                    "results": [[200, TEXT]],
                    "times": [[200, 0]],
                }
            }
        }
        incremental_results, _ = JsonResults._get_incremental_json(self._builder, self._make_test_json(incremental_data), is_full_results_format=False)
        aggregated_results = ""
        merged_results, _ = JsonResults.merge(self._builder, aggregated_results, incremental_results, num_runs=jsonresults.JSON_RESULTS_MAX_BUILDS, sort_keys=True)
        self.assert_json_equal(merged_results, incremental_results)

    def test_failures_by_type_added(self):
        aggregated_results = self._make_test_json({
            "builds": ["2", "1"],
            "tests": {
                "001.html": {
                    "results": [[100, TEXT], [100, FAIL]],
                    "times": [[200, 0]],
                }
            }
        }, json_string=JSON_RESULTS_OLD_TEMPLATE)
        incremental_results = self._make_test_json({
            "builds": ["3"],
            "tests": {
                "001.html": {
                    "results": [[1, TEXT]],
                    "times": [[1, 0]],
                }
            }
        }, json_string=JSON_RESULTS_OLD_TEMPLATE)
        incremental_json, _ = JsonResults._get_incremental_json(self._builder, incremental_results, is_full_results_format=False)
        merged_results, _ = JsonResults.merge(self._builder, aggregated_results, incremental_json, num_runs=201, sort_keys=True)
        self.assert_json_equal(merged_results, self._make_test_json({
            "builds": ["3", "2", "1"],
            "tests": {
                "001.html": {
                    "results": [[101, TEXT], [100, FAIL]],
                    "times": [[201, 0]],
                }
            }
        }))

    def test_merge_full_results_format(self):
        expected_incremental_results = {
            "Webkit": {
                "blinkRevision": ["1234"],
                "buildNumbers": ["3"],
                "chromeRevision": ["5678"],
                "failure_map": CHAR_TO_FAILURE,
                "num_failures_by_type": {"AUDIO": [0], "CRASH": [3], "FAIL": [2], "IMAGE": [1], "IMAGE+TEXT": [0], "MISSING": [0], "PASS": [10], "SKIP": [2], "TEXT": [3], "TIMEOUT": [16], "LEAK": [1]},
                "secondsSinceEpoch": [1368146629],
                "tests": {
                    "media": {
                        "W3C": {
                            "audio": {
                                "src": {
                                    "src_removal_does_not_trigger_loadstart.html": {
                                        "results": [[1, PASS]],
                                        "times": [[1, 4]],
                                    }
                                }
                            }
                        },
                        "encrypted-media": {
                            "encrypted-media-v2-events.html": {
                                "bugs": ["crbug.com/1234"],
                                "expected": "TIMEOUT",
                                "results": [[1, TIMEOUT]],
                                "times": [[1, 6]],
                            },
                            "encrypted-media-v2-syntax.html": {
                                "expected": "TIMEOUT",
                                "results": [[1, TIMEOUT]],
                                "times": [[1, 0]],
                            }
                        },
                        "media-document-audio-repaint.html": {
                            "expected": "IMAGE",
                            "results": [[1, IMAGE]],
                            "times": [[1, 0]],
                        },
                        "progress-events-generated-correctly.html": {
                            "expected": "PASS FAIL IMAGE TIMEOUT CRASH MISSING",
                            "results": [[1, TIMEOUT]],
                            "times": [[1, 6]],
                        },
                        "flaky-failed.html": {
                            "expected": "PASS FAIL",
                            "results": [[1, FAIL]],
                            "times": [[1, 0]],
                        },
                        "unexpected-fail.html": {
                            "results": [[1, FAIL]],
                            "times": [[1, 0]],
                        },
                        "unexpected-leak.html": {
                            "results": [[1, LEAK]],
                            "times": [[1, 0]],
                        },
                    }
                }
            },
            "version": 4
        }

        aggregated_results = ""
        incremental_json, _ = JsonResults._get_incremental_json(self._builder, FULL_RESULT_EXAMPLE, is_full_results_format=True)
        merged_results, _ = JsonResults.merge("Webkit", aggregated_results, incremental_json, num_runs=jsonresults.JSON_RESULTS_MAX_BUILDS, sort_keys=True)
        self.assert_json_equal(merged_results, expected_incremental_results)

    def test_merge_empty_aggregated_results(self):
        # No existing aggregated results.
        # Merged results == new incremental results.
        self._test_merge(
            # Aggregated results
            None,
            # Incremental results
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": [[200, TEXT]],
                           "times": [[200, 0]]}}},
            # Expected result
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": [[200, TEXT]],
                           "times": [[200, 0]]}}})

    def test_merge_duplicate_build_number(self):
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": [[100, TEXT]],
                           "times": [[100, 0]]}}},
            # Incremental results
            {"builds": ["2"],
             "tests": {"001.html": {
                           "results": [[1, TEXT]],
                           "times": [[1, 0]]}}},
            # Expected results
            None)

    def test_merge_incremental_single_test_single_run_same_result(self):
        # Incremental results has the latest build and same test results for
        # that run.
        # Insert the incremental results at the first place and sum number
        # of runs for TEXT (200 + 1) to get merged results.
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": [[200, TEXT]],
                           "times": [[200, 0]]}}},
            # Incremental results
            {"builds": ["3"],
             "tests": {"001.html": {
                           "results": [[1, TEXT]],
                           "times": [[1, 0]]}}},
            # Expected results
            {"builds": ["3", "2", "1"],
             "tests": {"001.html": {
                           "results": [[201, TEXT]],
                           "times": [[201, 0]]}}})

    def test_merge_single_test_single_run_different_result(self):
        # Incremental results has the latest build but different test results
        # for that run.
        # Insert the incremental results at the first place.
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": [[200, TEXT]],
                           "times": [[200, 0]]}}},
            # Incremental results
            {"builds": ["3"],
             "tests": {"001.html": {
                           "results": [[1, IMAGE]],
                           "times": [[1, 1]]}}},
            # Expected results
            {"builds": ["3", "2", "1"],
             "tests": {"001.html": {
                           "results": [[1, IMAGE], [200, TEXT]],
                           "times": [[1, 1], [200, 0]]}}})

    def test_merge_single_test_single_run_result_changed(self):
        # Incremental results has the latest build but results which differ from
        # the latest result (but are the same as an older result).
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": [[200, TEXT], [10, IMAGE]],
                           "times": [[200, 0], [10, 1]]}}},
            # Incremental results
            {"builds": ["3"],
             "tests": {"001.html": {
                           "results": [[1, IMAGE]],
                           "times": [[1, 1]]}}},
            # Expected results
            {"builds": ["3", "2", "1"],
             "tests": {"001.html": {
                           "results": [[1, IMAGE], [200, TEXT], [10, IMAGE]],
                           "times": [[1, 1], [200, 0], [10, 1]]}}})

    def test_merge_multiple_tests_single_run(self):
        # All tests have incremental updates.
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": [[200, TEXT]],
                           "times": [[200, 0]]},
                       "002.html": {
                           "results": [[100, IMAGE]],
                           "times": [[100, 1]]}}},
            # Incremental results
            {"builds": ["3"],
             "tests": {"001.html": {
                           "results": [[1, TEXT]],
                           "times": [[1, 0]]},
                       "002.html": {
                           "results": [[1, IMAGE]],
                           "times": [[1, 1]]}}},
            # Expected results
            {"builds": ["3", "2", "1"],
             "tests": {"001.html": {
                           "results": [[201, TEXT]],
                           "times": [[201, 0]]},
                       "002.html": {
                           "results": [[101, IMAGE]],
                           "times": [[101, 1]]}}})

    def test_merge_multiple_tests_single_run_one_no_result(self):
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": [[200, TEXT]],
                           "times": [[200, 0]]},
                       "002.html": {
                           "results": [[100, IMAGE]],
                           "times": [[100, 1]]}}},
            # Incremental results
            {"builds": ["3"],
             "tests": {"002.html": {
                           "results": [[1, IMAGE]],
                           "times": [[1, 1]]}}},
            # Expected results
            {"builds": ["3", "2", "1"],
             "tests": {"001.html": {
                           "results": [[1, NO_DATA], [200, TEXT]],
                           "times": [[201, 0]]},
                       "002.html": {
                           "results": [[101, IMAGE]],
                           "times": [[101, 1]]}}})

    def test_merge_single_test_multiple_runs(self):
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": [[200, TEXT]],
                           "times": [[200, 0]]}}},
            # Incremental results
            {"builds": ["4", "3"],
             "tests": {"001.html": {
                           "results": [[2, IMAGE], [1, FAIL]],
                           "times": [[3, 2]]}}},
            # Expected results
            {"builds": ["4", "3", "2", "1"],
             "tests": {"001.html": {
                           "results": [[1, FAIL], [2, IMAGE], [200, TEXT]],
                           "times": [[3, 2], [200, 0]]}}})

    def test_merge_multiple_tests_multiple_runs(self):
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": [[200, TEXT]],
                           "times": [[200, 0]]},
                       "002.html": {
                           "results": [[10, IMAGE_PLUS_TEXT]],
                           "times": [[10, 0]]}}},
            # Incremental results
            {"builds": ["4", "3"],
             "tests": {"001.html": {
                           "results": [[2, IMAGE]],
                           "times": [[2, 2]]},
                       "002.html": {
                           "results": [[1, CRASH]],
                           "times": [[1, 1]]}}},
            # Expected results
            {"builds": ["4", "3", "2", "1"],
             "tests": {"001.html": {
                           "results": [[2, IMAGE], [200, TEXT]],
                           "times": [[2, 2], [200, 0]]},
                       "002.html": {
                           "results": [[1, CRASH], [10, IMAGE_PLUS_TEXT]],
                           "times": [[1, 1], [10, 0]]}}})

    def test_merge_incremental_result_older_build(self):
        # Test the build in incremental results is older than the most recent
        # build in aggregated results.
        self._test_merge(
            # Aggregated results
            {"builds": ["3", "1"],
             "tests": {"001.html": {
                           "results": [[5, TEXT]],
                           "times": [[5, 0]]}}},
            # Incremental results
            {"builds": ["2"],
             "tests": {"001.html": {
                           "results": [[1, TEXT]],
                           "times": [[1, 0]]}}},
            # Expected no merge happens.
            {"builds": ["2", "3", "1"],
             "tests": {"001.html": {
                           "results": [[6, TEXT]],
                           "times": [[6, 0]]}}})

    def test_merge_incremental_result_same_build(self):
        # Test the build in incremental results is same as the build in
        # aggregated results.
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": [[5, TEXT]],
                           "times": [[5, 0]]}}},
            # Incremental results
            {"builds": ["3", "2"],
             "tests": {"001.html": {
                           "results": [[2, TEXT]],
                           "times": [[2, 0]]}}},
            # Expected no merge happens.
            {"builds": ["3", "2", "2", "1"],
             "tests": {"001.html": {
                           "results": [[7, TEXT]],
                           "times": [[7, 0]]}}})

    def test_merge_remove_new_test(self):
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": [[199, TEXT]],
                           "times": [[199, 0]]},
                       }},
            # Incremental results
            {"builds": ["3"],
             "tests": {"001.html": {
                           "results": [[1, TEXT]],
                           "times": [[1, 0]]},
                       "002.html": {
                           "results": [[1, PASS]],
                           "times": [[1, 0]]},
                       "notrun.html": {
                           "results": [[1, NOTRUN]],
                           "times": [[1, 0]]},
                       "003.html": {
                           "results": [[1, NO_DATA]],
                           "times": [[1, 0]]},
                        }},
            # Expected results
            {"builds": ["3", "2", "1"],
             "tests": {"001.html": {
                           "results": [[200, TEXT]],
                           "times": [[200, 0]]},
                       }},
            max_builds=200)

    def test_merge_remove_test(self):
        self._test_merge(
            # Aggregated results
            {
                "builds": ["2", "1"],
                "tests": {
                    "directory": {
                        "directory": {
                            "001.html": {
                                "results": [[200, PASS]],
                                "times": [[200, 0]]
                            }
                        }
                    },
                    "002.html": {
                        "results": [[10, TEXT]],
                        "times": [[10, 0]]
                    },
                    "003.html": {
                        "results": [[190, PASS], [9, NO_DATA], [1, TEXT]],
                        "times": [[200, 0]]
                    },
                }
            },
            # Incremental results
            {
                "builds": ["3"],
                "tests": {
                    "directory": {
                        "directory": {
                            "001.html": {
                                "results": [[1, PASS]],
                                "times": [[1, 0]]
                            }
                        }
                    },
                    "002.html": {
                        "results": [[1, PASS]],
                        "times": [[1, 0]]
                    },
                    "003.html": {
                        "results": [[1, PASS]],
                        "times": [[1, 0]]
                    },
                }
            },
            # Expected results
            {
                "builds": ["3", "2", "1"],
                "tests": {
                    "002.html": {
                        "results": [[1, PASS], [10, TEXT]],
                        "times": [[11, 0]]
                    }
                }
            },
            max_builds=200)

    def test_merge_updates_expected(self):
        self._test_merge(
            # Aggregated results
            {
                "builds": ["2", "1"],
                "tests": {
                    "directory": {
                        "directory": {
                            "001.html": {
                                "expected": "FAIL",
                                "results": [[200, PASS]],
                                "times": [[200, 0]]
                            }
                        }
                    },
                    "002.html": {
                        "bugs": ["crbug.com/1234"],
                        "expected": "FAIL",
                        "results": [[10, TEXT]],
                        "times": [[10, 0]]
                    },
                    "003.html": {
                        "expected": "FAIL",
                        "results": [[190, PASS], [9, NO_DATA], [1, TEXT]],
                        "times": [[200, 0]]
                    },
                    "004.html": {
                        "results": [[199, PASS], [1, TEXT]],
                        "times": [[200, 0]]
                    },
                }
            },
            # Incremental results
            {
                "builds": ["3"],
                "tests": {
                    "002.html": {
                        "expected": "PASS",
                        "results": [[1, PASS]],
                        "times": [[1, 0]]
                    },
                    "003.html": {
                        "expected": "TIMEOUT",
                        "results": [[1, PASS]],
                        "times": [[1, 0]]
                    },
                    "004.html": {
                        "bugs": ["crbug.com/1234"],
                        "results": [[1, PASS]],
                        "times": [[1, 0]]
                    },
                }
            },
            # Expected results
            {
                "builds": ["3", "2", "1"],
                "tests": {
                    "002.html": {
                        "results": [[1, PASS], [10, TEXT]],
                        "times": [[11, 0]]
                    },
                    "003.html": {
                        "expected": "TIMEOUT",
                        "results": [[191, PASS], [9, NO_DATA]],
                        "times": [[200, 0]]
                    },
                    "004.html": {
                        "bugs": ["crbug.com/1234"],
                        "results": [[200, PASS]],
                        "times": [[200, 0]]
                    },
                }
            },
            max_builds=200)


    def test_merge_keep_test_with_all_pass_but_slow_time(self):
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": [[200, PASS]],
                           "times": [[200, jsonresults.JSON_RESULTS_MIN_TIME]]},
                       "002.html": {
                           "results": [[10, TEXT]],
                           "times": [[10, 0]]}}},
            # Incremental results
            {"builds": ["3"],
             "tests": {"001.html": {
                           "results": [[1, PASS]],
                           "times": [[1, 1]]},
                       "002.html": {
                           "results": [[1, PASS]],
                           "times": [[1, 0]]}}},
            # Expected results
            {"builds": ["3", "2", "1"],
             "tests": {"001.html": {
                           "results": [[201, PASS]],
                           "times": [[1, 1], [200, jsonresults.JSON_RESULTS_MIN_TIME]]},
                       "002.html": {
                           "results": [[1, PASS], [10, TEXT]],
                           "times": [[11, 0]]}}})

    def test_merge_pruning_slow_tests_for_debug_builders(self):
        self._builder = "MockBuilder(dbg)"
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": [[200, PASS]],
                           "times": [[200, 3 * jsonresults.JSON_RESULTS_MIN_TIME]]},
                       "002.html": {
                           "results": [[10, TEXT]],
                           "times": [[10, 0]]}}},
            # Incremental results
            {"builds": ["3"],
             "tests": {"001.html": {
                           "results": [[1, PASS]],
                           "times": [[1, 1]]},
                       "002.html": {
                           "results": [[1, PASS]],
                           "times": [[1, 0]]},
                       "003.html": {
                           "results": [[1, PASS]],
                           "times": [[1, jsonresults.JSON_RESULTS_MIN_TIME]]}}},
            # Expected results
            {"builds": ["3", "2", "1"],
             "tests": {"001.html": {
                           "results": [[201, PASS]],
                           "times": [[1, 1], [200, 3 * jsonresults.JSON_RESULTS_MIN_TIME]]},
                       "002.html": {
                           "results": [[1, PASS], [10, TEXT]],
                           "times": [[11, 0]]}}})

    def test_merge_prune_extra_results(self):
        # Remove items from test results and times that exceed the max number
        # of builds to track.
        max_builds = jsonresults.JSON_RESULTS_MAX_BUILDS
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": [[max_builds, TEXT], [1, IMAGE]],
                           "times": [[max_builds, 0], [1, 1]]}}},
            # Incremental results
            {"builds": ["3"],
             "tests": {"001.html": {
                           "results": [[1, TIMEOUT]],
                           "times": [[1, 1]]}}},
            # Expected results
            {"builds": ["3", "2", "1"],
             "tests": {"001.html": {
                           "results": [[1, TIMEOUT], [max_builds, TEXT]],
                           "times": [[1, 1], [max_builds, 0]]}}})

    def test_merge_prune_extra_results_small(self):
        # Remove items from test results and times that exceed the max number
        # of builds to track, using smaller threshold.
        max_builds = jsonresults.JSON_RESULTS_MAX_BUILDS_SMALL
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": [[max_builds, TEXT], [1, IMAGE]],
                           "times": [[max_builds, 0], [1, 1]]}}},
            # Incremental results
            {"builds": ["3"],
             "tests": {"001.html": {
                           "results": [[1, TIMEOUT]],
                           "times": [[1, 1]]}}},
            # Expected results
            {"builds": ["3", "2", "1"],
             "tests": {"001.html": {
                           "results": [[1, TIMEOUT], [max_builds, TEXT]],
                           "times": [[1, 1], [max_builds, 0]]}}},
            int(max_builds))

    def test_merge_prune_extra_results_with_new_result_of_same_type(self):
        # Test that merging in a new result of the same type as the last result
        # causes old results to fall off.
        max_builds = jsonresults.JSON_RESULTS_MAX_BUILDS_SMALL
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": [[max_builds, TEXT], [1, NO_DATA]],
                           "times": [[max_builds, 0], [1, 1]]}}},
            # Incremental results
            {"builds": ["3"],
             "tests": {"001.html": {
                           "results": [[1, TEXT]],
                           "times": [[1, 0]]}}},
            # Expected results
            {"builds": ["3", "2", "1"],
             "tests": {"001.html": {
                           "results": [[max_builds, TEXT]],
                           "times": [[max_builds, 0]]}}},
            int(max_builds))

    def test_merge_build_directory_hierarchy(self):
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"bar": {"baz": {
                           "003.html": {
                                "results": [[25, TEXT]],
                                "times": [[25, 0]]}}},
                       "foo": {
                           "001.html": {
                                "results": [[50, TEXT]],
                                "times": [[50, 0]]},
                           "002.html": {
                                "results": [[100, IMAGE]],
                                "times": [[100, 0]]}}},
              "version": 4},
            # Incremental results
            {"builds": ["3"],
             "tests": {"baz": {
                           "004.html": {
                               "results": [[1, IMAGE]],
                               "times": [[1, 0]]}},
                       "foo": {
                           "001.html": {
                               "results": [[1, TEXT]],
                               "times": [[1, 0]]},
                           "002.html": {
                               "results": [[1, IMAGE]],
                               "times": [[1, 0]]}}},
             "version": 4},
            # Expected results
            {"builds": ["3", "2", "1"],
             "tests": {"bar": {"baz": {
                           "003.html": {
                               "results": [[1, NO_DATA], [25, TEXT]],
                               "times": [[26, 0]]}}},
                       "baz": {
                           "004.html": {
                               "results": [[1, IMAGE]],
                               "times": [[1, 0]]}},
                       "foo": {
                           "001.html": {
                               "results": [[51, TEXT]],
                               "times": [[51, 0]]},
                           "002.html": {
                               "results": [[101, IMAGE]],
                               "times": [[101, 0]]}}},
             "version": 4})

    # FIXME(aboxhall): Add some tests for xhtml/svg test results.

    def test_get_test_name_list(self):
        # Get test name list only. Don't include non-test-list data and
        # of test result details.
        # FIXME: This also tests a temporary bug in the data where directory-level
        # results have a results and times values. Once that bug is fixed,
        # remove this test-case and assert we don't ever hit it.
        self._test_get_test_list(
            # Input results
            {"builds": ["3", "2", "1"],
             "tests": {"foo": {
                           "001.html": {
                               "results": [[200, PASS]],
                               "times": [[200, 0]]},
                           "results": [[1, NO_DATA]],
                           "times": [[1, 0]]},
                       "002.html": {
                           "results": [[10, TEXT]],
                           "times": [[10, 0]]}}},
            # Expected results
            {"foo": {"001.html": {}}, "002.html": {}})

    def test_gtest(self):
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"foo.bar": {
                           "results": [[50, TEXT]],
                           "times": [[50, 0]]},
                       "foo.bar2": {
                           "results": [[100, IMAGE]],
                           "times": [[100, 0]]},
                       "test.failed": {
                           "results": [[5, FAIL]],
                           "times": [[5, 0]]},
                       },
             "version": 3},
            # Incremental results
            {"builds": ["3"],
             "tests": {"foo.bar2": {
                           "results": [[1, IMAGE]],
                           "times": [[1, 0]]},
                       "foo.bar3": {
                           "results": [[1, TEXT]],
                           "times": [[1, 0]]},
                       "test.failed": {
                           "results": [[5, FAIL]],
                           "times": [[5, 0]]},
                       },
             "version": 4},
            # Expected results
            {"builds": ["3", "2", "1"],
             "tests": {"foo.bar": {
                           "results": [[1, NO_DATA], [50, TEXT]],
                           "times": [[51, 0]]},
                       "foo.bar2": {
                           "results": [[101, IMAGE]],
                           "times": [[101, 0]]},
                       "foo.bar3": {
                           "results": [[1, TEXT]],
                           "times": [[1, 0]]},
                       "test.failed": {
                           "results": [[10, FAIL]],
                           "times": [[10, 0]]},
                       },
             "version": 4})

if __name__ == '__main__':
    unittest.main()
