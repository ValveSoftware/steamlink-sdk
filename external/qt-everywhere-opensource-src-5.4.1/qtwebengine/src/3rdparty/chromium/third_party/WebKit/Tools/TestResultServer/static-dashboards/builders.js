// Copyright (C) 2012 Google Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

function LOAD_BUILDBOT_DATA(builderData)
{
    builders.masters = {};
    var groups = {};
    var testTypes = {};
    var testTypesThatDoNotUpload = {};
    builders.noUploadTestTypes = builderData['no_upload_test_types']
    builderData['masters'].forEach(function(master) {
        builders.masters[master.name] = new builders.BuilderMaster(master);

        master.groups.forEach(function(group) { groups[group] = true; });

        Object.keys(master.tests).forEach(function(testType) {
            if (builders.testTypeUploadsToFlakinessDashboardServer(testType))
                testTypes[testType] = true;
            else
                testTypesThatDoNotUpload[testType] = true;
        });
    });
    builders.groups = Object.keys(groups);
    builders.groups.sort();
    builders.testTypes = Object.keys(testTypes);
    builders.testTypes.sort();
    // FIXME: Expose this in the flakiness dashboard UI and give a clear error message
    // pointing to a bug about getting the test type in question uploading results.
    builders.testTypesThatDoNotUpload = Object.keys(testTypesThatDoNotUpload);
    builders.testTypesThatDoNotUpload.sort();
}

var builders = builders || {};

(function() {

builders.testTypeUploadsToFlakinessDashboardServer = function(testType)
{
    for (var i = 0; i < builders.noUploadTestTypes.length; i++) {
        if (string.contains(testType, builders.noUploadTestTypes[i]))
            return false;
    }
    return true;
}

var currentBuilderGroup = {};
var testTypesThatRunToTBlinkBots = ['layout-tests', 'webkit_unit_tests'];

builders.getBuilderGroup = function(groupName, testType)
{
    if (!builders in currentBuilderGroup) {
        currentBuilderGroup = builders.loadBuildersList(groupName, testType);
    }
    return currentBuilderGroup;
}

function isChromiumWebkitTipOfTreeTestRunner(builder)
{
    return !isChromiumWebkitDepsTestRunner(builder);
}

function isChromiumWebkitDepsTestRunner(builder)
{
    return builder.indexOf('(deps)') != -1;
}

builders._builderFilter = function(groupName, masterName, testType)
{
    if (testTypesThatRunToTBlinkBots.indexOf(testType) == -1) {
        if (masterName == 'ChromiumWebkit' && groupName != '@ToT Blink')
            return function() { return false };
        return null;
    }

    if (groupName == '@ToT Blink')
        return isChromiumWebkitTipOfTreeTestRunner;

    if (groupName == '@ToT Chromium')
        return isChromiumWebkitDepsTestRunner;

    return null;
}

// FIXME: When we change to show multiple groups at once, this will need to
// change to key off groupName and builderName.
builders.master = function(builderName)
{
    return builders.builderToMaster[builderName];
}

builders.loadBuildersList = function(groupName, testType)
{
    if (!groupName || !testType) {
        console.warn("Group name and/or test type were empty.");
        return new builders.BuilderGroup(false);
    }
    var builderGroup = new builders.BuilderGroup(groupName == '@ToT Blink');
    builders.builderToMaster = {};

    for (masterName in builders.masters) {
        if (!builders.masters[masterName])
            continue;

        var master = builders.masters[masterName];
        var hasTest = testType in master.tests;
        var isInGroup = master.groups.indexOf(groupName) != -1;

        if (hasTest && isInGroup) {
            var builderList = master.tests[testType].builders;
            var builderFilter = builders._builderFilter(groupName, masterName, testType);
            if (builderFilter)
                builderList = builderList.filter(builderFilter);
            builderGroup.append(builderList);

            builderList.forEach(function (builderName) {
                builders.builderToMaster[builderName] = master;
            });
        }
    }

    currentBuilderGroup = builderGroup;
    return currentBuilderGroup;
}

builders.getAllGroupNames = function()
{
    return builders.groups;
}

builders.BuilderMaster = function(master_data)
{
    this.name = master_data.name;
    this.basePath = 'http://build.chromium.org/p/' + master_data.url_name;
    this.tests = master_data.tests;
    this.groups = master_data.groups;
}

builders.BuilderMaster.prototype = {
    logPath: function(builder, buildNumber)
    {
        return this.builderPath(builder) + '/builds/' + buildNumber;
    },
    builderPath: function(builder)
    {
        return this.basePath + '/builders/' + builder;
    },
    builderJsonPath: function()
    {
        return this.basePath + '/json/builders';
    },
}

builders.BuilderGroup = function(isToTBlink)
{
    this.isToTBlink = isToTBlink;
    // Map of builderName (the name shown in the waterfall) to builderPath (the
    // path used in the builder's URL)
    this.builders = {};
}

builders.BuilderGroup.prototype = {
    append: function(builders) {
        builders.forEach(function(builderName) {
            this.builders[builderName] = builderName.replace(/[ .()]/g, '_');
        }, this);
    },
    defaultBuilder: function()
    {
        for (var builder in this.builders)
            return builder;
        console.error('There are no builders in this builder group.');
    },
    master: function()
    {
        return builders.master(this.defaultBuilder());
    },
}

builders.groupNamesForTestType = function(testType)
{
    var groupNames = [];
    for (masterName in builders.masters) {
        var master = builders.masters[masterName];
        if (testType in master.tests) {
            groupNames = groupNames.concat(master.groups);
        }
    }

    if (groupNames.length == 0) {
        console.error("The current test type wasn't present in any groups:", testType);
        return groupNames;
    }

    var groupNames = groupNames.reduce(function(prev, curr) {
        if (prev.indexOf(curr) == -1) {
            prev.push(curr);
        }
        return prev;
    }, []);

    return groupNames;
}

})();
