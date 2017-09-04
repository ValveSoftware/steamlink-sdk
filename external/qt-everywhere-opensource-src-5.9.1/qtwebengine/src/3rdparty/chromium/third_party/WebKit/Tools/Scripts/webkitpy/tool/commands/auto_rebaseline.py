# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A command to download new baselines for NeedsRebaseline tests.

This command checks the list of tests with NeedsRebaseline expectations,
and downloads the latest baselines for those tests from the results archived
by the continuous builders.
"""

import logging
import optparse
import re
import sys
import time
import traceback
import urllib2

from webkitpy.common.net.buildbot import Build
from webkitpy.layout_tests.models.test_expectations import TestExpectations, BASELINE_SUFFIX_LIST
from webkitpy.tool.commands.rebaseline import AbstractParallelRebaselineCommand


_log = logging.getLogger(__name__)


class AutoRebaseline(AbstractParallelRebaselineCommand):
    name = "auto-rebaseline"
    help_text = "Rebaselines any NeedsRebaseline lines in TestExpectations that have cycled through all the bots."
    AUTO_REBASELINE_BRANCH_NAME = "auto-rebaseline-temporary-branch"
    AUTO_REBASELINE_ALT_BRANCH_NAME = "auto-rebaseline-alt-temporary-branch"

    # Rietveld uploader stinks. Limit the number of rebaselines in a given patch to keep upload from failing.
    # FIXME: http://crbug.com/263676 Obviously we should fix the uploader here.
    MAX_LINES_TO_REBASELINE = 200

    SECONDS_BEFORE_GIVING_UP = 300

    def __init__(self):
        super(AutoRebaseline, self).__init__(options=[
            # FIXME: Remove this option.
            self.no_optimize_option,
            # FIXME: Remove this option.
            self.results_directory_option,
            optparse.make_option("--auth-refresh-token-json", help="Rietveld auth refresh JSON token."),
            optparse.make_option("--dry-run", action='store_true', default=False,
                                 help='Run without creating a temporary branch, committing locally, or uploading/landing '
                                 'changes to the remote repository.')
        ])
        self._blame_regex = re.compile(r'''
                ^(\S*)      # Commit hash
                [^(]* \(    # Whitespace and open parenthesis
                <           # Email address is surrounded by <>
                (
                    [^@]+   # Username preceding @
                    @
                    [^@>]+  # Domain terminated by @ or >, some lines have an additional @ fragment after the email.
                )
                .*?([^ ]*)  # Test file name
                \ \[        # Single space followed by opening [ for expectation specifier
                [^[]*$      # Prevents matching previous [ for version specifiers instead of expectation specifiers
            ''', re.VERBOSE)

    def bot_revision_data(self, scm):
        revisions = []
        for builder_name in self._release_builders():
            result = self._tool.buildbot.fetch_results(Build(builder_name))
            if result.run_was_interrupted():
                _log.error("Can't rebaseline because the latest run on %s exited early.", result.builder_name())
                return []
            revisions.append({
                "builder": result.builder_name(),
                "revision": result.chromium_revision(scm),
            })
        return revisions

    @staticmethod
    def _strip_comments(line):
        comment_index = line.find("#")
        if comment_index == -1:
            comment_index = len(line)
        return re.sub(r"\s+", " ", line[:comment_index].strip())

    def tests_to_rebaseline(self, tool, min_revision, print_revisions):
        port = tool.port_factory.get()
        expectations_file_path = port.path_to_generic_test_expectations_file()

        tests = set()
        revision = None
        commit = None
        author = None
        bugs = set()
        has_any_needs_rebaseline_lines = False

        for line in tool.scm().blame(expectations_file_path).split("\n"):
            line = self._strip_comments(line)
            if "NeedsRebaseline" not in line:
                continue

            has_any_needs_rebaseline_lines = True

            parsed_line = self._blame_regex.match(line)
            if not parsed_line:
                # Deal gracefully with inability to parse blame info for a line in TestExpectations.
                # Parsing could fail if for example during local debugging the developer modifies
                # TestExpectations and does not commit.
                _log.info("Couldn't find blame info for expectations line, skipping [line=%s].", line)
                continue

            commit_hash = parsed_line.group(1)
            commit_position = tool.scm().commit_position_from_git_commit(commit_hash)

            test = parsed_line.group(3)
            if print_revisions:
                _log.info("%s is waiting for r%s", test, commit_position)

            if not commit_position or commit_position > min_revision:
                continue

            if revision and commit_position != revision:
                continue

            if not revision:
                revision = commit_position
                commit = commit_hash
                author = parsed_line.group(2)

            bugs.update(re.findall(r"crbug\.com\/(\d+)", line))
            tests.add(test)

            if len(tests) >= self.MAX_LINES_TO_REBASELINE:
                _log.info("Too many tests to rebaseline in one patch. Doing the first %d.", self.MAX_LINES_TO_REBASELINE)
                break

        return tests, revision, commit, author, bugs, has_any_needs_rebaseline_lines

    @staticmethod
    def link_to_patch(commit):
        return "https://chromium.googlesource.com/chromium/src/+/" + commit

    def commit_message(self, author, revision, commit, bugs):
        bug_string = ""
        if bugs:
            bug_string = "BUG=%s\n" % ",".join(bugs)

        return """Auto-rebaseline for r%s

%s

%sTBR=%s
""" % (revision, self.link_to_patch(commit), bug_string, author)

    def get_test_prefix_list(self, tests):
        test_prefix_list = {}
        lines_to_remove = {}

        for builder_name in self._release_builders():
            port_name = self._tool.builders.port_name_for_builder_name(builder_name)
            port = self._tool.port_factory.get(port_name)
            expectations = TestExpectations(port, include_overrides=True)
            for test in expectations.get_needs_rebaseline_failures():
                if test not in tests:
                    continue

                if test not in test_prefix_list:
                    lines_to_remove[test] = []
                    test_prefix_list[test] = {}
                lines_to_remove[test].append(builder_name)
                test_prefix_list[test][Build(builder_name)] = BASELINE_SUFFIX_LIST

        return test_prefix_list, lines_to_remove

    def _run_git_cl_command(self, options, command):
        subprocess_command = ['git', 'cl'] + command
        if options.verbose:
            subprocess_command.append('--verbose')
        if options.auth_refresh_token_json:
            subprocess_command.append('--auth-refresh-token-json')
            subprocess_command.append(options.auth_refresh_token_json)

        process = self._tool.executive.popen(subprocess_command, stdout=self._tool.executive.PIPE,
                                             stderr=self._tool.executive.STDOUT)
        last_output_time = time.time()

        # git cl sometimes completely hangs. Bail if we haven't gotten any output to stdout/stderr in a while.
        while process.poll() is None and time.time() < last_output_time + self.SECONDS_BEFORE_GIVING_UP:
            # FIXME: This doesn't make any sense. readline blocks, so all this code to
            # try and bail is useless. Instead, we should do the readline calls on a
            # subthread. Then the rest of this code would make sense.
            out = process.stdout.readline().rstrip('\n')
            if out:
                last_output_time = time.time()
                _log.info(out)

        if process.poll() is None:
            _log.error('Command hung: %s', subprocess_command)
            return False
        return True

    # FIXME: Move this somewhere more general.
    @staticmethod
    def tree_status():
        blink_tree_status_url = "http://chromium-status.appspot.com/status"
        status = urllib2.urlopen(blink_tree_status_url).read().lower()
        if 'closed' in status or status == "0":
            return 'closed'
        elif 'open' in status or status == "1":
            return 'open'
        return 'unknown'

    def execute(self, options, args, tool):
        self._tool = tool
        if tool.scm().executable_name == "svn":
            _log.error("Auto rebaseline only works with a git checkout.")
            return

        if not options.dry_run and tool.scm().has_working_directory_changes():
            _log.error("Cannot proceed with working directory changes. Clean working directory first.")
            return

        revision_data = self.bot_revision_data(tool.scm())
        if not revision_data:
            return

        min_revision = int(min([item["revision"] for item in revision_data]))
        tests, revision, commit, author, bugs, _ = self.tests_to_rebaseline(
            tool, min_revision, print_revisions=options.verbose)

        if options.verbose:
            _log.info("Min revision across all bots is %s.", min_revision)
            for item in revision_data:
                _log.info("%s: r%s", item["builder"], item["revision"])

        if not tests:
            _log.debug('No tests to rebaseline.')
            return

        if self.tree_status() == 'closed':
            _log.info('Cannot proceed. Tree is closed.')
            return

        _log.info('Rebaselining %s for r%s by %s.', list(tests), revision, author)

        test_prefix_list, _ = self.get_test_prefix_list(tests)

        did_switch_branches = False
        did_finish = False
        old_branch_name_or_ref = ''
        rebaseline_branch_name = self.AUTO_REBASELINE_BRANCH_NAME
        try:
            # Save the current branch name and check out a clean branch for the patch.
            old_branch_name_or_ref = tool.scm().current_branch_or_ref()
            if old_branch_name_or_ref == self.AUTO_REBASELINE_BRANCH_NAME:
                rebaseline_branch_name = self.AUTO_REBASELINE_ALT_BRANCH_NAME
            if not options.dry_run:
                tool.scm().delete_branch(rebaseline_branch_name)
                tool.scm().create_clean_branch(rebaseline_branch_name)
                did_switch_branches = True

            if test_prefix_list:
                self.rebaseline(options, test_prefix_list)

            if options.dry_run:
                return

            tool.scm().commit_locally_with_message(
                self.commit_message(author, revision, commit, bugs))

            # FIXME: It would be nice if we could dcommit the patch without uploading, but still
            # go through all the precommit hooks. For rebaselines with lots of files, uploading
            # takes a long time and sometimes fails, but we don't want to commit if, e.g. the
            # tree is closed.
            did_finish = self._run_git_cl_command(options, ['upload', '-f'])

            if did_finish:
                # Uploading can take a very long time. Do another pull to make sure TestExpectations is up to date,
                # so the dcommit can go through.
                # FIXME: Log the pull and dcommit stdout/stderr to the log-server.
                tool.executive.run_command(['git', 'pull'])

                self._run_git_cl_command(options, ['land', '-f', '-v'])
        except OSError:
            traceback.print_exc(file=sys.stderr)
        finally:
            if did_switch_branches:
                if did_finish:
                    # Close the issue if dcommit failed.
                    issue_already_closed = tool.executive.run_command(
                        ['git', 'config', 'branch.%s.rietveldissue' % rebaseline_branch_name],
                        return_exit_code=True)
                    if not issue_already_closed:
                        self._run_git_cl_command(options, ['set_close'])

                tool.scm().ensure_cleanly_tracking_remote_master()
                if old_branch_name_or_ref:
                    tool.scm().checkout_branch(old_branch_name_or_ref)
                tool.scm().delete_branch(rebaseline_branch_name)
