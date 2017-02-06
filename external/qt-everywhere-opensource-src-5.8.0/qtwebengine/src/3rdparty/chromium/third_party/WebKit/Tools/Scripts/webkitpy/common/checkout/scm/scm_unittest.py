# Copyright (C) 2009 Google Inc. All rights reserved.
# Copyright (C) 2009 Apple Inc. All rights reserved.
# Copyright (C) 2011 Daniel Bates (dbates@intudata.com). All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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

import unittest

from webkitpy.common.system.executive import Executive, ScriptError
from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.common.system.filesystem import FileSystem
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.checkout.scm.detection import detect_scm_system
from webkitpy.common.checkout.scm.git import AmbiguousCommitError
from webkitpy.common.checkout.scm.git import Git
from webkitpy.common.checkout.scm.scm import SCM


class SCMTestBase(unittest.TestCase):

    def __init__(self, *args, **kwargs):
        super(SCMTestBase, self).__init__(*args, **kwargs)
        self.scm = None
        self.executive = None
        self.fs = None
        self.original_cwd = None

    def setUp(self):
        self.executive = Executive()
        self.fs = FileSystem()
        self.original_cwd = self.fs.getcwd()

    def tearDown(self):
        self._chdir(self.original_cwd)

    def _join(self, *comps):
        return self.fs.join(*comps)

    def _chdir(self, path):
        self.fs.chdir(path)

    def _mkdir(self, path):
        assert not self.fs.exists(path)
        self.fs.maybe_make_directory(path)

    def _mkdtemp(self, **kwargs):
        return str(self.fs.mkdtemp(**kwargs))

    def _remove(self, path):
        self.fs.remove(path)

    def _rmtree(self, path):
        self.fs.rmtree(path)

    def _run(self, *args, **kwargs):
        return self.executive.run_command(*args, **kwargs)

    def _run_silent(self, args, **kwargs):
        self.executive.run_command(args, **kwargs)

    def _write_text_file(self, path, contents):
        self.fs.write_text_file(path, contents)

    def _write_binary_file(self, path, contents):
        self.fs.write_binary_file(path, contents)

    def _make_diff(self, command, *args):
        # We use this wrapper to disable output decoding. diffs should be treated as
        # binary files since they may include text files of multiple different encodings.
        return self._run([command, "diff"] + list(args), decode_output=False)

    def _git_diff(self, *args):
        return self._make_diff("git", *args)

    def _shared_test_add_recursively(self):
        self._mkdir("added_dir")
        self._write_text_file("added_dir/added_file", "new stuff")
        self.scm.add("added_dir/added_file")
        self.assertIn("added_dir/added_file", self.scm._added_files())

    def _shared_test_delete_recursively(self):
        self._mkdir("added_dir")
        self._write_text_file("added_dir/added_file", "new stuff")
        self.scm.add("added_dir/added_file")
        self.assertIn("added_dir/added_file", self.scm._added_files())
        self.scm.delete("added_dir/added_file")
        self.assertNotIn("added_dir", self.scm._added_files())

    def _shared_test_delete_recursively_or_not(self):
        self._mkdir("added_dir")
        self._write_text_file("added_dir/added_file", "new stuff")
        self._write_text_file("added_dir/another_added_file", "more new stuff")
        self.scm.add("added_dir/added_file")
        self.scm.add("added_dir/another_added_file")
        self.assertIn("added_dir/added_file", self.scm._added_files())
        self.assertIn("added_dir/another_added_file", self.scm._added_files())
        self.scm.delete("added_dir/added_file")
        self.assertIn("added_dir/another_added_file", self.scm._added_files())

    def _shared_test_exists(self, scm, commit_function):
        self._chdir(scm.checkout_root)
        self.assertFalse(scm.exists('foo.txt'))
        self._write_text_file('foo.txt', 'some stuff')
        self.assertFalse(scm.exists('foo.txt'))
        scm.add('foo.txt')
        commit_function('adding foo')
        self.assertTrue(scm.exists('foo.txt'))
        scm.delete('foo.txt')
        commit_function('deleting foo')
        self.assertFalse(scm.exists('foo.txt'))

    def _shared_test_move(self):
        self._write_text_file('added_file', 'new stuff')
        self.scm.add('added_file')
        self.scm.move('added_file', 'moved_file')
        self.assertIn('moved_file', self.scm._added_files())

    def _shared_test_move_recursive(self):
        self._mkdir("added_dir")
        self._write_text_file('added_dir/added_file', 'new stuff')
        self._write_text_file('added_dir/another_added_file', 'more new stuff')
        self.scm.add('added_dir')
        self.scm.move('added_dir', 'moved_dir')
        self.assertIn('moved_dir/added_file', self.scm._added_files())
        self.assertIn('moved_dir/another_added_file', self.scm._added_files())


class GitTest(SCMTestBase):

    def setUp(self):
        super(GitTest, self).setUp()
        self._set_up_git_checkouts()

    def tearDown(self):
        super(GitTest, self).tearDown()
        self._tear_down_git_checkouts()

    def _set_up_git_checkouts(self):
        """Sets up fresh git repository with one commit. Then sets up a second git repo that tracks the first one."""

        self.untracking_checkout_path = self._mkdtemp(suffix="git_test_checkout2")
        self._run(['git', 'init', self.untracking_checkout_path])

        self._chdir(self.untracking_checkout_path)
        self._write_text_file('foo_file', 'foo')
        self._run(['git', 'add', 'foo_file'])
        self._run(['git', 'commit', '-am', 'dummy commit'])
        self.untracking_scm = detect_scm_system(self.untracking_checkout_path)

        self.tracking_git_checkout_path = self._mkdtemp(suffix="git_test_checkout")
        self._run(['git', 'clone', '--quiet', self.untracking_checkout_path, self.tracking_git_checkout_path])
        self._chdir(self.tracking_git_checkout_path)
        self.tracking_scm = detect_scm_system(self.tracking_git_checkout_path)

    def _tear_down_git_checkouts(self):
        self._run(['rm', '-rf', self.tracking_git_checkout_path])
        self._run(['rm', '-rf', self.untracking_checkout_path])

    def test_remote_branch_ref(self):
        self.assertEqual(self.tracking_scm._remote_branch_ref(), 'refs/remotes/origin/master')
        self._chdir(self.untracking_checkout_path)
        self.assertRaises(ScriptError, self.untracking_scm._remote_branch_ref)

    def test_create_patch(self):
        self._write_text_file('test_file_commit1', 'contents')
        self._run(['git', 'add', 'test_file_commit1'])
        scm = self.tracking_scm
        scm.commit_locally_with_message('message')

        patch = scm.create_patch()
        self.assertNotRegexpMatches(patch, r'Subversion Revision:')

    def test_patches_have_filenames_with_prefixes(self):
        self._write_text_file('test_file_commit1', 'contents')
        self._run(['git', 'add', 'test_file_commit1'])
        scm = self.tracking_scm
        scm.commit_locally_with_message('message')

        # Even if diff.noprefix is enabled, create_patch() produces diffs with prefixes.
        self._run(['git', 'config', 'diff.noprefix', 'true'])
        patch = scm.create_patch()
        self.assertRegexpMatches(patch, r'^diff --git a/test_file_commit1 b/test_file_commit1')

    def test_exists(self):
        scm = self.untracking_scm
        self._shared_test_exists(scm, scm.commit_locally_with_message)

    def test_rename_files(self):
        scm = self.tracking_scm
        scm.move('foo_file', 'bar_file')
        scm.commit_locally_with_message('message')

    def test_commit_position_from_git_log(self):
        git_log = """
commit 624c3081c0
Author: foobarbaz1 <foobarbaz1@chromium.org>
Date:   Mon Sep 28 19:10:30 2015 -0700

    Test foo bar baz qux 123.

    BUG=000000

    Review URL: https://codereview.chromium.org/999999999

    Cr-Commit-Position: refs/heads/master@{#1234567}
"""
        scm = self.tracking_scm
        self.assertEqual(scm._commit_position_from_git_log(git_log), 1234567)

    def test_timestamp_of_revision(self):
        scm = self.tracking_scm
        scm.most_recent_log_matching(scm._commit_position_regex_for_timestamp(), scm.checkout_root)


class GitTestWithMock(SCMTestBase):

    def make_scm(self):
        scm = Git(cwd=".", executive=MockExecutive(), filesystem=MockFileSystem())
        scm.read_git_config = lambda *args, **kw: "MOCKKEY:MOCKVALUE"
        return scm

    def test_timestamp_of_revision(self):
        scm = self.make_scm()
        scm.find_checkout_root = lambda path: ''
        scm._run_git = lambda args: 'Date: 2013-02-08 08:05:49 +0000'
        self.assertEqual(scm.timestamp_of_revision('some-path', '12345'), '2013-02-08T08:05:49Z')

        scm._run_git = lambda args: 'Date: 2013-02-08 01:02:03 +0130'
        self.assertEqual(scm.timestamp_of_revision('some-path', '12345'), '2013-02-07T23:32:03Z')

        scm._run_git = lambda args: 'Date: 2013-02-08 01:55:21 -0800'
        self.assertEqual(scm.timestamp_of_revision('some-path', '12345'), '2013-02-08T09:55:21Z')
