// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class stores information of one single entry of log
 */

var CrosLogEntry = function() {

  /**
    * @constructor
    */
  function CrosLogEntry() {
    // The entry is visible by default
    this.visibility = true;
  }

  CrosLogEntry.prototype = {
    //------------------------------------------------------------------------
    // Log input text parser
    // Parses network log into tokens like time, name, pid
    // and description.
    //--------------------------------------------------------------------------
    tokenizeNetworkLog: function(NetworkLogEntry) {
      var tokens = NetworkLogEntry.split(' ');
      var timeTokens = tokens[0].split(/[\s|\:|\-|T|\.]/);

      // List of all parameters for Date Object
      var year = timeTokens[0];
      var month = timeTokens[1];
      var day = timeTokens[2];
      var hour = timeTokens[3];
      var minute = timeTokens[4];
      var second = timeTokens[5];
      var millisecond = (parseInt(timeTokens[6]) / 1000).toFixed(0);
      this.time = new Date(year, month, day, hour, minute,
                           second, millisecond);

      // Parses for process name and ID.
      var process = tokens[2];
      if (hasProcessID(process)) {
        var processTokens = process.split(/[\[|\]]/);
        this.processName = processTokens[0];
        this.processID = processTokens[1];
      } else {
        this.processName = process.split(/\:/)[0];
        this.processID = 'Unknown';
      }

      // Gets level of the log: error|warning|info|unknown if failed.
      this.level = hasLevelInfo(tokens[3]);

      // Treats the rest of the entry as description.
      var descriptionStartPoint = NetworkLogEntry.indexOf(tokens[2]) +
          tokens[2].length;
      this.description = NetworkLogEntry.substr(descriptionStartPoint);
    },

    // Represents the Date object as a string.
    getTime: function() {
      return this.time.getMonth() + '/' + this.time.getDate() +
          ' ' + this.time.getHours() + ':' + this.time.getMinutes() +
          ':' + this.time.getSeconds() + ':' + this.time.getMilliseconds();
    }
  };

  /**
   * Helper function
   * Takes a token as input and searches for '['.
   * We assume if the token contains '[' it contains a process ID.
   *
   * @param {string} token A token from log
   * @return {boolean} true if '[' is found
   */
  var hasProcessID = function(token) {
    return token != undefined && token.indexOf('[') != -1;
  }

  /**
   * Helper function
   * Checks if the input token contains level information.
   *
   * @param {string} token A token from log
   * @return {string} Level found in the token
   */
  var hasLevelInfo = function(token) {
    if (token == undefined)
      return 'Unknown';
    if (token.toLowerCase().indexOf('err') != -1) {
      return 'Error';
    } else if (token.toLowerCase().indexOf('warn') != -1) {
      return 'Warning';
    } else if (token.toLowerCase().indexOf('info') != -1) {
      return 'Info';
    } else {
      return 'Unknown';
    }
  }

  return CrosLogEntry;
}();
