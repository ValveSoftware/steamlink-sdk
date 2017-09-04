# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A script for exporting and importing changes between the Chromium repo
and the web-platform-tests repo.

TODO(jeffcarp): does not handle reverted changes right now
TODO(jeffcarp): it also doesn't handle changes to -expected.html files
TODO(jeffcarp): Currently this script only does export; also add an option
import as well.
"""

import argparse
import logging

from webkitpy.common.host import Host
from webkitpy.common.system.executive import ScriptError
from webkitpy.w3c.chromium_commit import ChromiumCommit
from webkitpy.w3c.chromium_wpt import ChromiumWPT
from webkitpy.w3c.github import GitHub
from webkitpy.w3c.local_wpt import LocalWPT
from webkitpy.w3c.test_importer import configure_logging

_log = logging.getLogger(__name__)


def main():
    configure_logging()
    options = parse_args()
    host = Host()
    github = GitHub(host)

    local_wpt = LocalWPT(host, no_fetch=options.no_fetch, use_github=True)
    chromium_wpt = ChromiumWPT(host)

    wpt_commit, chromium_commit = local_wpt.most_recent_chromium_commit()
    assert chromium_commit, 'No Chromium commit found, this is impossible'

    _log.info('web-platform-tests@%s (%d behind origin/master)',
              wpt_commit, local_wpt.commits_behind_master(wpt_commit))
    _log.info('chromium@%s (%d behind origin/master)',
              chromium_commit.sha, chromium_commit.num_behind_master())

    exportable_commits = chromium_wpt.exportable_commits_since(chromium_commit.sha)

    if exportable_commits:
        _log.info('Found %s exportable commits in chromium:', len(exportable_commits))
        for commit in exportable_commits:
            _log.info('- %s %s', commit, chromium_wpt.subject(commit))
    else:
        _log.info('No exportable commits found in Chromium, stopping.')
        return

    for commit in exportable_commits:
        _log.info('Uploading %s', chromium_wpt.subject(commit))
        chromium_commit = ChromiumCommit(host, sha=commit)

        patch = chromium_wpt.format_patch(commit)
        message = chromium_wpt.message(commit)

        local_wpt.create_branch_with_patch(branch_name, message, patch)

        github.create_pr(
            local_branch_name='chromium-try-{}'.format(commit),
            desc_title=chromium_commit.subject(),
            body=chromium_commit.body())


def parse_args():
    parser = argparse.ArgumentParser(description='WPT Sync')
    parser.add_argument('--no-fetch', action='store_true')
    return parser.parse_args()
