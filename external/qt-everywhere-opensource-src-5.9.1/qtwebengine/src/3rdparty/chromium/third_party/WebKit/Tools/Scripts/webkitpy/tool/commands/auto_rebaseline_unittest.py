# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse

from webkitpy.common.net.buildbot import Build
from webkitpy.common.net.layouttestresults import LayoutTestResults
from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.layout_tests.builder_list import BuilderList
from webkitpy.tool.commands.auto_rebaseline import AutoRebaseline
from webkitpy.tool.commands.rebaseline_unittest import BaseTestCase
from webkitpy.tool.commands.rebaseline_unittest import MockLineRemovingExecutive


class TestAutoRebaseline(BaseTestCase):
    command_constructor = AutoRebaseline

    def _write_test_file(self, port, path, contents):
        abs_path = self.tool.filesystem.join(port.layout_tests_dir(), path)
        self.tool.filesystem.write_text_file(abs_path, contents)

    def _execute_with_mock_options(self, auth_refresh_token_json=None, commit_author=None, dry_run=False):
        self.command.execute(
            optparse.Values({
                'optimize': True,
                'verbose': False,
                'results_directory': False,
                'auth_refresh_token_json': auth_refresh_token_json,
                'commit_author': commit_author,
                'dry_run': dry_run
            }),
            [],
            self.tool)

    def setUp(self):
        super(TestAutoRebaseline, self).setUp()
        self.tool.builders = BuilderList({
            "MOCK Mac10.10": {"port_name": "test-mac-mac10.10", "specifiers": ["Mac10.10", "Release"]},
            "MOCK Mac10.11": {"port_name": "test-mac-mac10.11", "specifiers": ["Mac10.11", "Release"]},
            "MOCK Precise": {"port_name": "test-linux-precise", "specifiers": ["Precise", "Release"]},
            "MOCK Trusty": {"port_name": "test-linux-trusty", "specifiers": ["Trusty", "Release"]},
            "MOCK Win7": {"port_name": "test-win-win7", "specifiers": ["Win7", "Release"]},
            "MOCK Win7 (dbg)": {"port_name": "test-win-win7", "specifiers": ["Win7", "Debug"]},
        })
        self.command.latest_revision_processed_on_all_bots = lambda: 9000
        self.command.bot_revision_data = lambda scm: [{"builder": "MOCK Win7", "revision": "9000"}]

    def test_release_builders(self):
        # Testing private method - pylint: disable=protected-access
        self.tool.builders = BuilderList({
            "MOCK Mac10.10": {"port_name": "test-mac-mac10.10", "specifiers": ["Mac10.10", "Release"]},
            "MOCK Mac10.11 (dbg)": {"port_name": "test-mac-mac10.11", "specifiers": ["Mac10.11", "Debug"]},
        })
        self.assertEqual(self.command._release_builders(), ['MOCK Mac10.10'])

    def test_tests_to_rebaseline(self):
        def blame(_):
            return """
624c3081c0 path/to/TestExpectations                   (<foobarbaz1@chromium.org> 2013-06-14 20:18:46 +0000   11) crbug.com/24182 [ Debug ] path/to/norebaseline.html [ Failure ]
624c3081c0 path/to/TestExpectations                   (<foobarbaz1@chromium.org@bbb929c8-8fbe-4397-9dbb-9b2b20218538> 2013-06-14 20:18:46 +0000   11) crbug.com/24182 [ Debug ] path/to/norebaseline-email-with-hash.html [ Failure ]
624c3081c0 path/to/TestExpectations                   (<foobarbaz1@chromium.org> 2013-04-28 04:52:41 +0000   13) Bug(foo) path/to/rebaseline-without-bug-number.html [ NeedsRebaseline ]
624c3081c0 path/to/TestExpectations                   (<foobarbaz1@chromium.org> 2013-06-14 20:18:46 +0000   11) crbug.com/24182 [ Debug ] path/to/rebaseline-with-modifiers.html [ NeedsRebaseline ]
624c3081c0 path/to/TestExpectations                   (<foobarbaz1@chromium.org> 2013-04-28 04:52:41 +0000   12) crbug.com/24182 crbug.com/234 path/to/rebaseline-without-modifiers.html [ NeedsRebaseline ]
6469e754a1 path/to/TestExpectations                   (<foobarbaz1@chromium.org@bbb929c8-8fbe-4397-9dbb-9b2b20218538> 2013-04-28 04:52:41 +0000   12) crbug.com/24182 path/to/rebaseline-new-revision.html [ NeedsRebaseline ]
624caaaaaa path/to/TestExpectations                   (<foo@chromium.org>        2013-04-28 04:52:41 +0000   12) crbug.com/24182 path/to/not-cycled-through-bots.html [ NeedsRebaseline ]
0000000000 path/to/TestExpectations                   (<foo@chromium.org@@bbb929c8-8fbe-4397-9dbb-9b2b20218538>        2013-04-28 04:52:41 +0000   12) crbug.com/24182 path/to/locally-changed-lined.html [ NeedsRebaseline ]
"""
        self.tool.scm().blame = blame

        min_revision = 9000
        self.assertEqual(self.command.tests_to_rebaseline(self.tool, min_revision, print_revisions=False), (
            set(['path/to/rebaseline-without-bug-number.html',
                 'path/to/rebaseline-with-modifiers.html', 'path/to/rebaseline-without-modifiers.html']),
            5678,
            '624c3081c0',
            'foobarbaz1@chromium.org',
            set(['24182', '234']),
            True))

    def test_tests_to_rebaseline_over_limit(self):
        def blame(_):
            result = ""
            for i in range(0, self.command.MAX_LINES_TO_REBASELINE + 1):
                result += ("624c3081c0 path/to/TestExpectations                   "
                           "(<foobarbaz1@chromium.org> 2013-04-28 04:52:41 +0000   13) "
                           "crbug.com/24182 path/to/rebaseline-%s.html [ NeedsRebaseline ]\n" % i)
            return result
        self.tool.scm().blame = blame

        expected_list_of_tests = []
        for i in range(0, self.command.MAX_LINES_TO_REBASELINE):
            expected_list_of_tests.append("path/to/rebaseline-%s.html" % i)

        min_revision = 9000
        self.assertEqual(self.command.tests_to_rebaseline(self.tool, min_revision, print_revisions=False), (
            set(expected_list_of_tests),
            5678,
            '624c3081c0',
            'foobarbaz1@chromium.org',
            set(['24182']),
            True))

    def test_commit_message(self):
        author = "foo@chromium.org"
        revision = 1234
        commit = "abcd567"
        bugs = set()
        self.assertEqual(self.command.commit_message(author, revision, commit, bugs),
                         """Auto-rebaseline for r1234

https://chromium.googlesource.com/chromium/src/+/abcd567

TBR=foo@chromium.org
""")

        bugs = set(["234", "345"])
        self.assertEqual(self.command.commit_message(author, revision, commit, bugs),
                         """Auto-rebaseline for r1234

https://chromium.googlesource.com/chromium/src/+/abcd567

BUG=234,345
TBR=foo@chromium.org
""")

    def test_no_needs_rebaseline_lines(self):
        def blame(_):
            return """
6469e754a1 path/to/TestExpectations                   (<foobarbaz1@chromium.org> 2013-06-14 20:18:46 +0000   11) crbug.com/24182 [ Debug ] path/to/norebaseline.html [ Failure ]
"""
        self.tool.scm().blame = blame

        self._execute_with_mock_options()
        self.assertEqual(self.tool.executive.calls, [])

    def test_execute(self):
        def blame(_):
            return """
6469e754a1 path/to/TestExpectations                   (<foobarbaz1@chromium.org> 2013-06-14 20:18:46 +0000   11) # Test NeedsRebaseline being in a comment doesn't bork parsing.
6469e754a1 path/to/TestExpectations                   (<foobarbaz1@chromium.org> 2013-06-14 20:18:46 +0000   11) crbug.com/24182 [ Debug ] path/to/norebaseline.html [ Failure ]
6469e754a1 path/to/TestExpectations                   (<foobarbaz1@chromium.org> 2013-04-28 04:52:41 +0000   13) Bug(foo) fast/dom/prototype-taco.html [ NeedsRebaseline ]
6469e754a1 path/to/TestExpectations                   (<foobarbaz1@chromium.org> 2013-06-14 20:18:46 +0000   11) crbug.com/24182 [ Mac10.11 ] fast/dom/prototype-strawberry.html [ NeedsRebaseline ]
6469e754a1 path/to/TestExpectations                   (<foobarbaz1@chromium.org> 2013-04-28 04:52:41 +0000   12) crbug.com/24182 fast/dom/prototype-chocolate.html [ NeedsRebaseline ]
624caaaaaa path/to/TestExpectations                   (<foo@chromium.org>        2013-04-28 04:52:41 +0000   12) crbug.com/24182 path/to/not-cycled-through-bots.html [ NeedsRebaseline ]
0000000000 path/to/TestExpectations                   (<foo@chromium.org>        2013-04-28 04:52:41 +0000   12) crbug.com/24182 path/to/locally-changed-lined.html [ NeedsRebaseline ]
"""
        self.tool.scm().blame = blame

        test_port = self.tool.port_factory.get('test')

        # Have prototype-chocolate only fail on "MOCK Mac10.11",
        # and pass on "Mock Mac10.10".
        self.tool.buildbot.set_results(Build('MOCK Mac10.11'), LayoutTestResults({
            "tests": {
                "fast": {
                    "dom": {
                        "prototype-taco.html": {
                            "expected": "PASS",
                            "actual": "PASS TEXT",
                            "is_unexpected": True
                        },
                        "prototype-chocolate.html": {
                            "expected": "FAIL",
                            "actual": "PASS"
                        },
                        "prototype-strawberry.html": {
                            "expected": "PASS",
                            "actual": "IMAGE PASS",
                            "is_unexpected": True
                        }
                    }
                }
            }
        }))
        self.tool.buildbot.set_results(Build('MOCK Mac10.10'), LayoutTestResults({
            "tests": {
                "fast": {
                    "dom": {
                        "prototype-taco.html": {
                            "expected": "PASS",
                            "actual": "PASS",
                        },
                        "prototype-chocolate.html": {
                            "expected": "FAIL",
                            "actual": "FAIL"
                        },
                        "prototype-strawberry.html": {
                            "expected": "PASS",
                            "actual": "PASS",
                        }
                    }
                }
            }
        }))

        self.tool.filesystem.write_text_file(test_port.path_to_generic_test_expectations_file(), """
crbug.com/24182 [ Debug ] path/to/norebaseline.html [ Rebaseline ]
Bug(foo) fast/dom/prototype-taco.html [ NeedsRebaseline ]
crbug.com/24182 [ Mac10.11 ] fast/dom/prototype-strawberry.html [ NeedsRebaseline ]
crbug.com/24182 fast/dom/prototype-chocolate.html [ NeedsRebaseline ]
crbug.com/24182 path/to/not-cycled-through-bots.html [ NeedsRebaseline ]
crbug.com/24182 path/to/locally-changed-lined.html [ NeedsRebaseline ]
""")

        self._write_test_file(test_port, 'fast/dom/prototype-taco.html', "Dummy test contents")
        self._write_test_file(test_port, 'fast/dom/prototype-strawberry.html', "Dummy test contents")
        self._write_test_file(test_port, 'fast/dom/prototype-chocolate.html', "Dummy test contents")

        self.tool.executive = MockLineRemovingExecutive()

        self.tool.builders = BuilderList({
            "MOCK Mac10.10": {"port_name": "test-mac-mac10.10", "specifiers": ["Mac10.10", "Release"]},
            "MOCK Mac10.11": {"port_name": "test-mac-mac10.11", "specifiers": ["Mac10.11", "Release"]},
        })

        self.command.tree_status = lambda: 'closed'
        self._execute_with_mock_options()
        self.assertEqual(self.tool.executive.calls, [])

        self.command.tree_status = lambda: 'open'
        self.tool.executive.calls = []
        self._execute_with_mock_options()

        self.assertEqual(self.tool.executive.calls, [
            [
                ['python', 'echo', 'copy-existing-baselines-internal', '--suffixes', 'png',
                 '--builder', 'MOCK Mac10.11', '--test', 'fast/dom/prototype-strawberry.html'],
                ['python', 'echo', 'copy-existing-baselines-internal', '--suffixes', 'txt',
                 '--builder', 'MOCK Mac10.11', '--test', 'fast/dom/prototype-taco.html'],
            ],
            [
                ['python', 'echo', 'rebaseline-test-internal', '--suffixes', 'png',
                 '--builder', 'MOCK Mac10.11', '--test', 'fast/dom/prototype-strawberry.html'],
                ['python', 'echo', 'rebaseline-test-internal', '--suffixes', 'txt',
                 '--builder', 'MOCK Mac10.11', '--test', 'fast/dom/prototype-taco.html'],
            ],
            [
                ['python', 'echo', 'optimize-baselines',
                 '--suffixes', 'png', 'fast/dom/prototype-strawberry.html'],
                ['python', 'echo', 'optimize-baselines',
                 '--suffixes', 'txt', 'fast/dom/prototype-taco.html'],
            ],
            ['git', 'cl', 'upload', '-f'],
            ['git', 'pull'],
            ['git', 'cl', 'land', '-f', '-v'],
            ['git', 'config', 'branch.auto-rebaseline-temporary-branch.rietveldissue'],
        ])

        # The mac ports should both be removed since they're the only ones in builders._exact_matches.
        self.assertEqual(self.tool.filesystem.read_text_file(test_port.path_to_generic_test_expectations_file()), """
crbug.com/24182 [ Debug ] path/to/norebaseline.html [ Rebaseline ]
Bug(foo) [ Linux Win ] fast/dom/prototype-taco.html [ NeedsRebaseline ]
crbug.com/24182 [ Linux Win ] fast/dom/prototype-chocolate.html [ NeedsRebaseline ]
crbug.com/24182 path/to/not-cycled-through-bots.html [ NeedsRebaseline ]
crbug.com/24182 path/to/locally-changed-lined.html [ NeedsRebaseline ]
""")

    def test_execute_git_cl_hangs(self):
        def blame(_):
            return """
6469e754a1 path/to/TestExpectations                   (<foobarbaz1@chromium.org> 2013-04-28 04:52:41 +0000   13) Bug(foo) fast/dom/prototype-taco.html [ NeedsRebaseline ]
"""
        self.tool.scm().blame = blame

        test_port = self.tool.port_factory.get('test')

        # Have prototype-chocolate only fail on "MOCK Mac10.11".
        self.tool.buildbot.set_results(Build('MOCK Mac10.11'), LayoutTestResults({
            "tests": {
                "fast": {
                    "dom": {
                        "prototype-taco.html": {
                            "expected": "PASS",
                            "actual": "PASS TEXT",
                            "is_unexpected": True
                        }
                    }
                }
            }
        }))

        self.tool.filesystem.write_text_file(test_port.path_to_generic_test_expectations_file(), """
Bug(foo) fast/dom/prototype-taco.html [ NeedsRebaseline ]
""")

        self._write_test_file(test_port, 'fast/dom/prototype-taco.html', "Dummy test contents")

        self.tool.builders = BuilderList({
            "MOCK Mac10.11": {"port_name": "test-mac-mac10.11", "specifiers": ["Mac10.11", "Release"]},
        })

        self.command.SECONDS_BEFORE_GIVING_UP = 0
        self.command.tree_status = lambda: 'open'
        self.tool.executive = MockExecutive()
        self.tool.executive.calls = []
        self._execute_with_mock_options()

        self.assertEqual(self.tool.executive.calls, [
            [['python', 'echo', 'copy-existing-baselines-internal', '--suffixes', 'txt',
              '--builder', 'MOCK Mac10.11', '--test', 'fast/dom/prototype-taco.html']],
            [['python', 'echo', 'rebaseline-test-internal', '--suffixes', 'txt',
              '--builder', 'MOCK Mac10.11', '--test', 'fast/dom/prototype-taco.html']],
            [['python', 'echo', 'optimize-baselines', '--suffixes', 'txt', 'fast/dom/prototype-taco.html']],
            ['git', 'cl', 'upload', '-f'],
        ])

    def test_execute_test_passes_everywhere(self):
        def blame(_):
            return """
6469e754a1 path/to/TestExpectations                   (<foobarbaz1@chromium.org> 2013-04-28 04:52:41 +0000   13) Bug(foo) fast/dom/prototype-taco.html [ NeedsRebaseline ]
"""
        self.tool.scm().blame = blame

        test_port = self.tool.port_factory.get('test')

        for builder in ['MOCK Mac10.10', 'MOCK Mac10.11']:
            self.tool.buildbot.set_results(Build(builder), LayoutTestResults({
                "tests": {
                    "fast": {
                        "dom": {
                            "prototype-taco.html": {
                                "expected": "FAIL",
                                "actual": "PASS",
                                "is_unexpected": True
                            }
                        }
                    }
                }
            }))

        self.tool.filesystem.write_text_file(test_port.path_to_generic_test_expectations_file(), """
Bug(foo) fast/dom/prototype-taco.html [ NeedsRebaseline ]
""")

        self._write_test_file(test_port, 'fast/dom/prototype-taco.html', "Dummy test contents")

        self.tool.executive = MockLineRemovingExecutive()

        self.tool.builders = BuilderList({
            "MOCK Mac10.10": {"port_name": "test-mac-mac10.10", "specifiers": ["Mac10.10", "Release"]},
            "MOCK Mac10.11": {"port_name": "test-mac-mac10.11", "specifiers": ["Mac10.11", "Release"]},
        })

        self.command.tree_status = lambda: 'open'
        self._execute_with_mock_options()
        self.assertEqual(self.tool.executive.calls, [
            ['git', 'cl', 'upload', '-f'],
            ['git', 'pull'],
            ['git', 'cl', 'land', '-f', '-v'],
            ['git', 'config', 'branch.auto-rebaseline-temporary-branch.rietveldissue'],
        ])

        # The mac ports should both be removed since they're the only ones in builders._exact_matches.
        self.assertEqual(self.tool.filesystem.read_text_file(test_port.path_to_generic_test_expectations_file()), """
Bug(foo) [ Linux Win ] fast/dom/prototype-taco.html [ NeedsRebaseline ]
""")

    def test_execute_use_alternate_rebaseline_branch(self):
        def blame(_):
            return """
6469e754a1 path/to/TestExpectations                   (<foobarbaz1@chromium.org> 2013-04-28 04:52:41 +0000   13) Bug(foo) fast/dom/prototype-taco.html [ NeedsRebaseline ]
"""
        self.tool.scm().blame = blame

        test_port = self.tool.port_factory.get('test')

        self.tool.buildbot.set_results(Build('MOCK Win7'), LayoutTestResults({
            "tests": {
                "fast": {
                    "dom": {
                        "prototype-taco.html": {
                            "expected": "FAIL",
                            "actual": "PASS",
                            "is_unexpected": True
                        }
                    }
                }
            }
        }))

        self.tool.filesystem.write_text_file(test_port.path_to_generic_test_expectations_file(), """
Bug(foo) fast/dom/prototype-taco.html [ NeedsRebaseline ]
""")

        self._write_test_file(test_port, 'fast/dom/prototype-taco.html', "Dummy test contents")

        self.tool.executive = MockLineRemovingExecutive()

        self.tool.builders = BuilderList({
            "MOCK Win7": {"port_name": "test-win-win7", "specifiers": ["Win7", "Release"]},
        })
        old_branch_name = self.tool.scm().current_branch_or_ref
        try:
            self.command.tree_status = lambda: 'open'
            self.tool.scm().current_branch_or_ref = lambda: 'auto-rebaseline-temporary-branch'
            self._execute_with_mock_options()
            self.assertEqual(self.tool.executive.calls, [
                ['git', 'cl', 'upload', '-f'],
                ['git', 'pull'],
                ['git', 'cl', 'land', '-f', '-v'],
                ['git', 'config', 'branch.auto-rebaseline-alt-temporary-branch.rietveldissue'],
            ])

            self.assertEqual(self.tool.filesystem.read_text_file(test_port.path_to_generic_test_expectations_file()), """
Bug(foo) [ Linux Mac Win10 ] fast/dom/prototype-taco.html [ NeedsRebaseline ]
""")
        finally:
            self.tool.scm().current_branch_or_ref = old_branch_name

    def test_execute_stuck_on_alternate_rebaseline_branch(self):
        def blame(_):
            return """
6469e754a1 path/to/TestExpectations                   (<foobarbaz1@chromium.org> 2013-04-28 04:52:41 +0000   13) Bug(foo) fast/dom/prototype-taco.html [ NeedsRebaseline ]
"""
        self.tool.scm().blame = blame

        test_port = self.tool.port_factory.get('test')

        self.tool.buildbot.set_results(Build('MOCK Win7'), LayoutTestResults({
            "tests": {
                "fast": {
                    "dom": {
                        "prototype-taco.html": {
                            "expected": "FAIL",
                            "actual": "PASS",
                            "is_unexpected": True
                        }
                    }
                }
            }
        }))

        self.tool.filesystem.write_text_file(test_port.path_to_generic_test_expectations_file(), """
Bug(foo) fast/dom/prototype-taco.html [ NeedsRebaseline ]
""")

        self._write_test_file(test_port, 'fast/dom/prototype-taco.html', "Dummy test contents")

        self.tool.executive = MockLineRemovingExecutive()

        self.tool.builders = BuilderList({
            "MOCK Win7": {"port_name": "test-win-win7", "specifiers": ["Win7", "Release"]},
        })
        old_branch_name = self.tool.scm().current_branch_or_ref
        try:
            self.command.tree_status = lambda: 'open'
            self.tool.scm().current_branch_or_ref = lambda: 'auto-rebaseline-alt-temporary-branch'
            self._execute_with_mock_options()
            self.assertEqual(self.tool.executive.calls, [
                ['git', 'cl', 'upload', '-f'],
                ['git', 'pull'],
                ['git', 'cl', 'land', '-f', '-v'],
                ['git', 'config', 'branch.auto-rebaseline-temporary-branch.rietveldissue'],
            ])

            self.assertEqual(self.tool.filesystem.read_text_file(test_port.path_to_generic_test_expectations_file()), """
Bug(foo) [ Linux Mac Win10 ] fast/dom/prototype-taco.html [ NeedsRebaseline ]
""")
        finally:
            self.tool.scm().current_branch_or_ref = old_branch_name

    def _basic_execute_test(self, expected_executive_calls, auth_refresh_token_json=None, commit_author=None, dry_run=False):
        def blame(_):
            return """
6469e754a1 path/to/TestExpectations                   (<foobarbaz1@chromium.org> 2013-04-28 04:52:41 +0000   13) Bug(foo) fast/dom/prototype-taco.html [ NeedsRebaseline ]
"""
        self.tool.scm().blame = blame

        test_port = self.tool.port_factory.get('test')

        for builder in ['MOCK Mac10.10', 'MOCK Mac10.11']:
            self.tool.buildbot.set_results(Build(builder), LayoutTestResults({
                "tests": {
                    "fast": {
                        "dom": {
                            "prototype-taco.html": {
                                "expected": "FAIL",
                                "actual": "PASS",
                                "is_unexpected": True
                            }
                        }
                    }
                }
            }))

        self.tool.filesystem.write_text_file(test_port.path_to_generic_test_expectations_file(), """
Bug(foo) fast/dom/prototype-taco.html [ NeedsRebaseline ]
""")

        self._write_test_file(test_port, 'fast/dom/prototype-taco.html', "Dummy test contents")

        self.tool.executive = MockLineRemovingExecutive()

        self.tool.builders = BuilderList({
            "MOCK Mac10.10": {"port_name": "test-mac-mac10.10", "specifiers": ["Mac10.10", "Release"]},
            "MOCK Mac10.11": {"port_name": "test-mac-mac10.11", "specifiers": ["Mac10.11", "Release"]},
        })

        self.command.tree_status = lambda: 'open'
        self._execute_with_mock_options(auth_refresh_token_json=auth_refresh_token_json,
                                        commit_author=commit_author, dry_run=dry_run)
        self.assertEqual(self.tool.executive.calls, expected_executive_calls)

        # The mac ports should both be removed since they're the only ones in builders._exact_matches.
        self.assertEqual(self.tool.filesystem.read_text_file(test_port.path_to_generic_test_expectations_file()), """
Bug(foo) [ Linux Win ] fast/dom/prototype-taco.html [ NeedsRebaseline ]
""")

    def test_execute_with_rietveld_auth_refresh_token(self):
        rietveld_refresh_token = '/creds/refresh_tokens/test_rietveld_token'
        self._basic_execute_test(
            [
                ['git', 'cl', 'upload', '-f', '--auth-refresh-token-json', rietveld_refresh_token],
                ['git', 'pull'],
                ['git', 'cl', 'land', '-f', '-v', '--auth-refresh-token-json', rietveld_refresh_token],
                ['git', 'config', 'branch.auto-rebaseline-temporary-branch.rietveldissue'],
            ],
            auth_refresh_token_json=rietveld_refresh_token)

    def test_execute_with_dry_run(self):
        self._basic_execute_test([], dry_run=True)
        self.assertEqual(self.tool.scm().local_commits(), [])

    def test_bot_revision_data(self):
        self._setup_mock_build_data()
        self.assertEqual(
            self.command.bot_revision_data(self.tool.scm()),
            [{'builder': 'MOCK Win7', 'revision': '9000'}])
