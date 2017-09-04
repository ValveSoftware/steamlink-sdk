# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.executive_mock import MockExecutive2
from webkitpy.w3c.chromium_commit import ChromiumCommit


class ChromiumCommitTest(unittest.TestCase):

    def test_accepts_sha(self):
        chromium_commit = ChromiumCommit(MockHost(), sha='deadbeefcafe')

        self.assertEqual(chromium_commit.sha, 'deadbeefcafe')
        self.assertIsNone(chromium_commit.position)

    def test_derives_sha_from_position(self):
        host = MockHost()
        host.executive = MockExecutive2(output='deadbeefcafe')
        pos = 'Cr-Commit-Position: refs/heads/master@{#789}'
        chromium_commit = ChromiumCommit(host, position=pos)

        self.assertEqual(chromium_commit.position, 'refs/heads/master@{#789}')
        self.assertEqual(chromium_commit.sha, 'deadbeefcafe')
