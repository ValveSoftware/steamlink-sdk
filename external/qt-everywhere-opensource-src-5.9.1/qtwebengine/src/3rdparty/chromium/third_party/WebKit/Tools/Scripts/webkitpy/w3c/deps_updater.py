# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Fetches a copy of the latest state of a W3C test repository and commits.

If this script is given the argument --auto-update, it will also attempt to
upload a CL, triggery try jobs, and make any changes that are required for
new failing tests before committing.
"""

import logging
import argparse
import json

from webkitpy.common.net.git_cl import GitCL
from webkitpy.common.webkit_finder import WebKitFinder
from webkitpy.layout_tests.models.test_expectations import TestExpectations, TestExpectationParser

# Import destination directories (under LayoutTests/imported/).
WPT_DEST_NAME = 'wpt'
CSS_DEST_NAME = 'csswg-test'

# Our mirrors of the official w3c repos, which we pull from.
WPT_REPO_URL = 'https://chromium.googlesource.com/external/w3c/web-platform-tests.git'
CSS_REPO_URL = 'https://chromium.googlesource.com/external/w3c/csswg-test.git'


_log = logging.getLogger(__file__)


class DepsUpdater(object):

    def __init__(self, host):
        self.host = host
        self.executive = host.executive
        self.fs = host.filesystem
        self.finder = WebKitFinder(self.fs)
        self.verbose = False
        self.git_cl = None

    def main(self, argv=None):
        options = self.parse_args(argv)
        self.verbose = options.verbose
        log_level = logging.DEBUG if self.verbose else logging.INFO
        logging.basicConfig(level=log_level, format='%(message)s')

        if not self.checkout_is_okay(options.allow_local_commits):
            return 1

        self.git_cl = GitCL(self.host, auth_refresh_token_json=options.auth_refresh_token_json)

        _log.info('Noting the current Chromium commit.')
        _, show_ref_output = self.run(['git', 'show-ref', 'HEAD'])
        chromium_commitish = show_ref_output.split()[0]

        if options.target == 'wpt':
            import_commitish = self.update(WPT_DEST_NAME, WPT_REPO_URL, options.keep_w3c_repos_around, options.revision)
            self._copy_resources()
        elif options.target == 'css':
            import_commitish = self.update(CSS_DEST_NAME, CSS_REPO_URL, options.keep_w3c_repos_around, options.revision)
        else:
            raise AssertionError("Unsupported target %s" % options.target)

        has_changes = self.commit_changes_if_needed(chromium_commitish, import_commitish)
        if options.auto_update and has_changes:
            commit_successful = self.do_auto_update()
            if not commit_successful:
                return 1
        return 0

    def parse_args(self, argv):
        parser = argparse.ArgumentParser()
        parser.description = __doc__
        parser.add_argument('-v', '--verbose', action='store_true',
                            help='log what we are doing')
        parser.add_argument('--allow-local-commits', action='store_true',
                            help='allow script to run even if we have local commits')
        parser.add_argument('--keep-w3c-repos-around', action='store_true',
                            help='leave the w3c repos around that were imported previously.')
        parser.add_argument('-r', dest='revision', action='store',
                            help='Target revision.')
        parser.add_argument('target', choices=['css', 'wpt'],
                            help='Target repository.  "css" for csswg-test, "wpt" for web-platform-tests.')
        parser.add_argument('--auto-update', action='store_true',
                            help='uploads CL and initiates commit queue.')
        parser.add_argument('--auth-refresh-token-json',
                            help='Rietveld auth refresh JSON token.')
        return parser.parse_args(argv)

    def checkout_is_okay(self, allow_local_commits):
        git_diff_retcode, _ = self.run(['git', 'diff', '--quiet', 'HEAD'], exit_on_failure=False)
        if git_diff_retcode:
            _log.warning('Checkout is dirty; aborting.')
            return False

        local_commits = self.run(['git', 'log', '--oneline', 'origin/master..HEAD'])[1]
        if local_commits and not allow_local_commits:
            _log.warning('Checkout has local commits; aborting. Use --allow-local-commits to allow this.')
            return False

        if self.fs.exists(self.path_from_webkit_base(WPT_DEST_NAME)):
            _log.warning('WebKit/%s exists; aborting.', WPT_DEST_NAME)
            return False

        if self.fs.exists(self.path_from_webkit_base(CSS_DEST_NAME)):
            _log.warning('WebKit/%s repo exists; aborting.', CSS_DEST_NAME)
            return False

        return True

    def _copy_resources(self):
        """Copies resources from LayoutTests/resources to wpt and vice versa.

        There are resources from our repository that we use instead of the
        upstream versions. Conversely, there are also some resources that
        are copied in the other direction.

        Specifically:
          - testharnessreport.js contains code needed to integrate our testing
            with testharness.js; we also want our code to be used for tests
            in wpt.
          - TODO(qyearsley, jsbell): Document why other other files are copied,
            or stop copying them if it's unnecessary.

        If this method is changed, the lists of files expected to be identical
        in LayoutTests/PRESUBMIT.py should also be changed.
        """
        # TODO(tkent): resources_to_copy_to_wpt is unnecessary after enabling
        # WPTServe.
        resources_to_copy_to_wpt = [
            ('testharnessreport.js', 'resources'),
            ('WebIDLParser.js', 'resources'),
            ('vendor-prefix.js', 'common'),
        ]
        resources_to_copy_from_wpt = [
            ('idlharness.js', 'resources'),
            ('testharness.js', 'resources'),
        ]
        for filename, wpt_subdir in resources_to_copy_to_wpt:
            source = self.path_from_webkit_base('LayoutTests', 'resources', filename)
            destination = self.path_from_webkit_base('LayoutTests', 'imported', WPT_DEST_NAME, wpt_subdir, filename)
            self.copyfile(source, destination)
            self.run(['git', 'add', destination])
        for filename, wpt_subdir in resources_to_copy_from_wpt:
            source = self.path_from_webkit_base('LayoutTests', 'imported', WPT_DEST_NAME, wpt_subdir, filename)
            destination = self.path_from_webkit_base('LayoutTests', 'resources', filename)
            self.copyfile(source, destination)
            self.run(['git', 'add', destination])

    def _generate_manifest(self, original_repo_path, dest_path):
        """Generate MANIFEST.json for imported tests.

        Run 'manifest' command if it exists in original_repo_path, and
        add generated MANIFEST.json to dest_path.
        """
        manifest_command = self.fs.join(original_repo_path, 'manifest')
        if not self.fs.exists(manifest_command):
            # Do nothing for csswg-test.
            return
        _log.info('Generating MANIFEST.json')
        self.run([manifest_command, '--tests-root', dest_path])
        self.run(['git', 'add', self.fs.join(dest_path, 'MANIFEST.json')])

    def update(self, dest_dir_name, url, keep_w3c_repos_around, revision):
        """Updates an imported repository.

        Args:
            dest_dir_name: The destination directory name.
            url: URL of the git repository.
            revision: Commit hash or None.

        Returns:
            A string for the commit description "<destination>@<commitish>".
        """
        temp_repo_path = self.path_from_webkit_base(dest_dir_name)
        _log.info('Cloning %s into %s.', url, temp_repo_path)
        self.run(['git', 'clone', url, temp_repo_path])

        if revision is not None:
            _log.info('Checking out %s', revision)
            self.run(['git', 'checkout', revision], cwd=temp_repo_path)
        self.run(['git', 'submodule', 'update', '--init', '--recursive'], cwd=temp_repo_path)

        _log.info('Noting the revision we are importing.')
        _, show_ref_output = self.run(['git', 'show-ref', 'origin/master'], cwd=temp_repo_path)
        master_commitish = show_ref_output.split()[0]

        _log.info('Cleaning out tests from LayoutTests/imported/%s.', dest_dir_name)
        dest_path = self.path_from_webkit_base('LayoutTests', 'imported', dest_dir_name)
        is_not_baseline_filter = lambda fs, dirname, basename: not self.is_baseline(basename)
        files_to_delete = self.fs.files_under(dest_path, file_filter=is_not_baseline_filter)
        for subpath in files_to_delete:
            self.remove('LayoutTests', 'imported', subpath)

        _log.info('Importing the tests.')
        src_repo = self.path_from_webkit_base(dest_dir_name)
        import_path = self.path_from_webkit_base('Tools', 'Scripts', 'import-w3c-tests')
        self.run([self.host.executable, import_path, '-d', 'imported', src_repo])

        self.run(['git', 'add', '--all', 'LayoutTests/imported/%s' % dest_dir_name])

        _log.info('Deleting any orphaned baselines.')

        is_baseline_filter = lambda fs, dirname, basename: self.is_baseline(basename)
        previous_baselines = self.fs.files_under(dest_path, file_filter=is_baseline_filter)

        for subpath in previous_baselines:
            full_path = self.fs.join(dest_path, subpath)
            if self.fs.glob(full_path.replace('-expected.txt', '*')) == [full_path]:
                self.fs.remove(full_path)

        self._generate_manifest(temp_repo_path, dest_path)

        if not keep_w3c_repos_around:
            _log.info('Deleting temp repo directory %s.', temp_repo_path)
            self.rmtree(temp_repo_path)

        _log.info('Updating TestExpectations for any removed or renamed tests.')
        self.update_all_test_expectations_files(self._list_deleted_tests(), self._list_renamed_tests())

        return '%s@%s' % (dest_dir_name, master_commitish)

    def commit_changes_if_needed(self, chromium_commitish, import_commitish):
        if self.run(['git', 'diff', '--quiet', 'HEAD'], exit_on_failure=False)[0]:
            _log.info('Committing changes.')
            commit_msg = ('Import %s\n'
                          '\n'
                          'Using update-w3c-deps in Chromium %s.\n'
                          % (import_commitish, chromium_commitish))
            path_to_commit_msg = self.path_from_webkit_base('commit_msg')
            _log.debug('cat > %s <<EOF', path_to_commit_msg)
            _log.debug(commit_msg)
            _log.debug('EOF')
            self.fs.write_text_file(path_to_commit_msg, commit_msg)
            self.run(['git', 'commit', '-a', '-F', path_to_commit_msg])
            self.remove(path_to_commit_msg)
            _log.info('Done: changes imported and committed.')
            return True
        else:
            _log.info('Done: no changes to import.')
            return False

    @staticmethod
    def is_baseline(basename):
        return basename.endswith('-expected.txt')

    def run(self, cmd, exit_on_failure=True, cwd=None):
        _log.debug('Running command: %s', ' '.join(cmd))

        cwd = cwd or self.finder.webkit_base()
        proc = self.executive.popen(cmd, stdout=self.executive.PIPE, stderr=self.executive.PIPE, cwd=cwd)
        out, err = proc.communicate()
        if proc.returncode or self.verbose:
            _log.info('# ret> %d', proc.returncode)
            if out:
                for line in out.splitlines():
                    _log.info('# out> %s', line)
            if err:
                for line in err.splitlines():
                    _log.info('# err> %s', line)
        if exit_on_failure and proc.returncode:
            self.host.exit(proc.returncode)
        return proc.returncode, out

    def check_run(self, command):
        return_code, out = self.run(command)
        if return_code:
            raise Exception('%s failed with exit code %d.' % ' '.join(command), return_code)
        return out

    def copyfile(self, source, destination):
        _log.debug('cp %s %s', source, destination)
        self.fs.copyfile(source, destination)

    def remove(self, *comps):
        dest = self.path_from_webkit_base(*comps)
        _log.debug('rm %s', dest)
        self.fs.remove(dest)

    def rmtree(self, *comps):
        dest = self.path_from_webkit_base(*comps)
        _log.debug('rm -fr %s', dest)
        self.fs.rmtree(dest)

    def path_from_webkit_base(self, *comps):
        return self.finder.path_from_webkit_base(*comps)

    def do_auto_update(self):
        """Attempts to upload a CL, make any required adjustments, and commit.

        This function assumes that the imported repo has already been updated,
        and that change has been committed. There may be newly-failing tests,
        so before being able to commit these new changes, we may need to update
        TestExpectations or download new baselines.

        Returns:
            True if successfully committed, False otherwise.
        """
        self._upload_cl()
        _log.info('Issue: %s', self.git_cl.run(['issue']).strip())

        # First try: if there are failures, update expectations.
        _log.info('Triggering try jobs.')
        for try_bot in self.host.builders.all_try_builder_names():
            self.git_cl.run(['try', '-b', try_bot])
        try_results = self.git_cl.wait_for_try_jobs(timeout_seconds=180 * 60)
        if not try_results:
            _log.error('Timed out waiting for try results.')
            return
        if try_results and self.git_cl.has_failing_try_results(try_results):
            self.fetch_new_expectations_and_baselines()

        # Second try: if there are failures, then abort.
        self.git_cl.run(['set-commit', '--rietveld'])
        try_results = self.git_cl.wait_for_try_jobs(timeout_seconds=180 * 60)
        if not try_results:
            _log.info('Timed out waiting for try results.')
            self.git_cl.run(['set-close'])
            return False
        if self.git_cl.has_failing_try_results(try_results):
            _log.info('CQ failed; aborting.')
            self.git_cl.run(['set-close'])
            return False
        _log.info('Update completed.')
        return True

    def _upload_cl(self):
        _log.info('Uploading change list.')
        cc_list = self.get_directory_owners_to_cc()
        description = self._cl_description()
        self.git_cl.run([
            'upload',
            '-f',
            '--rietveld',
            '-m',
            description,
        ] + ['--cc=' + email for email in cc_list])

    def _cl_description(self):
        description = self.check_run(['git', 'log', '-1', '--format=%B'])
        build_link = self._build_link()
        if build_link:
            description += 'Build: %s\n\n' % build_link
        description += 'TBR=qyearsley@chromium.org'
        return description

    def _build_link(self):
        """Returns a link to a job, if running on buildbot."""
        master_name = self.host.environ.get('BUILDBOT_MASTERNAME')
        builder_name = self.host.environ.get('BUILDBOT_BUILDERNAME')
        build_number = self.host.environ.get('BUILDBOT_BUILDNUMBER')
        if not (master_name and builder_name and build_number):
            return None
        return 'https://build.chromium.org/p/%s/builders/%s/builds/%s' % (master_name, builder_name, build_number)

    def get_directory_owners_to_cc(self):
        """Returns a list of email addresses to CC for the current import."""
        _log.info('Gathering directory owners emails to CC.')
        directory_owners_file_path = self.finder.path_from_webkit_base(
            'Tools', 'Scripts', 'webkitpy', 'w3c', 'directory_owners.json')
        with open(directory_owners_file_path) as data_file:
            directory_to_owner = self.parse_directory_owners(json.load(data_file))
        out = self.check_run(['git', 'diff', 'origin/master', '--name-only'])
        changed_files = out.splitlines()
        return self.generate_email_list(changed_files, directory_to_owner)

    @staticmethod
    def parse_directory_owners(decoded_data_file):
        directory_dict = {}
        for dict_set in decoded_data_file:
            if dict_set['notification-email']:
                directory_dict[dict_set['directory']] = dict_set['notification-email']
        return directory_dict

    def generate_email_list(self, changed_files, directory_to_owner):
        """Returns a list of email addresses based on the given file list and
        directory-to-owner mapping.

        Args:
            changed_files: A list of file paths relative to the repository root.
            directory_to_owner: A dict mapping layout test directories to emails.

        Returns:
            A list of the email addresses to be notified for the current import.
        """
        email_addresses = set()
        for file_path in changed_files:
            test_path = self.finder.layout_test_name(file_path)
            if test_path is None:
                continue
            test_dir = self.fs.dirname(test_path)
            if test_dir in directory_to_owner:
                email_addresses.add(directory_to_owner[test_dir])
        return sorted(email_addresses)

    def fetch_new_expectations_and_baselines(self):
        """Adds new expectations and downloads baselines based on try job results, then commits and uploads the change."""
        _log.info('Adding test expectations lines to LayoutTests/TestExpectations.')
        script_path = self.path_from_webkit_base('Tools', 'Scripts', 'update-w3c-test-expectations')
        self.run([self.host.executable, script_path, '--verbose'])
        message = 'Modify TestExpectations or download new baselines for tests.'
        self.check_run(['git', 'commit', '-a', '-m', message])
        self.git_cl.run(['upload', '-m', message, '--rietveld'])

    def update_all_test_expectations_files(self, deleted_tests, renamed_tests):
        """Updates all test expectations files for tests that have been deleted or renamed."""
        port = self.host.port_factory.get()
        for path, file_contents in port.all_expectations_dict().iteritems():

            parser = TestExpectationParser(port, all_tests=None, is_lint_mode=False)
            expectation_lines = parser.parse(path, file_contents)
            self._update_single_test_expectations_file(path, expectation_lines, deleted_tests, renamed_tests)

    def _update_single_test_expectations_file(self, path, expectation_lines, deleted_tests, renamed_tests):
        """Updates single test expectations file."""
        # FIXME: This won't work for removed or renamed directories with test expectations
        # that are directories rather than individual tests.
        new_lines = []
        changed_lines = []
        for expectation_line in expectation_lines:
            if expectation_line.name in deleted_tests:
                continue
            if expectation_line.name in renamed_tests:
                expectation_line.name = renamed_tests[expectation_line.name]
                # Upon parsing the file, a "path does not exist" warning is expected
                # to be there for tests that have been renamed, and if there are warnings,
                # then the original string is used. If the warnings are reset, then the
                # expectation line is re-serialized when output.
                expectation_line.warnings = []
                changed_lines.append(expectation_line)
            new_lines.append(expectation_line)
        new_file_contents = TestExpectations.list_to_string(new_lines, reconstitute_only_these=changed_lines)
        self.host.filesystem.write_text_file(path, new_file_contents)

    def _list_deleted_tests(self):
        """Returns a list of layout tests that have been deleted."""
        out = self.check_run(['git', 'diff', 'origin/master', '-M100%', '--diff-filter=D', '--name-only'])
        deleted_tests = []
        for line in out.splitlines():
            test = self.finder.layout_test_name(line)
            if test:
                deleted_tests.append(test)
        return deleted_tests

    def _list_renamed_tests(self):
        """Returns a dict mapping source to dest name for layout tests that have been renamed."""
        out = self.check_run(['git', 'diff', 'origin/master', '-M100%', '--diff-filter=R', '--name-status'])
        renamed_tests = {}
        for line in out.splitlines():
            _, source_path, dest_path = line.split()
            source_test = self.finder.layout_test_name(source_path)
            dest_test = self.finder.layout_test_name(dest_path)
            if source_test and dest_test:
                renamed_tests[source_test] = dest_test
        return renamed_tests
