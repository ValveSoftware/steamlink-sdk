/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

var builders = builders || {};

(function() {

var kUpdateStepName = 'update';
var kUpdateScriptsStepName = 'update_scripts';
var kCompileStepName = 'compile';
var kWebKitTestsStepNames = ['webkit_tests', 'layout-test'];

var kCrashedOrHungOutputMarker = 'crashed or hung';

function urlForBuildInfo(builderName, buildNumber)
{
    return config.buildConsoleURL + '/json/builders/' + encodeURIComponent(builderName) + '/builds/' + encodeURIComponent(buildNumber);
}

function didFail(step)
{
    if (kWebKitTestsStepNames.indexOf(step.name) != -1) {
        // run-webkit-tests fails to generate test coverage when it crashes or hangs.
        // FIXME: Do build.webkit.org bots output this marker when the tests fail to run?
        return step.text.indexOf(kCrashedOrHungOutputMarker) != -1;
    }
    // The first item in step.results is the success of the step:
    // 0 == pass, 1 == warning, 2 == fail
    return step.results[0] == 2;
}

function failingSteps(buildInfo)
{
    return buildInfo.steps.filter(didFail);
}

function mostRecentCompletedBuildNumber(individualBuilderStatus)
{
    if (!individualBuilderStatus)
        return null;

    for (var i = individualBuilderStatus.cachedBuilds.length - 1; i >= 0; --i) {
        var buildNumber = individualBuilderStatus.cachedBuilds[i];
        if (individualBuilderStatus.currentBuilds.indexOf(buildNumber) == -1)
            return buildNumber;
    }

    return null;
}

var g_buildInfoCache = new base.AsynchronousCache(function(key) {
    var explodedKey = key.split('\n');
    return net.json(urlForBuildInfo(explodedKey[0], explodedKey[1]));
});

builders.clearBuildInfoCache = function()
{
    g_buildInfoCache.clear();
};

function fetchMostRecentBuildInfoByBuilder()
{
    var buildInfoByBuilder = {};
    var requestPromises = [];
    return net.json(config.buildConsoleURL + '/json/builders').then(function(builderStatus) {
        var builderNames = Object.keys(builderStatus);
        builderNames.forEach(function(builderName) {
            if (!config.builderApplies(builderName))
                return;

            var buildNumber = mostRecentCompletedBuildNumber(builderStatus[builderName]);
            if (!buildNumber)
                return;

            requestPromises.push(g_buildInfoCache.get(builderName + '\n' + buildNumber)
                                 .then(function(buildInfo) {
                                     buildInfoByBuilder[builderName] = buildInfo;
                                 }));
        });

        return Promise.all(requestPromises).then(function() {
            return buildInfoByBuilder;
        });
    });
}

builders.buildersFailingNonLayoutTests = function()
{
    return fetchMostRecentBuildInfoByBuilder().then(function(buildInfoByBuilder) {
        var failureList = {};
        $.each(buildInfoByBuilder, function(builderName, buildInfo) {
            if (!buildInfo)
                return;
            var failures = failingSteps(buildInfo);
            if (failures.length)
                failureList[builderName] = failures.map(function(failure) { return failure.name; });
        });
        return failureList;
    });
};

builders.mostRecentBuildForBuilder = function(builderName) {
    return net.json(config.buildConsoleURL + '/json/builders/' + builderName).then(function(builderStatus) {
        var cachedBuilds = builderStatus.cachedBuilds;
        var mostRecentBuild = Math.max.apply(Math, cachedBuilds);
        return mostRecentBuild;
    });
};

})();
