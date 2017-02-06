// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An UI component to host gaia auth extension in an iframe.
 * After the component binds with an iframe, call its {@code load} to start the
 * authentication flow. There are two events would be raised after this point:
 * a 'ready' event when the authentication UI is ready to use and a 'completed'
 * event when the authentication is completed successfully. If caller is
 * interested in the user credentials, he may supply a success callback with
 * {@code load} call. The callback will be invoked when the authentication is
 * completed successfully and with the available credential data.
 */

cr.define('cr.login', function() {
  'use strict';

  /**
   * Base URL of gaia auth extension.
   * @const
   */
  var AUTH_URL_BASE = 'chrome-extension://mfffpogegjflfpflabcdkioaeobkgjik';

  /**
   * Auth URL to use for online flow.
   * @const
   */
  var AUTH_URL = AUTH_URL_BASE + '/main.html';

  /**
   * Auth URL to use for offline flow.
   * @const
   */
  var OFFLINE_AUTH_URL = AUTH_URL_BASE + '/offline.html';

  /**
   * Origin of the gaia sign in page.
   * @const
   */
  var GAIA_ORIGIN = 'https://accounts.google.com';

  /**
   * Supported params of auth extension. For a complete list, check out the
   * auth extension's main.js.
   * @type {!Array<string>}
   * @const
   */
  var SUPPORTED_PARAMS = [
    'gaiaUrl',       // Gaia url to use;
    'gaiaPath',      // Gaia path to use without a leading slash;
    'hl',            // Language code for the user interface;
    'email',         // Pre-fill the email field in Gaia UI;
    'service',       // Name of Gaia service;
    'continueUrl',   // Continue url to use;
    'frameUrl',      // Initial frame URL to use. If empty defaults to gaiaUrl.
    'useEafe',       // Whether to use EAFE.
    'clientId',      // Chrome's client id.
    'constrained'    // Whether the extension is loaded in a constrained window;
  ];

  /**
   * Supported localized strings. For a complete list, check out the auth
   * extension's offline.js
   * @type {!Array<string>}
   * @const
   */
  var LOCALIZED_STRING_PARAMS = [
      'stringSignIn',
      'stringEmail',
      'stringPassword',
      'stringEmptyEmail',
      'stringEmptyPassword',
      'stringError'
  ];

  /**
   * Enum for the authorization mode, must match AuthMode defined in
   * chrome/browser/ui/webui/inline_login_ui.cc.
   * @enum {number}
   */
  var AuthMode = {
    DEFAULT: 0,
    OFFLINE: 1,
    DESKTOP: 2
  };

  /**
   * Enum for the auth flow.
   * @enum {number}
   */
  var AuthFlow = {
    GAIA: 0,
    SAML: 1
  };

  /**
   * Creates a new gaia auth extension host.
   * @param {HTMLIFrameElement|string} container The iframe element or its id
   *     to host the auth extension.
   * @constructor
   * @extends {cr.EventTarget}
   */
  function GaiaAuthHost(container) {
    this.frame_ = typeof container == 'string' ? $(container) : container;
    assert(this.frame_);
    window.addEventListener('message',
                            this.onMessage_.bind(this), false);
  }

  GaiaAuthHost.prototype = {
    __proto__: cr.EventTarget.prototype,

    /**
     * Auth extension params
     * @type {Object}
     */
    authParams_: {},

    /**
     * An url to use with {@code reload}.
     * @type {?string}
     * @private
     */
    reloadUrl_: null,

    /**
     * Invoked when authentication is completed successfully with credential
     * data. A credential data object looks like this:
     * <pre>
     * {@code
     * {
     *   email: 'xx@gmail.com',
     *   password: 'xxxx',  // May not present
     *   authCode: 'x/xx',  // May not present
     *   authMode: 'x',     // Authorization mode, default/offline/desktop.
     * }
     * }
     * </pre>
     * @type {function(Object)}
     * @private
     */
    successCallback_: null,

    /**
     * Invoked when the auth flow needs a user to confirm his/her passwords.
     * This could happen when there are more than one passwords scraped during
     * SAML flow. The embedder of GaiaAuthHost should show an UI to collect a
     * password from user then call GaiaAuthHost.verifyConfirmedPassword to
     * verify. If the password is good, the auth flow continues with success
     * path. Otherwise, confirmPasswordCallback_ is invoked again.
     * @type {function()}
     */
    confirmPasswordCallback_: null,

    /**
     * Similar to confirmPasswordCallback_ but is used when there is no
     * password scraped after a success authentication. The authenticated user
     * account is passed to the callback. The embedder should take over the
     * flow and decide what to do next.
     * @type {function(string)}
     */
    noPasswordCallback_: null,

    /**
     * Invoked when the authentication flow had to be aborted because content
     * served over an unencrypted connection was detected.
     */
    insecureContentBlockedCallback_: null,

    /**
     * Invoked to display an error message to the user when a GAIA error occurs
     * during authentication.
     * @type {function()}
     */
    missingGaiaInfoCallback_: null,

    /**
     * Invoked to record that the credentials passing API was used.
     * @type {function()}
     */
    samlApiUsedCallback_: null,

    /**
     * The iframe container.
     * @type {HTMLIFrameElement}
     */
    get frame() {
      return this.frame_;
    },

    /**
     * Sets confirmPasswordCallback_.
     * @type {function()}
     */
    set confirmPasswordCallback(callback) {
      this.confirmPasswordCallback_ = callback;
    },

    /**
     * Sets noPasswordCallback_.
     * @type {function()}
     */
    set noPasswordCallback(callback) {
      this.noPasswordCallback_ = callback;
    },

    /**
     * Sets insecureContentBlockedCallback_.
     * @type {function(string)}
     */
    set insecureContentBlockedCallback(callback) {
      this.insecureContentBlockedCallback_ = callback;
    },

    /**
     * Sets missingGaiaInfoCallback_.
     * @type {function()}
     */
    set missingGaiaInfoCallback(callback) {
      this.missingGaiaInfoCallback_ = callback;
    },

    /**
     * Sets samlApiUsedCallback_.
     * @type {function()}
     */
    set samlApiUsedCallback(callback) {
      this.samlApiUsedCallback_ = callback;
    },

    /**
     * Loads the auth extension.
     * @param {AuthMode} authMode Authorization mode.
     * @param {Object} data Parameters for the auth extension. See the auth
     *     extension's main.js for all supported params and their defaults.
     * @param {function(Object)} successCallback A function to be called when
     *     the authentication is completed successfully. The callback is
     *     invoked with a credential object.
     */
    load: function(authMode, data, successCallback) {
      var params = {};

      var populateParams = function(nameList, values) {
        if (!values)
          return;

        for (var i in nameList) {
          var name = nameList[i];
          if (values[name])
            params[name] = values[name];
        }
      };

      populateParams(SUPPORTED_PARAMS, data);
      populateParams(LOCALIZED_STRING_PARAMS, data.localizedStrings);
      params['needPassword'] = true;

      var url;
      switch (authMode) {
        case AuthMode.OFFLINE:
          url = OFFLINE_AUTH_URL;
          break;
        case AuthMode.DESKTOP:
          url = AUTH_URL;
          params['desktopMode'] = true;
          break;
        default:
          url = AUTH_URL;
      }

      this.authParams_ = params;
      this.reloadUrl_ = url;
      this.successCallback_ = successCallback;

      this.reload();
    },

    /**
     * Reloads the auth extension.
     */
    reload: function() {
      this.frame_.src = this.reloadUrl_;
      this.authFlow = AuthFlow.GAIA;
    },

    /**
     * Verifies the supplied password by sending it to the auth extension,
     * which will then check if it matches the scraped passwords.
     * @param {string} password The confirmed password that needs verification.
     */
    verifyConfirmedPassword: function(password) {
      var msg = {
        method: 'verifyConfirmedPassword',
        password: password
      };
      this.frame_.contentWindow.postMessage(msg, AUTH_URL_BASE);
    },

    /**
     * Invoked to process authentication success.
     * @param {Object} credentials Credential object to pass to success
     *     callback.
     * @private
     */
    onAuthSuccess_: function(credentials) {
      if (this.successCallback_)
        this.successCallback_(credentials);
      cr.dispatchSimpleEvent(this, 'completed');
    },

    /**
     * Checks if message comes from the loaded authentication extension.
     * @param {Object} e Payload of the received HTML5 message.
     * @type {boolean}
     */
    isAuthExtMessage_: function(e) {
      return this.frame_.src &&
          this.frame_.src.indexOf(e.origin) == 0 &&
          e.source == this.frame_.contentWindow;
    },

    /**
     * Event handler that is invoked when HTML5 message is received.
     * @param {object} e Payload of the received HTML5 message.
     */
    onMessage_: function(e) {
      var msg = e.data;

      if (!this.isAuthExtMessage_(e))
        return;

      if (msg.method == 'loginUIDOMContentLoaded') {
        this.frame_.contentWindow.postMessage(this.authParams_, AUTH_URL_BASE);
        return;
      }

      if (msg.method == 'loginUILoaded') {
        cr.dispatchSimpleEvent(this, 'ready');
        return;
      }

      if (/^complete(Login|Authentication)$|^offlineLogin$/.test(msg.method)) {
        if (!msg.email && !this.email_ && !msg.skipForNow) {
          var msg = {method: 'redirectToSignin'};
          this.frame_.contentWindow.postMessage(msg, AUTH_URL_BASE);
          return;
        }
        this.onAuthSuccess_({email: msg.email,
                             password: msg.password,
                             gaiaId: msg.gaiaId,
                             useOffline: msg.method == 'offlineLogin',
                             usingSAML: msg.usingSAML || false,
                             chooseWhatToSync: msg.chooseWhatToSync,
                             skipForNow: msg.skipForNow || false,
                             sessionIndex: msg.sessionIndex || ''});
        return;
      }

      if (msg.method == 'completeAuthenticationAuthCodeOnly') {
        if (!msg.authCode) {
          console.error(
              'GaiaAuthHost: completeAuthentication without auth code.');
          var msg = {method: 'redirectToSignin'};
          this.frame_.contentWindow.postMessage(msg, AUTH_URL_BASE);
          return;
        }
        this.onAuthSuccess_({authCodeOnly: true, authCode: msg.authCode});
        return;
      }

      if (msg.method == 'confirmPassword') {
        if (this.confirmPasswordCallback_)
          this.confirmPasswordCallback_(msg.email, msg.passwordCount);
        else
          console.error('GaiaAuthHost: Invalid confirmPasswordCallback_.');
        return;
      }

      if (msg.method == 'noPassword') {
        if (this.noPasswordCallback_)
          this.noPasswordCallback_(msg.email);
        else
          console.error('GaiaAuthHost: Invalid noPasswordCallback_.');
        return;
      }

      if (msg.method == 'authPageLoaded') {
        this.authDomain = msg.domain;
        this.authFlow = msg.isSAML ? AuthFlow.SAML : AuthFlow.GAIA;
        return;
      }

      if (msg.method == 'resetAuthFlow') {
        this.authFlow = AuthFlow.GAIA;
        return;
      }

      if (msg.method == 'insecureContentBlocked') {
        if (this.insecureContentBlockedCallback_) {
          this.insecureContentBlockedCallback_(msg.url);
        } else {
          console.error(
              'GaiaAuthHost: Invalid insecureContentBlockedCallback_.');
        }
        return;
      }

      if (msg.method == 'switchToFullTab') {
        chrome.send('switchToFullTab', [msg.url]);
        return;
      }

      if (msg.method == 'missingGaiaInfo') {
        if (this.missingGaiaInfoCallback_) {
          this.missingGaiaInfoCallback_();
        } else {
          console.error('GaiaAuthHost: Invalid missingGaiaInfoCallback_.');
        }
        return;
      }

      if (msg.method == 'samlApiUsed') {
        if (this.samlApiUsedCallback_) {
          this.samlApiUsedCallback_();
        } else {
          console.error('GaiaAuthHost: Invalid samlApiUsedCallback_.');
        }
        return;
      }

      console.error('Unknown message method=' + msg.method);
    }
  };

  /**
   * The domain name of the current auth page.
   * @type {string}
   */
  cr.defineProperty(GaiaAuthHost, 'authDomain');

  /**
   * The current auth flow of the hosted gaia_auth extension.
   * @type {AuthFlow}
   */
  cr.defineProperty(GaiaAuthHost, 'authFlow');

  GaiaAuthHost.SUPPORTED_PARAMS = SUPPORTED_PARAMS;
  GaiaAuthHost.LOCALIZED_STRING_PARAMS = LOCALIZED_STRING_PARAMS;
  GaiaAuthHost.AuthMode = AuthMode;
  GaiaAuthHost.AuthFlow = AuthFlow;

  return {
    GaiaAuthHost: GaiaAuthHost
  };
});
