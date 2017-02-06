# Copyright (C) 2013 Google Inc. All rights reserved.
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

from webkitpy.tool.bot.commit_announcer import CommitAnnouncer
from webkitpy.tool.mock_tool import MockTool


class CommitAnnouncerTest(unittest.TestCase):

    def test_format_commit(self):
        tool = MockTool()
        bot = CommitAnnouncer(tool, "test/directory", "test_password")
        self.assertEqual(
            'https://crrev.com/456789 authorABC@chromium.org committed "Commit test subject line"',
            bot._format_commit_detail("""\
1234commit1234
authorABC@chromium.org
Commit test subject line
Multiple
lines
of
description.

BUG=654321

Review URL: https://codereview.chromium.org/123456

Cr-Commit-Position: refs/heads/master@{#456789}
"""))

        self.assertEqual(
            'https://crrev.com/456789 '
            'authorABC@chromium.org committed "Commit test subject line"',
            bot._format_commit_detail("""\
1234commit1234
authorABC@chromium.org
Commit test subject line
Multiple
lines
of
description.

BUG=654321

Cr-Commit-Position: refs/heads/master@{#456789}
"""))

        self.assertEqual(
            'https://crrev.com/1234comm authorABC@chromium.org committed "Commit test subject line"',
            bot._format_commit_detail("""\
1234commit1234
authorABC@chromium.org
Commit test subject line
Multiple
lines
of
description.

BUG=654321

Review URL: https://codereview.chromium.org/123456
"""))

        self.assertEqual(
            'https://crrev.com/1234comm authorABC@chromium.org committed "Commit test subject line"',
            bot._format_commit_detail("""\
1234commit1234
authorABC@chromium.org
Commit test subject line
Multiple
lines
of
description.
"""))

        self.assertEqual(
            'https://crrev.com/456789 authorABC@chromium.org committed "Commit test subject line"',
            bot._format_commit_detail("""\
1234commit1234
authorABC@chromium.org
Commit test subject line
Multiple
lines
of
description.
Review URL: http://fake.review.url
Cr-Commit-Position: refs/heads/master@{#000000}

BUG=654321

Review URL: https://codereview.chromium.org/123456

Cr-Commit-Position: refs/heads/master@{#456789}
"""))

        self.assertEqual(
            'https://crrev.com/456789 authorABC@chromium.org committed "Commit test subject line" '
            '\x037TBR=reviewerDEF@chromium.org\x03',
            bot._format_commit_detail("""\
1234commit1234
authorABC@chromium.org
Commit test subject line
Multiple
lines
of
description.

BUG=654321
TBR=reviewerDEF@chromium.org

Review URL: https://codereview.chromium.org/123456

Cr-Commit-Position: refs/heads/master@{#456789}
"""))

        self.assertEqual(
            'https://crrev.com/456789 authorABC@chromium.org committed "Commit test subject line" '
            '\x037NOTRY=true\x03',
            bot._format_commit_detail("""\
1234commit1234
authorABC@chromium.org
Commit test subject line
Multiple
lines
of
description.

BUG=654321
NOTRY=true

Review URL: https://codereview.chromium.org/123456

Cr-Commit-Position: refs/heads/master@{#456789}
"""))

        self.assertEqual(
            'https://crrev.com/456789 authorABC@chromium.org committed "Commit test subject line" '
            '\x037NOTRY=true TBR=reviewerDEF@chromium.org\x03',
            bot._format_commit_detail("""\
1234commit1234
authorABC@chromium.org
Commit test subject line
Multiple
lines
of
description.

NOTRY=true
BUG=654321
TBR=reviewerDEF@chromium.org

Review URL: https://codereview.chromium.org/123456

Cr-Commit-Position: refs/heads/master@{#456789}
"""))

        self.assertEqual(
            'https://crrev.com/456789 authorABC@chromium.org committed "Commit test subject line" '
            '\x037tbr=reviewerDEF@chromium.org, reviewerGHI@chromium.org, reviewerJKL@chromium.org notry=TRUE\x03',
            bot._format_commit_detail("""\
1234commit1234
authorABC@chromium.org
Commit test subject line
Multiple
lines
of
description.

BUG=654321
tbr=reviewerDEF@chromium.org, reviewerGHI@chromium.org, reviewerJKL@chromium.org
notry=TRUE

Review URL: https://codereview.chromium.org/123456

Cr-Commit-Position: refs/heads/master@{#456789}
"""))

    def test_sanitize_string(self):
        bot = CommitAnnouncer(MockTool(), "test/directory", "test_password")
        self.assertEqual('normal ascii', bot._sanitize_string('normal ascii'))
        self.assertEqual('uni\\u0441ode!', bot._sanitize_string(u'uni\u0441ode!'))
