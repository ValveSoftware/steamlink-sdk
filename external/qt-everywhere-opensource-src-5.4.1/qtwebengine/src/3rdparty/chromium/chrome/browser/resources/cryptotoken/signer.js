// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles web page requests for gnubby sign requests.
 */

'use strict';

var signRequestQueue = new OriginKeyedRequestQueue();

/**
 * Handles a sign request.
 * @param {!SignHelperFactory} factory Factory to create a sign helper.
 * @param {MessageSender} sender The sender of the message.
 * @param {Object} request The web page's sign request.
 * @param {Function} sendResponse Called back with the result of the sign.
 * @param {boolean} toleratesMultipleResponses Whether the sendResponse
 *     callback can be called more than once, e.g. for progress updates.
 * @return {Closeable} Request handler that should be closed when the browser
 *     message channel is closed.
 */
function handleSignRequest(factory, sender, request, sendResponse,
    toleratesMultipleResponses) {
  var sentResponse = false;
  function sendResponseOnce(r) {
    if (queuedSignRequest) {
      queuedSignRequest.close();
      queuedSignRequest = null;
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
    var response = formatWebPageResponse(GnubbyMsgTypes.SIGN_WEB_REPLY, code);
    sendResponseOnce(response);
  }

  function sendSuccessResponse(challenge, info, browserData) {
    var responseData = {};
    for (var k in challenge) {
      responseData[k] = challenge[k];
    }
    responseData['browserData'] = B64_encode(UTIL_StringToBytes(browserData));
    responseData['signatureData'] = info;
    var response = formatWebPageResponse(GnubbyMsgTypes.SIGN_WEB_REPLY,
        GnubbyCodeTypes.OK, responseData);
    sendResponseOnce(response);
  }

  var origin = getOriginFromUrl(/** @type {string} */ (sender.url));
  if (!origin) {
    sendErrorResponse(GnubbyCodeTypes.BAD_REQUEST);
    return null;
  }
  // More closure type inference fail.
  var nonNullOrigin = /** @type {string} */ (origin);

  if (!isValidSignRequest(request)) {
    sendErrorResponse(GnubbyCodeTypes.BAD_REQUEST);
    return null;
  }

  var signData = request['signData'];
  // A valid sign data has at least one challenge, so get the first appId from
  // the first challenge.
  var firstAppId = signData[0]['appId'];
  var timeoutMillis = Signer.DEFAULT_TIMEOUT_MILLIS;
  if (request['timeout']) {
    // Request timeout is in seconds.
    timeoutMillis = request['timeout'] * 1000;
  }
  var timer = new CountdownTimer(timeoutMillis);
  var logMsgUrl = request['logMsgUrl'];

  // Queue sign requests from the same origin, to protect against simultaneous
  // sign-out on many tabs resulting in repeated sign-in requests.
  var queuedSignRequest = new QueuedSignRequest(signData, factory, timer,
      nonNullOrigin, sendErrorResponse, sendSuccessResponse,
      sender.tlsChannelId, logMsgUrl);
  var requestToken = signRequestQueue.queueRequest(firstAppId, nonNullOrigin,
      queuedSignRequest.begin.bind(queuedSignRequest), timer);
  queuedSignRequest.setToken(requestToken);
  return queuedSignRequest;
}

/**
 * Returns whether the request appears to be a valid sign request.
 * @param {Object} request the request.
 * @return {boolean} whether the request appears valid.
 */
function isValidSignRequest(request) {
  if (!request.hasOwnProperty('signData'))
    return false;
  var signData = request['signData'];
  // If a sign request contains an empty array of challenges, it could never
  // be fulfilled. Fail.
  if (!signData.length)
    return false;
  return isValidSignData(signData);
}

/**
 * Adapter class representing a queued sign request.
 * @param {!SignData} signData Signature data
 * @param {!SignHelperFactory} factory Factory for SignHelper instances
 * @param {Countdown} timer Timeout timer
 * @param {string} origin Signature origin
 * @param {function(number)} errorCb Error callback
 * @param {function(SignChallenge, string, string)} successCb Success callback
 * @param {string|undefined} opt_tlsChannelId TLS Channel Id
 * @param {string|undefined} opt_logMsgUrl Url to post log messages to
 * @constructor
 * @implements {Closeable}
 */
function QueuedSignRequest(signData, factory, timer, origin, errorCb, successCb,
    opt_tlsChannelId, opt_logMsgUrl) {
  /** @private {!SignData} */
  this.signData_ = signData;
  /** @private {!SignHelperFactory} */
  this.factory_ = factory;
  /** @private {Countdown} */
  this.timer_ = timer;
  /** @private {string} */
  this.origin_ = origin;
  /** @private {function(number)} */
  this.errorCb_ = errorCb;
  /** @private {function(SignChallenge, string, string)} */
  this.successCb_ = successCb;
  /** @private {string|undefined} */
  this.tlsChannelId_ = opt_tlsChannelId;
  /** @private {string|undefined} */
  this.logMsgUrl_ = opt_logMsgUrl;
  /** @private {boolean} */
  this.begun_ = false;
  /** @private {boolean} */
  this.closed_ = false;
}

/** Closes this sign request. */
QueuedSignRequest.prototype.close = function() {
  if (this.closed_) return;
  if (this.begun_ && this.signer_) {
    this.signer_.close();
  }
  if (this.token_) {
    this.token_.complete();
  }
  this.closed_ = true;
};

/**
 * @param {QueuedRequestToken} token Token for this sign request.
 */
QueuedSignRequest.prototype.setToken = function(token) {
  /** @private {QueuedRequestToken} */
  this.token_ = token;
};

/**
 * Called when this sign request may begin work.
 * @param {QueuedRequestToken} token Token for this sign request.
 */
QueuedSignRequest.prototype.begin = function(token) {
  this.begun_ = true;
  this.setToken(token);
  this.signer_ = new Signer(this.factory_, this.timer_, this.origin_,
      this.signerFailed_.bind(this), this.signerSucceeded_.bind(this),
      this.tlsChannelId_, this.logMsgUrl_);
  if (!this.signer_.setChallenges(this.signData_)) {
    token.complete();
    this.errorCb_(GnubbyCodeTypes.BAD_REQUEST);
  }
};

/**
 * Called when this request's signer fails.
 * @param {number} code The failure code reported by the signer.
 * @private
 */
QueuedSignRequest.prototype.signerFailed_ = function(code) {
  this.token_.complete();
  this.errorCb_(code);
};

/**
 * Called when this request's signer succeeds.
 * @param {SignChallenge} challenge The challenge that was signed.
 * @param {string} info The sign result.
 * @param {string} browserData Browser data JSON
 * @private
 */
QueuedSignRequest.prototype.signerSucceeded_ =
    function(challenge, info, browserData) {
  this.token_.complete();
  this.successCb_(challenge, info, browserData);
};

/**
 * Creates an object to track signing with a gnubby.
 * @param {!SignHelperFactory} helperFactory Factory to create a sign helper.
 * @param {Countdown} timer Timer for sign request.
 * @param {string} origin The origin making the request.
 * @param {function(number)} errorCb Called when the sign operation fails.
 * @param {function(SignChallenge, string, string)} successCb Called when the
 *     sign operation succeeds.
 * @param {string=} opt_tlsChannelId the TLS channel ID, if any, of the origin
 *     making the request.
 * @param {string=} opt_logMsgUrl The url to post log messages to.
 * @constructor
 */
function Signer(helperFactory, timer, origin, errorCb, successCb,
    opt_tlsChannelId, opt_logMsgUrl) {
  /** @private {Countdown} */
  this.timer_ = timer;
  /** @private {string} */
  this.origin_ = origin;
  /** @private {function(number)} */
  this.errorCb_ = errorCb;
  /** @private {function(SignChallenge, string, string)} */
  this.successCb_ = successCb;
  /** @private {string|undefined} */
  this.tlsChannelId_ = opt_tlsChannelId;
  /** @private {string|undefined} */
  this.logMsgUrl_ = opt_logMsgUrl;

  /** @private {boolean} */
  this.challengesSet_ = false;
  /** @private {Array.<SignHelperChallenge>} */
  this.pendingChallenges_ = [];
  /** @private {boolean} */
  this.done_ = false;

  /** @private {Object.<string, string>} */
  this.browserData_ = {};
  /** @private {Object.<string, SignChallenge>} */
  this.serverChallenges_ = {};
  // Allow http appIds for http origins. (Broken, but the caller deserves
  // what they get.)
  /** @private {boolean} */
  this.allowHttp_ = this.origin_ ? this.origin_.indexOf('http://') == 0 : false;

  // Protect against helper failure with a watchdog.
  this.createWatchdog_(timer);
  /** @private {SignHelper} */
  this.helper_ = helperFactory.createHelper(
      timer, this.helperError_.bind(this), this.helperSuccess_.bind(this),
          this.logMsgUrl_);
}

/**
 * Creates a timer with an expiry greater than the expiration time of the given
 * timer.
 * @param {Countdown} timer Timeout timer
 * @private
 */
Signer.prototype.createWatchdog_ = function(timer) {
  var millis = timer.millisecondsUntilExpired();
  millis += CountdownTimer.TIMER_INTERVAL_MILLIS;
  /** @private {Countdown|undefined} */
  this.watchdogTimer_ = new CountdownTimer(millis, this.timeout_.bind(this));
};

/**
 * Default timeout value in case the caller never provides a valid timeout.
 */
Signer.DEFAULT_TIMEOUT_MILLIS = 30 * 1000;

/**
 * Sets the challenges to be signed.
 * @param {SignData} signData The challenges to set.
 * @return {boolean} Whether the challenges could be set.
 */
Signer.prototype.setChallenges = function(signData) {
  if (this.challengesSet_ || this.done_)
    return false;
  /** @private {SignData} */
  this.signData_ = signData;
  /** @private {boolean} */
  this.challengesSet_ = true;

  this.checkAppIds_();
  return true;
};

/**
 * Adds new challenges to the challenges being signed.
 * @param {SignData} signData Challenges to add.
 * @param {boolean} finalChallenges Whether these are the final challenges.
 * @return {boolean} Whether the challenge could be added.
 */
Signer.prototype.addChallenges = function(signData, finalChallenges) {
  var newChallenges = this.encodeSignChallenges_(signData);
  for (var i = 0; i < newChallenges.length; i++) {
    this.pendingChallenges_.push(newChallenges[i]);
  }
  if (!finalChallenges) {
    return true;
  }
  return this.helper_.doSign(this.pendingChallenges_);
};

/**
 * Creates challenges for helper from challenges.
 * @param {Array.<SignChallenge>} challenges Challenges to add.
 * @return {Array.<SignHelperChallenge>} Encoded challenges
 * @private
 */
Signer.prototype.encodeSignChallenges_ = function(challenges) {
  var newChallenges = [];
  for (var i = 0; i < challenges.length; i++) {
    var incomingChallenge = challenges[i];
    var serverChallenge = incomingChallenge['challenge'];
    var appId = incomingChallenge['appId'];
    var encodedKeyHandle = incomingChallenge['keyHandle'];
    var version = incomingChallenge['version'];

    var browserData =
        makeSignBrowserData(serverChallenge, this.origin_, this.tlsChannelId_);
    var encodedChallenge = makeChallenge(browserData, appId, encodedKeyHandle,
        version);

    var key = encodedKeyHandle + encodedChallenge['challengeHash'];
    this.browserData_[key] = browserData;
    this.serverChallenges_[key] = incomingChallenge;

    newChallenges.push(encodedChallenge);
  }
  return newChallenges;
};

/**
 * Checks the app ids of incoming requests, and, when this signer is enforcing
 * that app ids are valid, adds successful challenges to those being signed.
 * @private
 */
Signer.prototype.checkAppIds_ = function() {
  // Check the incoming challenges' app ids.
  /** @private {Array.<[string, Array.<Request>]>} */
  this.orderedRequests_ = requestsByAppId(this.signData_);
  if (!this.orderedRequests_.length) {
    // Safety check: if the challenges are somehow empty, the helper will never
    // be fed any data, so the request could never be satisfied. You lose.
    this.notifyError_(GnubbyCodeTypes.BAD_REQUEST);
    return;
  }
  /** @private {number} */
  this.fetchedAppIds_ = 0;
  /** @private {number} */
  this.validAppIds_ = 0;
  for (var i = 0, appIdRequestsPair; i < this.orderedRequests_.length; i++) {
    var appIdRequestsPair = this.orderedRequests_[i];
    var appId = appIdRequestsPair[0];
    var requests = appIdRequestsPair[1];
    if (appId == this.origin_) {
      // Trivially allowed.
      this.fetchedAppIds_++;
      this.validAppIds_++;
      this.addChallenges(requests,
          this.fetchedAppIds_ == this.orderedRequests_.length);
    } else {
      var start = new Date();
      fetchAllowedOriginsForAppId(appId, this.allowHttp_,
          this.fetchedAllowedOriginsForAppId_.bind(this, appId, start,
              requests));
    }
  }
};

/**
 * Called with the result of an app id fetch.
 * @param {string} appId the app id that was fetched.
 * @param {Date} start the time the fetch request started.
 * @param {Array.<SignChallenge>} challenges Challenges for this app id.
 * @param {number} rc The HTTP response code for the app id fetch.
 * @param {!Array.<string>} allowedOrigins The origins allowed for this app id.
 * @private
 */
Signer.prototype.fetchedAllowedOriginsForAppId_ = function(appId, start,
    challenges, rc, allowedOrigins) {
  var end = new Date();
  logFetchAppIdResult(appId, end - start, allowedOrigins, this.logMsgUrl_);
  if (rc != 200 && !(rc >= 400 && rc < 500)) {
    if (this.timer_.expired()) {
      // Act as though the helper timed out.
      this.helperError_(DeviceStatusCodes.TIMEOUT_STATUS, false);
    } else {
      start = new Date();
      fetchAllowedOriginsForAppId(appId, this.allowHttp_,
          this.fetchedAllowedOriginsForAppId_.bind(this, appId, start,
              challenges));
    }
    return;
  }
  this.fetchedAppIds_++;
  var finalChallenges = (this.fetchedAppIds_ == this.orderedRequests_.length);
  if (isValidAppIdForOrigin(appId, this.origin_, allowedOrigins)) {
    this.validAppIds_++;
    this.addChallenges(challenges, finalChallenges);
  } else {
    logInvalidOriginForAppId(this.origin_, appId, this.logMsgUrl_);
    // If this is the final request, sign the valid challenges.
    if (finalChallenges) {
      if (!this.helper_.doSign(this.pendingChallenges_)) {
        this.notifyError_(GnubbyCodeTypes.BAD_REQUEST);
        return;
      }
    }
  }
  if (finalChallenges && !this.validAppIds_) {
    // If all app ids are invalid, notify the caller, otherwise implicitly
    // allow the helper to report whether any of the valid challenges succeeded.
    this.notifyError_(GnubbyCodeTypes.BAD_APP_ID);
  }
};

/**
 * Called when the timeout expires on this signer.
 * @private
 */
Signer.prototype.timeout_ = function() {
  this.watchdogTimer_ = undefined;
  // The web page gets grumpy if it doesn't get WAIT_TOUCH within a reasonable
  // time.
  this.notifyError_(GnubbyCodeTypes.WAIT_TOUCH);
};

/** Closes this signer. */
Signer.prototype.close = function() {
  if (this.helper_) this.helper_.close();
};

/**
 * Notifies the caller of error with the given error code.
 * @param {number} code Error code
 * @private
 */
Signer.prototype.notifyError_ = function(code) {
  if (this.done_)
    return;
  this.close();
  this.done_ = true;
  this.errorCb_(code);
};

/**
 * Notifies the caller of success.
 * @param {SignChallenge} challenge The challenge that was signed.
 * @param {string} info The sign result.
 * @param {string} browserData Browser data JSON
 * @private
 */
Signer.prototype.notifySuccess_ = function(challenge, info, browserData) {
  if (this.done_)
    return;
  this.close();
  this.done_ = true;
  this.successCb_(challenge, info, browserData);
};

/**
 * Maps a sign helper's error code namespace to the page's error code namespace.
 * @param {number} code Error code from DeviceStatusCodes namespace.
 * @param {boolean} anyGnubbies Whether any gnubbies were found.
 * @return {number} A GnubbyCodeTypes error code.
 * @private
 */
Signer.mapError_ = function(code, anyGnubbies) {
  var reportedError;
  switch (code) {
    case DeviceStatusCodes.WRONG_DATA_STATUS:
      reportedError = anyGnubbies ? GnubbyCodeTypes.NONE_PLUGGED_ENROLLED :
          GnubbyCodeTypes.NO_GNUBBIES;
      break;

    case DeviceStatusCodes.OK_STATUS:
      // If the error callback is called with OK, it means the signature was
      // empty, which we treat the same as...
    case DeviceStatusCodes.WAIT_TOUCH_STATUS:
      reportedError = GnubbyCodeTypes.WAIT_TOUCH;
      break;

    case DeviceStatusCodes.BUSY_STATUS:
      reportedError = GnubbyCodeTypes.BUSY;
      break;

    default:
      reportedError = GnubbyCodeTypes.UNKNOWN_ERROR;
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
Signer.prototype.helperError_ = function(code, anyGnubbies) {
  this.clearTimeout_();
  var reportedError = Signer.mapError_(code, anyGnubbies);
  console.log(UTIL_fmt('helper reported ' + code.toString(16) +
      ', returning ' + reportedError));
  this.notifyError_(reportedError);
};

/**
 * Called by helper upon success.
 * @param {SignHelperChallenge} challenge The challenge that was signed.
 * @param {string} info The sign result.
 * @param {string=} opt_source The source, if any, if the signature.
 * @private
 */
Signer.prototype.helperSuccess_ = function(challenge, info, opt_source) {
  // Got a good reply, kill timer.
  this.clearTimeout_();

  if (this.logMsgUrl_ && opt_source) {
    var logMsg = 'signed&source=' + opt_source;
    logMessage(logMsg, this.logMsgUrl_);
  }

  var key = challenge['keyHandle'] + challenge['challengeHash'];
  var browserData = this.browserData_[key];
  // Notify with server-provided challenge, not the encoded one: the
  // server-provided challenge contains additional fields it relies on.
  var serverChallenge = this.serverChallenges_[key];
  this.notifySuccess_(serverChallenge, info, browserData);
};

/**
 * Clears the timeout for this signer.
 * @private
 */
Signer.prototype.clearTimeout_ = function() {
  if (this.watchdogTimer_) {
    this.watchdogTimer_.clearTimeout();
    this.watchdogTimer_ = undefined;
  }
};
