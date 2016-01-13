// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles web page requests for gnubby enrollment.
 */

'use strict';

/**
 * Handles an enroll request.
 * @param {!EnrollHelperFactory} factory Factory to create an enroll helper.
 * @param {MessageSender} sender The sender of the message.
 * @param {Object} request The web page's enroll request.
 * @param {Function} sendResponse Called back with the result of the enroll.
 * @param {boolean} toleratesMultipleResponses Whether the sendResponse
 *     callback can be called more than once, e.g. for progress updates.
 * @return {Closeable} A handler object to be closed when the browser channel
 *     closes.
 */
function handleEnrollRequest(factory, sender, request, sendResponse,
    toleratesMultipleResponses) {
  var sentResponse = false;
  function sendResponseOnce(r) {
    if (enroller) {
      enroller.close();
      enroller = null;
    }
    if (!sentResponse) {
      sentResponse = true;
      try {
        // If the page has gone away or the connection has otherwise gone,
        // sendResponse fails.
        sendResponse(r);
      } catch (exception) {
        console.warn('sendResponse failed: ' + exception);
      }
    } else {
      console.warn(UTIL_fmt('Tried to reply more than once! Juan, FIX ME'));
    }
  }

  function sendErrorResponse(code) {
    console.log(UTIL_fmt('code=' + code));
    var response = formatWebPageResponse(GnubbyMsgTypes.ENROLL_WEB_REPLY, code);
    if (request['requestId']) {
      response['requestId'] = request['requestId'];
    }
    sendResponseOnce(response);
  }

  var origin = getOriginFromUrl(/** @type {string} */ (sender.url));
  if (!origin) {
    sendErrorResponse(GnubbyCodeTypes.BAD_REQUEST);
    return null;
  }

  if (!isValidEnrollRequest(request)) {
    sendErrorResponse(GnubbyCodeTypes.BAD_REQUEST);
    return null;
  }

  var signData = request['signData'];
  var enrollChallenges = request['enrollChallenges'];
  var logMsgUrl = request['logMsgUrl'];
  var timeoutMillis = Enroller.DEFAULT_TIMEOUT_MILLIS;
  if (request['timeout']) {
    // Request timeout is in seconds.
    timeoutMillis = request['timeout'] * 1000;
  }

  function findChallengeOfVersion(enrollChallenges, version) {
    for (var i = 0; i < enrollChallenges.length; i++) {
      if (enrollChallenges[i]['version'] == version) {
        return enrollChallenges[i];
      }
    }
    return null;
  }

  function sendSuccessResponse(u2fVersion, info, browserData) {
    var enrollChallenge = findChallengeOfVersion(enrollChallenges, u2fVersion);
    if (!enrollChallenge) {
      sendErrorResponse(GnubbyCodeTypes.UNKNOWN_ERROR);
      return;
    }
    var enrollUpdateData = {};
    enrollUpdateData['enrollData'] = info;
    // Echo the used challenge back in the reply.
    for (var k in enrollChallenge) {
      enrollUpdateData[k] = enrollChallenge[k];
    }
    if (u2fVersion == 'U2F_V2') {
      // For U2F_V2, the challenge sent to the gnubby is modified to be the
      // hash of the browser data. Include the browser data.
      enrollUpdateData['browserData'] = browserData;
    }
    var response = formatWebPageResponse(
        GnubbyMsgTypes.ENROLL_WEB_REPLY, GnubbyCodeTypes.OK, enrollUpdateData);
    sendResponseOnce(response);
  }

  function sendNotification(code) {
    console.log(UTIL_fmt('notification, code=' + code));
    // Can the callback handle progress updates? If so, send one.
    if (toleratesMultipleResponses) {
      var response = formatWebPageResponse(
          GnubbyMsgTypes.ENROLL_WEB_NOTIFICATION, code);
      if (request['requestId']) {
        response['requestId'] = request['requestId'];
      }
      sendResponse(response);
    }
  }

  var timer = new CountdownTimer(timeoutMillis);
  var enroller = new Enroller(factory, timer, origin, sendErrorResponse,
      sendSuccessResponse, sendNotification, sender.tlsChannelId, logMsgUrl);
  enroller.doEnroll(enrollChallenges, signData);
  return /** @type {Closeable} */ (enroller);
}

/**
 * Returns whether the request appears to be a valid enroll request.
 * @param {Object} request the request.
 * @return {boolean} whether the request appears valid.
 */
function isValidEnrollRequest(request) {
  if (!request.hasOwnProperty('enrollChallenges'))
    return false;
  var enrollChallenges = request['enrollChallenges'];
  if (!enrollChallenges.length)
    return false;
  var seenVersions = {};
  for (var i = 0; i < enrollChallenges.length; i++) {
    var enrollChallenge = enrollChallenges[i];
    var version = enrollChallenge['version'];
    if (!version) {
      // Version is implicitly V1 if not specified.
      version = 'U2F_V1';
    }
    if (version != 'U2F_V1' && version != 'U2F_V2') {
      return false;
    }
    if (seenVersions[version]) {
      // Each version can appear at most once.
      return false;
    }
    seenVersions[version] = version;
    if (!enrollChallenge['appId']) {
      return false;
    }
    if (!enrollChallenge['challenge']) {
      // The challenge is required.
      return false;
    }
  }
  var signData = request['signData'];
  // An empty signData is ok, in the case the user is not already enrolled.
  if (signData && !isValidSignData(signData))
    return false;
  return true;
}

/**
 * Creates a new object to track enrolling with a gnubby.
 * @param {!EnrollHelperFactory} helperFactory factory to create an enroll
 *     helper.
 * @param {!Countdown} timer Timer for enroll request.
 * @param {string} origin The origin making the request.
 * @param {function(number)} errorCb Called upon enroll failure with an error
 *     code.
 * @param {function(string, string, (string|undefined))} successCb Called upon
 *     enroll success with the version of the succeeding gnubby, the enroll
 *     data, and optionally the browser data associated with the enrollment.
 * @param {(function(number)|undefined)} opt_progressCb Called with progress
 *     updates to the enroll request.
 * @param {string=} opt_tlsChannelId the TLS channel ID, if any, of the origin
 *     making the request.
 * @param {string=} opt_logMsgUrl The url to post log messages to.
 * @constructor
 */
function Enroller(helperFactory, timer, origin, errorCb, successCb,
    opt_progressCb, opt_tlsChannelId, opt_logMsgUrl) {
  /** @private {Countdown} */
  this.timer_ = timer;
  /** @private {string} */
  this.origin_ = origin;
  /** @private {function(number)} */
  this.errorCb_ = errorCb;
  /** @private {function(string, string, (string|undefined))} */
  this.successCb_ = successCb;
  /** @private {(function(number)|undefined)} */
  this.progressCb_ = opt_progressCb;
  /** @private {string|undefined} */
  this.tlsChannelId_ = opt_tlsChannelId;
  /** @private {string|undefined} */
  this.logMsgUrl_ = opt_logMsgUrl;

  /** @private {boolean} */
  this.done_ = false;
  /** @private {number|undefined} */
  this.lastProgressUpdate_ = undefined;

  /** @private {Object.<string, string>} */
  this.browserData_ = {};
  /** @private {Array.<EnrollHelperChallenge>} */
  this.encodedEnrollChallenges_ = [];
  /** @private {Array.<SignHelperChallenge>} */
  this.encodedSignChallenges_ = [];
  // Allow http appIds for http origins. (Broken, but the caller deserves
  // what they get.)
  /** @private {boolean} */
  this.allowHttp_ = this.origin_ ? this.origin_.indexOf('http://') == 0 : false;

  /** @private {EnrollHelper} */
  this.helper_ = helperFactory.createHelper(timer,
      this.helperError_.bind(this), this.helperSuccess_.bind(this),
      this.helperProgress_.bind(this));
}

/**
 * Default timeout value in case the caller never provides a valid timeout.
 */
Enroller.DEFAULT_TIMEOUT_MILLIS = 30 * 1000;

/**
 * Performs an enroll request with the given enroll and sign challenges.
 * @param {Array.<Object>} enrollChallenges A set of enroll challenges
 * @param {Array.<Object>} signChallenges A set of sign challenges for existing
 *     enrollments for this user and appId
 */
Enroller.prototype.doEnroll = function(enrollChallenges, signChallenges) {
  this.setEnrollChallenges_(enrollChallenges);
  this.setSignChallenges_(signChallenges);

  // Begin fetching/checking the app ids.
  var enrollAppIds = [];
  for (var i = 0; i < enrollChallenges.length; i++) {
    enrollAppIds.push(enrollChallenges[i]['appId']);
  }
  var self = this;
  this.checkAppIds_(enrollAppIds, signChallenges, function(result) {
    if (result) {
      self.helper_.doEnroll(self.encodedEnrollChallenges_,
          self.encodedSignChallenges_);
    } else {
      self.notifyError_(GnubbyCodeTypes.BAD_APP_ID);
    }
  });
};

/**
 * Encodes the enroll challenges for use by an enroll helper.
 * @param {Array.<Object>} enrollChallenges A set of enroll challenges
 * @return {Array.<EnrollHelperChallenge>} the encoded challenges.
 * @private
 */
Enroller.encodeEnrollChallenges_ = function(enrollChallenges) {
  var encodedChallenges = [];
  for (var i = 0; i < enrollChallenges.length; i++) {
    var enrollChallenge = enrollChallenges[i];
    var encodedChallenge = {};
    var version;
    if (enrollChallenge['version']) {
      version = enrollChallenge['version'];
    } else {
      // Version is implicitly V1 if not specified.
      version = 'U2F_V1';
    }
    encodedChallenge['version'] = version;
    encodedChallenge['challenge'] = enrollChallenge['challenge'];
    encodedChallenge['appIdHash'] =
        B64_encode(sha256HashOfString(enrollChallenge['appId']));
    encodedChallenges.push(encodedChallenge);
  }
  return encodedChallenges;
};

/**
 * Sets this enroller's enroll challenges.
 * @param {Array.<Object>} enrollChallenges The enroll challenges.
 * @private
 */
Enroller.prototype.setEnrollChallenges_ = function(enrollChallenges) {
  var challenges = [];
  for (var i = 0; i < enrollChallenges.length; i++) {
    var enrollChallenge = enrollChallenges[i];
    var version = enrollChallenge.version;
    if (!version) {
      // Version is implicitly V1 if not specified.
      version = 'U2F_V1';
    }

    if (version == 'U2F_V2') {
      var modifiedChallenge = {};
      for (var k in enrollChallenge) {
        modifiedChallenge[k] = enrollChallenge[k];
      }
      // V2 enroll responses contain signatures over a browser data object,
      // which we're constructing here. The browser data object contains, among
      // other things, the server challenge.
      var serverChallenge = enrollChallenge['challenge'];
      var browserData = makeEnrollBrowserData(
          serverChallenge, this.origin_, this.tlsChannelId_);
      // Replace the challenge with the hash of the browser data.
      modifiedChallenge['challenge'] =
          B64_encode(sha256HashOfString(browserData));
      this.browserData_[version] =
          B64_encode(UTIL_StringToBytes(browserData));
      challenges.push(modifiedChallenge);
    } else {
      challenges.push(enrollChallenge);
    }
  }
  // Store the encoded challenges for use by the enroll helper.
  this.encodedEnrollChallenges_ =
      Enroller.encodeEnrollChallenges_(challenges);
};

/**
 * Sets this enroller's sign data.
 * @param {Array=} signData the sign challenges to add.
 * @private
 */
Enroller.prototype.setSignChallenges_ = function(signData) {
  this.encodedSignChallenges_ = [];
  if (signData) {
    for (var i = 0; i < signData.length; i++) {
      var incomingChallenge = signData[i];
      var serverChallenge = incomingChallenge['challenge'];
      var appId = incomingChallenge['appId'];
      var encodedKeyHandle = incomingChallenge['keyHandle'];

      var challenge = makeChallenge(serverChallenge, appId, encodedKeyHandle,
          incomingChallenge['version']);

      this.encodedSignChallenges_.push(challenge);
    }
  }
};

/**
 * Checks the app ids associated with this enroll request, and calls a callback
 * with the result of the check.
 * @param {!Array.<string>} enrollAppIds The app ids in the enroll challenge
 *     portion of the enroll request.
 * @param {SignData} signData The sign data associated with the request.
 * @param {function(boolean)} cb Called with the result of the check.
 * @private
 */
Enroller.prototype.checkAppIds_ = function(enrollAppIds, signData, cb) {
  if (!enrollAppIds || !enrollAppIds.length) {
    // Defensive programming check: the enroll request is required to contain
    // its own app ids, so if there aren't any, reject the request.
    cb(false);
    return;
  }

  /** @private {Array.<string>} */
  this.distinctAppIds_ =
      UTIL_unionArrays(enrollAppIds, getDistinctAppIds(signData));
  /** @private {boolean} */
  this.anyInvalidAppIds_ = false;
  /** @private {boolean} */
  this.appIdFailureReported_ = false;
  /** @private {number} */
  this.fetchedAppIds_ = 0;

  for (var i = 0; i < this.distinctAppIds_.length; i++) {
    var appId = this.distinctAppIds_[i];
    if (appId == this.origin_) {
      // Trivially allowed.
      this.fetchedAppIds_++;
      if (this.fetchedAppIds_ == this.distinctAppIds_.length &&
          !this.anyInvalidAppIds_) {
        // Last app id was fetched, and they were all valid: we're done.
        // (Note that the case when anyInvalidAppIds_ is true doesn't need to
        // be handled here: the callback was already called with false at that
        // point, see fetchedAllowedOriginsForAppId_.)
        cb(true);
      }
    } else {
      var start = new Date();
      fetchAllowedOriginsForAppId(appId, this.allowHttp_,
          this.fetchedAllowedOriginsForAppId_.bind(this, appId, start, cb));
    }
  }
};

/**
 * Called with the result of an app id fetch.
 * @param {string} appId the app id that was fetched.
 * @param {Date} start the time the fetch request started.
 * @param {function(boolean)} cb Called with the result of the app id check.
 * @param {number} rc The HTTP response code for the app id fetch.
 * @param {!Array.<string>} allowedOrigins The origins allowed for this app id.
 * @private
 */
Enroller.prototype.fetchedAllowedOriginsForAppId_ =
    function(appId, start, cb, rc, allowedOrigins) {
  var end = new Date();
  this.fetchedAppIds_++;
  logFetchAppIdResult(appId, end - start, allowedOrigins, this.logMsgUrl_);
  if (rc != 200 && !(rc >= 400 && rc < 500)) {
    if (this.timer_.expired()) {
      // Act as though the helper timed out.
      this.helperError_(DeviceStatusCodes.TIMEOUT_STATUS, false);
    } else {
      start = new Date();
      fetchAllowedOriginsForAppId(appId, this.allowHttp_,
          this.fetchedAllowedOriginsForAppId_.bind(this, appId, start, cb));
    }
    return;
  }
  if (!isValidAppIdForOrigin(appId, this.origin_, allowedOrigins)) {
    logInvalidOriginForAppId(this.origin_, appId, this.logMsgUrl_);
    this.anyInvalidAppIds_ = true;
    if (!this.appIdFailureReported_) {
      // Only the failure case can happen more than once, so only report
      // it the first time.
      this.appIdFailureReported_ = true;
      cb(false);
    }
  }
  if (this.fetchedAppIds_ == this.distinctAppIds_.length &&
      !this.anyInvalidAppIds_) {
    // Last app id was fetched, and they were all valid: we're done.
    cb(true);
  }
};

/** Closes this enroller. */
Enroller.prototype.close = function() {
  if (this.helper_) this.helper_.close();
};

/**
 * Notifies the caller with the error code.
 * @param {number} code Error code
 * @private
 */
Enroller.prototype.notifyError_ = function(code) {
  if (this.done_)
    return;
  this.close();
  this.done_ = true;
  this.errorCb_(code);
};

/**
 * Notifies the caller of success with the provided response data.
 * @param {string} u2fVersion Protocol version
 * @param {string} info Response data
 * @param {string|undefined} opt_browserData Browser data used
 * @private
 */
Enroller.prototype.notifySuccess_ =
    function(u2fVersion, info, opt_browserData) {
  if (this.done_)
    return;
  this.close();
  this.done_ = true;
  this.successCb_(u2fVersion, info, opt_browserData);
};

/**
 * Notifies the caller of progress with the error code.
 * @param {number} code Status code
 * @private
 */
Enroller.prototype.notifyProgress_ = function(code) {
  if (this.done_)
    return;
  if (code != this.lastProgressUpdate_) {
    this.lastProgressUpdate_ = code;
    // If there is no progress callback, treat it like an error and clean up.
    if (this.progressCb_) {
      this.progressCb_(code);
    } else {
      this.notifyError_(code);
    }
  }
};

/**
 * Maps an enroll helper's error code namespace to the page's error code
 * namespace.
 * @param {number} code Error code from DeviceStatusCodes namespace.
 * @param {boolean} anyGnubbies Whether any gnubbies were found.
 * @return {number} A GnubbyCodeTypes error code.
 * @private
 */
Enroller.mapError_ = function(code, anyGnubbies) {
  var reportedError = GnubbyCodeTypes.UNKNOWN_ERROR;
  switch (code) {
    case DeviceStatusCodes.WRONG_DATA_STATUS:
      reportedError = anyGnubbies ? GnubbyCodeTypes.ALREADY_ENROLLED :
          GnubbyCodeTypes.NO_GNUBBIES;
      break;

    case DeviceStatusCodes.WAIT_TOUCH_STATUS:
      reportedError = GnubbyCodeTypes.WAIT_TOUCH;
      break;

    case DeviceStatusCodes.BUSY_STATUS:
      reportedError = GnubbyCodeTypes.BUSY;
      break;
  }
  return reportedError;
};

/**
 * Called by the helper upon error.
 * @param {number} code Error code
 * @param {boolean} anyGnubbies If any gnubbies were found
 * @private
 */
Enroller.prototype.helperError_ = function(code, anyGnubbies) {
  var reportedError = Enroller.mapError_(code, anyGnubbies);
  console.log(UTIL_fmt('helper reported ' + code.toString(16) +
      ', returning ' + reportedError));
  this.notifyError_(reportedError);
};

/**
 * Called by helper upon success.
 * @param {string} u2fVersion gnubby version.
 * @param {string} info enroll data.
 * @private
 */
Enroller.prototype.helperSuccess_ = function(u2fVersion, info) {
  console.log(UTIL_fmt('Gnubby enrollment succeeded!!!!!'));

  var browserData;
  if (u2fVersion == 'U2F_V2') {
    // For U2F_V2, the challenge sent to the gnubby is modified to be the hash
    // of the browser data. Include the browser data.
    browserData = this.browserData_[u2fVersion];
  }

  this.notifySuccess_(u2fVersion, info, browserData);
};

/**
 * Called by helper to notify progress.
 * @param {number} code Status code
 * @param {boolean} anyGnubbies If any gnubbies were found
 * @private
 */
Enroller.prototype.helperProgress_ = function(code, anyGnubbies) {
  var reportedError = Enroller.mapError_(code, anyGnubbies);
  console.log(UTIL_fmt('helper notified ' + code.toString(16) +
      ', returning ' + reportedError));
  this.notifyProgress_(reportedError);
};
