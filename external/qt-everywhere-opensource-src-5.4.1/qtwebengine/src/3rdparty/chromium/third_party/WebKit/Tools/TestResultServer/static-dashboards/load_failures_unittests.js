// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

module('loadfailures');

test('htmlForBuilder', 1, function() {
    var html = loadfailures._htmlForBuilder('MockBuilder', 'MockTestType', {
        'MockBuilder': new builders.BuilderMaster({name: 'MockMaster', url_name: 'mock.master', tests: [], groups: []}),
    });

    equal(html, '<tr class="builder">' +
        '<td>MockBuilder' +
        '<td><a href="http://test-results.appspot.com/testfile?testtype=MockTestType&builder=MockBuilder&master=MockMaster">uploaded results</a>' +
        '<td><a href="http://mockbasepath/builders/MockBuilder">buildbot</a>' +
    '</tr>');
});

test('html', 5, function() {
    var mockBuilderMaster = new builders.BuilderMaster({name: 'MockMaster', url_name: 'mock.master', tests: [], groups: []}),
    var failureData = {
        '@ToT Chromium': {
            failingBuilders: {
                'MockTestType': ['MockFailingBuilder'],
            },
            staleBuilders: {
                'MockTestType': ['MockStaleBuilder'],
            },
            testTypesWithNoSuccessfullLoads: [ 'MockTestType' ],
            builderToMaster: {
                'MockFailingBuilder': mockBuilderMaster,
                'MockStaleBuilder': mockBuilderMaster,
            },
        },
        '@ToT Blink': {
            failingBuilders: {
                'MockTestType': ['MockFailingBuilder'],
            },
            staleBuilders: {},
            testTypesWithNoSuccessfullLoads: [],
            builderToMaster: {
                'MockFailingBuilder': mockBuilderMaster,
            },
        },
    }

    var container = document.createElement('div');
    container.innerHTML = loadfailures._html(failureData);

    equal(container.querySelectorAll('h1').length, 2, 'There should be two group headers');
    equal(container.querySelectorAll('.builder').length, 3, 'There should be 3 builders');

    var firstFailingBuilder = container.querySelector('table').querySelector('tr:nth-child(2) > td:nth-child(2)');
    equal(firstFailingBuilder.querySelector('b').innerHTML, 'No builders with up to date results.');
    equal(firstFailingBuilder.querySelectorAll('.builder').length, 1, 'There should be one failing builder in the first group.');

    var firstStaleBuilder = container.querySelector('table').querySelector('tr:nth-child(2) > td:nth-child(3)');
    equal(firstFailingBuilder.querySelectorAll('.builder').length, 1, 'There should be one stale builder in the first group.');
});
