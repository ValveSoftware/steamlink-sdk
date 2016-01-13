/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */


(function () {

module("rollbot");

var kSearchResults = {
  "cursor": "long_string_we_call_cursor",
  "results": [
    {
      "description": "Blink roll 151668:151677\n\nhttp:\/\/build.chromium.org\/f\/chromium\/perf\/dashboard\/ui\/changelog_blink.html?url=\/trunk&range=151669:151677&mode=html\nTBR=\nBUG=",
      "cc": [
        "chromium-reviews@chromium.org",
      ],
      "reviewers": [
      ],
      "messages": [
        {
          "sender": "eseidel@chromium.org",
          "recipients": [
            "eseidel@chromium.org",
            "chromium-reviews@chromium.org",
          ],
          "text": "This roll was automatically created by the Blink AutoRollBot (crbug.com\/242461).\nInclude STOP in this message, but should be ignored.",
          "disapproval": false,
          "date": "2013-06-03 18:14:34.033780",
          "approval": false
        },
      ],
      "owner_email": "eseidel@chromium.org",
      "private": false,
      "base_url": "https:\/\/chromium.googlesource.com\/chromium\/src.git@master",
      "owner": "eseidel",
      "subject": "Blink roll 151668:151677",
      "created": "2013-06-03 18:14:28.926040",
      "patchsets": [
        1
      ],
      "modified": "2013-06-03 18:14:46.869990",
      "closed": false,
      "commit": true,
      "issue": 16337011
    },
    {
      "description": "Add --json-output option to layout_test_wrapper.py\n\nBUG=238381",
      "cc": [
        "chromium-reviews@chromium.org",
      ],
      "reviewers": [
        "iannucci@chromium.org"
      ],
      "messages": [
        {
          "sender": "eseidel@chromium.org",
          "recipients": [
            "eseidel@chromium.org",
            "chromium-reviews@chromium.org",
          ],
          "text": "I'm not quite sure how to test this code.\n\nI'm also ",
          "disapproval": false,
          "date": "2013-05-30 23:42:39.309160",
          "approval": false
        },
      ]
    }
  ]
};

var kStoppedIssue = {
  "description": "Blink roll 152079:152080\n\nhttp:\/\/build.chromium.org\/f\/chromium\/perf\/dashboard\/ui\/changelog_blink.html?url=\/trunk&range=152080:152080&mode=html\nTBR=\nBUG=",
  "cc": [
    "chromium-reviews@chromium.org",
    "none (channel is sheriff)@chromium.org"
  ],
  "reviewers": [
    "ilevy@chromium.org"
  ],
  "messages": [
    {
      "sender": "eseidel@chromium.org",
      "recipients": [
        "eseidel@chromium.org",
        "chromium-reviews@chromium.org",
      ],
      "text": "This string has STOP in it, but should be ignored as the first message.",
      "date": "2013-06-09 06:47:35.825820",
    },
    {
      "sender": "commit-bot@chromium.org",
      "recipients": [
        "eseidel@chromium.org",
        "chromium-reviews@chromium.org",
      ],
      "text": "CQ is trying da patch. Follow status at\nhttps:\/\/chromium-status.appspot.com\/cq\/eseidel@chromium.org\/16606004\/1",
      "date": "2013-06-09 06:47:45.529170",
    },
    {
      "sender": "ilevy@chromium.org",
      "recipients": [
        "eseidel@chromium.org",
        "ilevy@chromium.org",
        "chromium-reviews@chromium.org",
      ],
      "text": "STOP",
      "date": "2013-06-09 07:59:48.280360",
    },
    {
      "sender": "eseidel@chromium.org",
      "recipients": [
        "eseidel@chromium.org",
        "ilevy@chromium.org",
        "chromium-reviews@chromium.org",
      ],
      "text": "Rollbot was stopped by the presence of \"STOP\" in an earlier comment on this issue.\n",
      "date": "2013-06-10 19:35:44.710470",
    }
  ],
  "owner_email": "eseidel@chromium.org",
  "private": false,
  "base_url": "https:\/\/chromium.googlesource.com\/chromium\/src.git@master",
  "owner": "eseidel",
  "subject": "Blink roll 152079:152080",
  "created": "2013-06-09 06:47:31.518010",
  "patchsets": [
    1
  ],
  "modified": "2013-06-10 19:56:59.618710",
  "closed": true,
  "commit": false,
  "issue": 16606004
};

asyncTest("fetchCurrentRoll", 6, function() {
    var simulator = new NetworkSimulator();
    simulator.json = function(url)
    {
        return Promise.resolve(kSearchResults);
    };

    simulator.runTest(function() {
        rollbot.fetchCurrentRoll().then(function(roll) {
            equals(roll.issue, 16337011);
            equals(roll.url, "https://codereview.chromium.org/16337011");
            equals(roll.isStopped, false);
            equals(roll.fromRevision, "151668");
            equals(roll.toRevision, "151677");
        });
    }).then(start);
});

test("_isRollbotStopped", 1, function() {
    equals(true, rollbot._isRollbotStopped(kStoppedIssue));
});

})();
