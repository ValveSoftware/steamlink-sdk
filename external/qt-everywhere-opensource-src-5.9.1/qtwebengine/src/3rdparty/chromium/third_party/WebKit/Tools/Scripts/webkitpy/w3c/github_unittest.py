# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import unittest
from webkitpy.common.host_mock import MockHost
from webkitpy.w3c.github import GitHub


class GitHubEnvTest(unittest.TestCase):

    def setUp(self):
        self.host = MockHost()

    def test_requires_env_vars(self):
        self.assertRaises(AssertionError, lambda: GitHub(self.host))

    def test_requires_gh_user_env_var(self):
        self.host.environ['GH_USER'] = 'rutabaga'
        self.assertRaises(AssertionError, lambda: GitHub(self.host))

    def test_requires_gh_token_env_var(self):
        self.host.environ['GH_TOKEN'] = 'deadbeefcafe'
        self.assertRaises(AssertionError, lambda: GitHub(self.host))


class GitHubTest(unittest.TestCase):

    def setUp(self):
        self.host = MockHost()
        self.host.environ['GH_USER'] = 'rutabaga'
        self.host.environ['GH_TOKEN'] = 'deadbeefcafe'
        self.github = GitHub(self.host)

    def test_properties(self):
        self.assertEqual(self.github.user, 'rutabaga')
        self.assertEqual(self.github.token, 'deadbeefcafe')

    def test_auth_token(self):
        expected = base64.encodestring('rutabaga:deadbeefcafe')
        self.assertEqual(self.github.auth_token(), expected)

    # TODO(jeffcarp): add create_pr tests
