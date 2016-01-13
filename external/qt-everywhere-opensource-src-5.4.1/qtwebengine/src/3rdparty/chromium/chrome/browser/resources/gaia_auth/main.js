// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Authenticator class wraps the communications between Gaia and its host.
 */
function Authenticator() {
}

/**
 * Gaia auth extension url origin.
 * @type {string}
 */
Authenticator.THIS_EXTENSION_ORIGIN =
    'chrome-extension://mfffpogegjflfpflabcdkioaeobkgjik';

/**
 * The lowest version of the credentials passing API supported.
 * @type {number}
 */
Authenticator.MIN_API_VERSION_VERSION = 1;

/**
 * The highest version of the credentials passing API supported.
 * @type {number}
 */
Authenticator.MAX_API_VERSION_VERSION = 1;

/**
 * The key types supported by the credentials passing API.
 * @type {Array} Array of strings.
 */
Authenticator.API_KEY_TYPES = [
  'KEY_TYPE_PASSWORD_PLAIN',
];

/**
 * Singleton getter of Authenticator.
 * @return {Object} The singleton instance of Authenticator.
 */
Authenticator.getInstance = function() {
  if (!Authenticator.instance_) {
    Authenticator.instance_ = new Authenticator();
  }
  return Authenticator.instance_;
};

Authenticator.prototype = {
  email_: null,

  // Depending on the key type chosen, this will contain the plain text password
  // or a credential derived from it along with the information required to
  // repeat the derivation, such as a salt. The information will be encoded so
  // that it contains printable ASCII characters only. The exact encoding is TBD
  // when support for key types other than plain text password is added.
  passwordBytes_: null,

  attemptToken_: null,

  // Input params from extension initialization URL.
  inputLang_: undefined,
  intputEmail_: undefined,

  isSAMLFlow_: false,
  isSAMLEnabled_: false,
  supportChannel_: null,

  GAIA_URL: 'https://accounts.google.com/',
  GAIA_PAGE_PATH: 'ServiceLogin?skipvpage=true&sarp=1&rm=hide',
  PARENT_PAGE: 'chrome://oobe/',
  SERVICE_ID: 'chromeoslogin',
  CONTINUE_URL: Authenticator.THIS_EXTENSION_ORIGIN + '/success.html',
  CONSTRAINED_FLOW_SOURCE: 'chrome',

  initialize: function() {
    var params = getUrlSearchParams(location.search);
    this.parentPage_ = params.parentPage || this.PARENT_PAGE;
    this.gaiaUrl_ = params.gaiaUrl || this.GAIA_URL;
    this.gaiaPath_ = params.gaiaPath || this.GAIA_PAGE_PATH;
    this.inputLang_ = params.hl;
    this.inputEmail_ = params.email;
    this.service_ = params.service || this.SERVICE_ID;
    this.continueUrl_ = params.continueUrl || this.CONTINUE_URL;
    this.desktopMode_ = params.desktopMode == '1';
    this.isConstrainedWindow_ = params.constrained == '1';
    this.initialFrameUrl_ = params.frameUrl || this.constructInitialFrameUrl_();
    this.initialFrameUrlWithoutParams_ = stripParams(this.initialFrameUrl_);

    document.addEventListener('DOMContentLoaded', this.onPageLoad_.bind(this));
    if (!this.desktopMode_) {
      // SAML is always enabled in desktop mode, thus no need to listen for
      // enableSAML event.
      document.addEventListener('enableSAML', this.onEnableSAML_.bind(this));
    }
  },

  isGaiaMessage_: function(msg) {
    // Not quite right, but good enough.
    return this.gaiaUrl_.indexOf(msg.origin) == 0 ||
           this.GAIA_URL.indexOf(msg.origin) == 0;
  },

  isInternalMessage_: function(msg) {
    return msg.origin == Authenticator.THIS_EXTENSION_ORIGIN;
  },

  isParentMessage_: function(msg) {
    return msg.origin == this.parentPage_;
  },

  constructInitialFrameUrl_: function() {
    var url = this.gaiaUrl_ + this.gaiaPath_;

    url = appendParam(url, 'service', this.service_);
    url = appendParam(url, 'continue', this.continueUrl_);
    if (this.inputLang_)
      url = appendParam(url, 'hl', this.inputLang_);
    if (this.inputEmail_)
      url = appendParam(url, 'Email', this.inputEmail_);
    if (this.isConstrainedWindow_)
      url = appendParam(url, 'source', this.CONSTRAINED_FLOW_SOURCE);
    return url;
  },

  onPageLoad_: function() {
    window.addEventListener('message', this.onMessage.bind(this), false);

    var gaiaFrame = $('gaia-frame');
    gaiaFrame.src = this.initialFrameUrl_;

    if (this.desktopMode_) {
      var handler = function() {
        this.onLoginUILoaded_();
        gaiaFrame.removeEventListener('load', handler);

        this.initDesktopChannel_();
      }.bind(this);
      gaiaFrame.addEventListener('load', handler);
    }
  },

  initDesktopChannel_: function() {
    this.supportChannel_ = new Channel();
    this.supportChannel_.connect('authMain');

    var channelConnected = false;
    this.supportChannel_.registerMessage('channelConnected', function() {
      channelConnected = true;

      this.supportChannel_.send({
        name: 'initDesktopFlow',
        gaiaUrl: this.gaiaUrl_,
        continueUrl: stripParams(this.continueUrl_),
        isConstrainedWindow: this.isConstrainedWindow_
      });
      this.supportChannel_.registerMessage(
          'switchToFullTab', this.switchToFullTab_.bind(this));
      this.supportChannel_.registerMessage(
          'completeLogin', this.completeLogin_.bind(this));

      this.onEnableSAML_();
    }.bind(this));

    window.setTimeout(function() {
      if (!channelConnected) {
        // Re-initialize the channel if it is not connected properly, e.g.
        // connect may be called before background script started running.
        this.initDesktopChannel_();
      }
    }.bind(this), 200);
  },

  /**
   * Invoked when the login UI is initialized or reset.
   */
  onLoginUILoaded_: function() {
    var msg = {
      'method': 'loginUILoaded'
    };
    window.parent.postMessage(msg, this.parentPage_);
  },

  /**
   * Invoked when the background script sends a message to indicate that the
   * current content does not fit in a constrained window.
   * @param {Object=} opt_extraMsg Optional extra info to send.
   */
  switchToFullTab_: function(msg) {
    var parentMsg = {
      'method': 'switchToFullTab',
      'url': msg.url
    };
    window.parent.postMessage(parentMsg, this.parentPage_);
  },

  /**
   * Invoked when the signin flow is complete.
   * @param {Object=} opt_extraMsg Optional extra info to send.
   */
  completeLogin_: function(opt_extraMsg) {
    var msg = {
      'method': 'completeLogin',
      'email': (opt_extraMsg && opt_extraMsg.email) || this.email_,
      'password': (opt_extraMsg && opt_extraMsg.password) ||
                  this.passwordBytes_,
      'usingSAML': this.isSAMLFlow_,
      'chooseWhatToSync': this.chooseWhatToSync_ || false,
      'skipForNow': opt_extraMsg && opt_extraMsg.skipForNow,
      'sessionIndex': opt_extraMsg && opt_extraMsg.sessionIndex
    };
    window.parent.postMessage(msg, this.parentPage_);
    if (this.isSAMLEnabled_)
      this.supportChannel_.send({name: 'resetAuth'});
  },

  /**
   * Invoked when 'enableSAML' event is received to initialize SAML support on
   * Chrome OS, or when initDesktopChannel_ is called on desktop.
   */
  onEnableSAML_: function() {
    this.isSAMLEnabled_ = true;
    this.isSAMLFlow_ = false;

    if (!this.supportChannel_) {
      this.supportChannel_ = new Channel();
      this.supportChannel_.connect('authMain');
    }

    this.supportChannel_.registerMessage(
        'onAuthPageLoaded', this.onAuthPageLoaded_.bind(this));
    this.supportChannel_.registerMessage(
        'onInsecureContentBlocked', this.onInsecureContentBlocked_.bind(this));
    this.supportChannel_.registerMessage(
        'apiCall', this.onAPICall_.bind(this));
    this.supportChannel_.send({
      name: 'setGaiaUrl',
      gaiaUrl: this.gaiaUrl_
    });
    if (!this.desktopMode_ && this.gaiaUrl_.indexOf('https://') == 0) {
      // Abort the login flow when content served over an unencrypted connection
      // is detected on Chrome OS. This does not apply to tests that explicitly
      // set a non-https GAIA URL and want to perform all authentication over
      // http.
      this.supportChannel_.send({
        name: 'setBlockInsecureContent',
        blockInsecureContent: true
      });
    }
  },

  /**
   * Invoked when the background page sends 'onHostedPageLoaded' message.
   * @param {!Object} msg Details sent with the message.
   */
  onAuthPageLoaded_: function(msg) {
    var isSAMLPage = msg.url.indexOf(this.gaiaUrl_) != 0;

    if (isSAMLPage && !this.isSAMLFlow_) {
      // GAIA redirected to a SAML login page. The credentials provided to this
      // page will determine what user gets logged in. The credentials obtained
      // from the GAIA login form are no longer relevant and can be discarded.
      this.isSAMLFlow_ = true;
      this.email_ = null;
      this.passwordBytes_ = null;
    }

    window.parent.postMessage({
      'method': 'authPageLoaded',
      'isSAML': this.isSAMLFlow_,
      'domain': extractDomain(msg.url)
    }, this.parentPage_);
  },

  /**
   * Invoked when the background page sends an 'onInsecureContentBlocked'
   * message.
   * @param {!Object} msg Details sent with the message.
   */
  onInsecureContentBlocked_: function(msg) {
    window.parent.postMessage({
      'method': 'insecureContentBlocked',
      'url': stripParams(msg.url)
    }, this.parentPage_);
  },

  /**
   * Invoked when one of the credential passing API methods is called by a SAML
   * provider.
   * @param {!Object} msg Details of the API call.
   */
  onAPICall_: function(msg) {
    var call = msg.call;
    if (call.method == 'initialize') {
      if (!Number.isInteger(call.requestedVersion) ||
          call.requestedVersion < Authenticator.MIN_API_VERSION_VERSION) {
        this.sendInitializationFailure_();
        return;
      }

      this.apiVersion_ = Math.min(call.requestedVersion,
                                  Authenticator.MAX_API_VERSION_VERSION);
      this.initialized_ = true;
      this.sendInitializationSuccess_();
      return;
    }

    if (call.method == 'add') {
      if (Authenticator.API_KEY_TYPES.indexOf(call.keyType) == -1) {
        console.error('Authenticator.onAPICall_: unsupported key type');
        return;
      }
      this.apiToken_ = call.token;
      this.email_ = call.user;
      this.passwordBytes_ = call.passwordBytes;
    } else if (call.method == 'confirm') {
      if (call.token != this.apiToken_)
        console.error('Authenticator.onAPICall_: token mismatch');
    } else {
      console.error('Authenticator.onAPICall_: unknown message');
    }
  },

  sendInitializationSuccess_: function() {
    this.supportChannel_.send({name: 'apiResponse', response: {
      result: 'initialized',
      version: this.apiVersion_,
      keyTypes: Authenticator.API_KEY_TYPES
    }});
  },

  sendInitializationFailure_: function() {
    this.supportChannel_.send({
      name: 'apiResponse',
      response: {result: 'initialization_failed'}
    });
  },

  onConfirmLogin_: function() {
    if (!this.isSAMLFlow_) {
      this.completeLogin_();
      return;
    }

    var apiUsed = !!this.passwordBytes_;

    // Retrieve the e-mail address of the user who just authenticated from GAIA.
    window.parent.postMessage({method: 'retrieveAuthenticatedUserEmail',
                               attemptToken: this.attemptToken_,
                               apiUsed: apiUsed},
                              this.parentPage_);

    if (!apiUsed) {
      this.supportChannel_.sendWithCallback(
          {name: 'getScrapedPasswords'},
          function(passwords) {
            if (passwords.length == 0) {
              window.parent.postMessage(
                  {method: 'noPassword', email: this.email_},
                  this.parentPage_);
            } else {
              window.parent.postMessage({method: 'confirmPassword',
                                         email: this.email_,
                                         passwordCount: passwords.length},
                                        this.parentPage_);
            }
          }.bind(this));
    }
  },

  maybeCompleteSAMLLogin_: function() {
    // SAML login is complete when the user's e-mail address has been retrieved
    // from GAIA and the user has successfully confirmed the password.
    if (this.email_ !== null && this.passwordBytes_ !== null)
      this.completeLogin_();
  },

  onVerifyConfirmedPassword_: function(password) {
    this.supportChannel_.sendWithCallback(
        {name: 'getScrapedPasswords'},
        function(passwords) {
          for (var i = 0; i < passwords.length; ++i) {
            if (passwords[i] == password) {
              this.passwordBytes_ = passwords[i];
              this.maybeCompleteSAMLLogin_();
              return;
            }
          }
          window.parent.postMessage(
              {method: 'confirmPassword', email: this.email_},
              this.parentPage_);
        }.bind(this));
  },

  onMessage: function(e) {
    var msg = e.data;
    if (msg.method == 'attemptLogin' && this.isGaiaMessage_(e)) {
      this.email_ = msg.email;
      this.passwordBytes_ = msg.password;
      this.attemptToken_ = msg.attemptToken;
      this.chooseWhatToSync_ = msg.chooseWhatToSync;
      this.isSAMLFlow_ = false;
      if (this.isSAMLEnabled_)
        this.supportChannel_.send({name: 'startAuth'});
    } else if (msg.method == 'clearOldAttempts' && this.isGaiaMessage_(e)) {
      this.email_ = null;
      this.passwordBytes_ = null;
      this.attemptToken_ = null;
      this.isSAMLFlow_ = false;
      this.onLoginUILoaded_();
      if (this.isSAMLEnabled_)
        this.supportChannel_.send({name: 'resetAuth'});
    } else if (msg.method == 'setAuthenticatedUserEmail' &&
               this.isParentMessage_(e)) {
      if (this.attemptToken_ == msg.attemptToken) {
        this.email_ = msg.email;
        this.maybeCompleteSAMLLogin_();
      }
    } else if (msg.method == 'confirmLogin' && this.isInternalMessage_(e)) {
      if (this.attemptToken_ == msg.attemptToken)
        this.onConfirmLogin_();
      else
        console.error('Authenticator.onMessage: unexpected attemptToken!?');
    } else if (msg.method == 'verifyConfirmedPassword' &&
               this.isParentMessage_(e)) {
      this.onVerifyConfirmedPassword_(msg.password);
    } else if (msg.method == 'redirectToSignin' &&
               this.isParentMessage_(e)) {
      $('gaia-frame').src = this.constructInitialFrameUrl_();
    } else {
       console.error('Authenticator.onMessage: unknown message + origin!?');
    }
  }
};

Authenticator.getInstance().initialize();
