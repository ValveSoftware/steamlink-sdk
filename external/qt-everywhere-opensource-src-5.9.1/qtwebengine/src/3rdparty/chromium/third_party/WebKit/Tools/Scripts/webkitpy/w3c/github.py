# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import json
import httplib2
import logging

_log = logging.getLogger(__name__)


class GitHub(object):

    def __init__(self, host):
        self.host = host
        self.user = self.host.environ.get('GH_USER')
        self.token = self.host.environ.get('GH_TOKEN')

        assert self.user and self.token, 'must have GH_USER and GH_TOKEN env vars'

    def auth_token(self):
        return base64.encodestring('{}:{}'.format(self.user, self.token))

    def create_pr(self, local_branch_name, desc_title, body):
        """Creates a PR on GitHub.

        API doc: https://developer.github.com/v3/pulls/#create-a-pull-request

        Returns:
            A raw response object if successful, None if not.
        """
        assert local_branch_name
        assert desc_title
        assert body

        pr_branch_name = '{}:{}'.format(self.user, local_branch_name)

        # TODO(jeffcarp): add HTTP to Host and use that here
        conn = httplib2.Http()
        headers = {
            "Accept": "application/vnd.github.v3+json",
            "Authorization": "Basic " + self.auth_token()
        }
        body = {
            "title": desc_title,
            "body": body,
            "head": pr_branch_name,
            "base": "master"
        }
        resp, content = conn.request("https://api.github.com/repos/w3c/web-platform-tests/pulls",
                                     "POST", body=json.JSONEncoder().encode(body), headers=headers)
        _log.info("GitHub response: %s", content)
        if resp["status"] != "201":
            return None
        return json.loads(content)
