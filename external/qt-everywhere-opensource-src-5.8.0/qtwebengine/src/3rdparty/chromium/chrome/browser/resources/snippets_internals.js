// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('chrome.SnippetsInternals', function() {
  'use strict';

  // Stores the list of snippets we received in receiveSnippets.
  var lastSnippets = [];

  function initialize() {
    $('submit-download').addEventListener('click', function(event) {
      chrome.send('download', [$('hosts-input').value]);
      event.preventDefault();
    });

    $('submit-clear').addEventListener('click', function(event) {
      chrome.send('clear');
      event.preventDefault();
    });

    $('submit-dump').addEventListener('click', function(event) {
      downloadJson(JSON.stringify(lastSnippets));
      event.preventDefault();
    });

    $('last-json-button').addEventListener('click', function(event) {
      $('last-json-container').classList.toggle('hidden');
    });

    $('last-json-dump').addEventListener('click', function(event) {
      downloadJson($('last-json-text').innerText);
      event.preventDefault();
    });

    $('discarded-snippets-clear').addEventListener('click', function(event) {
      chrome.send('clearDiscarded');
      event.preventDefault();
    });

    chrome.send('loaded');
  }

  function setHostRestricted(restricted) {
    receiveProperty('switch-restrict-to-hosts', restricted ? 'True' : 'False');
    if (!restricted) {
      $('hosts-restrict').classList.add('hidden');
    }
  }

  function receiveProperty(propertyId, value) {
    $(propertyId).textContent = value;
  }

  function receiveHosts(hosts) {
    displayList(hosts, 'hosts');

    $('hosts-input').value = hosts.list.map(
      function(host) { return host.url;}).join(' ');
  }

  function receiveSnippets(snippets) {
    lastSnippets = snippets;
    displayList(snippets, 'snippets', 'snippet-title');
  }

  function receiveDiscardedSnippets(discardedSnippets) {
    displayList(discardedSnippets, 'discarded-snippets',
                'discarded-snippet-title');
  }

  function receiveJson(json) {
    var trimmed = json.trim();
    var hasContent = (trimmed && trimmed != '{}');

    if (hasContent) {
      receiveProperty('last-json-text', trimmed);
      $('last-json').classList.remove('hidden');
    } else {
      $('last-json').classList.add('hidden');
    }
  }

  function downloadJson(json) {
    // Redirect the browser to download data in |json| as a file "snippets.json"
    // (Setting Content-Disposition: attachment via a data: URL is not possible;
    // create a link with download attribute and simulate a click, instead.)
    var link = document.createElement('a');
    link.download = 'snippets.json';
    link.href = 'data:,' + json;
    link.click();
  }

  function displayList(object, domId, titleClass) {
    jstProcess(new JsEvalContext(object), $(domId));

    var text;
    var display;

    if (object.list.length > 0) {
      text = '';
      display = 'inline';
    } else {
      text = 'The list is empty.';
      display = 'none';
    }

    if ($(domId + '-empty')) $(domId + '-empty').textContent = text;
    if ($(domId + '-clear')) $(domId + '-clear').style.display = display;

    var links = document.getElementsByClassName(titleClass);
    for (var link of links) {
      link.addEventListener('click', function(event) {
        var id = event.currentTarget.getAttribute('snippet-id');
        $(id).classList.toggle('hidden');
      });
    }
  }

  // Return an object with all of the exports.
  return {
    initialize: initialize,
    setHostRestricted: setHostRestricted,
    receiveProperty: receiveProperty,
    receiveHosts: receiveHosts,
    receiveSnippets: receiveSnippets,
    receiveDiscardedSnippets: receiveDiscardedSnippets,
    receiveJson: receiveJson,
  };
});

document.addEventListener('DOMContentLoaded',
                          chrome.SnippetsInternals.initialize);
