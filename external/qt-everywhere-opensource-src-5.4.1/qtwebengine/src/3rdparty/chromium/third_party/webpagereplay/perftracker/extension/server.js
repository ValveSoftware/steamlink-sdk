// Copyright 2010 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Functions for uploading to the server

// These are the URLs used by the backend for posting data
var kServerPostSetUrl = "set";
var kServerPostResultUrl = "result";
var kServerPostSummaryUrl = "summary";

// BrowserDetect is thanks to www.quirksmode.org/js/detect.html
var BrowserDetect = {
  init: function () {
    this.browser = this.searchString(this.dataBrowser) || "An unknown browser";
    this.version = this.searchVersion(navigator.userAgent)
      || this.searchVersion(navigator.appVersion)
      || "an unknown version";
    this.OS = this.searchString(this.dataOS) || "an unknown OS";
  },
  searchString: function (data) {
    for (var i=0;i<data.length;i++)  {
      var dataString = data[i].string;
      var dataProp = data[i].prop;
      this.versionSearchString = data[i].versionSearch || data[i].identity;
      if (dataString) {
        if (dataString.indexOf(data[i].subString) != -1)
          return data[i].identity;
      }
      else if (dataProp)
        return data[i].identity;
    }
  },
  searchVersion: function (dataString) {
    var index = dataString.indexOf(this.versionSearchString);
    if (index == -1)
      return;
    var version_string = dataString.substring(index+this.versionSearchString.length+1);
    var end_index = version_string.indexOf(" ");
    if (end_index < 0) {
      return version_string;
    }
    return version_string.substring(0, end_index);
  },
  dataBrowser: [
    {
      string: navigator.userAgent,
      subString: "Chrome",
      identity: "Chrome"
    },
    {
      string: navigator.vendor,
      subString: "Apple",
      identity: "Safari",
      versionSearch: "Version"
    },
    {
      prop: window.opera,
      identity: "Opera"
    },
    {
      string: navigator.userAgent,
      subString: "Firefox",
      identity: "Firefox"
    },
    {    // for newer Netscapes (6+)
      string: navigator.userAgent,
      subString: "Netscape",
      identity: "Netscape"
    },
    {
      string: navigator.userAgent,
      subString: "MSIE",
      identity: "Explorer",
      versionSearch: "MSIE"
    },
    {
      string: navigator.userAgent,
      subString: "Gecko",
      identity: "Mozilla",
      versionSearch: "rv"
    },
  ],
  dataOS : [
    {
      string: navigator.platform,
      subString: "Win",
      identity: "Windows"
    },
    {
      string: navigator.platform,
      subString: "Mac",
      identity: "Mac"
    },
    {
      string: navigator.userAgent,
      subString: "iPhone",
      identity: "iPhone/iPod"
    },
    {
      string: navigator.platform,
      subString: "Linux",
      identity: "Linux"
    }
  ]
};
BrowserDetect.init();

function XHRGet(url, callback) {
  var self = this;
  var xhr = new XMLHttpRequest();
  xhr.open("GET", url, true);
  xhr.send();

  xhr.onreadystatechange = function() {
    if (xhr.readyState == 4) {
      if (xhr.status != 200)  {
        console.log("XHR error getting url " + url + ", error: " + xhr.status);
      }
      callback(xhr.responseText);
    }
  }
}

function XHRPost(url, data, callback) {
  var xhr = new XMLHttpRequest();
  xhr.open("POST", url, true);
  xhr.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
  xhr.send(data);
  var callback_complete = false;

  xhr.onreadystatechange = function() {
    if (xhr.readyState == 4) {
      if (xhr.status != 200)  {
        console.log("XHR error posting url " + url + ", error: " + xhr.status);
      }
      if (callback_complete) {
        alert("Error! Double XHR callback.");
      }
      callback(xhr.responseText);
      callback_complete = true;
    }
  }
}

function copy(obj) {
  var copy = {};
  for (var prop in obj)
    copy[prop] = obj[prop];
  return copy;
}

function jsonToPostData(json) {
  var post_data = [];
  for (var prop in json) {
    post_data.push(prop + "=" + encodeURIComponent(json[prop]));
  }
  return post_data.join("&");
}

// Submits a set of test runs up to the server.
function TestResultSubmitter(config) {
  var self = this;
  var user_callback;

  this.AppEngineLogin = function(callback) {
    if (config.server_login) {
      new XHRGet(config.server_login, callback);
    } else {
      console.log('appengine login skipped');
      callback();
    }
  }

  // Creates a test.
  // Upon test creation, the callback will be called with a single argument
  // containing the status of the creation.
  this.CreateTest = function(loadType, callback) {
    self.AppEngineLogin(function() {
      if (config.record) { setTimeout(callback, 0); return; }
      var data = copy(config);
      data["cmd"] = "create";
      data["version"] = BrowserDetect.browser + " " + BrowserDetect.version;
      data["platform"] = BrowserDetect.OS;
      data["load_type"] = loadType;

      url = config.server_url + kServerPostSetUrl;

      // When special notes are added, we consider the result a custom version.
      if (config.notes.length > 0) {
        data["version"] += "custom";
      }

      user_callback = callback;
      new XHRPost(url, jsonToPostData(data), function(result) {
        user_callback(loadType, result);
      });
    });
  }

  // Post a single result
  this.PostResult = function (result, callback) {
    if (config.record) { setTimeout(callback, 0); return; }
    var data = copy(result);

    url = config.server_url + kServerPostResultUrl;
    new XHRPost(url, jsonToPostData(data), callback);
  }

  // Post the rollup summary of a set of data
  this.PostSummary = function(data, callback) {
    if (config.record) { setTimeout(callback, 0); return; }
    var result = copy(data);
    url = config.server_url + kServerPostSummaryUrl;
    new XHRPost(url, jsonToPostData(result), callback);
  }

  // Update the set with its summary data
  this.UpdateSetSummary = function(data, callback) {
    if (config.record) { setTimeout(callback, 0); return; }
    var result = copy(data);
    result["cmd"] = "update";
    url = config.server_url + kServerPostSetUrl;
    new XHRPost(url, jsonToPostData(result), callback);
  }
}
