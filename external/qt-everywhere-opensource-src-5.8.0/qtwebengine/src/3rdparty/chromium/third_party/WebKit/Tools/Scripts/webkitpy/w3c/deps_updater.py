# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Pull latest revisions of a W3C test repo and make a local commit."""

import argparse

from webkitpy.common.webkit_finder import WebKitFinder

# Import destination directories (under LayoutTests/imported/).
WPT_DEST_NAME = 'wpt'
CSS_DEST_NAME = 'csswg-test'


class DepsUpdater(object):

    def __init__(self, host):
        self.host = host
        self.executive = host.executive
        self.fs = host.filesystem
        self.finder = WebKitFinder(self.fs)
        self.verbose = False
        self.allow_local_commits = False
        self.keep_w3c_repos_around = False
        self.target = None

    def main(self, argv=None):
        self.parse_args(argv)

        if not self.checkout_is_okay():
            return 1

        self.print_('## Noting the current Chromium commit.')
        _, show_ref_output = self.run(['git', 'show-ref', 'HEAD'])
        chromium_commitish = show_ref_output.split()[0]

        if self.target == 'wpt':
            import_commitish = self.update(
                WPT_DEST_NAME,
                'https://chromium.googlesource.com/external/w3c/web-platform-tests.git')

            for resource in ['testharnessreport.js', 'WebIDLParser.js']:
                source = self.path_from_webkit_base('LayoutTests', 'resources', resource)
                destination = self.path_from_webkit_base('LayoutTests', 'imported', WPT_DEST_NAME, 'resources', resource)
                self.copyfile(source, destination)
                self.run(['git', 'add', destination])
            for resource in ['vendor-prefix.js']:
                source = self.path_from_webkit_base('LayoutTests', 'resources', resource)
                destination = self.path_from_webkit_base('LayoutTests', 'imported', WPT_DEST_NAME, 'common', resource)
                self.copyfile(source, destination)
                self.run(['git', 'add', destination])

        elif self.target == 'css':
            import_commitish = self.update(
                CSS_DEST_NAME,
                'https://chromium.googlesource.com/external/w3c/csswg-test.git')
        else:
            raise AssertionError("Unsupported target %s" % self.target)

        self.commit_changes_if_needed(chromium_commitish, import_commitish)

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
        parser.add_argument('target', choices=['css', 'wpt'],
                            help='Target repository.  "css" for csswg-test, "wpt" for web-platform-tests.')

        args = parser.parse_args(argv)
        self.allow_local_commits = args.allow_local_commits
        self.keep_w3c_repos_around = args.keep_w3c_repos_around
        self.verbose = args.verbose
        self.target = args.target

    def checkout_is_okay(self):
        git_diff_retcode, _ = self.run(['git', 'diff', '--quiet', 'HEAD'], exit_on_failure=False)
        if git_diff_retcode:
            self.print_('## Checkout is dirty; aborting.')
            return False

        local_commits = self.run(['git', 'log', '--oneline', 'origin/master..HEAD'])[1]
        if local_commits and not self.allow_local_commits:
            self.print_('## Checkout has local commits; aborting. Use --allow-local-commits to allow this.')
            return False

        if self.fs.exists(self.path_from_webkit_base(WPT_DEST_NAME)):
            self.print_('## WebKit/%s exists; aborting.' % WPT_DEST_NAME)
            return False

        if self.fs.exists(self.path_from_webkit_base(CSS_DEST_NAME)):
            self.print_('## WebKit/%s repo exists; aborting.' % CSS_DEST_NAME)
            return False

        return True

    def update(self, dest_dir_name, url):
        """Updates an imported repository.

        Args:
            dest_dir_name: The destination directory name.
            url: URL of the git repository.

        Returns:
            A string for the commit description "<destination>@<commitish>".
        """
        temp_repo_path = self.path_from_webkit_base(dest_dir_name)
        self.print_('## Cloning %s into %s.' % (url, temp_repo_path))
        self.run(['git', 'clone', url, temp_repo_path])

        self.run(['git', 'submodule', 'update', '--init', '--recursive'], cwd=temp_repo_path)

        self.print_('## Noting the revision we are importing.')
        _, show_ref_output = self.run(['git', 'show-ref', 'origin/master'], cwd=temp_repo_path)
        master_commitish = show_ref_output.split()[0]

        self.print_('## Cleaning out tests from LayoutTests/imported/%s.' % dest_dir_name)
        dest_path = self.path_from_webkit_base('LayoutTests', 'imported', dest_dir_name)
        files_to_delete = self.fs.files_under(dest_path, file_filter=self.is_not_baseline)
        for subpath in files_to_delete:
            self.remove('LayoutTests', 'imported', subpath)

        self.print_('## Importing the tests.')
        src_repo = self.path_from_webkit_base(dest_dir_name)
        import_path = self.path_from_webkit_base('Tools', 'Scripts', 'import-w3c-tests')
        self.run([self.host.executable, import_path, '-d', 'imported', src_repo])

        self.run(['git', 'add', '--all', 'LayoutTests/imported/%s' % dest_dir_name])

        self.print_('## Deleting manual tests.')
        files_to_delete = self.fs.files_under(dest_path, file_filter=self.is_manual_test)
        for subpath in files_to_delete:
            self.remove('LayoutTests', 'imported', subpath)

        self.print_('## Deleting any orphaned baselines.')
        previous_baselines = self.fs.files_under(dest_path, file_filter=self.is_baseline)
        for subpath in previous_baselines:
            full_path = self.fs.join(dest_path, subpath)
            if self.fs.glob(full_path.replace('-expected.txt', '*')) == [full_path]:
                self.fs.remove(full_path)

        if not self.keep_w3c_repos_around:
            self.print_('## Deleting temp repo directory %s.' % temp_repo_path)
            self.rmtree(temp_repo_path)

        return '%s@%s' % (dest_dir_name, master_commitish)

    def commit_changes_if_needed(self, chromium_commitish, import_commitish):
        if self.run(['git', 'diff', '--quiet', 'HEAD'], exit_on_failure=False)[0]:
            self.print_('## Committing changes.')
            commit_msg = ('Import %s\n'
                          '\n'
                          'Using update-w3c-deps in Chromium %s.\n'
                          % (import_commitish, chromium_commitish))
            path_to_commit_msg = self.path_from_webkit_base('commit_msg')
            if self.verbose:
                self.print_('cat > %s <<EOF' % path_to_commit_msg)
                self.print_(commit_msg)
                self.print_('EOF')
            self.fs.write_text_file(path_to_commit_msg, commit_msg)
            self.run(['git', 'commit', '-a', '-F', path_to_commit_msg])
            self.remove(path_to_commit_msg)
            self.print_('## Done: changes imported and committed.')
        else:
            self.print_('## Done: no changes to import.')

    def is_manual_test(self, fs, dirname, basename):  # Callback for FileSystem.files_under; not all arguments used - pylint: disable=unused-argument
        # We are importing manual pointer event tests and we are automating them.
        return ("pointerevents" not in dirname) and (basename.endswith('-manual.html') or basename.endswith('-manual.htm'))

    def is_baseline(self, fs, dirname, basename):  # Callback for FileSystem.files_under; not all arguments used - pylint: disable=unused-argument
        return basename.endswith('-expected.txt')

    def is_not_baseline(self, fs, dirname, basename):
        return not self.is_baseline(fs, dirname, basename)

    def run(self, cmd, exit_on_failure=True, cwd=None):
        if self.verbose:
            self.print_(' '.join(cmd))

        cwd = cwd or self.finder.webkit_base()
        proc = self.executive.popen(cmd, stdout=self.executive.PIPE, stderr=self.executive.PIPE, cwd=cwd)
        out, err = proc.communicate()
        if proc.returncode or self.verbose:
            self.print_('# ret> %d' % proc.returncode)
            if out:
                for line in out.splitlines():
                    self.print_('# out> %s' % line)
            if err:
                for line in err.splitlines():
                    self.print_('# err> %s' % line)
        if exit_on_failure and proc.returncode:
            self.host.exit(proc.returncode)
        return proc.returncode, out

    def copyfile(self, source, destination):
        if self.verbose:
            self.print_('cp %s %s' % (source, destination))
        self.fs.copyfile(source, destination)

    def remove(self, *comps):
        dest = self.path_from_webkit_base(*comps)
        if self.verbose:
            self.print_('rm %s' % dest)
        self.fs.remove(dest)

    def rmtree(self, *comps):
        dest = self.path_from_webkit_base(*comps)
        if self.verbose:
            self.print_('rm -fr %s' % dest)
        self.fs.rmtree(dest)

    def path_from_webkit_base(self, *comps):
        return self.finder.path_from_webkit_base(*comps)

    def print_(self, msg):
        self.host.print_(msg)
