// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Does common handling for requests coming from web pages and
 * routes them to the provided handler.
 */

/**
 * Gets the scheme + origin from a web url.
 * @param {string} url Input url
 * @return {?string} Scheme and origin part if url parses
 */
function getOriginFromUrl(url) {
  var re = new RegExp('^(https?://)[^/]*/?');
  var originarray = re.exec(url);
  if (originarray == null) return originarray;
  var origin = originarray[0];
  while (origin.charAt(origin.length - 1) == '/') {
    origin = origin.substring(0, origin.length - 1);
  }
  if (origin == 'http:' || origin == 'https:')
    return null;
  return origin;
}

/**
 * Parses the text as JSON and returns it as an array of strings.
 * @param {string} text Input JSON
 * @return {Array.<string>} Array of origins
 */
function getOriginsFromJson(text) {
  try {
    var urls = JSON.parse(text);
    var origins = [];
    for (var i = 0, url; url = urls[i]; i++) {
      var origin = getOriginFromUrl(url);
      if (origin)
        origins.push(origin);
    }
    return origins;
  } catch (e) {
    console.log(UTIL_fmt('could not parse ' + text));
    return [];
  }
}

/**
 * Fetches the app id, and calls a callback with list of allowed origins for it.
 * @param {string} appId the app id to fetch.
 * @param {Function} cb called with a list of allowed origins for the app id.
 */
function fetchAppId(appId, cb) {
  var origin = getOriginFromUrl(appId);
  if (!origin) {
    cb(404, appId);
    return;
  }
  var xhr = new XMLHttpRequest();
  var origins = [];
  xhr.open('GET', appId, true);
  xhr.onloadend = function() {
    if (xhr.status != 200) {
      cb(xhr.status, appId);
      return;
    }
    cb(xhr.status, appId, getOriginsFromJson(xhr.responseText));
  };
  xhr.send();
}

/**
 * Retrieves a set of distinct app ids from the SignData.
 * @param {SignData=} signData Input signature data
 * @return {Array.<string>} array of distinct app ids.
 */
function getDistinctAppIds(signData) {
  var appIds = [];
  if (!signData) {
    return appIds;
  }
  for (var i = 0, request; request = signData[i]; i++) {
    var appId = request['appId'];
    if (appId && appIds.indexOf(appId) == -1) {
      appIds.push(appId);
    }
  }
  return appIds;
}

/**
 * Reorganizes the requests from the SignData to an array of
 * (appId, [Request]) tuples.
 * @param {SignData} signData Input signature data
 * @return {Array.<[string, Array.<Request>]>} array of
 *     (appId, [Request]) tuples.
 */
function requestsByAppId(signData) {
  var requests = {};
  var appIdOrder = {};
  var orderToAppId = {};
  var lastOrder = 0;
  for (var i = 0, request; request = signData[i]; i++) {
    var appId = request['appId'];
    if (appId) {
      if (!appIdOrder.hasOwnProperty(appId)) {
        appIdOrder[appId] = lastOrder;
        orderToAppId[lastOrder] = appId;
        lastOrder++;
      }
      if (requests[appId]) {
        requests[appId].push(request);
      } else {
        requests[appId] = [request];
      }
    }
  }
  var orderedRequests = [];
  for (var order = 0; order < lastOrder; order++) {
    appId = orderToAppId[order];
    orderedRequests.push([appId, requests[appId]]);
  }
  return orderedRequests;
}

/**
 * Fetches the allowed origins for an appId.
 * @param {string} appId Application id
 * @param {boolean} allowHttp Whether http is a valid scheme for an appId.
 *     (This should be false except on test domains.)
 * @param {function(number, !Array.<string>)} cb Called back with an HTTP
 *     response code and a list of allowed origins for appId.
 */
function fetchAllowedOriginsForAppId(appId, allowHttp, cb) {
  var allowedOrigins = [];
  if (!appId) {
    cb(200, allowedOrigins);
    return;
  }
  if (appId.indexOf('http://') == 0 && !allowHttp) {
    console.log(UTIL_fmt('http app ids disallowed, ' + appId + ' requested'));
    cb(200, allowedOrigins);
    return;
  }
  // TODO: hack for old enrolled gnubbies, don't treat
  // accounts.google.com/login.corp.google.com specially when cryptauth server
  // stops reporting them as appId.
  if (appId == 'https://accounts.google.com') {
    allowedOrigins = ['https://login.corp.google.com'];
    cb(200, allowedOrigins);
    return;
  }
  if (appId == 'https://login.corp.google.com') {
    allowedOrigins = ['https://accounts.google.com'];
    cb(200, allowedOrigins);
    return;
  }
  // Termination of this function relies in fetchAppId completing.
  // (Not completing would be a bug in XMLHttpRequest.)
  // TODO: provide a termination guarantee, e.g. with a timer?
  fetchAppId(appId, function(rc, fetchedAppId, origins) {
    if (rc != 200) {
      console.log(UTIL_fmt('fetching ' + fetchedAppId + ' failed: ' + rc));
      allowedOrigins = [];
    } else {
      allowedOrigins = origins;
    }
    cb(rc, allowedOrigins);
  });
}

/**
 * Checks whether an appId is valid for a given origin.
 * @param {!string} appId Application id
 * @param {!string} origin Origin
 * @param {!Array.<string>} allowedOrigins the list of allowed origins for each
 *    appId.
 * @return {boolean} whether the appId is allowed for the origin.
 */
function isValidAppIdForOrigin(appId, origin, allowedOrigins) {
  if (!appId)
    return false;
  if (appId == origin) {
    // trivially allowed
    return true;
  }
  if (!allowedOrigins)
    return false;
  return allowedOrigins.indexOf(origin) >= 0;
}

/**
 * Returns whether the signData object appears to be valid.
 * @param {Array.<Object>} signData the signData object.
 * @return {boolean} whether the object appears valid.
 */
function isValidSignData(signData) {
  for (var i = 0; i < signData.length; i++) {
    var incomingChallenge = signData[i];
    if (!incomingChallenge.hasOwnProperty('challenge'))
      return false;
    if (!incomingChallenge.hasOwnProperty('appId')) {
      return false;
    }
    if (!incomingChallenge.hasOwnProperty('keyHandle'))
      return false;
    if (incomingChallenge['version']) {
      if (incomingChallenge['version'] != 'U2F_V1' &&
          incomingChallenge['version'] != 'U2F_V2') {
        return false;
      }
    }
  }
  return true;
}

/** Posts the log message to the log url.
 * @param {string} logMsg the log message to post.
 * @param {string=} opt_logMsgUrl the url to post log messages to.
 */
function logMessage(logMsg, opt_logMsgUrl) {
  console.log(UTIL_fmt('logMessage("' + logMsg + '")'));

  if (!opt_logMsgUrl) {
    return;
  }
  // Image fetching is not allowed per packaged app CSP.
  // But video and audio is.
  var audio = new Audio();
  audio.src = opt_logMsgUrl + logMsg;
}

/**
 * Logs the result of fetching an appId.
 * @param {!string} appId Application Id
 * @param {number} millis elapsed time while fetching the appId.
 * @param {Array.<string>} allowedOrigins the allowed origins retrieved.
 * @param {string=} opt_logMsgUrl the url to post log messages to.
 */
function logFetchAppIdResult(appId, millis, allowedOrigins, opt_logMsgUrl) {
  var logMsg = 'log=fetchappid&appid=' + appId + '&millis=' + millis +
      '&numorigins=' + allowedOrigins.length;
  logMessage(logMsg, opt_logMsgUrl);
}

/**
 * Logs a mismatch between an origin and an appId.
 * @param {string} origin Origin
 * @param {!string} appId Application id
 * @param {string=} opt_logMsgUrl the url to post log messages to
 */
function logInvalidOriginForAppId(origin, appId, opt_logMsgUrl) {
  var logMsg = 'log=originrejected&origin=' + origin + '&appid=' + appId;
  logMessage(logMsg, opt_logMsgUrl);
}

/**
 * Formats response parameters as an object.
 * @param {string} type type of the post message.
 * @param {number} code status code of the operation.
 * @param {Object=} responseData the response data of the operation.
 * @return {Object} formatted response.
 */
function formatWebPageResponse(type, code, responseData) {
  var responseJsonObject = {};
  responseJsonObject['type'] = type;
  responseJsonObject['code'] = code;
  if (responseData)
    responseJsonObject['responseData'] = responseData;
  return responseJsonObject;
}

/**
 * @param {!string} string Input string
 * @return {Array.<number>} SHA256 hash value of string.
 */
function sha256HashOfString(string) {
  var s = new SHA256();
  s.update(UTIL_StringToBytes(string));
  return s.digest();
}

/**
 * Normalizes the TLS channel ID value:
 * 1. Converts semantically empty values (undefined, null, 0) to the empty
 *     string.
 * 2. Converts valid JSON strings to a JS object.
 * 3. Otherwise, returns the input value unmodified.
 * @param {Object|string|undefined} opt_tlsChannelId TLS Channel id
 * @return {Object|string} The normalized TLS channel ID value.
 */
function tlsChannelIdValue(opt_tlsChannelId) {
  if (!opt_tlsChannelId) {
    // Case 1: Always set some value for  TLS channel ID, even if it's the empty
    // string: this browser definitely supports them.
    return '';
  }
  if (typeof opt_tlsChannelId === 'string') {
    try {
      var obj = JSON.parse(opt_tlsChannelId);
      if (!obj) {
        // Case 1: The string value 'null' parses as the Javascript object null,
        // so return an empty string: the browser definitely supports TLS
        // channel id.
        return '';
      }
      // Case 2: return the value as a JS object.
      return /** @type {Object} */ (obj);
    } catch (e) {
      console.warn('Unparseable TLS channel ID value ' + opt_tlsChannelId);
      // Case 3: return the value unmodified.
    }
  }
  return opt_tlsChannelId;
}

/**
 * Creates a browser data object with the given values.
 * @param {!string} type A string representing the "type" of this browser data
 *     object.
 * @param {!string} serverChallenge The server's challenge, as a base64-
 *     encoded string.
 * @param {!string} origin The server's origin, as seen by the browser.
 * @param {Object|string|undefined} opt_tlsChannelId TLS Channel Id
 * @return {string} A string representation of the browser data object.
 */
function makeBrowserData(type, serverChallenge, origin, opt_tlsChannelId) {
  var browserData = {
    'typ' : type,
    'challenge' : serverChallenge,
    'origin' : origin
  };
  browserData['cid_pubkey'] = tlsChannelIdValue(opt_tlsChannelId);
  return JSON.stringify(browserData);
}

/**
 * Creates a browser data object for an enroll request with the given values.
 * @param {!string} serverChallenge The server's challenge, as a base64-
 *     encoded string.
 * @param {!string} origin The server's origin, as seen by the browser.
 * @param {Object|string|undefined} opt_tlsChannelId TLS Channel Id
 * @return {string} A string representation of the browser data object.
 */
function makeEnrollBrowserData(serverChallenge, origin, opt_tlsChannelId) {
  return makeBrowserData(
      'navigator.id.finishEnrollment', serverChallenge, origin,
      opt_tlsChannelId);
}

/**
 * Creates a browser data object for a sign request with the given values.
 * @param {!string} serverChallenge The server's challenge, as a base64-
 *     encoded string.
 * @param {!string} origin The server's origin, as seen by the browser.
 * @param {Object|string|undefined} opt_tlsChannelId TLS Channel Id
 * @return {string} A string representation of the browser data object.
 */
function makeSignBrowserData(serverChallenge, origin, opt_tlsChannelId) {
  return makeBrowserData(
      'navigator.id.getAssertion', serverChallenge, origin, opt_tlsChannelId);
}

/**
 * @param {string} browserData Browser data as JSON
 * @param {string} appId Application Id
 * @param {string} encodedKeyHandle B64 encoded key handle
 * @param {string=} version Protocol version
 * @return {SignHelperChallenge} Challenge object
 */
function makeChallenge(browserData, appId, encodedKeyHandle, version) {
  var appIdHash = B64_encode(sha256HashOfString(appId));
  var browserDataHash = B64_encode(sha256HashOfString(browserData));
  var keyHandle = encodedKeyHandle;

  var challenge = {
    'challengeHash': browserDataHash,
    'appIdHash': appIdHash,
    'keyHandle': keyHandle
  };
  // Version is implicitly U2F_V1 if not specified.
  challenge['version'] = (version || 'U2F_V1');
  return challenge;
}
