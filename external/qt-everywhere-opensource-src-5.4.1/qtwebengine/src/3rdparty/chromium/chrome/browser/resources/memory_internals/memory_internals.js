// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var g_main_view = null;

/**
 * This class is the root view object of the page.
 */
var MainView = (function() {
  'use strict';

  /**
   * @constructor
   */
  function MainView() {
    $('button-update').onclick = function() {
      chrome.send('update');
    };
  };

  MainView.prototype = {
    /**
     * Receiving notification to display memory snapshot.
     * @param {Object}  Information about memory in JSON format.
     */
    onSetSnapshot: function(browser) {
      $('json').textContent = JSON.stringify(browser);
      $('json').style.display = 'block';

      $('os-value').textContent = browser['os'] + ' (' +
          browser['os_version'] + ')';
      $('uptime-value').textContent =
          secondsToHMS(Math.floor(browser['uptime'] / 1000));

      this.updateSnapshot(browser['processes']);
      this.updateExtensions(browser['extensions']);
    },

    /**
     * Update process information table.
     * @param {Object} processes information about memory.
     */
    updateSnapshot: function(processes) {
      // Remove existing processes.
      var size = $('snapshot-view').getElementsByClassName('process').length;
      for (var i = 0; i < size; ++i) {
        $('snapshot-view').deleteRow(-1);
      }

      var template = $('process-template').childNodes;
      // Add processes.
      for (var p in processes) {
        var process = processes[p];

        var row = $('snapshot-view').insertRow(-1);
        // We skip |template[0]|, because it is a (invalid) Text object.
        for (var i = 1; i < template.length; ++i) {
          var value = '---';
          switch (template[i].className) {
          case 'process-id':
            value = process['pid'];
            break;
          case 'process-info':
            value = process['type'];
            if (process['type'].match(/^Tab/) && 'history' in process) {
              // Append each tab's history.
              for (var j = 0; j < process['history'].length; ++j) {
                value += '<dl><dt>History ' + j + ':' +
                    JoinLinks(process['history'][j]) + '</dl>';
              }
            } else {
              value += '<br>' + process['titles'].join('<br>');
            }
            break;
          case 'process-memory-private':
            value = process['memory_private'];
            break;
          case 'process-memory-v8':
            if (process['v8_alloc'] !== undefined) {
              value = process['v8_used'] + '<br>/ ' + process['v8_alloc'];
            }
            break;
          }
          var col = row.insertCell(-1);
          col.innerHTML = value;
          col.className = template[i].className;
        }
        row.setAttribute('class', 'process');
      }
    },

    /**
     * Update extension information table.
     * @param {Object} extensions information about memory.
     */
    updateExtensions: function(extensions) {
      // Remove existing information.
      var size =
          $('extension-view').getElementsByClassName('extension').length;
      for (var i = 0; i < size; ++i) {
        $('extension-view').deleteRow(-1);
      }

      var template = $('extension-template').childNodes;
      for (var id in extensions) {
        var extension = extensions[id];

        var row = $('extension-view').insertRow(-1);
        // We skip |template[0]|, because it is a (invalid) Text object.
        for (var i = 1; i < template.length; ++i) {
          var value = '---';
          switch (template[i].className) {
          case 'extension-id':
            value = extension['pid'];
            break;
          case 'extension-info':
            value = extension['titles'].join('<br>');
            break;
          case 'extension-memory':
            value = extension['memory_private'];
            break;
          }
          var col = row.insertCell(-1);
          col.innerHTML = value;
          col.className = template[i].className;
        }
        row.setAttribute('class', 'extension');
      }
    }
  };

  function JoinLinks(tab) {
    var line = '';
    for (var l in tab['history']) {
      var history = tab['history'][l];
      var title = (history['title'] == '') ? history['url'] : history['title'];
      var url = '<a href="' + history['url'] + '">' + HTMLEscape(title) +
          '</a> (' + secondsToHMS(history['time']) + ' ago)';
      if (l == tab['index']) {
        url = '<strong>' + url + '</strong>';
      }
      line += '<dd>' + url;
    }
    return line;
  };

  /**
   * Produces a readable string int the format '<HH> hours <MM> min. <SS> sec.'
   * representing the amount of time provided as the number of seconds.
   * @param {number} totalSeconds The total amount of seconds.
   * @return {string} The formatted HH hours/hours MM min. SS sec. string
   */
  function secondsToHMS(totalSeconds) {
    totalSeconds = Number(totalSeconds);
    var hour = Math.floor(totalSeconds / 3600);
    var min = Math.floor(totalSeconds % 3600 / 60);
    var sec = Math.floor(totalSeconds % 60);
    return (hour > 0 ? (hour + (hour > 1 ? ' hours ' : ' hour ')) : '') +
           (min > 0 ? (min + ' min. ') : '') +
           (sec + ' sec. ');
  }

  return MainView;
})();

/**
 * Initialize everything once we have access to chrome://memory-internals.
 */
document.addEventListener('DOMContentLoaded', function() {
  g_main_view = new MainView();
});
