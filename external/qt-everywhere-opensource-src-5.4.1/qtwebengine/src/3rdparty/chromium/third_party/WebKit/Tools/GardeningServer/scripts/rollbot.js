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

var rollbot = rollbot || {};

(function() {

// FIXME: This will need to change once we have a real account for the rollbot.
var rollBotAccount = "eseidel@chromium.org";
var issueSearchURL = config.kRietveldURL + "/search?" + $.param({
    "owner": rollBotAccount,
    "closed": 3, // Only open issues.
    "with_messages": "true",
    "format": "json",
});

var rollSubjectRegexp = /Blink roll (\d+):(\d+)/;

function findRollIssue(results) {
    results = results['results'];
    for (var i = 0; i < results.length; i++) {
        var result = results[i];
        if (result['subject'].match(rollSubjectRegexp))
            return result;
    }
    return null;
}

function isRollbotStopped(issue) {
    // Ignore the first message as it always contains "STOP"
    return issue.messages.slice(1).some(function(message) { return message.text.match(/STOP/); });
}

rollbot.fetchCurrentRoll = function() {
    return net.json(issueSearchURL).then(function(searchJSON) {
        var issue = findRollIssue(searchJSON);
        if (!issue)
            return null;

        var issueNumber = issue['issue'];
        var subjectMatch = issue['subject'].match(rollSubjectRegexp);
        return {
            'issue': issueNumber,
            'url': config.kRietveldURL + "/" + issueNumber,
            'isStopped': isRollbotStopped(issue),
            'fromRevision': subjectMatch[1],
            'toRevision': subjectMatch[2],
        };
    });
};

// Exposed for unittesting.
rollbot._isRollbotStopped = isRollbotStopped;

})();
