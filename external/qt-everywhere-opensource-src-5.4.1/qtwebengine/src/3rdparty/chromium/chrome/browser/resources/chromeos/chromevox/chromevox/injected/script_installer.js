// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines the ScriptInstaller functions which install scripts
 * into the web page (not a content script)
 *
 */

goog.provide('cvox.ScriptInstaller');

goog.require('cvox.DomUtil');

/**
 * URL pattern where we do not allow script installation.
 * @type {RegExp}
 */
cvox.ScriptInstaller.blacklistPattern = /chrome:\/\/|chrome-extension:\/\//;

/**
 * Installs a script in the web page.
 * @param {Array.<string>} srcs An array of URLs of scripts.
 * @param {string} uid A unique id.  This function won't install the same set of
 *      scripts twice.
 * @param {function()=} opt_onload A function called when the script has loaded.
 * @param {string=} opt_chromevoxScriptBase An optional chromevoxScriptBase
 *     attribute to add.
 * @return {boolean} False if the script already existed and this function
 * didn't do anything.
 */
cvox.ScriptInstaller.installScript = function(srcs, uid, opt_onload,
    opt_chromevoxScriptBase) {
  if (cvox.ScriptInstaller.blacklistPattern.test(document.URL)) {
    return false;
  }
  if (document.querySelector('script[' + uid + ']')) {
    return false;
  }
  if (!srcs) {
    return false;
  }
  for (var i = 0, scriptSrc; scriptSrc = srcs[i]; i++) {
    // Directly write the contents of the script we are trying to inject into
    // the page.
    var xhr = new XMLHttpRequest();
    var url = scriptSrc + '?' + new Date().getTime();
    xhr.onreadystatechange = function() {
        if (xhr.readyState == 4) {
          var scriptText = xhr.responseText;
          // Add a magic comment to the bottom of the file so that
          // Chrome knows the name of the script in the JavaScript debugger.
          scriptText += '\n//# sourceURL=' + scriptSrc + '\n';

          var apiScript = document.createElement('script');
          apiScript.type = 'text/javascript';
          apiScript.setAttribute(uid, '1');
          apiScript.textContent = scriptText;
          if (opt_chromevoxScriptBase) {
            apiScript.setAttribute('chromevoxScriptBase',
                opt_chromevoxScriptBase);
          }
          cvox.DomUtil.addNodeToHead(apiScript);
        }
      };
    try {
      xhr.open('GET', url, false);
      xhr.send(null);
    } catch (exception) {
      window.console.log('Warning: ChromeVox external script loading for ' +
          document.location + ' stopped after failing to install ' + scriptSrc);
      return false;
    }
  }
  if (opt_onload) {
    opt_onload();
  }
  return true;
};
